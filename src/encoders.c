/*
 * encoders.c
 *
 * Created: 6/28/2013 1:48:45 PM
 *  Author: Michael
 *
 *  Encoder Management & Control 
 *  This takes care of interfacing between the hardware input (encoders & switches)
 *  and the MIDI output. Because updating the display is such a processor intensive
 *  activity we avoid updating the display unless it should have changed. This is 
 *  achieved by buffers which hold the previous state of all elements, Encoder Value,
 *  RGB color setting, RGB animation setting for the current bank. Each main loop cycle
 *  the prev buffer is compared with the MIDI buffer of the selected bank and any changes
 *  are added to the display update cue. 1 display update task is performed per main loop
 * 
 * DJTT - Midi Fighter Twister - Embedded Software License
 * Copyright (c) 2026: DJ TechTools
 * Permission is hereby granted, free of charge, to any person owning or possessing 
 * a DJ TechTools Midi Fighter Twister Hardware Device to view and modify this source 
 * code for personal use. Person may not publish, distribute, sublicense, or sell 
 * the source code (modified or un-modified). Person may not use this source code 
 * or any diminutive works for commercial purposes. The permission to use this source 
 * code is also subject to the following conditions:
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, 
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,  FITNESS FOR A 
 * PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT 
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION 
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE 
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */ 

#include <encoders.h>
#include "native_mode.h"
#include <side_switch.h>

// Constants
const uint16_t encoder_detent_limit_low = 6240; // 6240 - ok for all modes (6250 causes high res mode to hit '64' on the way up)
const uint16_t encoder_detent_limit_high = 6450; // 6450 - ok for all modes, (note: scale_encoder_value adds 50 to these "raw" values in velocity calc)

// Locals

// Bank scaled storage ---------------------------------------------------------
//
// The live bank count is a runtime value, but C needs an array size at compile
// time. The large bank scaled arrays are pointers into one arena, which
// encoders_alloc() divides at boot. The arena is large enough for 8 banks.
//
// In 4 bank mode only one half of the arena is in use. The sequencer puts its
// large data in the other half. Refer to bank_arena_spare(). One binary
// holds both modes, and the live mode holds all of the storage.
//
// No use site changes, because arr[i] and ptr[i] have the same syntax, and
// because no site does sizeof() or memcpy() on these arrays.

#define ARENA_BYTES_PER_BANK	400		// refer to encoders_alloc()
#define ARENA_SIZE				(MAX_BANKS * ARENA_BYTES_PER_BANK)

// ARENA_BYTES_PER_BANK must agree with what encoders_alloc() gives out. If it is
// too small, the last bank runs past the arena. If it is too large,
// bank_arena_spare() gives the sequencer less space than is available.
_Static_assert(ARENA_BYTES_PER_BANK == (32 * 2) + (16 * 2) + (16 * ENC_CFG_SIZE) + (16 * 4),
               "ARENA_BYTES_PER_BANK disagrees with encoders_alloc()");

static uint8_t bank_arena[ARENA_SIZE];

uint8_t g_num_banks = MIN_BANKS;				 // 4 or 8, refer to encoders_alloc()

uint8_t (*indicator_value_buffer)[16];			 // Holds the 7 bit indicator value

uint8_t (*switch_color_buffer)[16];				 // Holds the switch color setting 

uint8_t (*switch_animation_buffer)[16];			 // Holds the switch animation setting
uint8_t (*encoder_animation_buffer)[16];		 // Holds the encoder animation setting

uint16_t (*enc_switch_midi_state)[16];			 // Holds the switch states

// These three are one 16 bit word for each bank. At 8 banks that is 6 bytes
// more than at 4 banks, so they stay as arrays. In the arena they cost more in
// pointers and code than they save.
uint16_t enc_switch_toggle_state[MAX_BANKS];	 // 16-Encoders Per Bank/ 1-bit per Encoder
							// toggle state is not tied directly 
							// to the MIDI state we use this to track.
												 
uint16_t switch_color_overide[MAX_BANKS];		 // Bit field to track if color override
												 // was received for switch color
												 
uint16_t enc_indicator_overide[MAX_BANKS];       // Bit field to track if indicator override
												 // was received for indicator
												 
											
// Shift Mode Variables

uint16_t shift_mode_switch_state[2];			// Bit field to track the switch state for											    
uint16_t shift_mode_midi_override[2];		    // Bit field to track the midi state for the
												// two shift pages

// - this is necessary because some parameters are shared between 'shifted' and 'non-shifted' encoders.

int8_t encoder_detent_counter[PHYSICAL_ENCODERS]; 
//~ input_map_t input_map[BANKED_ENCODERS]; // !Summer2016Update: Removed in favor of expanding encoder_settings
int16_t  *raw_encoder_value; // !Summer2016Update: Expanded to store values for all banks and shifted encoders
// - indexed by Virtual Encoder ID
// - the unshifted encoders of all banks come first, then the shifted encoders
//   of all banks. With 4 banks that is 0-63 unshifted and 64-127 shifted. With
//   8 banks it is 0-127 unshifted and 128-255 shifted.

static uint8_t encoder_bank = 0;
//static encoder_config_t encoder_settings[PHYSICAL_ENCODERS];
encoder_config_t *encoder_settings;
//static encoder_config_t encoder_settings_transfer_buffer[1];


// Private Functions
uint8_t get_virtual_encoder_id (uint8_t bank, uint8_t encoder_id);
void encoderConfig(encoder_config_t *settings);
bool switch_value_from_encoder_value_enabled(uint8_t banked_encoder_idx);
uint8_t adjust_switch_value_by_encoder_value(uint8_t banked_encoder_idx, uint8_t value, bool state);

void send_element_midi(enc_control_type_t type, uint8_t banked_encoder_index, uint8_t value, bool state);
void send_encoder_midi(uint8_t banked_encoder_idx, uint8_t value, bool state, bool shifted);
//void send_encoder_pitchbend(uint8_t banked_encoder_idx, uint16_t value, bool shifted);
uint8_t scale_encoder_value(int16_t value);
int16_t clamp_encoder_raw_value(int16_t value);
bool encoder_is_in_detent(int16_t value);
static void process_vpot_ring_update(uint8_t idx, uint8_t ring, uint8_t rx_msg_shifted_mapping);
bool color_overide_active(uint8_t bank, uint8_t encoder);
// - Encoder Banks
bool encoder_maps_match(uint8_t this_banked_encoder_id, uint8_t that_banked_encoder_id);
void transfer_this_encoder_value_to_other_banks(uint8_t current_bank, uint8_t encoder_id);
void transfer_encoder_values_to_other_banks(uint8_t current_bank);
// - Animation Related
bool animation_is_encoder_indicator(uint8_t animation_value);
bool animation_is_switch_rgb(uint8_t animation_value);
bool animation_buffer_conflict_exists(uint8_t bank, uint8_t encoder);

#if VELOCITY_CALC_METHOD > VELOCITY_CALC_M_NONE 
	uint16_t convert_ticks_per_scan_to_value_multiplier(uint8_t tick_count, uint16_t cycles_count);
	void send_midi_velocity_sensitive_encoder(uint8_t encoder_id, uint16_t output_value);
#endif


/** 
 *  Initializes all encoder buffers and EEPROM settings
**/

uint8_t get_virtual_encoder_id (uint8_t bank, uint8_t encoder_id){
	uint8_t virtual_encoder_id = encoder_id + bank*16;
	uint8_t this_bank = bank;
	if (encoder_is_in_shift_state(this_bank, encoder_id))
		{virtual_encoder_id += BANKED_ENCODERS;}
	return virtual_encoder_id;
}

// Writes an encoder's raw value into the indicator (LED ring) display buffer.
// Relative encoders are skipped on purpose: they have no local value, so
// raw_encoder_value only ever carries detent bookkeeping for them (the detent
// handler parks it at encoder_detent_limit_low/high on escape and the relative
// path never moves it again). Their ring must keep whatever the host last sent.
static void sync_indicator_from_raw(uint8_t bank, uint8_t encoder, uint8_t virtual_encoder_id)
{
	uint8_t banked_encoder_id = encoder + bank*PHYSICAL_ENCODERS;
	uint8_t midi_type = encoder_settings[banked_encoder_id].encoder_midi_type;
	if (midi_type == SEND_REL_ENC || midi_type == SEND_REL_ENC_MOUSE_EMU_DRAG ||
		midi_type == SEND_REL_ENC_MOUSE_EMU_SCROLL ||
		midi_type == SEND_MCU_VPOT) {
		return;
	}
	indicator_value_buffer[bank][encoder] = (uint8_t)(raw_encoder_value[virtual_encoder_id]/100);
}

// Returns the value of the CC0 or CC32 knob on the current page on this channel, or -1 if none.
// Reads raw_encoder_value, not indicator_value_buffer: both layers share that buffer, and the
// detent handler writes 63 into it without sending.
static int8_t bank_select_value(uint8_t channel, uint8_t cc)
{
	uint8_t first = encoder_bank * PHYSICAL_ENCODERS;
	for (uint8_t i = 0; i < PHYSICAL_ENCODERS; ++i) {
		const encoder_config_t *cfg = &encoder_settings[first + i];
		if (cfg->encoder_midi_type != SEND_CC || cfg->encoder_midi_number != cc) { continue; }
		bool on_base  = (cfg->encoder_midi_channel == channel);
		bool on_shift = (cfg->encoder_shift_midi_channel == channel);
		if (!on_base && !on_shift) { continue; }
		// If both layers are on this channel, use the layer on display
		bool use_shift = on_shift && (!on_base || encoder_is_in_shift_state(encoder_bank, i));
		uint8_t idx = first + i + (use_shift ? BANKED_ENCODERS : 0);
		return (int8_t)scale_encoder_value(raw_encoder_value[idx]);
	}
	return -1;
}

// Sends Bank Select MSB, then LSB, then Program Change, in the order the MIDI spec requires.
// A bank select is only sent if a knob on the page supplies it.
static void send_program_selection(uint8_t channel, uint8_t program)
{
	int8_t msb = bank_select_value(channel, 0);
	int8_t lsb = bank_select_value(channel, 32);
	if (msb >= 0) { midi_stream_raw_cc(channel, 0, (uint8_t)msb); }
	if (lsb >= 0) { midi_stream_raw_cc(channel, 32, (uint8_t)lsb); }
	midi_stream_raw_program_change(channel, program);
}

// encoder_maps_match: Compares MIDI Mapping Parameters common to both 'shift' and 'unshifted' states.
// - returns true if the mappings match, otherwise returns false.
// True when this encoder's shift layer borrows the switch MIDI number. The
// switch then sends nothing, which is what frees the number. Refer to
// EE_DISABLE_SHIFT_SW_MIDI.
static bool shift_number_active(uint8_t banked_encoder_id) {
	if (!global_disable_shift_sw_midi) { return false; }
	uint8_t action = encoder_settings[banked_encoder_id].switch_action_type;
	return (action == ENC_SHIFT_HOLD) || (action == ENC_SHIFT_TOGGLE);
}

// Fader touch state, per physical encoder. The note is stored and not
// recomputed, so a bank change during a gesture cannot leave the host holding a
// touch on a strip the device no longer sends.
static uint8_t mcu_touch_note[PHYSICAL_ENCODERS];
static uint8_t mcu_touch_timer[PHYSICAL_ENCODERS];

// Mackie faders are 14 bit pitch bend where raw_encoder_value is 0..12700.
static uint16_t raw_to_pitchbend(int16_t raw) {
	if (raw < 0) { raw = 0; }
	if (raw > 12700) { raw = 12700; }
	return (uint16_t)(((uint32_t)raw * MCU_FADER_MAX) / 12700u);
}

static int16_t pitchbend_to_raw(uint16_t bend) {
	if (bend > MCU_FADER_MAX) { bend = MCU_FADER_MAX; }
	return (int16_t)(((uint32_t)bend * 12700u + (MCU_FADER_MAX / 2)) / MCU_FADER_MAX);
}

// Surface state, read from the lamps the host sends back. It owns them, so
// the echo is right even when the user changes Flip in the DAW or on another
// surface. Both start off, because the device cannot know until the host sends
// its first refresh.
static bool mcu_flip_on = false;
static uint8_t mcu_assignment = MCU_ASSIGN_UNKNOWN;

// The color a V-Pot's assignment mode calls for, or -1 to leave the encoder its
// own color. The host names the mode with the row of lamps it lights, so before
// it has said anything there is nothing to show.
static int8_t mcu_mode_color(uint8_t banked_encoder_id)
{
	if (encoder_settings[banked_encoder_id].encoder_midi_type != SEND_MCU_VPOT) { return -1; }

	// Only the eight strip V-Pots follow the page. The same output type also
	// drives the jog wheel, which belongs to no strip and no page.
	{
		uint8_t number = encoder_settings[banked_encoder_id].encoder_midi_number;
		if ((number < MCU_VPOT_CC_FIRST) || (number >= (MCU_VPOT_CC_FIRST + MCU_STRIPS))) { return -1; }
	}

	// The index means a different color in each map, so read the one in use.
	bool ext = (activeColorMap == colorMap64);

	switch (mcu_assignment) {
		case MCU_ASSIGN_IO:      return ext ? MCU_COLOR_MODE_IO_64      : MCU_COLOR_MODE_IO;
		case MCU_ASSIGN_SENDS:   return ext ? MCU_COLOR_MODE_SENDS_64   : MCU_COLOR_MODE_SENDS;
		case MCU_ASSIGN_PAN:     return ext ? MCU_COLOR_MODE_PAN_64     : MCU_COLOR_MODE_PAN;
		case MCU_ASSIGN_PLUGINS: return ext ? MCU_COLOR_MODE_PLUGINS_64 : MCU_COLOR_MODE_PLUGINS;
		default:                 return -1;
	}
}

// Watches the lamp feedback without consuming it, since an encoder may also be
// mapped to these notes.
void mcu_observe_feedback_note(uint8_t channel, uint8_t note, uint8_t velocity, bool note_on)
{
	if (channel != MCU_TOUCH_CHANNEL) { return; }

	bool lit = note_on && (velocity > 0);

	if (note == MCU_FLIP_NOTE) {
		mcu_flip_on = lit;
	} else if ((note >= MCU_ASSIGN_FIRST) && (note <= MCU_ASSIGN_LAST) && lit) {
		mcu_assignment = note;  // the host lights one at a time
	}
}

// A Mackie fader's detent is tactile only. Passing has_detent to the
// display would blank its ring at mid travel, because build_indicator_pattern()
// draws the detent dot alone at position 63 and 64.
static bool encoder_shows_detent(uint8_t banked_encoder_id)
{
	if (encoder_settings[banked_encoder_id].encoder_midi_type == SEND_MCU_FADER) { return false; }
	return encoder_settings[banked_encoder_id].has_detent;
}

// The indicator type a fader draws with.
//
// Flip swaps the fader and the V-Pot, and the assignment mode says what the
// V-Pot holds, so a flipped fader in the pan assignment holds a pan and reads
// from the center. Both facts come from the host: note 50 for flip and notes 40
// to 45 for the mode. mcu_fader_detent_raw() puts the tactile detent there,
// and this keeps the ring from disagreeing with the feel.
//
// The ring byte cannot answer this. Captured under flip, it changed from boost
// and cut to wrap, so it follows what the V-Pot took on and never describes the
// fader.
static uint16_t mcu_fader_display_type(uint8_t banked_encoder_id)
{
	if ((encoder_settings[banked_encoder_id].encoder_midi_type == SEND_MCU_FADER)
	 && mcu_flip_on && (mcu_assignment == MCU_ASSIGN_PAN)) {
		return CENTER_BAR;
	}
	return encoder_settings[banked_encoder_id].indicator_display_type;
}

// Where a fader detents, in raw units, or -1 for no detent.
//
// Only a flipped fader in the pan assignment has one. Flip hands it whatever the
// V-Pot held, a pan centers, and a send or a plugin parameter does not.
static int16_t mcu_fader_detent_raw(uint8_t banked_encoder_id)
{
	if (encoder_settings[banked_encoder_id].encoder_midi_type != SEND_MCU_FADER) { return -1; }
	if (!encoder_settings[banked_encoder_id].has_detent) { return -1; }

	return (mcu_flip_on && (mcu_assignment == MCU_ASSIGN_PAN)) ? MCU_CENTER_RAW : -1;
}

// Starts or extends a fader touch. A real fader reports the hand, so this
// follows movement and not the push switch.
static void mcu_fader_touch(uint8_t encoder_idx, uint8_t banked_encoder_id, bool shifted)
{
	uint8_t channel = shifted ? encoder_settings[banked_encoder_id].encoder_shift_midi_channel
	                          : encoder_settings[banked_encoder_id].encoder_midi_channel;
	uint8_t note = MCU_TOUCH_NOTE_BASE + channel;

	if (mcu_touch_note[encoder_idx] == MCU_TOUCH_NONE) {
		mcu_touch_note[encoder_idx] = note;
		midi_stream_raw_note(MCU_TOUCH_CHANNEL, note, true, 127);
	}
	mcu_touch_timer[encoder_idx] = MCU_TOUCH_HOLD_SCANS;
}

// Releases a touch once the encoder has been still. Call one time per scan for
// each encoder.
static void mcu_fader_touch_tick(uint8_t encoder_idx)
{
	if (mcu_touch_note[encoder_idx] == MCU_TOUCH_NONE) { return; }
	if (mcu_touch_timer[encoder_idx] > 0) {
		mcu_touch_timer[encoder_idx]--;
		return;
	}
	midi_stream_raw_note(MCU_TOUCH_CHANNEL, mcu_touch_note[encoder_idx], false, 0);
	mcu_touch_note[encoder_idx] = MCU_TOUCH_NONE;
}

// The MIDI number an encoder sends on. The shift layer takes the switch number
// when the switch gave its mapping up.
static uint8_t encoder_output_number(uint8_t banked_encoder_id, bool shifted) {
	if (shifted && shift_number_active(banked_encoder_id)) {
		return encoder_settings[banked_encoder_id].switch_midi_number;
	}
	return encoder_settings[banked_encoder_id].encoder_midi_number;
}

bool encoder_maps_match(uint8_t this_banked_encoder_id, uint8_t that_banked_encoder_id) {
	if (encoder_settings[this_banked_encoder_id].encoder_midi_number != encoder_settings[that_banked_encoder_id].encoder_midi_number)
		{ return false; }
	if (encoder_settings[this_banked_encoder_id].encoder_midi_type != encoder_settings[that_banked_encoder_id].encoder_midi_type)
		{ return false; }
	if (encoder_settings[this_banked_encoder_id].encoder_midi_type == SEND_REL_ENC || encoder_settings[this_banked_encoder_id].encoder_midi_type == SEND_REL_ENC_MOUSE_EMU_DRAG || encoder_settings[this_banked_encoder_id].encoder_midi_type == SEND_REL_ENC_MOUSE_EMU_SCROLL || encoder_settings[this_banked_encoder_id].encoder_midi_type == SEND_MCU_VPOT) // Don't Transfer values for relative encoders or V-Pots
		{ return false; }
	return true;
}

void transfer_this_encoder_value_to_other_banks(uint8_t current_bank, uint8_t encoder_id){
	uint8_t this_banked_encoder_id = current_bank * PHYSICAL_ENCODERS + encoder_id;
	for (uint8_t that_bank = 0; that_bank < NUM_BANKS; that_bank++) {
		wdt_reset(); // reset the watchdog timer, to keep controller from resetting while processing these updates
		if (that_bank == current_bank) { continue; } // We don't allow this feature within the same bank (too much processing for a frivolous feature)
			
		for (uint8_t that_encoder = 0; that_encoder < PHYSICAL_ENCODERS; that_encoder++) {
			uint8_t that_banked_encoder_id = that_bank*PHYSICAL_ENCODERS + that_encoder;
			if (encoder_maps_match(this_banked_encoder_id, that_banked_encoder_id)) {
				// Mappings Match! (only midi channel needs checked now)
				bool current_shift_state = encoder_is_in_shift_state(that_bank, that_encoder); 
				// - Check the non-shifted channel
				if (encoder_settings[this_banked_encoder_id].encoder_midi_channel == encoder_settings[that_banked_encoder_id].encoder_midi_channel) {
					// Transfer the Value
					raw_encoder_value[that_banked_encoder_id] = raw_encoder_value[this_banked_encoder_id];
					// Update Display if applicable				
					if (!current_shift_state) { // If Value is currently on display, update the display
						indicator_value_buffer[that_bank][that_encoder] = indicator_value_buffer[current_bank][encoder_id];
					}
				}
				// - Check the shifted channel and number. The numbers can differ from
				//   the unshifted ones when a switch gave its number to the shift layer.
				if (encoder_settings[this_banked_encoder_id].encoder_shift_midi_channel == encoder_settings[that_banked_encoder_id].encoder_shift_midi_channel
				    && encoder_output_number(this_banked_encoder_id, true) == encoder_output_number(that_banked_encoder_id, true)) {
					// Transfer the Value (note: must translate banked_encoder_id to virtual_encoder_id)
					raw_encoder_value[that_banked_encoder_id+BANKED_ENCODERS] = raw_encoder_value[this_banked_encoder_id+BANKED_ENCODERS];
					// Update Display if applicable				
					if (current_shift_state) { // If Value is currently on display, update the display
						indicator_value_buffer[that_bank][that_encoder] = indicator_value_buffer[current_bank][encoder_id];
					}
				
				}
				
			}
		}
	}
}

void transfer_encoder_values_to_other_banks(uint8_t current_bank){
	
	for (uint8_t this_encoder = 0; this_encoder < PHYSICAL_ENCODERS; this_encoder++){
		transfer_this_encoder_value_to_other_banks(current_bank, this_encoder);
	}
}

/**
 * Divides bank_arena between the bank scaled arrays, for the given bank count.
 *
 * Call this one time at boot, before any other code reads or writes bank
 * scaled storage. encoders_init() and load_config() both do, so this must
 * come before them. Refer to bank_mode_init() in setup_mode.c.
 *
 * The clear keeps a condition that the sequencer overlay needs: its buffers
 * must start at zero. No other code initializes slot_memory, because
 * pattern_memory_init() only reads the EEPROM slot flags. bank_arena is in
 * .bss now, and start up clears .bss. The clear protects against a later
 * change, and is not necessary now.
 *
 * \param banks [in]	MIN_BANKS or MAX_BANKS
 */
void encoders_alloc(uint8_t banks)
{
	if (banks != MAX_BANKS) { banks = MIN_BANKS; }
	g_num_banks = banks;

	memset(bank_arena, 0, ARENA_SIZE);

	uint8_t *p = bank_arena;

	// The 16 bit arrays come first, to keep each one on an even offset.
	raw_encoder_value     = (int16_t *)p;             p += banks * 32 * 2;
	enc_switch_midi_state = (uint16_t (*)[16])p;      p += banks * 16 * 2;
	encoder_settings      = (encoder_config_t *)p;    p += banks * 16 * sizeof(encoder_config_t);

	indicator_value_buffer   = (uint8_t (*)[16])p;    p += banks * 16;
	switch_color_buffer      = (uint8_t (*)[16])p;    p += banks * 16;
	switch_animation_buffer  = (uint8_t (*)[16])p;    p += banks * 16;
	encoder_animation_buffer = (uint8_t (*)[16])p;    p += banks * 16;

}

/**
 * Returns the part of the arena that the live bank mode does not use.
 *
 * With 4 banks this is one half of the arena, which is enough for the
 * sequencer. With 8 banks it is empty, and the sequencer must not run.
 *
 * \param size [out]	Number of bytes available
 */
uint8_t* bank_arena_spare(uint16_t *size)
{
	uint16_t used = (uint16_t)g_num_banks * ARENA_BYTES_PER_BANK;
	*size = ARENA_SIZE - used;
	return bank_arena + used;
}

void encoders_init(void)
{
	// Read in all the encoder settings for all banks
	// - in to the encoder_settings RAM Table
	//for(uint8_t i=0;i<16;++i){
	for(uint8_t i=0;i<BANKED_ENCODERS;++i){
		uint8_t this_bank = i/16;
		uint8_t this_phys_encoder = i%16;
		get_encoder_config(this_bank, this_phys_encoder, &encoder_settings[i]);
		//get_encoder_config(encoder_bank, i, &encoder_settings[i]);
	}
	
	// Build all encoder color state buffer banks
	// To do this efficiently we read the values directly from
	// EEPROM rather than reading the entire config structure
	// Load encoder settings for the currently selected bank
	
	uint16_t addr = (ENC_SETTINGS_START_PAGE * 32) + EE_INACTIVE_COLOR_OFFSET;

	for (uint8_t i = 0; i<NUM_BANKS;++i){
		for(uint8_t j=0;j<16;++j){
			switch_color_buffer[i][j] = eeprom_read(addr);
			addr+= ENC_EE_SIZE;
		}
	}
	
	// Initialize raw encoder values for Bank 1
	
	// Initialize Per-Bank related variables
	for (uint8_t i = 0; i<NUM_BANKS;++i){ 
		// !Summer2016Update: Moved this encoder_value init, so it initializes ALL values.
		enc_switch_toggle_state[i] = 0x0000;
	}	 
	 
	// Initialize Per-Physical Encoder related variables
	for (uint8_t i = 0; i<PHYSICAL_ENCODERS;++i){ 
		encoder_detent_counter[i] = 0;
		mcu_touch_note[i] = MCU_TOUCH_NONE;
		mcu_touch_timer[i] = 0;
		enc_switch_toggle_state[encoder_bank] = 0;
	}

	// Initialize Per-Banked Encoder related variables 
	for (uint8_t i = 0; i<BANKED_ENCODERS;++i){ 
		if (encoder_settings[i].has_detent) {
			raw_encoder_value[i] = 6300;
			raw_encoder_value[i+BANKED_ENCODERS] = 6300; // Also set value of the 'shifted encoder')
		} else {
			raw_encoder_value[i] = 0;
			raw_encoder_value[i+BANKED_ENCODERS] = 0; // Also set the value of the 'shifted encoder')
		}
		// Seed the ring display too. Relative encoders no longer take their
		// indicator value from raw_encoder_value, so without this a detent
		// encoder would boot at 0 and light the LEDs hard left.
		indicator_value_buffer[i/PHYSICAL_ENCODERS][i%PHYSICAL_ENCODERS] =
				encoder_settings[i].has_detent ? 63 : 0;
	}
	
	// Initialize MIDI input map for all encoders	
	// !Summer2016Update mark: sync_input_map_to_output_map 
	// - (added and then removed in favor of expanding encoder_settings)
	//~ for (uint8_t i=0;i<(NUM_BANKS*16);++i){
		//~ sync_input_map_to_output_map(i);
	//~ }
		
}

/**
 *	Reads the configuration data for a given encoder and writes it to the table
 *  specified by cfg_ptr. See header file for a map of the EEPROM layout for 
 *	encoder settings
 * 
 * \param [in] bank		The bank of the encoder we are getting the settings for
 *
 * \param [in] encoder	The index of encoder we are getting the settings for
 *
 * \param [in] cfg_ptr	Pointer to the table to load the settings into
 */

void get_encoder_config(uint8_t bank, uint8_t encoder, encoder_config_t *cfg_ptr)
{
	uint16_t addr = (ENC_SETTINGS_START_PAGE * 32) + (bank * 128) + (encoder * 8);
	
	uint8_t buffer[8];
	
	// We need to disable interrupts while interacting with the EEPROM drivers
	cpu_irq_disable();
	nvm_eeprom_read_buffer(addr, buffer, 8);
	cpu_irq_enable();
	
	// Expand compressed settings
	cfg_ptr->switch_action_type		= buffer[0] & 0x0F;
	cfg_ptr->switch_midi_type		= 0;//(buffer[0] >> 1) & 0x01;
	cfg_ptr->switch_midi_channel	= (buffer[0] >> 4) & 0x0F;
	cfg_ptr->switch_midi_number		= buffer[1] & 0x7F;
	cfg_ptr->active_color			= buffer[2];
	cfg_ptr->inactive_color			= buffer[3];
	cfg_ptr->detent_color			= buffer[4] & 0x7F;
	cfg_ptr->has_detent				= (buffer[4] >> 7) & 0x01;
	cfg_ptr->indicator_display_type = buffer[5] & 0x03;
	cfg_ptr->movement				= (buffer[5] >> 2) & 0x03;
	cfg_ptr->encoder_shift_midi_channel = (buffer[5] >> 4) & 0x0F; // !Summer2016Update: Shifted Encoder MIDI Channel
	cfg_ptr->encoder_midi_type		= buffer[6] & 0x0F; // 4 bits since SEND_MCU_FADER, which spent the spare bit 0x08
	cfg_ptr->encoder_midi_channel   = (buffer[6] >> 4) & 0x0F;
	cfg_ptr->encoder_midi_number	= buffer[7] & 0x7F;
	cfg_ptr->is_super_knob          = (buffer[7] >> 7) & 0x01;
}

// !review: this may not be correct
// !revision: less overhead for animations, by removing eeprom reads
//void get_encoder_config_locally(uint8_t bank, uint8_t encoder, encoder_config_t *cfg_ptr) {
	//uint8_t virtual_encoder_id = get_virtual_encoder_id(bank, encoder);
	//uint8_t banked_encoder_id = virtual_encoder_id & BANKED_ENCODER_MASK;
	//cfg_ptr = &encoder_settings[banked_encoder_id];
//}

/**
 * Takes a table of new configuration data for a given encoder and saves all 
 * valid settings to EEPROM. Valid settings values are 0 - 127. Settings with
 * value above 127 will be ignored. When passing configuration data to this 
 * function any setting which is not being updated must have its value set to
 * 0x80 or above, otherwise it will be overwritten. See header file for a map
 * of the EEPROM layout of encoder settings
 * 
 * \param bank [in]		The bank of the encoder
 *
 * \param encoder [in]	The index of the encoder
 *
 * \param encoder [in]	The tag table containing the new settings
 * 
 */
void save_encoder_config(uint8_t bank, uint8_t encoder, encoder_config_t *cfg_ptr)
{	
	// NOTE: the end of this function refreshes the RAM copy from the stored data.
	// Do not assign *cfg_ptr into encoder_settings[] here. Fields that the push
	// does not update arrive as 0x80 sentinels, so a full copy of the tag table
	// fills the RAM config with garbage.
	
	// Create a tempory page_buffer;
	uint8_t page_buffer[EEPROM_PAGE_SIZE];
	
	// Each page contains setting for four encoders, so to avoid overwriting the
	// data for the other 3 encoders we have to read in their setting first.
	uint16_t page_address = (ENC_SETTINGS_START_PAGE * 32) + (bank * 128) + ((encoder/4)*32);
	nvm_eeprom_read_buffer(page_address, page_buffer, EEPROM_PAGE_SIZE);
	
	uint8_t* buffer_ptr = page_buffer;
	buffer_ptr += (8 * (encoder % 4));
	
	// Keep a copy of the 8 bytes of this encoder. Then the code below can tell if
	// the page needs a new write. Only these 8 bytes can change below.
	uint8_t slot_original[8];
	for (uint8_t i = 0; i < 8; ++i) { slot_original[i] = buffer_ptr[i]; }
	
	// Switch Action, Switch MIDI type & Switch MIDI channel are saved
	// in the first byte
	if (cfg_ptr->switch_action_type < 0x80){
		*buffer_ptr &= ~0x0F; 
		*buffer_ptr |= cfg_ptr->switch_action_type & 0x0F;
	}

	if (cfg_ptr->switch_midi_channel < 0x80){
		*buffer_ptr &= ~0xF0;
		*buffer_ptr |= (0xF0 & ((cfg_ptr->switch_midi_channel - 1) << 4));
	}
	buffer_ptr++;  // Full
	
	// Switch MIDI number is saved in the second byte
	if (cfg_ptr->switch_midi_number < 0x80){
		*buffer_ptr = cfg_ptr->switch_midi_number;
	}
	buffer_ptr++;  // 1-bit open
	
	// Active and Inactive colors are saved in the third and fourth bytes	
	if (cfg_ptr->active_color < 0x80){
		*buffer_ptr = cfg_ptr->active_color;
	}
	buffer_ptr++;  // 1-bit open
		
	if (cfg_ptr->inactive_color < 0x80){
		*buffer_ptr = cfg_ptr->inactive_color;
	}
	buffer_ptr++;  // 1-bit open
	
	// Has de-tent and de-tent color are saved in the 5th byte
	if (cfg_ptr->has_detent < 0x80){
		*buffer_ptr &= ~0x80;
		*buffer_ptr |= (0x80 & (cfg_ptr->has_detent << 7));
	}	
	if (cfg_ptr->detent_color < 0x80){
		*buffer_ptr &= ~0x7F;
		*buffer_ptr |=  (0x7F & cfg_ptr->detent_color);
	}
	buffer_ptr++; // Full
	
	// Encoder Indicator & Movement type are save in the 6th byte
	if (cfg_ptr->indicator_display_type < 0x80){
		*buffer_ptr &= ~0x03;
		*buffer_ptr |= (0x03 & cfg_ptr->indicator_display_type);
	}	
	if (cfg_ptr->movement < 0x80){
		*buffer_ptr &= ~0x0C;
		*buffer_ptr |= (0x0C & (cfg_ptr->movement << 2));
	}
	if (cfg_ptr->encoder_shift_midi_channel < 0x80){ // !Summer2016Update shifted encoders midi channel
		*buffer_ptr &= ~0xF0;
		*buffer_ptr |= (0xF0 & ((cfg_ptr->encoder_shift_midi_channel - 1) << 4));
	}
	buffer_ptr++; // Full
	
	// Encoder MIDI Type & MIDI channel are saved in the 7th byte
	if (cfg_ptr->encoder_midi_type < 0x80){
		*buffer_ptr &= ~0x0F; // 4 bits since SEND_MCU_FADER, which spent the spare bit 0x08
		*buffer_ptr |= (0x0F & cfg_ptr->encoder_midi_type);
	}
	if (cfg_ptr->encoder_midi_channel < 0x80){
		*buffer_ptr &= ~0xF0;
		*buffer_ptr |= (0xF0 & ((cfg_ptr->encoder_midi_channel - 1) << 4));
	}
	buffer_ptr++;	// Full
	
	// Encoder MIDI number & is super knob flag are saved in the 8th byte
	if (cfg_ptr->encoder_midi_number < 0x80){
		*buffer_ptr &= ~0x7F;
		*buffer_ptr |= cfg_ptr->encoder_midi_number;
	}
	if (cfg_ptr->is_super_knob < 0x80){
		*buffer_ptr &= ~0x80;
		*buffer_ptr |= (0x80 & (cfg_ptr->is_super_knob << 7));
	}
	buffer_ptr++;  // Full
	
	uint8_t page_index = ENC_SETTINGS_START_PAGE + (4 * bank) + (encoder / 4);
	
	// Write the page only if a value changed. A config push sends two messages for
	// each encoder, and the utility sends the full sweep several times. Most of
	// these writes are not necessary. Each write blocks for milliseconds with
	// interrupts disabled (nvm_wait_until_ready). During a bulk transfer, the USB
	// endpoint gets no service for that time.
	uint8_t* slot_ptr = page_buffer + (8 * (encoder % 4));
	bool slot_changed = false;
	for (uint8_t i = 0; i < 8; ++i) {
		if (slot_ptr[i] != slot_original[i]) { slot_changed = true; break; }
	}
	
	if (slot_changed) {
		// Save the updated page buffer back to EEPROM
		cpu_irq_disable();
		nvm_eeprom_load_page_to_buffer(page_buffer);
		nvm_eeprom_atomic_write_page(page_index);
		cpu_irq_enable();
	}
	
	// Refresh the RAM copy from the stored data, so encoder_settings[] always
	// agrees with EEPROM. The index is the banked id. get_virtual_encoder_id() adds
	// BANKED_ENCODERS for a shifted encoder, and that index runs past the array.
	get_encoder_config(bank, encoder, &encoder_settings[encoder + bank*PHYSICAL_ENCODERS]);

	// !Summer2016Update: Match the newly changed input_map to match the output_map saved in eeprom
	// - removed in favor of expanding encoder_map
	//~ sync_input_map_to_output_map(bank*16 + encoder); // !Summer2016Update: midi_input_map bugfix
}

/**
 * Returns all encoder configuration data to the factory defaults  
**/
void factory_reset_encoder_config(void)
{
	factory_reset_encoder_pages(0, (4 * NUM_BANKS) - 1);
}

/**
 * Returns a range of encoder settings pages to the factory defaults.
 *
 * A change of bank mode uses this to give defaults to the pages that the new
 * mode is about to read. Without it, banks 5 to 8 come up with the 0xFF of an
 * unwritten EEPROM, which is not a default but nonsense.
 *
 * The page count starts at the first encoder settings page, so page 0 here is
 * EEPROM page ENC_SETTINGS_START_PAGE. There are 4 pages for each bank, one
 * for each row of 4 encoders.
 *
 * The loop goes through all of the pages, but writes only the pages in range.
 * This keeps the MIDI numbers the same as a full reset gives, because each
 * page adds 4 to the number of the page before it.
 *
 * \param first_page [in]	First page to reset, 0 based
 * \param last_page [in]	Last page to reset, 0 based
 */
void factory_reset_encoder_pages(uint8_t first_page, uint8_t last_page)
{
	// Create a table of the default settings common to all encoders
	encoder_config_t enc_default;
			
	enc_default.has_detent = DEF_ENC_DETENT;
	enc_default.detent_color = DEF_DETENT_COLOR;
	enc_default.active_color = DEF_ACTIVE_COLOR;      // !Summer2016Update: Overwritten later in this function by active_colors[]
	enc_default.inactive_color = DEF_INACTIVE_COLOR;  // !Summer2016Update: Overwritten later in this function by inactive_colors[]
	enc_default.movement = DEF_ENC_MOVEMENT;
	enc_default.indicator_display_type = DEF_INDICATOR_TYPE;
	enc_default.switch_action_type = DEF_SW_ACTION;
	enc_default.switch_midi_channel = DEF_SW_CH;
	enc_default.encoder_midi_channel = DEF_ENC_CH;
	enc_default.encoder_midi_type = DEF_ENC_MIDI_TYPE;
	enc_default.is_super_knob = DEF_IS_SUPER_KNOB;
	enc_default.encoder_midi_number = 0;
	enc_default.switch_midi_number = 0;
	enc_default.encoder_shift_midi_channel = DEF_ENC_SHIFT_CH; // !Summer2016Update: Shifted Encoders MIDI Channel
	 
	// !Summer2016Update active/inactive colors modified to be fixed per bank
	uint8_t color_map = eeprom_read_byte((uint8_t*)EE_COLOR_MAP);

	// MAX_BANKS, not NUM_BANKS: these have 8 initializers, and NUM_BANKS is a
	// runtime value. The code reads only the first NUM_BANKS entries.
	uint8_t active_colors[MAX_BANKS] = {
		DEF_ACTIVE_COLOR_BANK1_CLASSIC, DEF_ACTIVE_COLOR_BANK2_CLASSIC,
		DEF_ACTIVE_COLOR_BANK3_CLASSIC, DEF_ACTIVE_COLOR_BANK4_CLASSIC,
		DEF_ACTIVE_COLOR_BANK5_CLASSIC, DEF_ACTIVE_COLOR_BANK6_CLASSIC,
		DEF_ACTIVE_COLOR_BANK7_CLASSIC, DEF_ACTIVE_COLOR_BANK8_CLASSIC
	};
	uint8_t inactive_colors[MAX_BANKS] = {
		DEF_INACTIVE_COLOR_BANK1_CLASSIC, DEF_INACTIVE_COLOR_BANK2_CLASSIC,
		DEF_INACTIVE_COLOR_BANK3_CLASSIC, DEF_INACTIVE_COLOR_BANK4_CLASSIC,
		DEF_INACTIVE_COLOR_BANK5_CLASSIC, DEF_INACTIVE_COLOR_BANK6_CLASSIC,
		DEF_INACTIVE_COLOR_BANK7_CLASSIC, DEF_INACTIVE_COLOR_BANK8_CLASSIC
	};
	/* We then fill a page buffer with the compressed settings for 4 encoders
	   (each encoder has 8 bytes of data allocated for its setting) */
	uint8_t page_buffer[EEPROM_PAGE_SIZE];
	memset(&page_buffer, 0, EEPROM_PAGE_SIZE);
	
	uint8_t* buffer_ptr = page_buffer;
	
	uint8_t data_byte = 0;
	
	//uint8_t bit;
	
	// Set up a Template Memory Page - (One Column of 4-Encoders) - with the default settings
	// - buffer_ptr will be the actual table passed to the EEPROM Write Routine
	// - It will be modified for each group of four encoders
	for(uint8_t i=0;i<4;++i) {
		// Switch Settings are saved in the first and second bytes
		data_byte  = (enc_default.switch_action_type & 0x0F);
		data_byte |= (0xF0 & (enc_default.switch_midi_channel << 4));		
		
		*buffer_ptr++ = data_byte;
		
		*buffer_ptr++ = enc_default.switch_midi_number+i;
		
		// Active and Inactive colors are saved in the third and fourth bytes
		// *buffer_ptr++ = enc_default.active_color+(i*2);
		// *buffer_ptr++ = enc_default.inactive_color+(i*2);
		// *buffer_ptr++ = enc_default.active_color;
		// *buffer_ptr++ = enc_default.inactive_color;
		*buffer_ptr++ = 127;// active_colors[0];
		*buffer_ptr++ = 127;// inactive_colors[0];
		
		// Has de-tent and de-tent color are saved in the 5th byte
		*buffer_ptr++ =  (0x80 & (enc_default.has_detent << 7)) | 
						  enc_default.detent_color;
		
		// Encoder Indicator & Movement type are save in the 6th byte (and now encoder_shift_midi_channel)
		data_byte  = (0x03 & enc_default.indicator_display_type);
		data_byte |= (0x0C & (enc_default.movement << 2));
		data_byte |= (0xF0 & (enc_default.encoder_shift_midi_channel << 4)); // !Summer2016Update: shifted encoders midi channel
		*buffer_ptr++ = data_byte;
		
		// Encoder MIDI Type & MIDI channel are saved in the 7th byte
		data_byte  = (0x07 & enc_default.encoder_midi_type); // !Spring2019Update: Switch Velocity Control and Mouse Emulation
		data_byte |= (0xF0 & (enc_default.encoder_midi_channel << 4));
		*buffer_ptr++ = data_byte;
		
		// Encoder MIDI number is saved in the 8th byte
		data_byte = (0x7f & (enc_default.encoder_midi_number+i));
		data_byte |= (0x80 & (enc_default.is_super_knob << 7));
		*buffer_ptr++ = data_byte;
		
	}
	
	uint8_t page_index = ENC_SETTINGS_START_PAGE;
	
	// For Each Memory Page
	// - Modify the Template, and write the page
	for(uint8_t i=0;i<4*MAX_BANKS;++i){		// 4*MAX_BANKS, aka. NUM_ROWS_PER_BANK*MAX_BANKS 
		bool in_range = (i >= first_page) && (i <= last_page);
		// for each Row in each Bank modify the default settings
		/* We now save the buffer to EEPROM, then increment the settings 
		   which are unique to each encoder ie MIDI number/pitch and repeat 
		   until we have saved all 64 encoders data */
		
		// !Summer2016Update: Modify Default On/Off colors by bank rather than per encoder.
		uint8_t color_index = i >> 2;
		uint8_t active_color = active_colors[color_index];  // Changes with each Bank
		uint8_t inactive_color = inactive_colors[color_index]; // Changes with each bank
		for(uint8_t j=0;j<4;++j){  // For each Data Page (Corresponds to 1 - Horizontal Row of Encoders)
			page_buffer[(j*8)+2] = active_color; // Adjust Active Color  (All Rows should be the same for THIS BANK)
			page_buffer[(j*8)+3] = inactive_color; // Adjust Inactive Color (ALL Rows should be the same for THIS BANK)
			// 8 = Settings Size
		}

		// Write the Page
		if (in_range) {
			cpu_irq_disable();
			nvm_eeprom_load_page_to_buffer(page_buffer);
			nvm_eeprom_atomic_write_page(page_index);
			cpu_irq_enable();
		}
		
		// Modify Mapping Parameters for the next item.
		for(uint8_t j=0;j<4;++j){  // For each Data Page (Corresponds to 1 - Horizontal Row of Encoders)
			page_buffer[(j*8)+1] += 4; //Increment SW MIDI Pitch
			page_buffer[(j*8)+7] += 4; //Increment ENC MIDI Number			
		}
		page_index++;
	}
}
//void adjust_
#if VELOCITY_CALC_METHOD > VELOCITY_CALC_M_NONE 

uint16_t convert_ticks_per_scan_to_value_multiplier(uint8_t tick_count, uint16_t cycles_count) {
	float multiplier = velocity_calc_slope*(float)(tick_count)/(float)(cycles_count) + velocity_calc_offset;
	if (multiplier > VELOCITY_CALC_MAX_MULTIPLIER) {
		multiplier = VELOCITY_CALC_MAX_MULTIPLIER;
	} else if (multiplier < VELOCITY_CALC_MIN_MULTIPLIER) {
		multiplier = VELOCITY_CALC_MIN_MULTIPLIER;
	}
	return (uint16_t)(multiplier);
}

void send_midi_velocity_sensitive_encoder(uint8_t encoder_id, uint16_t output_value) { // DEPRECATED?
	#if VELOCITY_CALC_METHOD == VELOCITY_CALC_M_TPS_BLOCKS
		uint8_t midi_channel = 0x0F;
		uint16_t multiplier = convert_ticks_per_scan_to_value_multiplier(1, output_value+1); // !review: '1' no guarantee only 1 tick
		midi_stream_raw_pitchbend(midi_channel, multiplier);
	#endif
}
#endif


// ======================== Process Encoder Input Functions======================
/**
 * Contains the main encoder task, this checks encoder hardware for change
 * then translates this change into a MIDI value based on the encoders
 * settings, if the MIDI value changes then the updated value is sent
 * and saved into the MIDI state buffer.
 */                                                                
void process_encoder_input(void)  // MIDI Output: Digital Inputs -> Encoders (Output)
{
	
	uint16_t bit = 0x0001;
	// Update the current encoder switch states
	update_encoder_switch_state();
	
	for (uint8_t i=0;i<16;i++) {
		uint8_t virtual_encoder_id = get_virtual_encoder_id(encoder_bank, i);
		uint8_t banked_encoder_id = virtual_encoder_id & BANKED_ENCODER_MASK;
		process_encoder_input_rotary(i, virtual_encoder_id, banked_encoder_id, bit);  // First we check for movement on each encoder
		process_encoder_input_switch(i, virtual_encoder_id, banked_encoder_id, bit); // Then we check the movement of each switch
		mcu_fader_touch_tick(i); // Then release a fader touch that has gone still
		bit <<= 1;
	}
}
void process_encoder_input_rotary(uint8_t i, uint8_t virtual_encoder_id, uint8_t banked_encoder_id, uint16_t bit) { // i = encoder_id
	int16_t new_value;
	#if VELOCITY_CALC_METHOD == VELOCITY_CALC_M_TPS_BLOCKS
	uint16_t cycle_count;
	#endif
	//uint8_t virtual_encoder_id, banked_encoder_id;

	#if VELOCITY_CALC_METHOD == VELOCITY_CALC_M_TPS_BLOCKS
	cpu_irq_disable(); // disable interrupts to ensure we don't lose any data by reading midstream.
	new_value = get_encoder_value(i);  // !review: int8 return is being assigned to int16
	cycle_count = get_encoder_cycle_count(i);
	cpu_irq_enable();
	#else
	new_value = get_encoder_value(i);  // !review: int8 return is being assigned to int16
	#endif

	if (new_value) { // if Encoder Has Moved
    reset_idle_timer();
		#if VELOCITY_CALC_METHOD == VELOCITY_CALC_M_TPS_BLOCKS
		if(native_mode_process_encoder_input_rotary(i, new_value, cycle_count))
			return;
		#else
		if(native_mode_process_encoder_input_rotary(i, new_value, 0))
			return;
		#endif

		// A None encoder throws the movement away. The stored value does not
		// change, so the ring holds still. Native mode comes first, because it
		// replaces the bank configuration while it is on.
		if (encoder_settings[banked_encoder_id].encoder_midi_type == SEND_NONE) {
			return;
		}

		// A fader reports touch on any movement, even movement a detent swallows,
		// because a real fader reports the hand and not the value.
		if (encoder_settings[banked_encoder_id].encoder_midi_type == SEND_MCU_FADER) {
			mcu_fader_touch(i, banked_encoder_id, virtual_encoder_id >= BANKED_ENCODERS);
		}

		// --- Detents and Deadzones
		#if VELOCITY_CALC_METHOD == VELOCITY_CALC_M_TPS_BLOCKS
		if (process_encoder_input_rotary_detent(i, virtual_encoder_id, banked_encoder_id, new_value, cycle_count)) { }
		#else
		if (process_encoder_input_rotary_detent(i, virtual_encoder_id, banked_encoder_id, new_value)) { }
		#endif
		// everything has been done in 'pei_detent', if that function returned true
		#if VELOCITY_CALC_METHOD == VELOCITY_CALC_M_TPS_BLOCKS
		else if (process_encoder_input_rotary_deadzone(i, virtual_encoder_id, banked_encoder_id, new_value, cycle_count)) {}
		#else
		else if (process_encoder_input_rotary_deadzone(i, virtual_encoder_id, banked_encoder_id, new_value)) {}
		#endif
		// everything has been done in 'pei_deadzone', if that function returned true
		else { // Encoder Event Occurred, and Encoder is not in a Detent or Deadzone.
			// TODO TIDY THIS ALL UP - NEED TO RETHINK FUNCTION TOPOGRAPHY FOR ENCODERS *?
			#if VELOCITY_CALC_METHOD == VELOCITY_CALC_M_TPS_BLOCKS
			if(process_encoder_input_rotary_relative(i, virtual_encoder_id, banked_encoder_id, new_value, bit, cycle_count)) {}
			// everything has been done in 'pei_relative', if that function returned true
			#else
			if(process_encoder_input_rotary_relative(i, virtual_encoder_id, banked_encoder_id, new_value, bit)) {}
			// everything has been done in 'pei_relative', if that function returned true
			#endif

			else { // The encoder is not relataive nor in any detent or end zone
				#if VELOCITY_CALC_METHOD == VELOCITY_CALC_M_TPS_BLOCKS
				process_encoder_input_rotary_absolute(i, virtual_encoder_id, banked_encoder_id, new_value, bit, cycle_count);
				// everything has been done in 'pei_absolute', if that function returned true
				#else
				process_encoder_input_rotary_absolute(i, virtual_encoder_id, banked_encoder_id, new_value, bit);
				// everything has been done in 'pei_absolute', if that function returned true
				#endif
			}
		}
	}

}

#if VELOCITY_CALC_METHOD == VELOCITY_CALC_M_TPS_BLOCKS
bool process_encoder_input_rotary_detent(uint8_t i, uint8_t virtual_encoder_id, uint8_t banked_encoder_id, int8_t new_value, uint16_t cycle_count)
#else
bool process_encoder_input_rotary_detent(uint8_t i, uint8_t virtual_encoder_id, uint8_t banked_encoder_id, int8_t new_value)
#endif
{
	if (!encoder_settings[banked_encoder_id].has_detent) {
		//midi_stream_raw_cc(9,99,1);
		return false;
	}

	// A V-Pot has no local value, so nothing to detent. The host owns its ring.
	if (encoder_settings[banked_encoder_id].encoder_midi_type == SEND_MCU_VPOT) {
		return false;
	}

	// A flipped fader holding a pan detents at the center, and shows no detent
	// LED: the feel is the whole feature.
	int16_t unity = mcu_fader_detent_raw(banked_encoder_id);
	if (unity >= 0) {
		int16_t offset = raw_encoder_value[virtual_encoder_id] - unity;
		if ((offset > MCU_DETENT_WINDOW_RAW) || (offset < -MCU_DETENT_WINDOW_RAW)) {
			return false;
		}
		encoder_detent_counter[i] += new_value;
		if (encoder_detent_counter[i] > global_detent_size) {          // Leaving upward
			raw_encoder_value[virtual_encoder_id] = clamp_encoder_raw_value(unity + MCU_DETENT_WINDOW_RAW + 1);
		} else if (encoder_detent_counter[i] < -global_detent_size) {  // Leaving downward
			raw_encoder_value[virtual_encoder_id] = clamp_encoder_raw_value(unity - MCU_DETENT_WINDOW_RAW - 1);
		} else {                                                       // Held at 0 dB
			raw_encoder_value[virtual_encoder_id] = unity;
		}
		return true;
	}

	if (!encoder_is_in_detent(raw_encoder_value[virtual_encoder_id])) {
		//midi_stream_raw_cc(9,99,2);
		return false;
	}


	encoder_detent_counter[i] += new_value;
	if (encoder_detent_counter[i] > global_detent_size){ // Exiting detent (CW)
		//raw_encoder_value[virtual_encoder_id] = 6500;
		raw_encoder_value[virtual_encoder_id] = encoder_detent_limit_high;
		//midi_stream_raw_cc(9,99,11);
	} else if (encoder_detent_counter[i] < -global_detent_size) { // Exiting detent (CCW)
		//raw_encoder_value[virtual_encoder_id] = 6200;
		raw_encoder_value[virtual_encoder_id] = encoder_detent_limit_low;
		//midi_stream_raw_cc(9,99,12);
	} else { // Still In Detent
		//midi_stream_raw_cc(9,99,13);
		// Patch: Ensure the Detent LED is set even for higher resolution modes, like velocity sensitivity
		indicator_value_buffer[encoder_bank][i] = 63;
	}
	return true;
}

// - Rotary Sub Function
#if VELOCITY_CALC_METHOD == VELOCITY_CALC_M_TPS_BLOCKS
  bool process_encoder_input_rotary_deadzone(uint8_t i, uint8_t virtual_encoder_id, uint8_t banked_encoder_id, int8_t new_value, uint16_t cycle_count)
#else
  bool process_encoder_input_rotary_deadzone(uint8_t i, uint8_t virtual_encoder_id, uint8_t banked_encoder_id, int8_t new_value)
#endif
{
	uint16_t value = raw_encoder_value[virtual_encoder_id];
	if (encoder_settings[banked_encoder_id].encoder_midi_type == SEND_REL_ENC || encoder_settings[banked_encoder_id].encoder_midi_type == SEND_REL_ENC_MOUSE_EMU_DRAG || encoder_settings[banked_encoder_id].encoder_midi_type == SEND_REL_ENC_MOUSE_EMU_SCROLL || encoder_settings[banked_encoder_id].encoder_midi_type == SEND_MCU_VPOT) {
		return false;
	} else if (!encoder_is_in_deadzone(value)) {
		return false;
	}
	// The encoder is in an end zone. Only consecutive ticks away from the end count, so a
	// knob resting against the end can wobble without leaving it.
	if( ((raw_encoder_value[virtual_encoder_id] < 100) && (new_value > 0)) ||
	((raw_encoder_value[virtual_encoder_id] > 12600) && (new_value < 0)) ) {
		encoder_detent_counter[i] += new_value;
	} else {
		encoder_detent_counter[i] = 0;
	}
	// - Allow movement to get us out of the dead zone, if we've traveled enough.
	bool leaving = false;
	if ((encoder_detent_counter[i] > global_dead_zone_size) &&
	(raw_encoder_value[virtual_encoder_id] < 100)) { // CW
		raw_encoder_value[virtual_encoder_id] = 100;
		leaving = true;
	} else if ((encoder_detent_counter[i] < -global_dead_zone_size) &&
	(raw_encoder_value[virtual_encoder_id] > 12600)) {  // CCW
		raw_encoder_value[virtual_encoder_id] = 12600;
		leaving = true;
	}
	if (leaving) { // Leave on exactly one step, whatever the sensitivity, and send it now
		encoder_detent_counter[i] = 0;
		uint8_t control_change_value = scale_encoder_value(raw_encoder_value[virtual_encoder_id]);
		bool shifted = encoder_is_in_shift_state(encoder_bank, i);
		send_encoder_midi(banked_encoder_id, control_change_value, true, shifted);
		indicator_value_buffer[encoder_bank][i] = control_change_value;
	}
	return true;
}


#if VELOCITY_CALC_METHOD == VELOCITY_CALC_M_TPS_BLOCKS
  bool process_encoder_input_rotary_relative(uint8_t i, uint8_t virtual_encoder_id, uint8_t banked_encoder_id, int8_t new_value, uint16_t bit, uint16_t cycle_count)
#else
  bool process_encoder_input_rotary_relative(uint8_t i, uint8_t virtual_encoder_id, uint8_t banked_encoder_id, int8_t new_value, uint16_t bit)
#endif
{
	// A V-Pot is relative, but it encodes sign and magnitude where the modes
	// below use a binary offset. It takes its own path and returns here.
	if (encoder_settings[banked_encoder_id].encoder_midi_type == SEND_MCU_VPOT) {
		int8_t ticks = (new_value < 0) ? -new_value : new_value;
		if (ticks > MCU_VPOT_TICK_MAX) { ticks = MCU_VPOT_TICK_MAX; }

		uint8_t out = (uint8_t)ticks;
		if (new_value < 0) { out |= MCU_VPOT_DIRECTION; }

		bool shifted = encoder_is_in_shift_state(encoder_bank, i);
		midi_stream_raw_cc(shifted ? encoder_settings[banked_encoder_id].encoder_shift_midi_channel
		                           : encoder_settings[banked_encoder_id].encoder_midi_channel,
		                   encoder_settings[banked_encoder_id].encoder_midi_number,
		                   out);
		return true;
	}

	if (!(encoder_settings[banked_encoder_id].encoder_midi_type == SEND_REL_ENC || encoder_settings[banked_encoder_id].encoder_midi_type == SEND_REL_ENC_MOUSE_EMU_DRAG || encoder_settings[banked_encoder_id].encoder_midi_type == SEND_REL_ENC_MOUSE_EMU_SCROLL)) {
		return false;
	}

	// Calculate Output Value 
	// new_value will be 1 or -1 if it gets here(though technically it could hold values higher)
	int16_t output_value;// = 64 + new_value;	// Relative: Bin Offset (int16 so the clamp below can catch overflow)
	// Case A: Relative Encoder Fine Adjustments
	if (encoder_settings[banked_encoder_id].switch_action_type == ENC_FINE_ADJUST && get_enc_switch_state() & bit) { // !Summer2016Update: Relative Encoder Fine Adjustment Added
		static int16_t relative_fine_value[PHYSICAL_ENCODERS] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};  // Counter
		relative_fine_value[i] += new_value;
		output_value = 64;
		while (relative_fine_value[i] >= 4)  { // Adjust for Clockwise Output -> 4=ENC_REL_FINE_LIMIT
			relative_fine_value[i] -= 4;
			output_value++;
		}
		while (relative_fine_value[i] <= -4)  { // Adjust for CounterClockwise Output -> 4=ENC_REL_FINE_LIMIT
			relative_fine_value[i] += 4;
			output_value--;
		}
	}
	else if(encoder_settings[banked_encoder_id].movement == EMULATION) {
		// Case C: encoder set to "responsive" mode
		output_value = 64 + (new_value * ENCODER_RELATIVE_TICKS_RESPONSIVE);
	}
	else {	// Case B: Normal Relative Encoders
		#if VELOCITY_CALC_METHOD == VELOCITY_CALC_M_NONE
			output_value = 64 + new_value;	// Relative: Bin Offset
		#elif VELOCITY_CALC_METHOD == VELOCITY_CALC_M_TPS_BLOCKS
			if(encoder_settings[banked_encoder_id].movement == VELOCITY_SENSITIVE_ENC) {
				int8_t tick_count = new_value < 0 ? -1*new_value : new_value;
				//uint16_t cycle_count = encoder_event_cycle_counts[i];
				uint16_t base_multiplier = convert_ticks_per_scan_to_value_multiplier(tick_count , cycle_count);
				int16_t scaled_mult = ENC_REL_VELOCITY_STEP(base_multiplier); // 1..17
				output_value = 64 + new_value*scaled_mult;	// Relative: Bin Offset
			} else {
				output_value = 64 + new_value;	// Relative: Bin Offset
			}
				
		#endif
	}

	//uint8_t output_value = (uint8_t)(new_value) & 0x7F;	// Relative: Two's Complement
	uint8_t midi_channel;
	uint8_t this_bank = encoder_bank;
	if (!encoder_is_in_shift_state(this_bank, i)){ // !Summer2016Update Shifted Encoders Midi Channel
		midi_channel = encoder_settings[banked_encoder_id].encoder_midi_channel;
	} else{
		midi_channel = encoder_settings[banked_encoder_id].encoder_shift_midi_channel;
		// !revision use the unused switch midi number: midi_number = encoder_settings[banked_encoder_id].switch_midi_number ? 20190806
		//midi_channel = encoder_settings[banked_encoder_id].switch_midi_channel;
	}
	// Clamp to the 7-bit binary offset range, a multi-tick event at a high
	//   velocity multiplier can push the raw sum past 127 and wrap the direction.
	if (output_value > ENC_REL_OUTPUT_MAX) {
		output_value = ENC_REL_OUTPUT_MAX;
	} else if (output_value < ENC_REL_OUTPUT_MIN) {
		output_value = ENC_REL_OUTPUT_MIN;
	}
	// Send Message
	// Relative output does not need to call 'Send Encoder' as it just always outputs 'CC' 
	if (output_value != 64) { // !Summer2016Update: Relative Encoder Fine Adjustment Bugfix, disallow non-moving
		midi_stream_raw_cc(midi_channel, 
					encoder_settings[banked_encoder_id].encoder_midi_number, 
					(uint8_t)output_value);	
	}
	return true;
}

#if VELOCITY_CALC_METHOD == VELOCITY_CALC_M_TPS_BLOCKS
  bool process_encoder_input_rotary_absolute(uint8_t i, uint8_t virtual_encoder_id, uint8_t banked_encoder_id, int8_t new_value, uint16_t bit, uint16_t cycle_count) {
	uint16_t base_multiplier;
	int16_t scaled_value;
	bool fine_adj = (encoder_settings[banked_encoder_id].switch_action_type == ENC_FINE_ADJUST && get_enc_switch_state() & bit);
	
	// Calculate Value adjustment
	if(encoder_settings[banked_encoder_id].movement == VELOCITY_SENSITIVE_ENC) { // Velocity Sensitive
		int8_t tick_count = new_value < 0 ? -1*new_value : new_value;
		base_multiplier = convert_ticks_per_scan_to_value_multiplier(tick_count , cycle_count);
		// A Mackie fader resolves 14 bits, so it has a finer floor than a 7 bit CC
		uint16_t velocity_floor = (encoder_settings[banked_encoder_id].encoder_midi_type == SEND_MCU_FADER)
		                        ? ENCODER_VALUE_SCALAR_MCU_FADER_MIN : ENCODER_VALUE_SCALAR_VELOCITY_MIN;
		if (fine_adj || base_multiplier < velocity_floor) { // Velocity Sensitive-Fine
			base_multiplier = velocity_floor;
		} else if (base_multiplier > ENCODER_VALUE_SCALAR_VELOCITY_MAX) {
			base_multiplier = ENCODER_VALUE_SCALAR_VELOCITY_MAX;  // TODO: 14-bit will have a min for 14-bit and 7-bit operations
		}
	}
	else if (fine_adj) { // Standard-Fine
		base_multiplier = ENCODER_VALUE_SCALAR_DIRECT_FINE; 
	}
	else if(encoder_settings[banked_encoder_id].movement == DIRECT) {  // Standard, 1 CC value per tick
		base_multiplier = ENCODER_VALUE_SCALAR_DIRECT;
	} else  {  // Quick Moving, 1.78 CC values per tick
		base_multiplier = ENCODER_VALUE_SCALAR_EMULATION; 
	}
	// Calculate Value to write into raw_encoder_value
	scaled_value = new_value*base_multiplier;

	uint8_t prev_value = scale_encoder_value(raw_encoder_value[virtual_encoder_id]);
	// Update the Stored raw_encoder_value
	raw_encoder_value[virtual_encoder_id] += scaled_value;
	raw_encoder_value[virtual_encoder_id] = clamp_encoder_raw_value(raw_encoder_value[virtual_encoder_id]);
	// Entering the detent window lands exactly on center, so the user never stops
	// near it. The escape count starts fresh from there.
	{
		int16_t unity_point = mcu_fader_detent_raw(banked_encoder_id);
		if (unity_point >= 0) {
			int16_t unity_offset = raw_encoder_value[virtual_encoder_id] - unity_point;
			if ((unity_offset <= MCU_DETENT_WINDOW_RAW) && (unity_offset >= -MCU_DETENT_WINDOW_RAW)) {
				raw_encoder_value[virtual_encoder_id] = unity_point;
				encoder_detent_counter[i] = 0;
			}
		}
	}
	if (encoder_is_in_deadzone(raw_encoder_value[virtual_encoder_id])) {
		encoder_detent_counter[i] = 0;	// Arriving at an end: count its sticky ticks from zero
	}
	// Translate the raw value into a MIDI value, send & save the new value
	uint8_t control_change_value = scale_encoder_value(raw_encoder_value[virtual_encoder_id]);
	//send_element_midi(SWITCH, i, control_change_value, true);
	uint8_t this_bank = encoder_bank;// banked_encoder_id >> 4; // !review !test this_bank
	bool shifted = encoder_is_in_shift_state(this_bank, i);
	// Fine adjust and slow velocity turns repeat a value; a synth reloads the patch on every Program Change
	if (encoder_settings[banked_encoder_id].encoder_midi_type != SEND_PROGRAM_CHANGE ||
	    control_change_value != prev_value) {
		send_encoder_midi(banked_encoder_id, control_change_value, true, shifted); // !Summer2016Update: Shifted Encoders now always output as Encoders
	}
	indicator_value_buffer[encoder_bank][i]=control_change_value;
	return true;
  }
#else
  bool process_encoder_input_rotary_absolute(uint8_t i, uint8_t virtual_encoder_id, uint8_t banked_encoder_id, int8_t new_value, uint16_t bit) {
	int16_t scaled_value;
		
	if (encoder_settings[banked_encoder_id].switch_action_type == ENC_FINE_ADJUST &&
		get_enc_switch_state() & bit) {
		// Fine adjust sensitivity, 1 pulse = 1/4 a CC step	
		scaled_value = new_value*ENCODER_VALUE_SCALAR_DIRECT_FINE; 
		
	} else if(encoder_settings[banked_encoder_id].movement == DIRECT) {
			// Standard sensitivity, 1 pulse = 1 CC step
			scaled_value = new_value*ENCODER_VALUE_SCALAR_DIRECT; 
			
	} else  {
			// Emulated sensitivity, 270 degress rotation = 127 CC steps
			scaled_value = new_value*ENCODER_VALUE_SCALAR_EMULATION;   
			//scaled_value = new_value*200;   
	}
	uint8_t prev_value = scale_encoder_value(raw_encoder_value[virtual_encoder_id]);
	// Update the Stored raw_encoder_value
	raw_encoder_value[virtual_encoder_id] += scaled_value;
	raw_encoder_value[virtual_encoder_id] = clamp_encoder_raw_value(raw_encoder_value[virtual_encoder_id]);
	// Entering the detent window lands exactly on center, so the user never stops
	// near it. The escape count starts fresh from there.
	{
		int16_t unity_point = mcu_fader_detent_raw(banked_encoder_id);
		if (unity_point >= 0) {
			int16_t unity_offset = raw_encoder_value[virtual_encoder_id] - unity_point;
			if ((unity_offset <= MCU_DETENT_WINDOW_RAW) && (unity_offset >= -MCU_DETENT_WINDOW_RAW)) {
				raw_encoder_value[virtual_encoder_id] = unity_point;
				encoder_detent_counter[i] = 0;
			}
		}
	}
	if (encoder_is_in_deadzone(raw_encoder_value[virtual_encoder_id])) {
		encoder_detent_counter[i] = 0;	// Arriving at an end: count its sticky ticks from zero
	}
	// Translate the raw value into a MIDI value, send & save the new value 	
	uint8_t control_change_value = scale_encoder_value(raw_encoder_value[virtual_encoder_id]);
	//send_element_midi(SWITCH, i, control_change_value, true);
	uint8_t this_bank = encoder_bank;// banked_encoder_id >> 4; // !review !test this_bank
	bool shifted = encoder_is_in_shift_state(this_bank, i);
	// Fine adjust and slow velocity turns repeat a value; a synth reloads the patch on every Program Change
	if (encoder_settings[banked_encoder_id].encoder_midi_type != SEND_PROGRAM_CHANGE ||
	    control_change_value != prev_value) {
		send_encoder_midi(banked_encoder_id, control_change_value, true, shifted); // !Summer2016Update: Shifted Encoders now always output as Encoders
	}
	indicator_value_buffer[encoder_bank][i]=control_change_value;
	return true;
}
#endif

void process_encoder_input_switch(uint8_t i, uint8_t virtual_encoder_id, uint8_t banked_encoder_id, uint16_t bit) {
	
	if (get_bank_select_active()) {
		if (bit & get_enc_switch_down()) {
			uint8_t target_bank = i % NUM_BANKS;
			midi_stream_raw_cc(midi_system_channel, current_encoder_bank(), 0);
			bank_change_animation(target_bank);
			change_encoder_bank(target_bank);
			midi_stream_raw_cc(midi_system_channel, target_bank, 127);
			draw_bank_select_overlay();
		}
		return;
	}

	if (bit & get_enc_switch_down() || bit & get_enc_switch_up()) {
		reset_idle_timer(); 
	// If the switch state has changed due its action
	
		if(native_mode_process_encoder_input_switch_pressed(i, bit & get_enc_switch_down()))
			return;
	
		switch (encoder_settings[banked_encoder_id].switch_action_type)
		{
			case CC_TOGGLE:{}
			// cascade to the next case ... NO BREAK ON PURPOSE
			case NOTE_TOGGLE:{
				if(bit & get_enc_switch_down()) {
					// Toggle the MIDI state
					enc_switch_midi_state[encoder_bank][i] = enc_switch_midi_state[encoder_bank][i] ? 
																0 : 127;
					// Update the display
					if (!color_overide_active(encoder_bank,i)) {
						switch_color_buffer[encoder_bank][i] = enc_switch_midi_state[encoder_bank][i] ?
						encoder_settings[banked_encoder_id].active_color : encoder_settings[banked_encoder_id].inactive_color;
					}
					// And send any MIDI
					send_element_midi(SWITCH, banked_encoder_id, enc_switch_midi_state[encoder_bank][i],
					enc_switch_midi_state[encoder_bank][i]);
				}
			}
			break;
			
			case CC_HOLD:{
			}
			// cascade to the next case ... NO BREAK ON PURPOSE
			case NOTE_HOLD:{
				if (bit & get_enc_switch_down()) {
					enc_switch_midi_state[encoder_bank][i] = 127;
				} else {
					enc_switch_midi_state[encoder_bank][i] = 0;
				}
				// Update the display
				if (!color_overide_active(encoder_bank,i)) {
					switch_color_buffer[encoder_bank][i] = enc_switch_midi_state[encoder_bank][i] ?
					encoder_settings[banked_encoder_id].active_color : encoder_settings[banked_encoder_id].inactive_color;
				}
				// And send any MIDI
				send_element_midi(SWITCH, banked_encoder_id, enc_switch_midi_state[encoder_bank][i],
				enc_switch_midi_state[encoder_bank][i]);
			}
			break;
			
			case MCU_BUTTON:{
				// Sends what NOTE_HOLD sends. The color is left alone on purpose,
				// because a Mackie lamp belongs to the host: it answers the press
				// with a note of its own, and a button the DAW ignores must not
				// light up as though it worked.
				if (bit & get_enc_switch_down()) {
					enc_switch_midi_state[encoder_bank][i] = 127;
				} else {
					enc_switch_midi_state[encoder_bank][i] = 0;
				}
				send_element_midi(SWITCH, banked_encoder_id, enc_switch_midi_state[encoder_bank][i],
				enc_switch_midi_state[encoder_bank][i]);
			}
			break;

			/*	The switch does nothing. The case is here so that a reader sees
				the action, and does not take it for an omission */
			case SW_ACTION_NONE:{}
			break;

			/*	The fine adjust action is handled in the encoder code, the
				LED does not change state with this action */
			case ENC_FINE_ADJUST:{}
			break;	
			
			/*	Re-sends the program knob's selection on press, even if it has not
				changed. The LED does not change state with this action */
			case PC_RESEND:{
				if ((bit & get_enc_switch_down()) &&
					encoder_settings[banked_encoder_id].encoder_midi_type == SEND_PROGRAM_CHANGE) {
					// The switch is taken by this action, so the knob can't be shifted
					send_program_selection(encoder_settings[banked_encoder_id].encoder_midi_channel,
										   scale_encoder_value(raw_encoder_value[banked_encoder_id]));
				}
			}
			break;
			
			case ENC_RESET_VALUE:{
				// Reset the encoder value
				uint8_t control_change_value; // = scale_encoder_value(raw_encoder_value[virtual_encoder_id]);
				if (encoder_settings[banked_encoder_id].has_detent){
					raw_encoder_value[virtual_encoder_id] = 6300;
					control_change_value = scale_encoder_value(raw_encoder_value[virtual_encoder_id]);
				} else {
					raw_encoder_value[virtual_encoder_id] = 0;
					control_change_value = scale_encoder_value(raw_encoder_value[virtual_encoder_id]);
				}
				// Send the updated encoder value
				encoder_detent_counter[i] = 0;
				send_element_midi(ENCODER, banked_encoder_id, control_change_value, true);  // Last Encoder call to 'send_element_midi', all others upgraded to 'send_encoder_midi'
				indicator_value_buffer[encoder_bank][i]=control_change_value;
				// Then send the switch action
				if (bit & get_enc_switch_down()) {						
					send_element_midi(SWITCH, banked_encoder_id , 127, true);
				} else if (bit & get_enc_switch_up()){
					send_element_midi(SWITCH, banked_encoder_id , 0, false);
				}
			}
			break;				
			case ENC_RESET_VALUE_INV:{
				// Reset the encoder value
				uint8_t control_change_value; // = scale_encoder_value(raw_encoder_value[virtual_encoder_id]);
				if (encoder_settings[banked_encoder_id].has_detent){
					raw_encoder_value[virtual_encoder_id] = 6300;
					control_change_value = scale_encoder_value(raw_encoder_value[virtual_encoder_id]);
					} else {
					raw_encoder_value[virtual_encoder_id] = 12700;
					control_change_value = scale_encoder_value(raw_encoder_value[virtual_encoder_id]);
				}
				// Send the updated encoder value
				encoder_detent_counter[i] = 0;
				send_element_midi(ENCODER, banked_encoder_id, control_change_value, true);  // Last Encoder call to 'send_element_midi', all others upgraded to 'send_encoder_midi'
				indicator_value_buffer[encoder_bank][i]=control_change_value;
				// Then send the switch action
				if (bit & get_enc_switch_down()) {
					send_element_midi(SWITCH, banked_encoder_id , 127, true);
					} else if (bit & get_enc_switch_up()){
					send_element_midi(SWITCH, banked_encoder_id , 0, false);
				}
			}
			break;

			case ENC_SHIFT_HOLD:{
				if (bit & get_enc_switch_down()){
					// Update the display with the active color
					if (!color_overide_active(encoder_bank,i)) { // 20160622 - Allow midi feedback override of RGB Indicator.
						switch_color_buffer[encoder_bank][i] = encoder_settings[banked_encoder_id].active_color;
					}
					//switch_color_buffer[encoder_bank][i] = encoder_settings[banked_encoder_id].active_color;
					
					// Update the indicator display with the shift value
					sync_indicator_from_raw(encoder_bank, i, virtual_encoder_id);
					// Send a CC, unless the switch gave its number to the shift layer
					if (!shift_number_active(banked_encoder_id)) {
						midi_stream_raw_cc(encoder_settings[banked_encoder_id].switch_midi_channel, // Update 20160622 - send according to switch settings
						encoder_settings[banked_encoder_id].switch_midi_number,
						127);
					}
					
				} else if (bit & get_enc_switch_up()) {
					// Update the display the inactive color
					if (!color_overide_active(encoder_bank,i)) { // 20160622 - Allow midi feedback override of RGB Indicator.
						switch_color_buffer[encoder_bank][i] = encoder_settings[banked_encoder_id].inactive_color;
					}
					//switch_color_buffer[encoder_bank][i] = encoder_settings[banked_encoder_id].inactive_color;
					
					// Update the indicator display with the base encoder value
					sync_indicator_from_raw(encoder_bank, i, virtual_encoder_id);
					// Send a CC, unless the switch gave its number to the shift layer
					if (!shift_number_active(banked_encoder_id)) {
						midi_stream_raw_cc(encoder_settings[banked_encoder_id].switch_midi_channel, // Update 20160622 - send according to switch settings
						encoder_settings[banked_encoder_id].switch_midi_number,
						0);
					}
				}
			}
			break;
			
			case ENC_SHIFT_TOGGLE:{
				if (bit & get_enc_switch_down()){
					// Toggle state
					enc_switch_toggle_state[encoder_bank] ^= bit;
					virtual_encoder_id = get_virtual_encoder_id(encoder_bank, i); // !Summer2016Update Re-Read virtual_encoder_id
													// - now that toggle_state has been updated
					// - this case is change indicator display before change value access to back to the 'non-shifted encoder'		
					if (bit & enc_switch_toggle_state[encoder_bank]){
						// Update the display the active color
						if (!color_overide_active(encoder_bank,i)) { // 20160622 - Allow midi feedback override of RGB Indicator.
							switch_color_buffer[encoder_bank][i] = encoder_settings[banked_encoder_id].active_color;
						}
						//switch_color_buffer[encoder_bank][i] = encoder_settings[banked_encoder_id].active_color;

						// Update the indicator display with the shift value
						sync_indicator_from_raw(encoder_bank, i, virtual_encoder_id);
						
						// Send a CC, unless the switch gave its number to the shift layer
						// !Summer2016Update: Encoder Shift Switches now output CCs on Switch Channel like other special function switches
						if (!shift_number_active(banked_encoder_id)) {
							midi_stream_raw_cc(encoder_settings[banked_encoder_id].switch_midi_channel,
							encoder_settings[banked_encoder_id].switch_midi_number,
							127);
						}
						
					} else {
						// Update the display the inactive color
						if (!color_overide_active(encoder_bank,i)) { // 20160622 - Allow midi feedback override of RGB Indicator.
							switch_color_buffer[encoder_bank][i] = encoder_settings[banked_encoder_id].inactive_color;
						}
						//switch_color_buffer[encoder_bank][i] = encoder_settings[banked_encoder_id].inactive_color;
						
						// Update the indicator display with the base encoder value
						sync_indicator_from_raw(encoder_bank, i, virtual_encoder_id);
						
						// Send a CC, unless the switch gave its number to the shift layer
						// !Summer2016Update: Encoder Shift Switches now output CCs on Switch Channel like other special function switches
						if (!shift_number_active(banked_encoder_id)) {
							midi_stream_raw_cc(encoder_settings[banked_encoder_id].switch_midi_channel,
							encoder_settings[banked_encoder_id].switch_midi_number,
							0);
						}
						
					}
				}
			}
			break;
		}
	}
}
// ======================== Process Encoder Input Functions==================END=



/**
 * Shift mode is an alternate operation mode which is accessed by the side buttons
 * In shift mode the encoders do not do anything, only the switches have effect.
 * All 11 position LEDs are turned on and the RGB set to white if active.  
 * 
 * \param page [in]	0, 1 Which shift page is being accessed
 */
void run_shift_mode(uint8_t page){
	
	static uint8_t idx = 0x00;
	
	// First we check the encoder switch states and send any MIDI
	update_encoder_switch_state();
	uint16_t bit = 0x01;
	
	//uint16_t sw_down = get_enc_switch_down();
	
	for(uint8_t i=0;i<16;++i){
		if (bit & get_enc_switch_down()) {
			// Switch was just pressed
			midi_stream_raw_note(midi_system_channel, SHIFT_OFFSET + (i+(page*16)) , true, 127);
			// Set state if override is not active
			if(!(shift_mode_midi_override[page] & bit)){
				shift_mode_switch_state[page] |= bit;
			}
			
		} else if (bit & get_enc_switch_up()) {
			// Switch was just released
			//send_element_midi(SHIFT, i+(page*16), 0, false);
			midi_stream_raw_note(midi_system_channel, SHIFT_OFFSET + (i+(page*16)), false, 0);
			// Clear state if override is not active
			if(!(shift_mode_midi_override[page] & bit)){
				shift_mode_switch_state[page] &= ~bit;
			}
		}
		bit <<=1;
	}
		
	// The we update the display.
	if (shift_mode_switch_state[page] & (0x01<<idx)){
		// Set the LEDs on
		set_encoder_rgb(idx, 0);
		set_encoder_indicator(idx,127, false, BAR, 0);
	} else {
		set_encoder_rgb(idx, 0);
		set_encoder_indicator(idx,0, false, BAR, 0);
	}
	
	// Increment the encoder index for next time
	idx += 1;
	if(idx > 15){idx = 0;}
}

//void send_encoder_pitchbend(uint8_t midi_channel, uint16_t value, bool state, bool shifted) {
//	
//}

void send_encoder_midi(uint8_t banked_encoder_idx, uint8_t value, bool state, bool shifted)
{
	uint8_t midi_channel = shifted ? encoder_settings[banked_encoder_idx].encoder_shift_midi_channel: encoder_settings[banked_encoder_idx].encoder_midi_channel;
	uint8_t midi_number = encoder_output_number(banked_encoder_idx, shifted);
	// Sending encoder as note is not useful so this can likely be simplified
	// once incremental messages are added
	if (encoder_settings[banked_encoder_idx].encoder_midi_type == SEND_MCU_FADER)
	{
		// The 7 bit value would throw away most of the travel, so take the raw
		// value. The virtual id carries the shift layer, as get_virtual_encoder_id()
		// builds it.
		uint8_t virtual_id = banked_encoder_idx + (shifted ? BANKED_ENCODERS : 0);
		midi_stream_raw_pitchbend(midi_channel, raw_to_pitchbend(raw_encoder_value[virtual_id]));
	}
	else if (encoder_settings[banked_encoder_idx].encoder_midi_type == SEND_CC || encoder_settings[banked_encoder_idx].encoder_midi_type == SEND_SWITCH_VEL_CONTROL)
	{
		midi_stream_raw_cc(midi_channel,
		midi_number,
		value);
		//if (encoder_settings[banked_encoder_idx].is_super_knob && (value >= global_super_knob_start)) {
		if (encoder_settings[banked_encoder_idx].is_super_knob) {

			float step = 1/(((float)(global_super_knob_end - global_super_knob_start)) / 127.0f);
					
			uint16_t secondary_value = 0;
			if (value >= global_super_knob_start) {
				secondary_value = ((uint16_t)(step * (float)(value - global_super_knob_start)));
			}

			// Clamp to 127
			secondary_value = secondary_value > 127 ? 127 : secondary_value;

			MIDI_Device_Flush(g_midi_interface_info);
					
			//midi_stream_raw_cc(encoder_settings[banked_encoder_idx].encoder_midi_channel,
			midi_stream_raw_cc(midi_channel, 
			encoder_settings[banked_encoder_idx].encoder_midi_number+64,
			(uint8_t)secondary_value);		
		}			
	} else if (encoder_settings[banked_encoder_idx].encoder_midi_type == SEND_NOTE) {
		midi_stream_raw_note(midi_channel,
		midi_number,
		true,
		value);
	} else if (encoder_settings[banked_encoder_idx].encoder_midi_type == SEND_PROGRAM_CHANGE) {
		send_program_selection(midi_channel, value);
	}
}

bool switch_value_from_encoder_value_enabled(uint8_t banked_encoder_idx) {
	if (encoder_settings[banked_encoder_idx].encoder_midi_type == SEND_SWITCH_VEL_CONTROL) {
		return true;
	} else {
		return false;
	}
}

uint8_t adjust_switch_value_by_encoder_value(uint8_t banked_encoder_idx, uint8_t value, bool state) {
	if (!state) {
		return value;
	} else if (!switch_value_from_encoder_value_enabled(banked_encoder_idx)) {
		return value;
	} else {
		uint8_t enc_val = scale_encoder_value(raw_encoder_value[banked_encoder_idx]); // virtual encoder id not used, because shift buttons have no value
		return enc_val;
	}
}


/**
 * Sends MIDI for a given encoder or switch. Because MIDI can only be SENT
 * for encoder of the current bank this function only accepts an index range
 * of 0 - 15
 * 
 * \param type  [in]	The type of control, encoder or switch
 * 
 * \param index [in]	The index of the control (0 - 15)
  * 
 * \param state [in]	The Note On/Off state for note messages
 * 
 * \param value [in]	The new value of the control
 * 
 */

void send_element_midi(enc_control_type_t type, uint8_t banked_encoder_idx, uint8_t value, bool state)
{
	//if(banked_encoder_idx>15){
	if(banked_encoder_idx >= BANKED_ENCODERS){
		return;
	}
	
	switch (type){
		case ENCODER:{
			// Sending encoder as note is not useful so this can likely be simplified
			// once incremental messages are added
			if (encoder_settings[banked_encoder_idx].encoder_midi_type == SEND_CC || encoder_settings[banked_encoder_idx].encoder_midi_type == SEND_SWITCH_VEL_CONTROL)
			{
				midi_stream_raw_cc(encoder_settings[banked_encoder_idx].encoder_midi_channel,
										   encoder_settings[banked_encoder_idx].encoder_midi_number,
						                   value);
				//if (encoder_settings[banked_encoder_idx].is_super_knob && (value >= global_super_knob_start)) {
				if (encoder_settings[banked_encoder_idx].is_super_knob) {
					float step = 1/(((float)(global_super_knob_end - global_super_knob_start)) / 127.0f);
					
					uint16_t secondary_value = 0;
					if (value >= global_super_knob_start) {
						secondary_value = ((uint16_t)(step * (float)(value - global_super_knob_start)));
					}

					// Clamp to 127
					secondary_value = secondary_value > 127 ? 127 : secondary_value;

					MIDI_Device_Flush(g_midi_interface_info);
						
					midi_stream_raw_cc(encoder_settings[banked_encoder_idx].encoder_midi_channel,
										encoder_settings[banked_encoder_idx].encoder_midi_number+64,
										(uint8_t)secondary_value);
				
				} 
	
			} else if (encoder_settings[banked_encoder_idx].encoder_midi_type == SEND_NOTE) {
				midi_stream_raw_note(encoder_settings[banked_encoder_idx].encoder_midi_channel, 
								     encoder_settings[banked_encoder_idx].encoder_midi_number, 
								     true,
									 value);
			} else if (encoder_settings[banked_encoder_idx].encoder_midi_type == SEND_REL_ENC  || encoder_settings[banked_encoder_idx].encoder_midi_type == SEND_REL_ENC_MOUSE_EMU_DRAG || encoder_settings[banked_encoder_idx].encoder_midi_type == SEND_REL_ENC_MOUSE_EMU_SCROLL){ 
				midi_stream_raw_cc(encoder_settings[banked_encoder_idx].encoder_midi_channel,
										   encoder_settings[banked_encoder_idx].encoder_midi_number,
										   value);
			} else if (encoder_settings[banked_encoder_idx].encoder_midi_type == SEND_PROGRAM_CHANGE) {
				send_program_selection(encoder_settings[banked_encoder_idx].encoder_midi_channel, value);
			}
		}
		break;
		case SWITCH:{		
			if (encoder_settings[banked_encoder_idx].switch_action_type == CC_HOLD ||
				encoder_settings[banked_encoder_idx].switch_action_type == CC_TOGGLE ||
				encoder_settings[banked_encoder_idx].switch_action_type == ENC_SHIFT_HOLD ||
				encoder_settings[banked_encoder_idx].switch_action_type == ENC_SHIFT_TOGGLE ||
				encoder_settings[banked_encoder_idx].switch_action_type == ENC_RESET_VALUE ||
				encoder_settings[banked_encoder_idx].switch_action_type == ENC_RESET_VALUE_INV){
					
				uint8_t adj_val = adjust_switch_value_by_encoder_value(banked_encoder_idx, value, state);
				//midi_stream_raw_cc(encoder_settings[banked_encoder_idx].switch_midi_channel, 
				//		encoder_settings[banked_encoder_idx].switch_midi_number,
				//		adj_val);
		
				uint8_t midi_channel = encoder_settings[banked_encoder_idx].switch_midi_channel;
				uint8_t midi_number = encoder_settings[banked_encoder_idx].switch_midi_number;
				midi_stream_raw_cc(midi_channel, midi_number, adj_val);
				// Allow direct Bank Changes from appropriately matched switches: 20190807
				if (value) { // only change bank on 'press' not 'release'
					if (midi_channel == midi_system_channel) {
						if (midi_number < NUM_BANKS) {
							uint8_t i = banked_encoder_idx & 0x0F; // NUM_ENCODERS_SINGLE_BANK_MASK
							// Turn LED Off, leads to hung 'on' indicators leds if you don't...
							// Reset the MIDI state
							enc_switch_midi_state[encoder_bank][i] = 0;
							// Reset the display state to 'inactive
							if (!color_overide_active(encoder_bank,i)) {
								switch_color_buffer[encoder_bank][i] = encoder_settings[banked_encoder_idx].inactive_color;
							}
							// Actually change the bank
							change_encoder_bank(midi_number);
						}
					}
				}		   
			} else if (encoder_settings[banked_encoder_idx].switch_action_type == NOTE_HOLD ||
						encoder_settings[banked_encoder_idx].switch_action_type == NOTE_TOGGLE ||
						encoder_settings[banked_encoder_idx].switch_action_type == MCU_BUTTON) {
					
				uint8_t adj_val = adjust_switch_value_by_encoder_value(banked_encoder_idx, value, state);
				midi_stream_raw_note(encoder_settings[banked_encoder_idx].switch_midi_channel, 
						encoder_settings[banked_encoder_idx].switch_midi_number, 
						state,
						adj_val);						   
			} 
		}
		break;
		default:{

		}
		break;
	}	
}



/**
 * Takes a midi message and translates it to its related state buffer
 * 
 * \param channel [in]	The MIDI channel 
  * 
 * \param channel [in]	The MIDI control type CC/NOTE 
 * 
 * \param channel [in]	The MIDI note / control number
  * 
 * \param channel [in]	The value 
  * 
 * \param channel [in]	The boolean state (ignored if not a note)
 * 
 */

#define ENABLE_DUPLICATE_INPUT_MAPPINGS 1

void process_element_midi(uint8_t channel, uint8_t type, uint8_t number, uint8_t value, uint8_t state) // Midi Feedback - Main Routine
{
	// Watch the surface lamps first. This only reads them, so an encoder
	// mapped to the same notes still works.
	if ((type == SEND_NOTE) || (type == SEND_NOTE_OFF)) {
		mcu_observe_feedback_note(channel, number, value, (type == SEND_NOTE));
	}

	if(native_mode_consume_midi_event(type, channel, number, value))
		return;
	
	// If the incoming midi is in the system channel then its mapping is fixed 
	if (channel == midi_system_channel) {
		// Fixed for notes for now
		if ((type == SEND_NOTE) || (type == SEND_NOTE_OFF)){
			if ((number >= SHIFT_OFFSET) && (number <= SHIFT_OFFSET+32)){
				number = number - SHIFT_OFFSET;
				uint8_t index  = number % 16;
				uint8_t bank   = number / 16;
				// Set the corresponding override bit
				shift_mode_midi_override[bank] |= (0x0001 << index);
				if (value){
					shift_mode_switch_state[bank] |= (0x0001 << index);
					} else {
					shift_mode_switch_state[bank] &= ~(0x0001 << index);
				}
			}
		} 
	} else {
		// Otherwise the input is re mappable so scan through the input map for a match
		for(uint8_t i=0;i<BANKED_ENCODERS;++i){
			// Search the input map for a match
			// A V-Pot's ring answers on its own CC plus 0x20, so the ring byte
			// arrives on a number no encoder is mapped to.
			if ((type == SEND_CC) && (encoder_settings[i].encoder_midi_type == SEND_MCU_VPOT)
			    && (number == (uint8_t)(encoder_settings[i].encoder_midi_number + MCU_VPOT_RING_OFFSET))) {
				if (encoder_settings[i].encoder_midi_channel == channel) {
					process_vpot_ring_update(i, value, 0);
				} else if (encoder_settings[i].encoder_shift_midi_channel == channel) {
					process_vpot_ring_update(i, value, 1);
				}
				#if ENABLE_DUPLICATE_INPUT_MAPPINGS == 0
				continue;
				#endif
			}

			// The shift layer answers on the switch number when the switch gave it up
			bool base_number_match  = (encoder_settings[i].encoder_midi_number == number);
			bool shift_number_match = (encoder_output_number(i, true) == number);
			if(base_number_match || shift_number_match){
				uint8_t output_type = encoder_settings[i].encoder_midi_type;
				// Check Encoder Mapping for a Match
				if(base_number_match && encoder_settings[i].encoder_midi_channel == channel){
					// Matched to an encoder indicator
					// - !Summer2016Update: MIDI Type Filtering for MIDI Feedback
					if(type == SEND_CC){  
						if (output_type == SEND_CC || output_type == SEND_SWITCH_VEL_CONTROL || output_type == SEND_REL_ENC || output_type == SEND_REL_ENC_MOUSE_EMU_DRAG || output_type == SEND_REL_ENC_MOUSE_EMU_SCROLL) // Ensure MIDI Type Matches
						{
							process_indicator_update(i, value, 0); // !Summer2016Update: 0 = non-shifted encoder
						}
					}
					else { // Other Valid types that reach here are SEND_NOTE and SEND_NOTE_OFF, encoders treat them the same
						if (output_type == SEND_NOTE)
						{
							process_indicator_update(i, value, 0); // !Summer2016Update: 0 = non-shifted encoder		
						}
					}
					// !Summer2016Update: To allow for duplicate mappings across banks, we must continue to check for duplicate mappings
					// - Same goes for all other statements.
					#if ENABLE_DUPLICATE_INPUT_MAPPINGS == 0
					return;
					#endif
				} else if(shift_number_match && encoder_settings[i].encoder_shift_midi_channel == channel){
					// Matched to an shifted encoder's indicator
					// - !Summer2016Update: MIDI Type Filtering for MIDI Feedback
					if(type == SEND_CC){ 
						if (output_type == SEND_CC || output_type == SEND_SWITCH_VEL_CONTROL || output_type == SEND_REL_ENC || output_type == SEND_REL_ENC_MOUSE_EMU_DRAG){
							process_indicator_update(i, value, 1); // !Summer2016Update: 1 = shifted encoder
						}
					}
					else{ // Other Valid types that reach here are SEND_NOTE and SEND_NOTE_OFF, encoders treat them the same
						if (output_type == SEND_NOTE) {
							process_indicator_update(i, value, 1); // !Summer2016Update: 1 = shifted encoder							
						}
					}
					// 2016: Must continue to check for duplicate mappings to allow proper operation of a Master Knob that is the same in all four banks
					#if ENABLE_DUPLICATE_INPUT_MAPPINGS == 0
					return;
					#endif
				}
			} 
			// Check Switch Mapping for a Match
			if (encoder_settings[i].switch_midi_number == number){
				if(encoder_settings[i].switch_midi_channel == channel){
					// Matched to an encoder switch
					uint8_t action_type = encoder_settings[i].switch_action_type;
					// Check for Message type Match
					// - !Summer2016Update: MIDI Type Filtering for MIDI Feedback
					if ((action_type == NOTE_HOLD) || (action_type == NOTE_TOGGLE)){
						if (type == SEND_NOTE){
							process_sw_rgb_update(i, value);
							process_sw_toggle_update(i, value); // !Summer2016Update: Switch Toggle State Feedback
							#if ENABLE_DUPLICATE_INPUT_MAPPINGS == 0
							return;
							#endif
						}
						else if (type == SEND_NOTE_OFF){
							// !review: handle value 64 to use Ableton's Ability to light up unpopulated clips
							// - if so, should light white
							process_sw_rgb_update(i, 0);
							process_sw_toggle_update(i, 0);							
							//process_sw_rgb_update(i, value);
							//process_sw_toggle_update(i, value);
							#if ENABLE_DUPLICATE_INPUT_MAPPINGS == 0
							return;
							#endif							
						}
					} else if (action_type == MCU_BUTTON) {
						// A Mackie lamp carries a state and not a color: 0 is off,
						// 1 blinks and 127 is solid. process_sw_rgb_update() reads
						// 0 as the inactive color and 127 as the active one, so the
						// user picks both, which the protocol never names.
						if ((type == SEND_NOTE) || (type == SEND_NOTE_OFF)) {
							uint8_t lamp = (type == SEND_NOTE) ? value : MCU_LAMP_OFF;
							process_sw_rgb_update(i, (lamp == MCU_LAMP_OFF) ? 0 : 127);
							process_sw_animation_update(i, (lamp == MCU_LAMP_BLINK)
								? (midi_clock_enabled ? MCU_BLINK_RATE_CLOCKED : MCU_BLINK_RATE_FREE)
								: 0);
							#if ENABLE_DUPLICATE_INPUT_MAPPINGS == 0
							return;
							#endif
						}
					} else if (action_type != PC_RESEND) { // PC_RESEND sends no CC, so it takes no CC feedback
						// All other Switch Output Types will output a CC Message, and should expect to receive one in return.
						if (type == SEND_CC){
							process_sw_rgb_update(i, value);
							process_sw_toggle_update(i, value); // !Summer2016Update: Switch Toggle State Feedback
							//if ((action_type == ENC_SHIFT_HOLD) || (action_type == ENC_SHIFT_TOGGLE)){
								// Note: ENC_SHIFT_HOLD can't support software toggling of encoder shift state
								// - this is because HOLD type buttons are continually checked at a hardware level
								// - adding this feature would take a lot of work/testing for little benefit.
							//if (action_type == ENC_SHIFT_TOGGLE){
							//	process_sw_encoder_shift_update(i, value);
							//}
							#if ENABLE_DUPLICATE_INPUT_MAPPINGS == 0
							return;
							#endif
						}
					}
				} else if (channel == GET_ENC_ANIM_CHANNEL(global_animation_channels)) { 
					// Note: Animations are executed, so long as the number matches the switch
					// - This allows backward compatibility with a very lenient earlier protocol for twister (2014 builds)
					// Matched to encoder switch animation
					process_encoder_animation_update(i, value);
					#if ENABLE_DUPLICATE_INPUT_MAPPINGS == 0
					return;
					#endif
				} else if (channel == GET_SW_ANIM_CHANNEL(global_animation_channels)) {  // 2016: Dual software animation channels update
					// Matched to encoder switch animation
					process_sw_animation_update(i, value);
					#if ENABLE_DUPLICATE_INPUT_MAPPINGS == 0
					return;
					#endif
				}
			}
		}
	}
}

// Midi Feedback - Encoder Value Indicator Displays
// value: MIDI Value (7-bit)
// shifted: 	0 = Incoming messages is referencing base encoder mapping
// 		1 = Incoming message is referencing shifted encoder mapping
//void process_indicator_update(uint8_t idx, uint8_t value)
// Stores the ring byte the host sent for a V-Pot. The byte goes in the indicator
// buffer as it stands, and update_vpot_display() decodes it when drawing, so a
// V-Pot needs no storage of its own.
static void process_vpot_ring_update(uint8_t idx, uint8_t ring, uint8_t rx_msg_shifted_mapping)
{
	uint8_t bank = idx / 16;
	uint8_t encoder = idx % 16;
	uint8_t current_shift_state = encoder_is_in_shift_state(bank, encoder);

	if (current_shift_state == rx_msg_shifted_mapping) {
		indicator_value_buffer[bank][encoder] = ring;
	}
}

// Moves one Mackie fader to the position the host sent. This mirrors
// process_indicator_update(), with the 14 bit value of a fader in place of the
// 7 bit value of a CC.
static void process_fader_update(uint8_t idx, uint16_t bend, uint8_t rx_msg_shifted_mapping)
{
	uint8_t bank = idx / 16;
	uint8_t encoder = idx % 16;
	uint8_t current_shift_state = encoder_is_in_shift_state(bank, encoder);
	uint8_t virtual_encoder_id = idx + (rx_msg_shifted_mapping ? BANKED_ENCODERS : 0);

	// An encoder the user is turning keeps its own value, as a CC one does. So
	// the host echo of a move cannot fight the hand that made it.
	if ((bank == current_encoder_bank()) && encoder_is_active(encoder)) {
		return;
	}

	raw_encoder_value[virtual_encoder_id] = pitchbend_to_raw(bend);
	if (current_shift_state == rx_msg_shifted_mapping) {
		indicator_value_buffer[bank][encoder] = scale_encoder_value(raw_encoder_value[virtual_encoder_id]);
	}
}

// Pitch bend carries no number, so a fader is addressed by its channel alone.
void process_pitchbend_midi(uint8_t channel, uint16_t bend)
{
	for (uint8_t i = 0; i < BANKED_ENCODERS; ++i) {
		if (encoder_settings[i].encoder_midi_type != SEND_MCU_FADER) { continue; }

		if (encoder_settings[i].encoder_midi_channel == channel) {
			process_fader_update(i, bend, 0);
		} else if (encoder_settings[i].encoder_shift_midi_channel == channel) {
			process_fader_update(i, bend, 1);
		}
	}
}

void process_indicator_update(uint8_t idx, uint8_t value, uint8_t rx_msg_shifted_mapping) // !Summer2016Update: Added MIDI Feedback for Shifted Encoders
{		
	if (encoder_settings[idx].has_detent && value == 0) {
		return;
	}
	uint8_t bank = idx / 16;
	uint8_t encoder = idx % 16;
	//uint16_t mask = 0x0001 << encoder;
	uint8_t current_shift_state = encoder_is_in_shift_state(bank, encoder); 
	uint8_t virtual_encoder_id = idx;// can't use get_virtual_encoder_id(bank, encoder) because we may be accessing encoder outside of current shift state.
	virtual_encoder_id += rx_msg_shifted_mapping ? BANKED_ENCODERS:0; // Increment virtual_encoder_id to enable addressing of rx_msg_shifted_mapping encoders (if applicable)
	
	// !review: should be able to merge cases here, now that raw_encoder_value has been expanded
	// Update the raw value if bank is active, and the encoder is not currently moving
	if (bank == current_encoder_bank()) {
		if ( !encoder_is_active(encoder)  ||  encoder_midi_type_is_relative(encoder) ) {
			int16_t raw_value= ((uint16_t)value)*100;
			raw_encoder_value[virtual_encoder_id] = raw_value;
			if (current_shift_state == rx_msg_shifted_mapping) { // If Value is currently on display, update the display
				indicator_value_buffer[bank][encoder] = value;
			}
		}	
	} else {
	     // otherwise update the value buffer
		int16_t raw_value= ((uint16_t)value)*100;
		raw_encoder_value[virtual_encoder_id] = raw_value;
		if (current_shift_state == rx_msg_shifted_mapping) { // If Value is currently on display, update the display
			indicator_value_buffer[bank][encoder] = value;
		}
	}
}

void process_sw_rgb_update(uint8_t idx, uint8_t value) // Midi Feedback - Switch State Indicators (RGB LEDs)
{
	uint8_t bank = idx / 16;
	uint8_t encoder = idx % 16;
	
	// Accept either note or CC for color control
	if (value == 0){
		// Disable the color over ride
		switch_color_overide[bank] &= ~(0x01<<encoder);
		switch_color_buffer[bank][encoder] = encoder_settings[idx].inactive_color;
	} else if (value >0 && value < 126) { // Exclude 126 as we don't allow user to set color to white
		// Enable the override and set the color to value
		switch_color_overide[bank] |= (0x01<<encoder);
		switch_color_buffer[bank][encoder] = value;
	} else {
		// Read Directly from RAM
		// uint8_t banked_encoder_id = idx;
		// Enable the override and set the color to active
		switch_color_overide[bank] |= (0x01<<encoder);
		switch_color_buffer[bank][encoder] = encoder_settings[idx].active_color;
	}
}

// Midi Feedback - Switch Stored Toggle State (RGB LEDs) - !Summer2016Update
void process_sw_toggle_update(uint8_t idx, uint8_t value) 
{
	uint8_t bank = idx / 16;
	uint8_t encoder = idx % 16;
	//uint8_t action_type = encoder_settings[i].switch_action_type;
	//enc_switch_toggle_state  // toggle_state is "used to track" midi_state
	enc_switch_midi_state[bank][encoder] = value ? 127:0;	// midi_state appears to be the proper variable for modification here
	/*if (action_type == ENC_SHIFT_TOGGLE){ 
		// SHIFT_TOGGLE encoders also use enc_switch_toggle_state, updated it
		uint16 bit = 0x01 << encoder;
		if (value){
			enc_switch_toggle_state[bank] |= bit;
		}
		else {
			enc_switch_toggle_state[bank] &= bit;
		}
		// Now change the active encoder based on this change

	}
	else if (action_type == ENC_SHIFT_HOLD){
		// Change active encoder
		
	}*/
}

// Midi Feedback - Switch Stored Toggle State for Shift Encoder Toggle Switches (also updates Encoder Value Indicator LEDS
// - !Summer2016Update
void process_sw_encoder_shift_update(uint8_t idx, uint8_t value){
	uint8_t bank = idx / 16;
	uint8_t encoder = idx % 16;
	uint16_t bit = 0x01 << encoder; // for updating enc_switch_toggle_state
			
	// SHIFT_TOGGLE encoders also use enc_switch_toggle_state, so we'll update it (this is different from enc_switch_midi_state)
	if (value){
		enc_switch_toggle_state[bank] |= bit;
	}
	else {
		enc_switch_toggle_state[bank] &= ~bit;
	}
	
	// Update Encoder Value Display
	// - Get virtual encoder number so we know which value to display (shifted or unshifted)
	// - this must stay after the update of enc_switch_toggle_state!
	uint8_t virtual_encoder_id = idx;
	if (encoder_is_in_shift_state(bank, encoder))
		{virtual_encoder_id += BANKED_ENCODERS;}
	sync_indicator_from_raw(bank, encoder, virtual_encoder_id); // update display buffer
}

void process_sw_animation_update(uint8_t idx, uint8_t value)
{
	uint8_t bank = idx / 16;
	uint8_t encoder = idx % 16;
	switch_animation_buffer[bank][encoder] = value;
}

void process_encoder_animation_update(uint8_t idx, uint8_t value)	// !Summer2016Update: dual animations
{
	uint8_t bank = idx / 16;
	uint8_t encoder = idx % 16;
	encoder_animation_buffer[bank][encoder] = value;
}

void process_shift_update(uint8_t idx, uint8_t value)
{
	
}

/**
 * 	For each iteration of the main loop we compare the current MIDI state with the
 *  previous state, if changed we update that display element.
 *
**/

static uint8_t prevIndicatorValue[16];
static uint8_t prevSwitchColorValue[16];
static uint8_t prevEncoderAnimationValue[16];
static uint8_t prevSwAnimationValue[16];

// - Switch Animations 1-48, 127
bool animation_is_switch_rgb(uint8_t animation_value) { // !Summer2016Update: Dual Animations - Identify to Eliminate Conflicts
	if (!animation_value) {return false;}
	else if (animation_value < 49 || animation_value == 127) {return true;}
	else {return false;}
}

// - Indicator Animations 49-96
bool animation_is_encoder_indicator(uint8_t animation_value) { // !Summer2016Update: Dual Animations - Identify to Eliminate Conflicts
	if (animation_value < 49 || animation_value == 127) {return false;}
	else if (animation_value < 97) {return true;}
	else {return false;}
}

// Detect if two Animations Buffers are both attempting to animate the same item
// - Switch Animations 1-48, 127
// - Indicator Animations 49-96
bool animation_buffer_conflict_exists(uint8_t bank, uint8_t encoder) {
	uint8_t this_animation = switch_animation_buffer[bank][encoder];
	uint8_t that_animation = encoder_animation_buffer[bank][encoder];
	
	if (!this_animation || !that_animation) { // Case: this or that animation is OFF
		return false;
	} else if (this_animation > 127 || that_animation > 127) { // Case: this or that animation is invalid
		return false;
	}else if (this_animation < 49 || this_animation == 127 ) { // Case: this_animation is a Switch animation
		if (that_animation < 49 || that_animation == 127){  // Case: Both are switch animations
			return true;
		} else {  // Case: one is switch, one is not
			return false;
		}
	} else if (this_animation < 97 ) { // if we get here, this_animation cannot be 0,1-48, or 127+ (must be 49-126)
		if (that_animation > 48 && that_animation < 97) { // Case: both are indicator animations
			return true;
		}
		else { // Case: one is a indicator animation, one is not
			return false;
		}
	} else { // Case: this_animation is invalid
		return false;
	}
}

// Draws a V-Pot from the ring byte the host sent. The byte holds the mode in
// bits 5-4, the position in bits 3-0 and the center LED in bit 6, so the host
// owns the ring style and not the encoder's own setting.
static void update_vpot_display(const uint8_t idx, uint8_t ring, uint8_t banked_encoder_idx)
{
	uint8_t mode = (ring >> 4) & 0x03;
	uint8_t ring_pos = ring & 0x0F;
	bool center_led = (ring & 0x40) != 0;
	uint16_t type;
	uint8_t position;

	if (mode == MCU_RING_SPREAD) {
		// Spread carries a half width of 0 to 6, where the others carry 1 to 11.
		// Treating it like the others would read at half scale.
		if (ring_pos > 5) { ring_pos = 5; }
		type = SPREAD_BAR;
		position = (uint8_t)(1 + (((uint16_t)ring_pos * 126u) / 5u));
	} else if (ring_pos == 0) {
		type = (mode == MCU_RING_BOOST_CUT) ? CENTER_BAR : DOT;
		position = 0;                       // all off
	} else if (mode == MCU_RING_BOOST_CUT) {
		type = CENTER_BAR;
		// Position 6 is the center, so 63 plus the offset in LED steps.
		position = (uint8_t)(63 + (((int16_t)ring_pos - 6) * 127) / 10);
	} else {
		type = (mode == MCU_RING_DOT) ? DOT : BLENDED_BAR;
		position = (uint8_t)(((uint16_t)ring_pos * 127u) / 11u);
	}

	set_encoder_indicator(idx, position, false, type, 0);

	// The center LED is the host's to command, so it lights the detent LED. A
	// detent the device invents does not. This must follow the ring write, which
	// clears the detent lane.
	if (center_led) {
		set_encoder_indent(idx, encoder_settings[banked_encoder_idx].detent_color);
	} else {
		clear_encoder_indent(idx);
	}
}

static void update_encoder_display_single(const uint8_t idx)
{
	if(native_mode_update_encoder_display_single(idx))
		return;
	
//#define FORCE_UPDATE
#ifdef FORCE_UPDATE
	#warning FORCE UPDATE is Enabled, may impede some LED Operations
	set_encoder_indicator(idx, indicator_value_buffer[encoder_bank][idx], encoder_shows_detent(idx),
	encoder_settings[idx].indicator_display_type,
	encoder_settings[idx].detent_color);
	set_encoder_rgb(idx, switch_color_buffer[encoder_bank][idx]);
#endif 
	
	// First the indicator display
	uint8_t currentValue = indicator_value_buffer[encoder_bank][idx];
	uint8_t banked_encoder_idx = idx + encoder_bank*PHYSICAL_ENCODERS;				
	if (currentValue != prevIndicatorValue[idx]) {
		if (encoder_settings[banked_encoder_idx].encoder_midi_type == SEND_MCU_VPOT) {
			// The buffer holds the host's ring byte for a V-Pot, not a position.
			update_vpot_display(idx, currentValue, banked_encoder_idx);
		} else {
			set_encoder_indicator(idx, currentValue, encoder_shows_detent(banked_encoder_idx),
								mcu_fader_display_type(banked_encoder_idx),
								encoder_settings[banked_encoder_idx].detent_color);
		}
		prevIndicatorValue[idx] = currentValue;
	}
	
	// Next the RGB display. A V-Pot shows the assignment mode instead of its own
	// color, because the protocol names no color and the mode is what the encoder
	// is doing. A fader keeps its switch color, which under MCU_BUTTON is already
	// the lamp the host lit.
	currentValue = switch_color_buffer[encoder_bank][idx];
	{
		int8_t mcu_color = mcu_mode_color(banked_encoder_idx);
		if (mcu_color >= 0) { currentValue = (uint8_t)mcu_color; }
	}
	if (currentValue != prevSwitchColorValue[idx]) {
		set_encoder_rgb(idx, currentValue);
		prevSwitchColorValue[idx] = currentValue;
	}
	
	// Finally if animation is active for this encoder run the animation
	// If it has just changed from active to inactive reset the RGB to its
	// pre-animation state
	
	// !Summer2016Update: Dual Animations 
	// - created encoder_animation_buffer, which double-uses run_encoder_animation

	// rmerrill 2023-08-23
	// we ignore animation types from the "wrong" buffer now.
	// encoder_animation_buffer's values are ignored unless they apply to encoder
	// likewise with switch_animation_buffer.

	currentValue = encoder_animation_buffer[encoder_bank][idx];
	if (currentValue && animation_is_encoder_indicator(currentValue)){
		run_encoder_animation(idx, encoder_bank, currentValue, switch_color_buffer[encoder_bank][idx]);
		prevEncoderAnimationValue[idx] = currentValue;
	} else if (prevEncoderAnimationValue[idx]) {
		set_encoder_indicator(idx, indicator_value_buffer[encoder_bank][idx], encoder_shows_detent(banked_encoder_idx),
				encoder_settings[banked_encoder_idx].indicator_display_type,
				encoder_settings[banked_encoder_idx].detent_color);
		prevEncoderAnimationValue[idx] = 0;
	}

	// Run Switch Animation, or set the indicator to the last set color.
	currentValue = switch_animation_buffer[encoder_bank][idx];
	if (currentValue && animation_is_switch_rgb(currentValue)) {
		run_encoder_animation(idx, encoder_bank, currentValue, switch_color_buffer[encoder_bank][idx]);
		prevSwAnimationValue[idx] = currentValue;
	}  else if (prevSwAnimationValue[idx]) {  // Animation Just Ended
		set_encoder_rgb(idx, switch_color_buffer[encoder_bank][idx]);
		prevSwAnimationValue[idx] = 0;
	}
}

void update_encoder_display(void)
{	
	static uint8_t idx = 0;
	
	update_encoder_display_single(idx);
	
	// Increment the encoder index for next time
	idx += 1;
	if(idx > 15){idx = 0;}
}

/**
 * Rebuilds the entire display, this is called when the unit powers up
 * or when changing banks.
 * 
 * \param[in] new_bank		The bank number we are changing to.
 * 
 */
void change_encoder_bank(uint8_t new_bank) // Change Bank
{
	// Prepare the state buffers for the new bank
	transfer_encoder_values_to_other_banks(encoder_bank);

	for(uint8_t i =0;i<16;++i){
		uint8_t old_virtual_encoder_id = get_virtual_encoder_id(encoder_bank, i);
		uint8_t new_virtual_encoder_id = get_virtual_encoder_id(new_bank, i);
		
		// Save previous raw values
		sync_indicator_from_raw(encoder_bank, i, old_virtual_encoder_id);
		
		// Update raw values for new bank 
		// - !Summer2016Update: Removed in favor of expansion of raw_encoder_value to include all banks and shifted encoders
		//raw_encoder_value[i] = indicator_value_buffer[new_bank][i] * 100;
		// Save current raw shift states for previous bank 
		//if((encoder_settings[i].switch_action_type == ENC_SHIFT_HOLD) || 
		   //(encoder_settings[i].switch_action_type == ENC_SHIFT_TOGGLE)){
			   //enc_switch_midi_state[encoder_bank][i] = raw_shift_encoder_value[i] / 100;
		//}

		// Set the prev values to -1 which forces a display update
		prevIndicatorValue[i] = -1;
		prevSwitchColorValue[i] = -1;	
		
		// Read in all the encoder settings for the current bank
		// - !Summer2016Update: Removed in favor of expanding encoder_settings to include all banks 
		//~ get_encoder_config(new_bank, i, &encoder_settings[i]);  
		
		// Update raw shift states for new bank
		// - !Summer2016Update: Removed in favor of expansion of raw_encoder_value to include all banks and shifted encoders
		//if((encoder_settings[i].switch_action_type == ENC_SHIFT_HOLD) || 
		   //(encoder_settings[i].switch_action_type == ENC_SHIFT_TOGGLE)){
			   //raw_shift_encoder_value[i] = enc_switch_midi_state[new_bank][i] * 100;
		   //}
		sync_indicator_from_raw(new_bank, i, new_virtual_encoder_id);

		/* !Summer2016Update: Removed Double use of enc_switch_midi_state by expanding of raw_encoder_value table
		 * // Check to see if encoder is in a shift state, and if so update its indicator
		if (encoder_is_in_shift_state(new_bank, i)){
			indicator_value_buffer[new_bank][i] = enc_switch_midi_state[new_bank][i];
		}*/		
	} 
	
	encoder_bank = new_bank;                                                 
}

/**
 * Returns the current encoder bank index
 * 
 * \return	The current encoder bank index
 */
uint8_t current_encoder_bank(void)
{
	return encoder_bank;
}

/**
 * Convenience function which forces a refresh of the encoder display and values
 */
void refresh_display(void){
	change_encoder_bank(current_encoder_bank());
}

uint8_t scale_encoder_value(int16_t value)
{
	uint8_t scaled_value;
	// Detent Checking removed for encoders without 'detent checking enabled' at user request 6/22/2016
	// - because doing that is redundant, and causes value '64' to not be output
	// -- if users want a detent, they will enable the center 'detent' feature in the MF Utility, which still functions.
	scaled_value = (uint8_t) ((value+50)/100);
	//~ if (encoder_is_in_detent(value))
	//~ {
		//~ scaled_value = 63;
	//~ } else {
		//~ scaled_value = (uint8_t) ((value+50)/100);
	//~ }
	return scaled_value;
}

int16_t clamp_encoder_raw_value(int16_t value)
{
	if (value < 0){value = 0;}
	if (value > 12700){value = 12700;}
	return value;
}



bool encoder_is_in_detent(int16_t value)
{
	//if (value > 6200 && value < 6500) // values of 62 do not trigger the 'detent' led routines, so we need to be sure value is at least 6300
	//if (value >= 6300 && value < 6500)
	if (value > encoder_detent_limit_low && value < encoder_detent_limit_high) 
	{									// - so to get value '64' to output in velocity sensing mode, we need to get down this low.
		return true;
	}
	return false;
}



bool encoder_is_in_deadzone(int16_t value)
{
	if (value > 12699 || value < 1){
		return true;
	}	
	return false;	
}

/**
 * Returns the color over ride state for a given encoder 
 * 
 * \param bank [in]		The encoders bank
 *
 * \param encoder [in]  The encoder
 * 
 * \return true if over ride is active
 */

bool color_overide_active(uint8_t bank, uint8_t encoder)
{
	uint16_t bit = 0x01 << encoder;
	bool active = false;
	active = (bit & switch_color_overide[bank]) ? true : false; 
	return active;
}

/**
 * Returns true if encoder is in a shift state
 */
bool encoder_is_in_shift_state(uint8_t bank, uint8_t encoder)
{
	uint8_t banked_encoder_idx = encoder + bank*PHYSICAL_ENCODERS;				
	uint16_t bit = 0x0001 << encoder;

	/*if ((encoder_settings[encoder].switch_action_type == ENC_SHIFT_HOLD ||
		 encoder_settings[encoder].switch_action_type == ENC_SHIFT_TOGGLE) &&
	   ((get_enc_switch_state() & bit) || (enc_switch_toggle_state[bank] & bit)))*/ 
	if ( ((encoder_settings[banked_encoder_idx].switch_action_type == ENC_SHIFT_HOLD)  && (get_enc_switch_state() & bit)) ||
		 ((encoder_settings[banked_encoder_idx].switch_action_type == ENC_SHIFT_TOGGLE) && (enc_switch_toggle_state[bank] & bit)) ) 
	{
		return true;
	} else  {
		return false;
	}
}
// This function assumes bank == current bank
bool encoder_midi_type_is_relative(uint8_t encoder) 
{
	uint8_t banked_encoder_idx = encoder + encoder_bank*PHYSICAL_ENCODERS;				
	if (encoder_settings[banked_encoder_idx].encoder_midi_type == SEND_REL_ENC || encoder_settings[banked_encoder_idx].encoder_midi_type == SEND_REL_ENC_MOUSE_EMU_DRAG || encoder_settings[banked_encoder_idx].encoder_midi_type == SEND_REL_ENC_MOUSE_EMU_SCROLL || encoder_settings[banked_encoder_idx].encoder_midi_type == SEND_MCU_VPOT){
		return true;
	}
	else{
		return false;
	}
}
