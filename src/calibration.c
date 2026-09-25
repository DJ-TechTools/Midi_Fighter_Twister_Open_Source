/*
 * calibration.c
 *
 * RGB white balance calibration. The three dies have different efficiency, and
 * the diffuser tints the mix. Thus #FFFFFF shows a color cast that is different
 * on each unit, and on each LED. This stores a trim for each encoder and each
 * channel, and build_rgb() applies it to all colors.
 *
 * The trim is on EEPROM page 63, with its own magic and checksum and not
 * EEPROM_LAYOUT. Thus a change of the layout version keeps the calibration.
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

#include "calibration.h"

#include "config.h"
#include "display_driver.h"
#include "input.h"

// Side switch assignments. Channel selectors on the right column, commands on
// the left. The masks live in input.h, shared with setup_mode.c.
//
// There is no save button here. Calibration edits RAM only, and the setup menu
// writes every setting on one press.
#define CAL_SW_BACK				SW_LEFT_TOP
#define CAL_SW_RESTORE			SW_LEFT_MID
#define CAL_SW_RED				SW_RIGHT_TOP
#define CAL_SW_GREEN			SW_RIGHT_MID
#define CAL_SW_BLUE				SW_RIGHT_BOT

// set_encoder_indent() mixes red and blue from a 0-127 index. Index 0 gives
// red only, 127 gives blue only, and the midpoint lights both. These values
// come from the function itself.
#define CAL_DETENT_RED				0
#define CAL_DETENT_BLUE				127
#define CAL_DETENT_BOTH				64

#define CAL_FLASH_MS				200

// Save confirmation color, passed to setting_confirmation_animation().
#define CAL_SAVED_COLOR				0x00FF00

// Ring readout for the channel that the user adjusts. BLENDED_BAR fades in the
// last LED, thus each level shows. The brightness is low, because
// the white ring is next to the color that the user judges. The usable scale is
// 0 to 79, thus this is approximately 40%.
#define CAL_RING_TYPE				BLENDED_BAR
#define CAL_RING_BRIGHTNESS			32

// The calibration in RAM: one level for each encoder and channel, 0 to
// CAL_LEVEL_MAX. The encoders are independent. There is no global value. The
// editor moves all 16 levels together to give the same control.
static uint8_t	cal_level[16][3];

// The encoders that the last move stopped at an end of the range, for the
// channel on display. They no longer follow the others, and the detent shows it.
static uint16_t	cal_blocked;

static const uint8_t cal_defaults[3] = { DEF_CAL_TRIM_R, DEF_CAL_TRIM_G, DEF_CAL_TRIM_B };


/**
 * Returns the trim of a level. Level 0 is CAL_TRIM_MIN and the top level is
 * CAL_TRIM_MAX.
 */
static uint8_t level_to_trim(uint8_t level)
{
	return (uint8_t)(CAL_TRIM_MIN + level * CAL_LEVEL_STEP);
}

/**
 * Returns the level nearest to a trim. A trim outside the range gives the end
 * level.
 */
static uint8_t trim_to_level(int16_t trim)
{
	if (trim < CAL_TRIM_MIN) { trim = CAL_TRIM_MIN; }
	if (trim > CAL_TRIM_MAX) { trim = CAL_TRIM_MAX; }
	return (uint8_t)((trim - CAL_TRIM_MIN + (CAL_LEVEL_STEP / 2)) / CAL_LEVEL_STEP);
}

/**
 * Gives the 48 trims to the display driver. Call this after each change to a
 * level. Thus build_rgb does not do this arithmetic.
 */
static void calibration_apply(void)
{
	for (uint8_t encoder = 0; encoder < 16; ++encoder) {
		set_rgb_trim(encoder,
					 level_to_trim(cal_level[encoder][0]),
					 level_to_trim(cal_level[encoder][1]),
					 level_to_trim(cal_level[encoder][2]));
	}
}

/**
 * Puts one encoder at the factory levels.
 */
static void calibration_default_encoder(uint8_t encoder)
{
	for (uint8_t channel = 0; channel < 3; ++channel) {
		cal_level[encoder][channel] = trim_to_level(cal_defaults[channel]);
	}
}

/**
 * Returns the sum of page[first] to page[end - 1].
 */
static uint8_t calibration_checksum(const uint8_t *page, uint8_t first, uint8_t end)
{
	uint8_t sum = 0;
	for (uint8_t i = first; i < end; ++i) {
		sum += page[i];
	}
	return sum;
}

/**
 * Returns the check byte of the 36 data bytes.
 */
static uint8_t calibration_check(const uint8_t *data)
{
	return (uint8_t)(EE_CAL_CHECK_SEED + calibration_checksum(data, 0, EE_CAL_DATA_BYTES));
}

/**
 * Reads one packed level. Level N uses bits 6N to 6N + 5, LSB first. The window
 * reads one byte past the level, thus the buffer has one spare byte at the end.
 * A value above CAL_LEVEL_MAX cannot come from a save, and gives the top level.
 */
static uint8_t unpack_level(const uint8_t *levels, uint8_t index)
{
	uint16_t bit    = (uint16_t)index * CAL_LEVEL_BITS;
	uint8_t  byte   = (uint8_t)(bit >> 3);
	uint8_t  shift  = (uint8_t)(bit & 0x07);
	uint16_t window = (uint16_t)levels[byte] | ((uint16_t)levels[byte + 1] << 8);
	uint8_t  level  = (uint8_t)((window >> shift) & ((1 << CAL_LEVEL_BITS) - 1));

	return (level > CAL_LEVEL_MAX) ? CAL_LEVEL_MAX : level;
}

/**
 * Writes one packed level into zeroed bytes.
 */
static void pack_level(uint8_t *levels, uint8_t index, uint8_t level)
{
	uint16_t bit    = (uint16_t)index * CAL_LEVEL_BITS;
	uint8_t  byte   = (uint8_t)(bit >> 3);
	uint8_t  shift  = (uint8_t)(bit & 0x07);
	uint16_t window = (uint16_t)(level & ((1 << CAL_LEVEL_BITS) - 1)) << shift;

	levels[byte]     |= (uint8_t)window;
	levels[byte + 1] |= (uint8_t)(window >> 8);
}

/**
 * Converts a format 1 page into levels. Format 1 had a global trim for each
 * channel, clamped to 64..158, and a signed 4 bit delta for each encoder
 * channel in steps of 2. The shown trim was the sum, clamped again. This keeps
 * that trim, at the nearest level. It changes RAM only. The next save writes
 * format 2.
 */
static void calibration_convert_v1(const uint8_t *page)
{
	for (uint8_t index = 0; index < 48; ++index) {
		uint8_t channel = index % 3;

		int16_t global = page[EE_CAL_V1_GLOBAL_OFFSET + channel];
		if (global < EE_CAL_V1_TRIM_MIN) { global = EE_CAL_V1_TRIM_MIN; }
		if (global > CAL_TRIM_MAX)       { global = CAL_TRIM_MAX; }

		uint8_t packed = page[EE_CAL_V1_DELTA_OFFSET + (index >> 1)];
		uint8_t nibble = (index & 1) ? (packed >> 4) : (packed & 0x0F);
		int8_t  delta  = (nibble & 0x08) ? (int8_t)(nibble | 0xF0) : (int8_t)nibble;

		int16_t trim = global + (int16_t)delta * EE_CAL_V1_DELTA_SCALE;
		if (trim < EE_CAL_V1_TRIM_MIN) { trim = EE_CAL_V1_TRIM_MIN; }

		cal_level[index / 3][channel] = trim_to_level(trim);
	}
}

void calibration_init(void)
{
	// Page 63, then the 4 bytes in page 0, then a spare byte for unpack_level().
	uint8_t data[EE_CAL_DATA_BYTES + 1];

	for (uint8_t encoder = 0; encoder < 16; ++encoder) {
		calibration_default_encoder(encoder);
	}
	cal_blocked = 0;

	uint8_t check;

	cpu_irq_disable();
	nvm_eeprom_read_buffer(EE_CAL_START, data, EEPROM_PAGE_SIZE);
	for (uint8_t i = 0; i < (EE_CAL_DATA_BYTES - EEPROM_PAGE_SIZE); ++i) {
		data[EEPROM_PAGE_SIZE + i] = eeprom_read(EE_CAL_EXTRA_START + i);
	}
	check = eeprom_read(EE_CAL_CHECK);
	cpu_irq_enable();
	data[EE_CAL_DATA_BYTES] = 0;

	// Format 1 first. Its page 0 bytes are unwritten, thus the format 2 check
	// could match by chance. An unwritten block fails both checks and keeps the
	// defaults.
	if ((data[0] == EE_CAL_V1_MAGIC) &&
		(data[EE_CAL_V1_VERSION_OFFSET] == EE_CAL_V1_VERSION) &&
		(data[EE_CAL_V1_CHECKSUM_OFFSET] ==
		 calibration_checksum(data, EE_CAL_V1_GLOBAL_OFFSET, EE_CAL_V1_CHECKSUM_OFFSET))) {

		calibration_convert_v1(data);
	} else if (check == calibration_check(data)) {
		for (uint8_t index = 0; index < 48; ++index) {
			cal_level[index / 3][index % 3] = unpack_level(data, index);
		}
	}

	calibration_apply();
}

/**
 * Packs the levels and writes them: page 63 in one atomic write, as
 * save_encoder_config() does, then the bytes in page 0, with the check byte
 * last. A power loss before the check byte leaves a check that fails, thus the
 * unit starts with the defaults and not with a mix.
 */
static void calibration_save(void)
{
	// The spare byte at the end is for pack_level().
	uint8_t data[EE_CAL_DATA_BYTES + 1];

	memset(data, 0, sizeof(data));
	for (uint8_t index = 0; index < 48; ++index) {
		pack_level(data, index, cal_level[index / 3][index % 3]);
	}

	// EEPROM writes are slow enough to trip the watchdog.
	wdt_disable();
	cpu_irq_disable();
	nvm_eeprom_load_page_to_buffer(data);
	nvm_eeprom_atomic_write_page(EE_CAL_PAGE);
	for (uint8_t i = 0; i < (EE_CAL_DATA_BYTES - EEPROM_PAGE_SIZE); ++i) {
		eeprom_write(EE_CAL_EXTRA_START + i, data[EEPROM_PAGE_SIZE + i]);
	}
	eeprom_write(EE_CAL_CHECK, calibration_check(data));
	cpu_irq_enable();
}

/**
 * Maps a level onto the 0-127 indicator scale.
 *
 * Starts at 1 and not 0. build_indicator_pattern turns the ring fully off at
 * position 0. The lowest level then looks like a broken indicator and not a
 * value.
 */
static uint8_t level_to_position(uint8_t level)
{
	return (uint8_t)(1u + (((uint16_t)level * 126u) / CAL_LEVEL_MAX));
}

/**
 * Paints the calibration display. All encoders show white through the trim,
 * thus the user judges what the trim gives.
 *
 * The white ring of each encoder shows its own level for the channel that the
 * user adjusts. Thus the user sees the differences across the 16 immediately.
 *
 * The detent LED is a separate red and blue part. It has no bits in common with
 * the RGB ring. Thus it can show selection and limit status, and not change the
 * white.
 */
static void calibration_draw(int8_t single, uint8_t channel)
{
	for (uint8_t encoder = 0; encoder < 16; ++encoder) {
		build_rgb(encoder, 0xFFFFFF, 0);

		// This must come before the detent write, because it clears bits 0 and 1.
		set_encoder_indicator_level(encoder, level_to_position(cal_level[encoder][channel]),
									false, CAL_RING_TYPE, 0, CAL_RING_BRIGHTNESS);

		bool blocked  = (cal_blocked & (0x0001 << encoder)) != 0;
		bool selected = (single == (int8_t)encoder);

		if (selected && blocked) {
			set_encoder_indent(encoder, CAL_DETENT_BOTH);
		} else if (selected) {
			set_encoder_indent(encoder, CAL_DETENT_BLUE);
		} else if (blocked) {
			set_encoder_indent(encoder, CAL_DETENT_RED);
		} else {
			clear_encoder_indent(encoder);
		}
	}
}

/**
 * Flashes all LEDs in the pure color of the selected channel, at full
 * brightness. The flash does not use the trim. With the trim, the color is not
 * pure and the brightness is not full.
 */
static void calibration_flash_channel(uint8_t channel)
{
	static const uint32_t channel_color[3] = { 0xFF0000, 0x00FF00, 0x0000FF };

	set_rgb_trim_bypass(true);
	for (uint8_t encoder = 0; encoder < 16; ++encoder) {
		build_rgb(encoder, channel_color[channel], 0);
	}
	_delay_ms(CAL_FLASH_MS);
	set_rgb_trim_bypass(false);
}

/**
 * Moves one encoder channel by a number of levels and clamps it. An encoder
 * that the clamp stopped is marked blocked. A free move clears the mark.
 */
static void calibration_move(uint8_t encoder, uint8_t channel, int16_t steps)
{
	uint16_t bit   = 0x0001 << encoder;
	int16_t  level = (int16_t)cal_level[encoder][channel] + steps;

	if (level < 0) {
		level = 0;
		cal_blocked |= bit;
	} else if (level > CAL_LEVEL_MAX) {
		level = CAL_LEVEL_MAX;
		cal_blocked |= bit;
	} else {
		cal_blocked &= (uint16_t)~bit;
	}
	cal_level[encoder][channel] = (uint8_t)level;
}

/**
 * Puts encoders at the factory levels, on all three channels: all 16, or only
 * the single encoder. This changes RAM only. The user must still save.
 */
static void calibration_restore(int8_t single)
{
	for (uint8_t encoder = 0; encoder < 16; ++encoder) {
		if ((single < 0) || (single == (int8_t)encoder)) {
			calibration_default_encoder(encoder);
			cal_blocked &= (uint16_t)~(0x0001 << encoder);
		}
	}
	calibration_apply();
}

/**
 * The interactive calibration editor.
 *
 * At the start a turn of any encoder moves all 16 encoders together. This
 * gives the control of a global value, without one. A press starts single
 * encoder mode for that encoder. Its detent dot comes on blue, and only that
 * encoder moves. A second press on it goes back to all 16, and a press on a
 * different encoder moves single encoder mode there.
 *
 * This editor changes the levels in RAM and paints the display. It never writes
 * EEPROM. It returns when the user presses Back, and the setup menu that called
 * it continues. The menu controls entry, exit and the save.
 *
 * Returns true if the user changed a value. Thus the menu knows if it must
 * write the calibration when the user saves.
 */
bool calibration_edit(void)
{
	uint8_t channel = 0;		// 0 = red, 1 = green, 2 = blue
	int8_t  single  = -1;		// -1 = all 16, otherwise the chosen encoder
	bool    dirty   = true;		// repaint only on change, not every pass
	bool    touched = false;	// true when the user changed a value

	cal_blocked = 0;

	// Paint first. Otherwise the panel stays dark while the user still holds the
	// encoder that opened this editor.
	calibration_draw(single, channel);

	// Discard the press that opened this editor, thus the first input is real.
	for (uint8_t encoder = 0; encoder < 16; ++encoder) {
		get_encoder_value(encoder);
	}
	update_encoder_switch_state();
	update_side_switch_state();

	while (true) {
		update_side_switch_state();
		uint16_t side_down = get_side_switch_down();

		if (side_down & (CAL_SW_RED | CAL_SW_GREEN | CAL_SW_BLUE)) {
			channel = (side_down & CAL_SW_RED) ? 0 : ((side_down & CAL_SW_GREEN) ? 1 : 2);
			calibration_flash_channel(channel);
			// The limit marks belong to the channel that was on display.
			cal_blocked = 0;
			dirty = true;
		}

		if (side_down & CAL_SW_RESTORE) {
			calibration_restore(single);
			touched = true;
			dirty   = true;
		}

		// Back to the menu. This writes nothing, and the levels stay live in RAM.
		if (side_down & CAL_SW_BACK) {
			return touched;
		}

		// A press starts single encoder mode on that encoder, or ends it. A press
		// on a different encoder moves single encoder mode there.
		update_encoder_switch_state();
		uint16_t enc_down = get_enc_switch_down();
		if (enc_down) {
			for (uint8_t encoder = 0; encoder < 16; ++encoder) {
				if (enc_down & (0x0001 << encoder)) {
					single = (single == (int8_t)encoder) ? -1 : (int8_t)encoder;
					dirty = true;
					break;
				}
			}
		}

		// With all 16, a turn of any encoder moves all of them. In single encoder
		// mode only that encoder moves. Read all 16 in both modes. Otherwise
		// movement collects in the other encoders and changes a later edit.
		int16_t movement = 0;
		for (uint8_t encoder = 0; encoder < 16; ++encoder) {
			int8_t value = get_encoder_value(encoder);
			if ((single < 0) || (single == (int8_t)encoder)) {
				movement += value;
			}
		}

		if (movement != 0) {
			for (uint8_t encoder = 0; encoder < 16; ++encoder) {
				if ((single < 0) || (single == (int8_t)encoder)) {
					calibration_move(encoder, channel, movement);
				}
			}
			calibration_apply();
			touched = true;
			dirty   = true;
		}

		// DMA sends the frame buffer out continuously, thus write it only on a
		// change.
		if (dirty) {
			calibration_draw(single, channel);
			dirty = false;
		}
		_delay_ms(2);
	}
}

/**
 * Writes the calibration to EEPROM. The save command of the setup menu calls
 * this.
 */
void calibration_commit(void)
{
	calibration_save();
}
