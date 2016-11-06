/*
 *  side_switch.h
 *
 *  Created: 9/3/2013 1:01:14 PM
 *  Author: Michael Mitchell 
 *
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
 */ 


#ifndef SIDE_SWITCH_H_
#define SIDE_SWITCH_H_

	/*	Includes: */
	
		#include <asf.h>
		#include <math.h>
		#include <encoders.h>
		
	/*	Macros: */

	/*	Types: */
	
		// Switch action type enum
		// Some names use SS suffix to differentiate from encoder actions 
		typedef enum side_sw_action {
			CC_HOLD_SS,
			CC_TOGGLE_SS,
			NOTE_HOLD_SS,
			NOTE_TOGGLE_SS,
			SHIFT_PAGE_1,
			SHIFT_PAGE_2,
			GLOBAL_BANK_UP,
			GLOBAL_BANK_DOWN,
			GLOBAL_BANK_1,
			GLOBAL_BANK_2,
			GLOBAL_BANK_3,
			GLOBAL_BANK_4,
			CYCLE_BANK,
		} side_sw_action_t;
	
		// Structure which hold side switch settings
		typedef struct {
			side_sw_action_t sw_action[6];
			bool			 side_is_banked;
		} side_sw_settings_t;
		
		// The different operational modes
		typedef enum {
			startup,	// Device is starting up or not yet configured
			normal,     // Device is in normal operation mode
			shift1,		// Device is in shift page 1
			shift2,		// Device is in shift page 2
			sequencer,  // Device is in sequencer mode
		} op_mode_t;

	/* Variables */

	/* Function Prototypes: */
	
		//External Functions
		
		void side_switch_init(void);
		void side_switch_config(side_sw_settings_t *settings);
		side_sw_settings_t* get_side_switch_config(void);
		void process_side_switch_input(void);
		
		void set_op_mode(op_mode_t new_mode);
		op_mode_t get_op_mode(void);
		
		//Internal Functions

#endif /* SIDE_SWITCH_H_ */