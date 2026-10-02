/*
 *  side_switch.c
 *
 *  Created: 9/3/2013 1:00:56 PM
 *  Author: Michael
 *
 *  Side Switch Management & Control 
 *  This takes care of all side switch actions. The side switches can be 
 *  configured to send various MIDI messages as well as perform various 
 *  system functions. 
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
**/
#include <constants.h>
#include <side_switch.h>
#include "native_mode.h"
#include <display_driver.h>


// Holds all configurable side switch settings
// A held direct bank jump comes back on release, as ToggleOrHoldBehaviour does
// in the Ableton scripts: a tap latches the bank and a hold shows it only while
// the button is down. Live splits the two at MOMENTARY_DELAY, 0.3 seconds, and
// process_side_switch_input() runs one time per main loop pass at about 1.21ms.
#define BANK_HOLD_REVERT_SCANS 248
#define BANK_HOLD_NONE         0xFF

static uint8_t bank_hold_switch = BANK_HOLD_NONE;	// side switch that owns the hold
static uint8_t bank_hold_origin = 0;				// bank to come back to
static uint8_t bank_hold_scans  = 0;				// scans the button has been down

static side_sw_settings_t side_sw_cfg;

// Holds toggle state for switch if configured for MIDI toggle action
// MAX_BANKS, not NUM_BANKS: this is one byte per bank, so 4 bytes more at 8
// banks. That is less than the cost of putting it in the arena.
static uint8_t side_switch_toggle_state[MAX_BANKS];
bool g_bank_select_active = false;
static uint8_t g_bank_select_origin = 0;


// Controls the operation mode of the device
// Startup = Device is still starting up
// Normal  = Standard Encoder/Switch Banked Operation
// Shift1  = shift page 1
// Shift2  = shift page 2
// Sequencer = Sequencer Mode
static op_mode_t mode = startup;

// Internal Functions
void side_switch_config(side_sw_settings_t *settings);
void do_side_switch_function(uint8_t switch_num, switch_event_t state);

// Initializes all side switch settings
void side_switch_init(void)
{
	//side_switch_config(side_sw_settings);
	for(uint8_t i=0;i<NUM_BANKS;++i){
		side_switch_toggle_state[i] = 0x00;
	}
}

/**
 * Returns the current op mode setting
 */

op_mode_t get_op_mode(void){
	return mode;
}

/**
 * Sets the op mode setting
 */
void set_op_mode(op_mode_t new_mode){
	mode = new_mode;
}

bool get_bank_select_active(void) {
	return g_bank_select_active;
}
void set_bank_select_active(bool value) {
	g_bank_select_active = value;
}
// Temporary until settings functionality is added
void side_switch_config(side_sw_settings_t *settings)
{
	side_sw_cfg.side_is_banked = settings->side_is_banked;
	
	side_sw_cfg.sw_action[0] = settings->sw_action[0];
	side_sw_cfg.sw_action[1] = settings->sw_action[1];
	side_sw_cfg.sw_action[2] = settings->sw_action[2];
	side_sw_cfg.sw_action[3] = settings->sw_action[3];
	side_sw_cfg.sw_action[4] = settings->sw_action[4];
	side_sw_cfg.sw_action[5] = settings->sw_action[5];
}

side_sw_settings_t* get_side_switch_config(void)
{
	side_sw_settings_t* settings = &side_sw_cfg;
	return settings;
}


/**
 * Checks for state changes for the side switches then carries out the configured
 * action for that switch.
 */

// Native mode toggle combo: both bottom side buttons pressed simultaneously.
// get_side_switch_state() bits 0-5 are SIDE_SW1..SW6. Bits 0, 1 and 2 are the
// left top, middle and bottom buttons. Bits 3, 4 and 5 are the right top,
// middle and bottom buttons. The bottom pair is bits 2 and 5. This agrees
// with the hardware, self_test.c side_pattern[], doc/NativeMode.md, and the
// sequencer combo below, which fires on the middle pair (0x12).
#define NATIVE_MODE_TOGGLE_COMBO 0x24

void process_side_switch_input(void)  // MIDI Output: Digital Inputs -> Side Switches
{
	update_side_switch_state();
	
	static uint8_t prev_side_switch_state = 0;
	
	uint8_t bit = 0x01;

	//uint8_t enc_bank = current_encoder_bank();
	
	// Native mode toggle: both bottom side buttons at the same time.
	// Checked before everything else and in either state, so it can also switch
	// native mode back off. Fires once, on the edge that completes the combo.
	if ((get_side_switch_state() & NATIVE_MODE_TOGGLE_COMBO) == NATIVE_MODE_TOGGLE_COMBO) {
		if ((prev_side_switch_state & NATIVE_MODE_TOGGLE_COMBO) != NATIVE_MODE_TOGGLE_COMBO) {
			native_mode_toggle();
		}
		prev_side_switch_state = get_side_switch_state();
		return;
	}

	// Check for Sequencer Activation Combination (Both Middle at same time)
	if (g_seq_available
		&& !native_mode_is_active() // native mode -> ignore sequencer
		&& (get_side_switch_state() & 0x12) == 0x12){
		if ((prev_side_switch_state & 0x12) == 0x02 || (prev_side_switch_state & 0x12) == 0x10){
			set_op_mode(sequencer);
			init_seq_display();
			prev_side_switch_state = 0;
		}
		return;
	}
	
	for(uint8_t i = 0; i <6;++i) {
		
		if(get_side_switch_down() & bit){
			do_side_switch_function(i, SW_DOWN);
		} else if (get_side_switch_up() & bit){
			do_side_switch_function(i, SW_UP);
		} else if (get_side_switch_state() & bit){
			do_side_switch_function(i, SW_HELD);
		}
	
		bit <<=1;
	}
	
	prev_side_switch_state = get_side_switch_state();
}

void draw_bank_select_overlay(void) {
	for (uint8_t i = 0; i < 16; i++) {
		build_rgb(i, 0, 1);
		set_indicator_pattern(i, 0x0000);
	}
	for (uint8_t i = 0; i < NUM_BANKS; i++) {
		if (i == current_encoder_bank()) {
			build_rgb(i, 0xFFFF00, 0);
			} else {
			build_rgb(i, 0x0000FF, 0);
		}
	}
}

/**
 * Changes bank the way a direct jump does: the MIDI pair, then the animation.
 *
 * \param target [in]	Bank to show, already known to be in range
 */
static void bank_jump_to(uint8_t target)
{
	midi_stream_raw_cc(midi_system_channel, current_encoder_bank(), 0);
	midi_stream_raw_cc(midi_system_channel, target, 127);
	if (global_bank_animations_enabled) {
		bank_change_animation(target);
	}
	change_encoder_bank(target);
}

void do_side_switch_function(uint8_t switch_num, switch_event_t state)
{
	// Reset idle on any side switch activity
	if (state == SW_DOWN || state == SW_UP) {
		reset_idle_timer();
	}
  switch(state)
	{
		case SW_DOWN:
		case SW_UP:
			if(native_mode_process_side_switch_pressed(switch_num, state == SW_DOWN))
				return;
			break;
		default:
			break;
	}
	
	uint8_t bank = side_sw_cfg.side_is_banked ? current_encoder_bank() : 0;
	
	// Perform the switch action
	switch (side_sw_cfg.sw_action[switch_num]) {
		case CC_HOLD_SS:{
			// The switch sends a CC so send that CC
			if(state == SW_DOWN){
				midi_stream_raw_cc(midi_system_channel, SIDE_SWITCH_OFFSET + switch_num + (bank*6) , 127);
			} else if(state == SW_UP) {
				midi_stream_raw_cc(midi_system_channel, SIDE_SWITCH_OFFSET + switch_num + (bank*6) , 0);
			}
		} break;
		case CC_TOGGLE_SS:{
			// The switch sends a CC so send that CC
			if(state == SW_DOWN){
				uint8_t bit = 0x01 << switch_num;
				side_switch_toggle_state[bank] ^= bit;
				uint8_t value = side_switch_toggle_state[bank] & bit ? 127 : 0;
				midi_stream_raw_cc(midi_system_channel, SIDE_SWITCH_OFFSET + switch_num + (bank*6) , value);
			} 
		} break;
		case NOTE_HOLD_SS:{
			// The switch sends a Note so send that Note
			if(state == SW_DOWN){
				midi_stream_raw_note(midi_system_channel, SIDE_SWITCH_OFFSET + switch_num + (bank*6), true, 127);
			} else if(state == SW_UP) {
				midi_stream_raw_note(midi_system_channel, SIDE_SWITCH_OFFSET + switch_num + (bank*6), false , 0);
			}
						
		} break;
		case NOTE_TOGGLE_SS:{
			// The switch sends a CC so send that CC
			if(state == SW_DOWN){
				uint8_t bit = 0x01 << switch_num;
				side_switch_toggle_state[bank] ^= bit;
				uint8_t velocity = side_switch_toggle_state[bank] & bit ? 127 : 0;
				bool is_note_on = side_switch_toggle_state[bank] & bit ? true : false;
				midi_stream_raw_note(midi_system_channel, SIDE_SWITCH_OFFSET + switch_num + (bank*6) , is_note_on, velocity);
			}
		} break;
		case SHIFT_PAGE_1:{
			// The switch activates shift page 1 so enable shift page 1
			if (state == SW_DOWN) {
				set_op_mode(shift1);
			} else if (state == SW_UP) {
				refresh_display();
				set_op_mode(normal);
			}
		} break;
		case SHIFT_PAGE_2:{
			// The switch activates shift page 2 so enable shift page 2
			if (state == SW_DOWN) {
				set_op_mode(shift2);
			} else if (state == SW_UP) {
				refresh_display();
				set_op_mode(normal);
			}
		} break;
		case SHIFT_PAGE_1_TOGGLE:{
			if (state == SW_DOWN) {
				if (get_op_mode() == shift1) {
					refresh_display();
					set_op_mode(normal);
					} else {
					set_op_mode(shift1);
				}
			}
		} break;
		case SHIFT_PAGE_2_TOGGLE:{
			if (state == SW_DOWN) {
				if (get_op_mode() == shift2) {
					refresh_display();
					set_op_mode(normal);
					} else {
					set_op_mode(shift2);
				}
			}
		} break;
		case GLOBAL_BANK_UP:{
			// The switch increments the global bank setting
			if((state == SW_DOWN) && (current_encoder_bank() < (NUM_BANKS-1))){
				// Send bank change MIDI output
				uint8_t next_bank = (current_encoder_bank()+1 >= NUM_BANKS) ? NUM_BANKS-1 : current_encoder_bank()+1;
				midi_stream_raw_cc(midi_system_channel, current_encoder_bank(), 0);
				if (global_bank_animations_enabled) {
					bank_change_animation(next_bank);
				}
				change_encoder_bank(next_bank);
				midi_stream_raw_cc(midi_system_channel, current_encoder_bank(), 127);
			}
						
		} break;
		case GLOBAL_BANK_DOWN:{
			// The switch decrements the global bank setting
			if((state == SW_DOWN) && (current_encoder_bank() > 0)){
				// Send bank change MIDI output
				uint8_t next_bank = (current_encoder_bank()-1 >= NUM_BANKS) ? 0 : current_encoder_bank()-1;
				midi_stream_raw_cc(midi_system_channel, current_encoder_bank(), 0);
				if (global_bank_animations_enabled) {
					bank_change_animation(next_bank);
				}
				change_encoder_bank(next_bank);
				midi_stream_raw_cc(midi_system_channel, current_encoder_bank(), 127);
			}
		} break;
		case BANK_SELECT:{
			if (state == SW_DOWN) {
				g_bank_select_active = true;
				g_bank_select_origin = current_encoder_bank();
				reset_pulse_animation();
				draw_bank_select_overlay();
			} else if (state == SW_HELD) {
				uint8_t bank = current_encoder_bank();
				uint8_t level = pulse_animation(5);
				build_rgb(bank, 0xFFFF00, level);
				set_indicator_pattern_level(bank, 0xFFFF, level);
			} else if (state == SW_UP) {
				g_bank_select_active = false;
				refresh_display();
			}
		} break;
		case GLOBAL_BANK_1:
		case GLOBAL_BANK_2:
		case GLOBAL_BANK_3:
		case GLOBAL_BANK_4:
		case GLOBAL_BANK_5:
		case GLOBAL_BANK_6:
		case GLOBAL_BANK_7:
		case GLOBAL_BANK_8:{
			// The eight actions are contiguous, so the bank is the offset from the first.
			uint8_t target = side_sw_cfg.sw_action[switch_num] - GLOBAL_BANK_1;
			if (target >= NUM_BANKS) { break; }

			if (state == SW_DOWN) {
				bank_hold_switch = switch_num;
				bank_hold_origin = current_encoder_bank();
				bank_hold_scans  = 0;
				bank_jump_to(target);
			} else if ((state == SW_HELD) && (bank_hold_switch == switch_num)) {
				if (bank_hold_scans < BANK_HOLD_REVERT_SCANS) { bank_hold_scans++; }
			} else if ((state == SW_UP) && (bank_hold_switch == switch_num)) {
				// A tap leaves the new bank latched. A hold comes back, unless the
				// jump went nowhere or something else moved the bank while it was down.
				if ((bank_hold_scans >= BANK_HOLD_REVERT_SCANS)
				 && (bank_hold_origin != target)
				 && (current_encoder_bank() == target)) {
					bank_jump_to(bank_hold_origin);
				}
				bank_hold_switch = BANK_HOLD_NONE;
			}
			// The sequencer combo routes the side buttons elsewhere, so a hold that
			// enters it never sees its SW_UP. The stale owner is harmless: the next
			// SW_DOWN overwrites all three values.
		} break;
		case CYCLE_BANK:{
			if(state == SW_DOWN){
				uint8_t next_bank = (current_encoder_bank() >= (NUM_BANKS-1)) ? 0 : current_encoder_bank()+1;
				midi_stream_raw_cc(midi_system_channel, current_encoder_bank(), 0);
				if (global_bank_animations_enabled) {
					bank_change_animation(next_bank);
				}
				change_encoder_bank(next_bank);
				midi_stream_raw_cc(midi_system_channel, current_encoder_bank(), 127);
			}
		}
		break;
	}
}
