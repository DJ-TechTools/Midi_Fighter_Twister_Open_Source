/*
 * encoder_types.h
 *
 * Created: 7/16/2013 12:06:05 PM
 *  Author: Michael
 *
 * Contains all encoder setting type defs and enums used in the Midi Fighter Twister Project 
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


#ifndef ENCODER_TYPES_H_
#define ENCODER_TYPES_H_


/*	Includes: */
#include <asf.h>
#include <math.h>

// DEPRECATED: This file is not imported by any file in this project! 
// - it has many declarations that are duplicates of encoders.h
// - importing it will cause errors (!Summer2016Update)

/*	Macros: */



/*	Types: */

	// Switch Type Enum
	typedef enum sw_type {
		TOGGLE,
		HOLD,
	} sw_type_t;
	
	typedef enum switch_movement {
		SW_UP,
		SW_DOWN,
	} switch_movement_t;

	// Encoder Switch MIDI Type Enum
	typedef enum enc_midi_type {
		SEND_CC,
		SEND_NOTE,
	} enc_midi_type_t;

	// Encoder Movement Type Enum
	typedef enum enc_move_type {
		DIRECT,
		EMULATION,
	} enc_move_type_t;
	
	// Switch Action Type Enum
	typedef enum sw_action {
		CC,
		NOTE,
		SHIFT_PAGE_1,
		SHIFT_PAGE_2,
		GLOBAL_BANK_UP,
		GLOBAL_BANK_DOWN,
		GLOBAL_BANK_1,
		GLOBAL_BANK_2,
		GLOBAL_BANK_3,
		GLOBAL_BANK_4,
	} sw_action_t;

	// Structure which holds all encoder settings
	typedef struct {
		sw_type_t	        switch_type;
		sw_action_t         switch_action;
		enc_move_type_t     movement;		
		bool				has_detent;
		uint8_t             group;
	} encoder_settings_t;

	/* Variables */

	/* Function Prototypes: */

#endif /* ENCODER_TYPES_H_ */
