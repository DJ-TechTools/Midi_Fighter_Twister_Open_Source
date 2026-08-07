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
static side_sw_settings_t side_sw_cfg;

// Holds toggle state for switch if configured for MIDI toggle action
static uint8_t side_switch_toggle_state[NUM_BANKS];
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

void process_side_switch_input(void)  // MIDI Output: Digital Inputs -> Side Switches
{
	update_side_switch_state();
	
	static uint8_t prev_side_switch_state = 0;
	
	uint8_t bit = 0x01;

	//uint8_t enc_bank = current_encoder_bank();
	
	// Check for Sequencer Activation Combination (Both Middle at same time)
	#ifndef EXTENDED_BANKS
  if (!native_mode_is_active() // native mode -> ignore sequencer
	if ((get_side_switch_state() & 0x12) == 0x12){
		if ((prev_side_switch_state & 0x12) == 0x02 || (prev_side_switch_state & 0x12) == 0x10){
			set_op_mode(sequencer);
			init_seq_display();
			prev_side_switch_state = 0;
		}
		return;
	}
	#endif
	
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
		case GLOBAL_BANK_1:{
			if((state == SW_DOWN) && (NUM_BANKS > 0)){
				midi_stream_raw_cc(midi_system_channel, current_encoder_bank(), 0);
				midi_stream_raw_cc(midi_system_channel, 0, 127);
				if (global_bank_animations_enabled) {
					bank_change_animation(0);
				}
				change_encoder_bank(0);
			}
		} break;
		case GLOBAL_BANK_2:{
			if((state == SW_DOWN) && (NUM_BANKS > 1)){
				midi_stream_raw_cc(midi_system_channel, current_encoder_bank(), 0);
				midi_stream_raw_cc(midi_system_channel, 1, 127);
				if (global_bank_animations_enabled) {
					bank_change_animation(1);
				}
				change_encoder_bank(1);
			}
		} break;
		case GLOBAL_BANK_3:{
			if((state == SW_DOWN) && (NUM_BANKS > 2)){
				midi_stream_raw_cc(midi_system_channel, current_encoder_bank(), 0);
				midi_stream_raw_cc(midi_system_channel, 2, 127);
				if (global_bank_animations_enabled) {
					bank_change_animation(2);
				}
				change_encoder_bank(2);
			}
		} break;
		case GLOBAL_BANK_4:{
			if((state == SW_DOWN) && (NUM_BANKS > 3)){
				midi_stream_raw_cc(midi_system_channel, current_encoder_bank(), 0);
				midi_stream_raw_cc(midi_system_channel, 3, 127);
				if (global_bank_animations_enabled) {
					bank_change_animation(3);
				}
				change_encoder_bank(3);
			}
		} break;
		case GLOBAL_BANK_5:{
			if((state == SW_DOWN) && (NUM_BANKS > 4)){
				midi_stream_raw_cc(midi_system_channel, current_encoder_bank(), 0);
				midi_stream_raw_cc(midi_system_channel, 4, 127);
				if (global_bank_animations_enabled) {
					bank_change_animation(4);
				}
				change_encoder_bank(4);
			}
		} break;
		case GLOBAL_BANK_6:{
			if((state == SW_DOWN) && (NUM_BANKS > 5)){
				midi_stream_raw_cc(midi_system_channel, current_encoder_bank(), 0);
				midi_stream_raw_cc(midi_system_channel, 5, 127);
				if (global_bank_animations_enabled) {
					bank_change_animation(5);
				}
				change_encoder_bank(5);
			}
		} break;
		case GLOBAL_BANK_7:{
			if((state == SW_DOWN) && (NUM_BANKS > 6)){
				midi_stream_raw_cc(midi_system_channel, current_encoder_bank(), 0);
				midi_stream_raw_cc(midi_system_channel, 6, 127);
				if (global_bank_animations_enabled) {
					bank_change_animation(6);
				}
				change_encoder_bank(6);
			}
		} break;
		case GLOBAL_BANK_8:{
			if((state == SW_DOWN) && (NUM_BANKS > 7)){
				midi_stream_raw_cc(midi_system_channel, current_encoder_bank(), 0);
				midi_stream_raw_cc(midi_system_channel, 7, 127);
				if (global_bank_animations_enabled) {
					bank_change_animation(7);
				}
				change_encoder_bank(7);
			}
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
