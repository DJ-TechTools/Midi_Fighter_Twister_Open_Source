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
 * DJTT - MIDI Fighter Twister - Embedded Software License
 * Copyright (c) 2016: DJ Tech Tools
 * Permission is hereby granted, free of charge, to any person owning or possessing 
 * a DJ Tech-Tools MIDI Fighter Twister Hardware Device to view and modify this source 
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
#include <side_switch.h>

// Holds all configurable side switch settings
static side_sw_settings_t side_sw_cfg;

// Holds toggle state for switch if configured for MIDI toggle action
static uint8_t side_switch_toggle_state[NUM_BANKS];

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

void process_side_switch_input(void)
{
	update_side_switch_state();
	
	static uint8_t prev_side_switch_state = 0;
	
	uint8_t bit = 0x01;

	uint8_t enc_bank = current_encoder_bank();
	
	// Check for Sequencer Activation Combination (Both Middle at same time)
	if ((get_side_switch_state() & 0x12) == 0x12){
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

void do_side_switch_function(uint8_t switch_num, switch_event_t state)
{
	
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
		case GLOBAL_BANK_UP:{
			// The switch increments the global bank setting
			if((state == SW_DOWN) && (current_encoder_bank() < (NUM_BANKS-1))){
				// Send bank change MIDI output
				midi_stream_raw_cc(midi_system_channel, current_encoder_bank(), 0);
				change_encoder_bank(current_encoder_bank()+1);
				midi_stream_raw_cc(midi_system_channel, current_encoder_bank(), 127);
			}
						
		} break;
		case GLOBAL_BANK_DOWN:{
			// The switch decrements the global bank setting
			if((state == SW_DOWN) && (current_encoder_bank() > 0)){
				// Send bank change MIDI output
				midi_stream_raw_cc(midi_system_channel, current_encoder_bank(), 0);
				change_encoder_bank(current_encoder_bank()-1);
				midi_stream_raw_cc(midi_system_channel, current_encoder_bank(), 127);
			}
		} break;
		case GLOBAL_BANK_1:{
			// The switch sets the global bank setting to 1
			if((state == SW_DOWN) && (NUM_BANKS > 0)){
				// Send bank change MIDI output
				midi_stream_raw_cc(midi_system_channel, current_encoder_bank(), 0);
				midi_stream_raw_cc(midi_system_channel, 0, 127);
				change_encoder_bank(0);
			}
		} break;
		case GLOBAL_BANK_2:{
			// The switch sets the global bank setting to 2
			if((state == SW_DOWN) && (NUM_BANKS > 1)){
				// Send bank change MIDI output
				midi_stream_raw_cc(midi_system_channel, current_encoder_bank(), 0);
				midi_stream_raw_cc(midi_system_channel, 1, 127);
				change_encoder_bank(1);
			}
		} break;
		case GLOBAL_BANK_3:{
			// The switch sets the global bank setting to 3
			if((state == SW_DOWN) && (NUM_BANKS > 2)){
				// Send bank change MIDI output
				midi_stream_raw_cc(midi_system_channel, current_encoder_bank(), 0);
				midi_stream_raw_cc(midi_system_channel, 2, 127);
				change_encoder_bank(2);
			}
		} break;
		case GLOBAL_BANK_4:{
			// The switch sets the global bank setting to 4
			if((state == SW_DOWN) && (NUM_BANKS > 3)){
				// Send bank change MIDI output
				midi_stream_raw_cc(midi_system_channel, current_encoder_bank(), 0);
				midi_stream_raw_cc(midi_system_channel, 3, 127);
				change_encoder_bank(3);
			}
		} break;
		case CYCLE_BANK:{
			// The switch sets the global bank setting to 4
			if(state == SW_DOWN){
				// Send bank change MIDI output
				midi_stream_raw_cc(midi_system_channel, current_encoder_bank(), 0);
				if(current_encoder_bank() == (NUM_BANKS-1)){
					change_encoder_bank(0);
				} else {
					change_encoder_bank(current_encoder_bank()+1);
				}
				midi_stream_raw_cc(midi_system_channel, current_encoder_bank(), 127);
			}
		}
		break;
	}
}
