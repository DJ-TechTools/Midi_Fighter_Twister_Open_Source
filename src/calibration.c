/*
 * calibration.c
 *
 * RGB white balance calibration. The three dies have different efficiency, and
 * the diffuser tints the mix. Thus #FFFFFF shows a color cast that is different
 * on each unit. This stores a trim for each channel, and build_rgb() applies it
 * to all colors.
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
// last LED. Without it, 48 trim steps across 11 LEDs move the bar only on each
// fourth detent. The brightness is low, because the white ring is next to the
// color that the user judges. The usable scale is 0 to 79, thus this is
// approximately 40%.
#define CAL_RING_TYPE				BLENDED_BAR
#define CAL_RING_BRIGHTNESS			32

// The trim values in RAM. The global value has the full resolution, and the
// delta of each encoder corrects the outliers. They are separate, thus a
// global change never touches the deltas. A global value at a limit also
// cannot remove the differences between the encoders.
static uint8_t	cal_global[3];			// R, G, B, always within the trim range
static int8_t	cal_delta[16][3];		// CAL_DELTA_MIN .. CAL_DELTA_MAX

static const uint8_t cal_defaults[3] = { DEF_CAL_TRIM_R, DEF_CAL_TRIM_G, DEF_CAL_TRIM_B };


/**
 * Clamps a trim to the range that the display can resolve.
 *
 * build_rgb has thresholds at 0, 2, 4 and up to 158. Thus the display shows
 * all values above CAL_TRIM_MAX the same as 255.
 */
static uint8_t clamp_trim(int16_t value)
{
	if (value < CAL_TRIM_MIN) { return CAL_TRIM_MIN; }
	if (value > CAL_TRIM_MAX) { return CAL_TRIM_MAX; }
	return (uint8_t)value;
}

/**
 * Returns the effective trim for one encoder channel, before the clamp.
 */
static int16_t effective_trim(uint8_t encoder, uint8_t channel)
{
	return (int16_t)cal_global[channel]
		 + (int16_t)cal_delta[encoder][channel] * CAL_DELTA_SCALE;
}

/**
 * Returns true when this encoder channel is at a limit. The channel then does
 * not follow the global control. The delta does not change, but the user must
 * see why the channel stopped.
 */
static bool is_railed(uint8_t encoder, uint8_t channel)
{
	int16_t value = effective_trim(encoder, channel);
	return (value < CAL_TRIM_MIN) || (value > CAL_TRIM_MAX);
}

/**
 * Adds each delta to the global value, and gives the 48 gains to the display
 * driver. Call this after each change to a trim value. Thus build_rgb does not
 * do this arithmetic.
 */
static void calibration_apply(void)
{
	for (uint8_t encoder = 0; encoder < 16; ++encoder) {
		set_rgb_trim(encoder,
					 clamp_trim(effective_trim(encoder, 0)),
					 clamp_trim(effective_trim(encoder, 1)),
					 clamp_trim(effective_trim(encoder, 2)));
	}
}

/**
 * Loads the defaults into the trim values in RAM. These are the gains that
 * display_driver.c had before, clamped to the usable range.
 */
static void calibration_defaults(void)
{
	for (uint8_t channel = 0; channel < 3; ++channel) {
		cal_global[channel] = clamp_trim(cal_defaults[channel]);
	}
	memset(cal_delta, 0, sizeof(cal_delta));
}

static uint8_t calibration_checksum(const uint8_t *page)
{
	uint8_t sum = 0;
	for (uint8_t i = EE_CAL_GLOBAL_OFFSET; i < EE_CAL_CHECKSUM_OFFSET; ++i) {
		sum += page[i];
	}
	return sum;
}

/**
 * Sign extends one packed 4 bit delta.
 */
static int8_t unpack_delta(uint8_t nibble)
{
	return (nibble & 0x08) ? (int8_t)(nibble | 0xF0) : (int8_t)nibble;
}

void calibration_init(void)
{
	uint8_t page[EEPROM_PAGE_SIZE];

	calibration_defaults();

	cpu_irq_disable();
	nvm_eeprom_read_buffer(EE_CAL_START, page, EEPROM_PAGE_SIZE);
	cpu_irq_enable();

	// An unwritten page reads 0xFF, fails the magic check and keeps the defaults.
	if ((page[EE_CAL_MAGIC_OFFSET] == EE_CAL_MAGIC) &&
		(page[EE_CAL_VERSION_OFFSET] == EE_CAL_VERSION) &&
		(page[EE_CAL_CHECKSUM_OFFSET] == calibration_checksum(page))) {

		for (uint8_t channel = 0; channel < 3; ++channel) {
			cal_global[channel] = clamp_trim(page[EE_CAL_GLOBAL_OFFSET + channel]);
		}

		for (uint8_t index = 0; index < 48; ++index) {
			uint8_t packed = page[EE_CAL_DELTA_OFFSET + (index >> 1)];
			uint8_t nibble = (index & 1) ? (packed >> 4) : (packed & 0x0F);
			cal_delta[index / 3][index % 3] = unpack_delta(nibble);
		}
	}

	calibration_apply();
}

/**
 * Scales the full calibration up until the brightest channel on the brightest
 * encoder is at the ceiling.
 *
 * A correction for a color cast makes the strong channels weaker, and the light
 * output decreases. A white point is a ratio. Thus the same factor on all three
 * channels keeps the white point and recovers the lost light. At a given white
 * point, the maximum light is when the strongest channel is at full duty. Thus
 * this result is the best possible.
 *
 * Usually this does nothing. A correction for one cast usually leaves one
 * channel at the ceiling, and then the function returns immediately. It helps
 * when all channels are lower. A cyan cast puts green and blue below the red
 * default, and this recovers approximately 13%. A large correction on all
 * three channels can recover 30%.
 *
 * The peak comes from the value of each encoder, not only from the global
 * value. Thus the scale never pushes an encoder into the clamp. The clamp
 * removes a correction that the user made, and gives no sign of it.
 */
static void calibration_normalize(void)
{
	int16_t peak = 0;

	for (uint8_t encoder = 0; encoder < 16; ++encoder) {
		for (uint8_t channel = 0; channel < 3; ++channel) {
			int16_t value = effective_trim(encoder, channel);
			if (value > peak) { peak = value; }
		}
	}

	if ((peak <= 0) || (peak >= CAL_TRIM_MAX)) {
		return;	// at the ceiling already, nothing to recover
	}

	// Fixed point scale factor in 256ths, for integer math.
	uint16_t scale = (uint16_t)(((uint32_t)CAL_TRIM_MAX * 256u) / (uint16_t)peak);

	for (uint8_t channel = 0; channel < 3; ++channel) {
		cal_global[channel] = clamp_trim((int16_t)(((uint32_t)cal_global[channel] * scale) / 256u));
	}

	// The deltas scale too, or the corrections become smaller against a larger
	// global value. Use a division and not a shift. The deltas are signed, and a
	// division truncates toward zero.
	for (uint8_t encoder = 0; encoder < 16; ++encoder) {
		for (uint8_t channel = 0; channel < 3; ++channel) {
			int16_t delta = (int16_t)(((int32_t)cal_delta[encoder][channel] * (int32_t)scale) / 256);
			if (delta < CAL_DELTA_MIN) { delta = CAL_DELTA_MIN; }
			if (delta > CAL_DELTA_MAX) { delta = CAL_DELTA_MAX; }
			cal_delta[encoder][channel] = (int8_t)delta;
		}
	}

	calibration_apply();
}

/**
 * Packs the trim values into page 63 and writes the page. This is one atomic
 * write of one page, as save_encoder_config() does.
 */
static void calibration_save(void)
{
	uint8_t page[EEPROM_PAGE_SIZE];

	memset(page, 0xFF, sizeof(page));
	page[EE_CAL_MAGIC_OFFSET]   = EE_CAL_MAGIC;
	page[EE_CAL_VERSION_OFFSET] = EE_CAL_VERSION;

	for (uint8_t channel = 0; channel < 3; ++channel) {
		page[EE_CAL_GLOBAL_OFFSET + channel] = cal_global[channel];
	}

	memset(&page[EE_CAL_DELTA_OFFSET], 0, 24);
	for (uint8_t index = 0; index < 48; ++index) {
		uint8_t nibble = (uint8_t)cal_delta[index / 3][index % 3] & 0x0F;
		page[EE_CAL_DELTA_OFFSET + (index >> 1)] |= (index & 1) ? (uint8_t)(nibble << 4) : nibble;
	}

	page[EE_CAL_CHECKSUM_OFFSET] = calibration_checksum(page);

	// EEPROM writes are slow enough to trip the watchdog.
	wdt_disable();
	cpu_irq_disable();
	nvm_eeprom_load_page_to_buffer(page);
	nvm_eeprom_atomic_write_page(EE_CAL_PAGE);
	cpu_irq_enable();
}

/**
 * Maps a trim onto the 0-127 indicator scale.
 *
 * Starts at 1 and not 0. build_indicator_pattern turns the ring fully off at
 * position 0. The minimum trim then looks like a broken indicator and not a
 * value.
 */
static uint8_t trim_to_position(uint8_t trim)
{
	return (uint8_t)(1u + (((uint16_t)(trim - CAL_TRIM_MIN) * 126u)
						   / (CAL_TRIM_MAX - CAL_TRIM_MIN)));
}

/**
 * Paints the calibration display. All encoders show white through the trim,
 * thus the user judges what the trim gives.
 *
 * The white ring of each encoder shows its own level for the channel that the
 * user adjusts. Each encoder shows its own value, thus the user sees the
 * differences across the 16 immediately. An encoder at a limit stops, and the
 * other encoders continue.
 *
 * The detent LED is a separate red and blue part. It has no bits in common with
 * the RGB ring. Thus it can show selection and limit status, and not change the
 * white.
 */
static void calibration_draw(int8_t selected, uint8_t channel)
{
	for (uint8_t encoder = 0; encoder < 16; ++encoder) {
		build_rgb(encoder, 0xFFFFFF, 0);

		// This must come before the detent write, because it clears bits 0 and 1.
		set_encoder_indicator_level(encoder,
									trim_to_position(clamp_trim(effective_trim(encoder, channel))),
									false, CAL_RING_TYPE, 0, CAL_RING_BRIGHTNESS);

		bool railed   = is_railed(encoder, channel);
		bool is_shown = (selected == (int8_t)encoder);

		if (is_shown && railed) {
			set_encoder_indent(encoder, CAL_DETENT_BOTH);
		} else if (is_shown) {
			set_encoder_indent(encoder, CAL_DETENT_BLUE);
		} else if (railed) {
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
 * Applies encoder movement to the active scope.
 *
 * Global mode is relative. One value changes, and all 16 trims change with it,
 * with no change to the differences. Single encoder mode changes the delta of
 * that encoder. Thus a later global change keeps the correction.
 */
static void calibration_adjust(int8_t selected, uint8_t channel, int16_t movement)
{
	if (selected < 0) {
		cal_global[channel] = clamp_trim((int16_t)cal_global[channel] + movement);
	} else {
		int16_t delta = (int16_t)cal_delta[selected][channel] + movement;
		if (delta < CAL_DELTA_MIN) { delta = CAL_DELTA_MIN; }
		if (delta > CAL_DELTA_MAX) { delta = CAL_DELTA_MAX; }
		cal_delta[selected][channel] = (int8_t)delta;
	}
	calibration_apply();
}

/**
 * Restores the default for the active mode. In global mode this is the global
 * trim. In single encoder mode this is the deltas of the selected encoder. A
 * global restore does not change the deltas, because they are a separate
 * correction. The user does not expect a global restore to erase them.
 *
 * This changes the trim values in RAM only. The user must still save.
 */
static void calibration_restore(int8_t selected)
{
	if (selected < 0) {
		for (uint8_t channel = 0; channel < 3; ++channel) {
			cal_global[channel] = clamp_trim(cal_defaults[channel]);
		}
	} else {
		for (uint8_t channel = 0; channel < 3; ++channel) {
			cal_delta[selected][channel] = 0;
		}
	}
	calibration_apply();
}

/**
 * The interactive calibration editor.
 *
 * This editor changes the trim in RAM and paints the display. It never writes
 * EEPROM. It returns when the user presses Back, and the setup menu that called
 * it continues. The menu controls entry, exit and the save.
 *
 * Returns true if the user changed a value. Thus the menu knows if it must
 * write the trim when the user saves.
 */
bool calibration_edit(void)
{
	uint8_t channel  = 0;		// 0 = red, 1 = green, 2 = blue
	int8_t  selected = -1;		// -1 = global, otherwise the chosen encoder
	bool    dirty    = true;	// repaint only on change, not every pass
	bool    touched  = false;	// true when the user changed a value

	// Paint first. Otherwise the panel stays dark while the user still holds the
	// encoder that opened this editor.
	calibration_draw(selected, channel);

	// Discard the press that opened this editor, thus the first input is real.
	for (uint8_t encoder = 0; encoder < 16; ++encoder) {
		get_encoder_value(encoder);
	}
	update_encoder_switch_state();
	update_side_switch_state();

	while (true) {
		update_side_switch_state();
		uint16_t side_down = get_side_switch_down();

		if (side_down & CAL_SW_RED) {
			channel = 0;
			calibration_flash_channel(channel);
			dirty = true;
		} else if (side_down & CAL_SW_GREEN) {
			channel = 1;
			calibration_flash_channel(channel);
			dirty = true;
		} else if (side_down & CAL_SW_BLUE) {
			channel = 2;
			calibration_flash_channel(channel);
			dirty = true;
		}

		if (side_down & CAL_SW_RESTORE) {
			calibration_restore(selected);
			touched = true;
			dirty   = true;
		}

		// Back to the menu. This writes nothing, and the trim stays live in RAM.
		if (side_down & CAL_SW_BACK) {
			return touched;
		}

		// A press toggles single encoder mode. A press on a different encoder
		// moves the selection to that encoder.
		update_encoder_switch_state();
		uint16_t enc_down = get_enc_switch_down();
		if (enc_down) {
			for (uint8_t encoder = 0; encoder < 16; ++encoder) {
				if (enc_down & (0x0001 << encoder)) {
					selected = (selected == (int8_t)encoder) ? -1 : (int8_t)encoder;
					dirty = true;
					break;
				}
			}
		}

		// In global mode all encoders adjust. In single encoder mode only the
		// selected encoder adjusts. Read all 16 in both modes. Otherwise movement
		// collects in the other encoders and changes a later edit.
		int16_t movement = 0;
		for (uint8_t encoder = 0; encoder < 16; ++encoder) {
			int8_t value = get_encoder_value(encoder);
			if ((selected < 0) || (selected == (int8_t)encoder)) {
				movement += value;
			}
		}
		if (movement != 0) {
			calibration_adjust(selected, channel, movement);
			touched = true;
			dirty   = true;
		}

		// DMA sends the frame buffer out continuously, thus write it only on a
		// change.
		if (dirty) {
			calibration_draw(selected, channel);
			dirty = false;
		}
		_delay_ms(2);
	}
}

/**
 * Writes the trim to EEPROM. It first recovers the brightness that the
 * correction decreased. The save command of the setup menu calls this.
 */
void calibration_commit(void)
{
	// Normalize first. The white point is a ratio and does not change. Only the
	// full level increases.
	calibration_normalize();
	calibration_save();
}
