#include "native_mode.h"
#include "encoders.h"
#include "input.h" // VELOCITY_CALC_METHOD (already reached via encoders.h, named here on purpose)

// Native mode lives entirely on the system channel, in a CC block of its own.
// - Base 64 clears bank change (CC 0-3 with 4 banks, or 0-7 with 8) and the side
//   switches (CC 8-31 with 4 banks, or 8-55 with 8). Thus the block is correct in
//   both bank modes. Shift shares the channel but uses notes, so it never
//   collides.
#define NATIVE_MODE_CC_BASE 64
#define NATIVE_MODE_CC_ENC_POS (NATIVE_MODE_CC_BASE)                             // 64-79, both directions
#define NATIVE_MODE_CC_ENC_SWITCH (NATIVE_MODE_CC_ENC_POS + PHYSICAL_ENCODERS)   // 80-95
#define NATIVE_MODE_CC_SIDE_SWITCH (NATIVE_MODE_CC_ENC_SWITCH + PHYSICAL_ENCODERS) // 96-101
#define NATIVE_MODE_SIDE_SWITCHES 6 // matches side_sw_settings_t.sw_action[6]

#if (NATIVE_MODE_CC_SIDE_SWITCH + NATIVE_MODE_SIDE_SWITCHES) > 128
#error "Native mode CC block overflows the 7-bit CC range"
#endif

enum // native mode SysEx commands
{
	NATIVE_MODE_SYSEX_COMMAND_SET_NATIVE_MODE = 0x0,
	NATIVE_MODE_SYSEX_COMMAND_ENC_CONFIG = 0x1,
};

enum // native mode SysEx encoder config commands
{
	NATIVE_MODE_SYSEX_COMMAND_ENC_INDICATOR_CONFIG = 0x0,
	NATIVE_MODE_SYSEX_COMMAND_ENC_SWITCH_CONFIG = 0x1,
	NATIVE_MODE_SYSEX_COMMAND_ENC_INPUT_CONFIG = 0x2,
};

enum // native mode encoder input modes
{
	NATIVE_MODE_ENC_INPUT_MODE_RAW = 0,      // one step per detent, the default
	NATIVE_MODE_ENC_INPUT_MODE_VELOCITY = 1, // step scales with turn speed
	NUM_NATIVE_MODE_ENC_INPUT_MODES          // this must always be the last entry
};

typedef struct
{
	uint8_t display_type;
	uint8_t has_detent;
	uint8_t detent_color;
} native_mode_enc_indicator_config_t;

typedef struct
{
	uint8_t color_r;
	uint8_t color_g;
	uint8_t color_b;
} native_mode_enc_switch_config_t;

typedef struct
{
	uint8_t mode;
	uint8_t reserved;
	uint8_t max_step; // 0: firmware default (uncapped) / 1-17: explicit cap
} native_mode_enc_input_config_t;

typedef struct
{
	bool is_active;
	native_mode_enc_indicator_config_t enc_indicator_configs[PHYSICAL_ENCODERS];
	native_mode_enc_switch_config_t enc_switch_configs[PHYSICAL_ENCODERS];
	native_mode_enc_input_config_t enc_input_configs[PHYSICAL_ENCODERS];
	uint8_t indicator_value_buffer[PHYSICAL_ENCODERS]; // Holds the 7 bit indicator value (native mode)
} native_mode_state_t;

native_mode_state_t nm_state;

bool native_mode_is_active(void)
{
	return nm_state.is_active;
}

// returns true if changed
static bool native_mode_set_active(bool active)
{
	if (active == native_mode_is_active())
		return false; // no change
	nm_state.is_active = active;
	return true;
}

static inline bool is_in_range(uint8_t value, uint8_t min, uint8_t max) { return value >= min && value <= max; }
static bool is_valid_enc_index(uint8_t value) { return value < PHYSICAL_ENCODERS; }
static bool is_valid_enc_indicator_display_type(uint8_t value) { return value < NUM_DISPLAY_TYPES; }
static bool is_sysex_byte_valid_bool(uint8_t value) { return value == 0 || value == 1; }
static bool sysex_byte_to_bool(uint8_t value) { return value != 0; }
static bool is_valid_enc_color(uint8_t value) { return is_in_range(value, 0, 0x7F); }

static void native_mode_reset_enc_indicator_config(uint8_t dest_index)
{
	static const native_mode_enc_indicator_config_t ENC_INDICATOR_CONFIG_DEFAULT = {0, 0, 0};
	nm_state.enc_indicator_configs[dest_index] = ENC_INDICATOR_CONFIG_DEFAULT;
}
static bool native_mode_is_enc_indicator_config_valid(const native_mode_enc_indicator_config_t *const config)
{
	return is_valid_enc_indicator_display_type(config->display_type) && is_sysex_byte_valid_bool(config->has_detent) && is_valid_enc_color(config->detent_color);
}

static void native_mode_reset_enc_switch_config(uint8_t dest_index)
{
	static const native_mode_enc_switch_config_t ENC_SWITCH_CONFIG_DEFAULT = {0, 0, 0};
	nm_state.enc_switch_configs[dest_index] = ENC_SWITCH_CONFIG_DEFAULT;
}
static bool native_mode_is_enc_switch_config_valid(const native_mode_enc_switch_config_t *const config)
{
	return is_valid_enc_color(config->color_r) && is_valid_enc_color(config->color_g) && is_valid_enc_color(config->color_b);
}

static void native_mode_reset_enc_input_config(uint8_t dest_index)
{
	static const native_mode_enc_input_config_t ENC_INPUT_CONFIG_DEFAULT = {NATIVE_MODE_ENC_INPUT_MODE_RAW, 0, 0};
	nm_state.enc_input_configs[dest_index] = ENC_INPUT_CONFIG_DEFAULT;
}
static bool native_mode_is_enc_input_config_valid(const native_mode_enc_input_config_t *const config)
{
	return config->mode < NUM_NATIVE_MODE_ENC_INPUT_MODES && config->reserved == 0 && config->max_step <= ENC_REL_VELOCITY_STEP_MAX;
}

#define NATIVE_MODE_SYSEX_COMMAND_DATA_ENC_INDICATOR_CFG_SIZE 4
typedef union
{ // Each of these fields is designed to be written to directly from MIDI SysEx Data
	struct
	{ // - so you can only use 7-bits of these uint8_t's to store data.
		uint8_t index;
		native_mode_enc_indicator_config_t config;
	};
	uint8_t bytes[NATIVE_MODE_SYSEX_COMMAND_DATA_ENC_INDICATOR_CFG_SIZE];
} native_mode_sysex_command_data_enc_indicator_config_t;
static bool native_mode_parse_sysex_enc_indicator_config(uint8_t length, uint8_t *buffer)
{
	if (length < NATIVE_MODE_SYSEX_COMMAND_DATA_ENC_INDICATOR_CFG_SIZE)
		return false;

	native_mode_sysex_command_data_enc_indicator_config_t config_data = *(native_mode_sysex_command_data_enc_indicator_config_t *)buffer;

	if (!is_valid_enc_index(config_data.index))
		return false;
	if (!native_mode_is_enc_indicator_config_valid(&config_data.config))
		return false;

	nm_state.enc_indicator_configs[config_data.index] = config_data.config;
	// native_mode_set_enc_indicator_config(index, enc_indicator_cfg_buffer);
	return true;
}

#define NATIVE_MODE_SYSEX_COMMAND_DATA_ENC_SWITCH_CFG_SIZE 4
typedef union
{ // Each of these fields is designed to be written to directly from MIDI SysEx Data
	struct
	{ // - so you can only use 7-bits of these uint8_t's to store data.
		uint8_t index;
		native_mode_enc_switch_config_t config;
	};
	uint8_t bytes[NATIVE_MODE_SYSEX_COMMAND_DATA_ENC_SWITCH_CFG_SIZE];
} native_mode_sysex_command_data_enc_swtich_config_t;
static bool native_mode_parse_sysex_enc_switch_config(uint8_t length, uint8_t *buffer)
{
	if (length < NATIVE_MODE_SYSEX_COMMAND_DATA_ENC_SWITCH_CFG_SIZE)
		return false;

	native_mode_sysex_command_data_enc_swtich_config_t config_data = *(native_mode_sysex_command_data_enc_swtich_config_t *)buffer;

	if (!is_valid_enc_index(config_data.index))
		return false;
	if (!native_mode_is_enc_switch_config_valid(&config_data.config))
		return false;

	nm_state.enc_switch_configs[config_data.index] = config_data.config;
	return true;
}

#define NATIVE_MODE_SYSEX_COMMAND_DATA_ENC_INPUT_CFG_SIZE 4
typedef union
{ // Each of these fields is designed to be written to directly from MIDI SysEx Data
	struct
	{ // - so you can only use 7-bits of these uint8_t's to store data.
		uint8_t index;
		native_mode_enc_input_config_t config;
	};
	uint8_t bytes[NATIVE_MODE_SYSEX_COMMAND_DATA_ENC_INPUT_CFG_SIZE];
} native_mode_sysex_command_data_enc_input_config_t;
static bool native_mode_parse_sysex_enc_input_config(uint8_t length, uint8_t *buffer)
{
	if (length < NATIVE_MODE_SYSEX_COMMAND_DATA_ENC_INPUT_CFG_SIZE)
		return false;

	native_mode_sysex_command_data_enc_input_config_t config_data = *(native_mode_sysex_command_data_enc_input_config_t *)buffer;

	if (!is_valid_enc_index(config_data.index))
		return false;
	if (!native_mode_is_enc_input_config_valid(&config_data.config))
		return false;

	nm_state.enc_input_configs[config_data.index] = config_data.config;
	return true;
}

static void native_mode_reset_encoder_configs(void)
{
	for (int i = 0; i < PHYSICAL_ENCODERS; i++)
	{
		native_mode_reset_enc_indicator_config(i);
		native_mode_reset_enc_switch_config(i);
		native_mode_reset_enc_input_config(i);
	}
}

static void native_mode_handle_sysex_set_native_mode(uint8_t length, uint8_t *buffer)
{
	if (length != 1)
		return;
	const uint8_t value = *buffer;
	if (!is_sysex_byte_valid_bool(value))
		return;
	if (native_mode_set_active(sysex_byte_to_bool(value)))
	{
		if (native_mode_is_active())
			native_mode_reset_encoder_configs();
		refresh_display();
	}
}

void native_mode_toggle(void)
{
	// Flip the active state (always a change), applying the same entry
	// behaviour as the SysEx "set native mode" command.
	if (native_mode_set_active(!native_mode_is_active()))
	{
		if (native_mode_is_active())
			native_mode_reset_encoder_configs();
		refresh_display();
	}
}

static void native_mode_handle_sysex_enc_config(uint8_t length, uint8_t *buffer)
{
	if (!native_mode_is_active())
		return;
	while (length > 0)
	{
		switch (*buffer)
		{
		case NATIVE_MODE_SYSEX_COMMAND_ENC_INDICATOR_CONFIG:
			if (!native_mode_parse_sysex_enc_indicator_config(--length, ++buffer))
				return;
			length -= NATIVE_MODE_SYSEX_COMMAND_DATA_ENC_INDICATOR_CFG_SIZE;
			buffer += NATIVE_MODE_SYSEX_COMMAND_DATA_ENC_INDICATOR_CFG_SIZE;
			break;
		case NATIVE_MODE_SYSEX_COMMAND_ENC_SWITCH_CONFIG:
			if (!native_mode_parse_sysex_enc_switch_config(--length, ++buffer))
				return;
			length -= NATIVE_MODE_SYSEX_COMMAND_DATA_ENC_SWITCH_CFG_SIZE;
			buffer += NATIVE_MODE_SYSEX_COMMAND_DATA_ENC_SWITCH_CFG_SIZE;
			break;
		case NATIVE_MODE_SYSEX_COMMAND_ENC_INPUT_CONFIG:
			if (!native_mode_parse_sysex_enc_input_config(--length, ++buffer))
				return;
			length -= NATIVE_MODE_SYSEX_COMMAND_DATA_ENC_INPUT_CFG_SIZE;
			buffer += NATIVE_MODE_SYSEX_COMMAND_DATA_ENC_INPUT_CFG_SIZE;
			break;
		default:
			return;
		}
	}
	refresh_display();
}

void native_mode_handle_sysex_command(uint8_t length, uint8_t *buffer)
{
	if (length == 0)
		return;

	switch (*buffer)
	{
	case NATIVE_MODE_SYSEX_COMMAND_SET_NATIVE_MODE:
		native_mode_handle_sysex_set_native_mode(--length, ++buffer);
		break;
	case NATIVE_MODE_SYSEX_COMMAND_ENC_CONFIG:
		native_mode_handle_sysex_enc_config(--length, ++buffer);
		break;
	}
}

bool native_mode_consume_midi_event(uint8_t type, uint8_t channel, uint8_t number, uint8_t value)
{
	if (!native_mode_is_active() || type != SEND_CC || channel != midi_system_channel)
		return false;
	// Bound the CC to our own block. Returning false outside it is required, not
	//   just defensive: the system channel also carries bank change and side switch
	//   traffic, which must still reach its own handlers.
	if (!is_in_range(number, NATIVE_MODE_CC_ENC_POS, NATIVE_MODE_CC_ENC_POS + PHYSICAL_ENCODERS - 1))
		return false;

	nm_state.indicator_value_buffer[number - NATIVE_MODE_CC_ENC_POS] = value;
	return true;
}

#define NATIVE_MODE_BITSHIFT_SYSEX_TO_BYTE 1
bool native_mode_update_encoder_display_single(const uint8_t idx)
{
	if (!native_mode_is_active())
		return false;
	set_encoder_indicator_level(
		idx, nm_state.indicator_value_buffer[idx],
		nm_state.enc_indicator_configs[idx].has_detent,
		nm_state.enc_indicator_configs[idx].display_type,
		nm_state.enc_indicator_configs[idx].detent_color,
		0x7F);
	uint32_t rgb_color =
		((uint32_t)nm_state.enc_switch_configs[idx].color_r << (16 + NATIVE_MODE_BITSHIFT_SYSEX_TO_BYTE)) | ((uint32_t)nm_state.enc_switch_configs[idx].color_g << (8 + NATIVE_MODE_BITSHIFT_SYSEX_TO_BYTE)) | ((uint32_t)nm_state.enc_switch_configs[idx].color_b << (0 + NATIVE_MODE_BITSHIFT_SYSEX_TO_BYTE));
	build_rgb(idx, rgb_color, false);
	return true;
}

bool native_mode_process_encoder_input_rotary(const uint8_t idx, const int16_t delta, const uint16_t cycle_count)
{
	if (!native_mode_is_active())
		return false;

	int16_t value = 64 + delta;
#if VELOCITY_CALC_METHOD > VELOCITY_CALC_M_NONE
	const native_mode_enc_input_config_t *const config = &nm_state.enc_input_configs[idx];
	if (config->mode == NATIVE_MODE_ENC_INPUT_MODE_VELOCITY)
	{
		// Same curve as a relative encoder set to velocity sensitive in a normal bank.
		const uint8_t tick_count = (uint8_t)(delta < 0 ? -delta : delta);
		const uint16_t base_multiplier = convert_ticks_per_scan_to_value_multiplier(tick_count, cycle_count);
		uint8_t step = ENC_REL_VELOCITY_STEP(base_multiplier);
		const uint8_t max_step = config->max_step ? config->max_step : ENC_REL_VELOCITY_STEP_MAX;
		if (step > max_step)
			step = max_step;
		value = 64 + delta * step;
	}
#endif
	// Clamp to the 7-bit binary offset range. A multi-tick event at a high
	//   velocity multiplier can push the raw sum past 127 and wrap the direction.
	if (value > ENC_REL_OUTPUT_MAX)
		value = ENC_REL_OUTPUT_MAX;
	else if (value < ENC_REL_OUTPUT_MIN)
		value = ENC_REL_OUTPUT_MIN;

	midi_stream_raw_cc(midi_system_channel, NATIVE_MODE_CC_ENC_POS + idx, (uint8_t)value);
	return true;
}

bool native_mode_process_encoder_input_switch_pressed(const uint8_t idx, const bool pressed)
{
	if (!native_mode_is_active())
		return false;
	midi_stream_raw_cc(midi_system_channel, NATIVE_MODE_CC_ENC_SWITCH + idx, pressed ? 0x7F : 0x0);
	return true;
}

bool native_mode_process_side_switch_pressed(uint8_t idx, bool pressed)
{
	if (!native_mode_is_active())
		return false;
	midi_stream_raw_cc(midi_system_channel, NATIVE_MODE_CC_SIDE_SWITCH + idx, pressed ? 0x7F : 0);
	return true;
}
