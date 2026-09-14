/*
 * setup_mode.c
 *
 * Boot time settings menu. It holds the settings that the unit must have before
 * USB enumerates, and the settings that the user judges on the panel. To enter
 * it, hold the bottom right side button while you connect USB. The unit writes
 * nothing to EEPROM until the user saves.
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

#include "setup_mode.h"

#include "calibration.h"
#include "Descriptors.h"
#include "display_driver.h"
#include "config.h"
#include "encoders.h"
#include "input.h"
#include "sequencer_input.h"
#include "jump_to_bootloader.h"

// Entry combo. Test it for equality. A subset test also matches 0x24, the
// native mode toggle.
//
// Entry is at boot only, by design. The user must connect the unit again and
// hold the button, thus the user cannot enter setup mode during use. Do not add
// a runtime combo for it.
#define SETUP_SW_ENTER				SW_RIGHT_BOT

// Menu items are on the bottom row, next to the button that opens setup mode.
// Commands are on the top row, away from the items that the user edits.
#define SETUP_ENC_DEVICE_ID			12
#define SETUP_ENC_BANKS				13
#define SETUP_ENC_CALIBRATE			14
#define SETUP_ENC_BOOTLOADER		0
#define SETUP_ENC_DISCARD			2
#define SETUP_ENC_SAVE				3

#define SETUP_COLOR_CALIBRATE		0xFFFFFF
#define SETUP_COLOR_DEVICE_ID		0x0000FF
#define SETUP_COLOR_SAVE			0x00FF00
#define SETUP_COLOR_DISCARD			0xFF0000
#define SETUP_COLOR_BANKS			0xFF8000

// Ring readouts for the bank mode. The 11 LEDs run from bit 15 for LED 1 down
// to bit 5 for LED 11.
// 4 banks: LEDs 2, 4, 8 and 10.
#define SETUP_BANKS_4_PATTERN		0x5140
// 8 banks: the first four LEDs and the last four.
#define SETUP_BANKS_8_PATTERN		0xF1E0

// Bootloader, shown with the odd LEDs 1, 3, 5, 7, 9 and 11. The RGB stays off,
// because this is not a setting to edit but a way out of the firmware.
#define SETUP_BOOTLOADER_LEDS		6
#define SETUP_BOOTLOADER_PATTERN	0xAAA0

// Hold time to start the bootloader. Only the Utility, a DFU tool, or a power
// cycle leaves the bootloader again. Thus one press must not start it. The
// loop waits 2 ms on each pass, thus this is approximately one second.
#define SETUP_BOOTLOADER_HOLD		500

// Save confirmation color. It is the same green as the encoder that starts it.
#define SETUP_SAVED_COLOR			0x00FF00

// Encoder pulses per step of a menu value. The encoders give four pulses per
// detent click, so this makes one click one step.
#define SETUP_PULSES_PER_STEP		4

// Ring readout brightness, low enough to read in a dark room.
#define SETUP_RING_BRIGHTNESS		40

// Menu state. It is static, to keep it off the stack. Only approximately 750
// bytes of RAM are free for the stack, and setting_confirmation_animation() and
// the EEPROM page buffers use them.
static uint8_t	setup_identity;			// device id in the menu, 0..NUM_DEVICE_IDS-1
static bool		setup_identity_dirty;
static bool		setup_trim_dirty;
static int8_t	setup_detent_accum;		// pulses not yet worth a step
static uint8_t	setup_banks;			// bank count in the menu, MIN_BANKS or MAX_BANKS
static bool		setup_banks_dirty;
static int8_t	setup_banks_accum;		// pulses not yet worth a step
static uint16_t	setup_boot_held;		// passes the bootloader encoder was held


/**
 * Loads the stored identity. An unwritten byte reads 0xFF, and the unit then
 * uses the compile time default. This function is not in Descriptors.c. There,
 * config.h makes the endian macros of asf.h clash with the macros of LUFA.
 */
void identity_init(void)
{
	uint8_t stored = eeprom_read(EE_DEVICE_IDENTITY);

	g_device_identity = (stored < NUM_DEVICE_IDS) ? stored : (uint8_t)USE_ALTERNATE_ID;
}

/**
 * Reads the stored bank mode and divides the arena for it.
 *
 * Call this first in main(), before config_init(). config_init() can do a
 * factory reset, which writes a number of EEPROM pages that depends on the
 * bank count, thus the count must be live before it runs.
 *
 * Only EE_BANK_MODE_8 selects 8 banks. An unwritten byte reads 0xFF and gives
 * 4 banks with the sequencer, which is what a new unit and all units in the
 * field must do.
 */
void bank_mode_init(void)
{
	uint8_t stored = eeprom_read(EE_BANK_MODE);

	encoders_alloc((stored == EE_BANK_MODE_8) ? MAX_BANKS : MIN_BANKS);
}

/**
 * Stores the bank mode and gives defaults to the banks the new mode will read.
 *
 * The caller must reset the unit after this. encoders_alloc() divides the
 * arena at boot only, thus the new count cannot take effect in place.
 *
 * The last 8 encoders of bank 8 and sequencer memory slot 1 are on the same
 * two EEPROM pages. Only one of the two modes can hold them. Thus the mode
 * that becomes live always takes them, and the other mode loses them.
 *
 * The rest of banks 5 to 8 gets defaults one time only, and EE_BANK_HI_INIT
 * records this. Without that flag, each change to 8 banks erases the work that
 * the user did in 8 bank mode before.
 *
 * \param banks [in]	MIN_BANKS or MAX_BANKS
 */
void bank_mode_set(uint8_t banks)
{
	// 4 pages make one bank. The page count starts at the first encoder
	// settings page. Refer to factory_reset_encoder_pages().
	const uint8_t first_hi_page  = 4 * MIN_BANKS;		// bank 5, first page
	const uint8_t last_hi_page   = (4 * MAX_BANKS) - 1;	// bank 8, last page
	const uint8_t first_seq_page = (4 * MAX_BANKS) - 2;	// bank 8, encoders 9 to 16

	if (banks == MAX_BANKS) {
		eeprom_write(EE_BANK_MODE, EE_BANK_MODE_8);

		if (eeprom_read(EE_BANK_HI_INIT) != EE_BANK_HI_INIT_DONE) {
			factory_reset_encoder_pages(first_hi_page, last_hi_page);
			eeprom_write(EE_BANK_HI_INIT, EE_BANK_HI_INIT_DONE);
		} else {
			// The other pages of banks 5 to 8 keep what the user set, but
			// these two held sequencer slot 1 while 4 bank mode was live.
			factory_reset_encoder_pages(first_seq_page, last_hi_page);
		}
	} else {
		eeprom_write(EE_BANK_MODE, 0x00);

		// Sequencer slot 1 is on the pages that bank 8 used. Clear its full
		// flag, or the sequencer reads bank data as a pattern.
		// delete_pattern_memory() keeps the flag offset in one place.
		delete_pattern_memory(0);
	}
}

/**
 * Stores the identity and makes it live. The host reads the new product ID on
 * the next enumeration. setup_shell_exit() and sysExCmdSystem case 3 both start
 * one.
 */
void identity_set(uint8_t identity)
{
	if (identity >= NUM_DEVICE_IDS) { return; }

	eeprom_write(EE_DEVICE_IDENTITY, identity);
	g_device_identity = identity;
}

/**
 * Builds a bar of the low N ring LEDs. The 11 LEDs run from bit 15 for LED 1
 * down to bit 5 for LED 11.
 */
static uint16_t bar_pattern(uint8_t count)
{
	if (count == 0) { return 0x0000; }
	if (count > 11) { count = 11; }

	return (uint16_t)(0xFFFFu << (16 - count));
}

/**
 * Builds the bootloader ring, with the given number of odd LEDs lit.
 *
 * The odd LEDs are 1, 3, 5, 7, 9 and 11, thus LED number 1 + 2i. LED n is bit
 * 16 - n, which makes the bit for each one 15 - 2i.
 */
static uint16_t bootloader_pattern(uint8_t lit)
{
	uint16_t pattern = 0x0000;

	if (lit > SETUP_BOOTLOADER_LEDS) { lit = SETUP_BOOTLOADER_LEDS; }

	for (uint8_t i = 0; i < lit; ++i) {
		pattern |= (uint16_t)(1u << (15 - (2 * i)));
	}

	return pattern;
}

/**
 * Paints the menu, one lit encoder per item. Colors are 24 bit literals, not
 * palette indices, which differ per unit with the active color map.
 */
static void setup_draw(void)
{
	for (uint8_t encoder = 0; encoder < 16; ++encoder) {
		// Level 0 is full brightness, level 1 is off.
		build_rgb(encoder, 0x000000, 1);
		set_indicator_pattern(encoder, 0x0000);
	}

	build_rgb(SETUP_ENC_CALIBRATE, SETUP_COLOR_CALIBRATE, 0);

	build_rgb(SETUP_ENC_DEVICE_ID, SETUP_COLOR_DEVICE_ID, 0);
	set_indicator_pattern_level(SETUP_ENC_DEVICE_ID,
								bar_pattern((uint8_t)(setup_identity + 1)),
								SETUP_RING_BRIGHTNESS);

	// Bank mode. The ring shows the selected mode and not a count, because
	// there are two values and not eight.
	build_rgb(SETUP_ENC_BANKS, SETUP_COLOR_BANKS, 0);
	set_indicator_pattern_level(SETUP_ENC_BANKS,
								(setup_banks == MAX_BANKS) ? SETUP_BANKS_8_PATTERN
														   : SETUP_BANKS_4_PATTERN,
								SETUP_RING_BRIGHTNESS);

	build_rgb(SETUP_ENC_DISCARD, SETUP_COLOR_DISCARD, 0);
	build_rgb(SETUP_ENC_SAVE,    SETUP_COLOR_SAVE,    0);

	// Bootloader. The loop above leaves the RGB off, thus only the ring marks it.
	// This keeps it apart from the settings and the two commands.
	set_indicator_pattern_level(SETUP_ENC_BOOTLOADER,
								SETUP_BOOTLOADER_PATTERN,
								SETUP_RING_BRIGHTNESS);
}

/**
 * Brings the panel up and USB down, for a mode that blocks. When the host
 * enumerates, EVENT_USB_Device_ConfigurationChanged() paints over this display
 * and arms a two second watchdog that this loop never resets.
 */
static void setup_shell_enter(uint16_t entry_mask)
{
	USB_Disable();
	wdt_disable();

	// Turn on all three interrupt levels. From boot this changes nothing. It
	// protects a future entry from the MIDI input handler, which masks the low
	// and medium levels.
	PMIC.CTRL = PMIC_LOLVLEN_bm | PMIC_MEDLVLEN_bm | PMIC_HILVLEN_bm;

	// main() disables the display before the boot checks, and enables it again
	// only when USB is up. Without this, the mode runs dark.
	display_enable();
	clear_display_buffer();

	// Paint before the release wait. The user still holds the entry button. A
	// panel that stays dark until the release looks like nothing happened.
	setup_draw();

	// Wait for all bits of the entry mask to come up. Otherwise the first pass
	// reads the entry press as a command.
	while (update_side_switch_state() & entry_mask) {
		_delay_ms(1);
	}

	// Discard start up noise so the first movement is a real one.
	for (uint8_t encoder = 0; encoder < 16; ++encoder) {
		get_encoder_value(encoder);
	}
	update_encoder_switch_state();
	update_side_switch_state();
}

/**
 * Hands the device back to the normal boot sequence. It restores nothing
 * directly. USB_Init() lets the host enumerate, and
 * EVENT_USB_Device_ConfigurationChanged() enables the display again, sets the
 * op mode and arms the watchdog. A reset is not necessary, because this only
 * changed USB.
 *
 * This is not sufficient after a change of bank mode. Refer to setup_reboot().
 */
static void setup_shell_exit(void)
{
	clear_display_buffer();
	display_disable();
	USB_Init();
}

/**
 * Resets the unit, for a setting that cannot take effect in place.
 *
 * encoders_alloc() divides the bank arena at boot, thus a change of bank count
 * needs this.
 *
 * sysExCmdSystem case 2 has a USB_Disable() and a wait, and this does not.
 * setup_shell_enter() already took USB down, thus the host saw the disconnect
 * when setup mode started.
 *
 * setup_shell_enter() disabled the watchdog, thus this arms it again. Do not
 * expect it to run.
 */
static void setup_reboot(void)
{
	clear_display_buffer();
	display_disable();

	wdt_set_timeout_period(WDT_TIMEOUT_PERIOD_16CLK);
	wdt_enable();
	while (true) { }
}

/**
 * Writes all settings that the user changed. There is a test for each setting,
 * thus the unit does not write a setting that did not change.
 */
static void setup_commit_all(void)
{
	if (setup_trim_dirty) {
		calibration_commit();
	}

	if (setup_identity_dirty) {
		identity_set(setup_identity);
	}

	if (setup_banks_dirty) {
		bank_mode_set(setup_banks);
	}
}

/**
 * The setup menu loop. It blocks, as elec_self_test() does.
 */
static void setup_run(uint16_t entry_mask)
{
	bool dirty = false;

	setup_identity       = g_device_identity;
	setup_identity_dirty = false;
	setup_trim_dirty     = false;
	setup_detent_accum   = 0;
	setup_banks          = g_num_banks;
	setup_banks_dirty    = false;
	setup_banks_accum    = 0;
	setup_boot_held      = 0;

	setup_shell_enter(entry_mask);

	while (true) {
		update_encoder_switch_state();
		uint16_t enc_down = get_enc_switch_down();

		if (enc_down & (0x0001 << SETUP_ENC_SAVE)) {
			bool banks_changed = setup_banks_dirty;

			setup_commit_all();

			// Confirm after the write, so it reports the save and not the press.
			setting_confirmation_animation(SETUP_SAVED_COLOR);

			if (banks_changed) {
				setup_reboot();		// does not return
			}

			setup_shell_exit();
			return;
		}

		// Bootloader, on a hold and not a press. Only the Utility, a DFU tool,
		// or a power cycle leaves the bootloader again. Thus a wrong press must
		// not stop the user in the middle of a set.
		//
		// While the user holds the encoder, the ring goes off one LED at a time.
		// Thus the user sees what the hold does before it occurs.
		if (get_enc_switch_state() & (0x0001 << SETUP_ENC_BOOTLOADER)) {
			if (setup_boot_held < SETUP_BOOTLOADER_HOLD) {
				++setup_boot_held;

				uint8_t lit = (uint8_t)(SETUP_BOOTLOADER_LEDS
				                        - ((setup_boot_held * SETUP_BOOTLOADER_LEDS)
				                           / SETUP_BOOTLOADER_HOLD));

				set_indicator_pattern_level(SETUP_ENC_BOOTLOADER,
											bootloader_pattern(lit),
											SETUP_RING_BRIGHTNESS);
			} else {
				// This writes nothing first. The unit is about to get new
				// firmware, and this is not the save command.
				//
				// Jump_To_Bootloader() disables USB and waits 2 seconds, thus the
				// host sees the detach. Then it forces a reset with the boot key
				// set. Bootloader_Jump_Check() reads that key before main() runs.
				Jump_To_Bootloader();		// does not return
			}
		} else if (setup_boot_held != 0) {
			// The user released too soon. Fill the ring again.
			setup_boot_held = 0;
			set_indicator_pattern_level(SETUP_ENC_BOOTLOADER,
										SETUP_BOOTLOADER_PATTERN,
										SETUP_RING_BRIGHTNESS);
		}

		if (enc_down & (0x0001 << SETUP_ENC_DISCARD)) {
			// Load the saved trim again. The editor changed it live in the display
			// driver. Without this, the edit stays after the user leaves.
			if (setup_trim_dirty) {
				calibration_init();
			}
			setup_shell_exit();
			return;
		}

		if (enc_down & (0x0001 << SETUP_ENC_CALIBRATE)) {
			if (calibration_edit()) {
				setup_trim_dirty = true;
			}
			dirty = true;
		}

		// The user edits the device number and the bank mode in place. Read all
		// encoders on each pass. Otherwise an encoder keeps its movement for later.
		int16_t movement = 0;
		int16_t banks_movement = 0;
		for (uint8_t encoder = 0; encoder < 16; ++encoder) {
			int8_t value = get_encoder_value(encoder);
			if (encoder == SETUP_ENC_DEVICE_ID) {
				movement += value;
			}
			if (encoder == SETUP_ENC_BANKS) {
				banks_movement += value;
			}
		}

		if (banks_movement != 0) {
			// Step on whole detents, as the device number does.
			setup_banks_accum += (int8_t)banks_movement;

			int8_t steps = setup_banks_accum / SETUP_PULSES_PER_STEP;
			if (steps != 0) {
				setup_banks_accum -= (int8_t)(steps * SETUP_PULSES_PER_STEP);

				// Two values, thus any step to the right gives 8 banks and any
				// step to the left gives 4.
				uint8_t banks = (steps > 0) ? MAX_BANKS : MIN_BANKS;

				if (banks != setup_banks) {
					setup_banks       = banks;
					setup_banks_dirty = (banks != g_num_banks);
					dirty             = true;
				}
			}
		}
		if (movement != 0) {
			// Step on whole detents, so the number follows the clicks.
			setup_detent_accum += (int8_t)movement;

			int8_t steps = setup_detent_accum / SETUP_PULSES_PER_STEP;
			if (steps != 0) {
				// Keep the remainder, or a slow turn never lands four pulses in
				// one pass.
				setup_detent_accum -= (int8_t)(steps * SETUP_PULSES_PER_STEP);

				int16_t identity = (int16_t)setup_identity + steps;

				// Clamp and do not wrap. A wrap is easy to do by accident.
				if (identity < 0)                    { identity = 0; }
				if (identity > (NUM_DEVICE_IDS - 1)) { identity = NUM_DEVICE_IDS - 1; }

				if ((uint8_t)identity != setup_identity) {
					setup_identity       = (uint8_t)identity;
					setup_identity_dirty = true;
					dirty                = true;
				}
			}
		}

		// DMA sends the frame buffer out continuously, thus write it only on a
		// change.
		if (dirty) {
			setup_draw();
			dirty = false;
		}
		_delay_ms(2);
	}
}

bool check_setup_mode(void)
{
	// This reads the side switches directly from the pins, thus one call gives
	// the live state.
	update_side_switch_state();

	// Equality, not a subset test. See SETUP_SW_ENTER.
	if (get_side_switch_state() != SETUP_SW_ENTER) {
		return false;
	}

	setup_run(SETUP_SW_ENTER);
	return true;
}
