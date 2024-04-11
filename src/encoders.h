/*
 * encoders.h
 *
 * Created: 6/28/2013 1:48:45 PM
 *  Author: Michael
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

#ifndef ENCODERS_H_
#define ENCODERS_H_

	/*	Includes: */
	
		#include <asf.h>
		#include <midi.h>
		#include <input.h>
		#include <config.h>
		
	/*	Macros: */

	/* Constants: */
	#define PHYSICAL_ENCODERS 16  // 16 Encoders are placed on the MIDI Fighter Twister Hardware
	#define BANKED_ENCODERS 64 // essentially virtual encoders but not including 'shifted' encoders.
	#define BANKED_ENCODER_MASK 0x3F // For Determining banked encoder id from the virtual encoder id
	#define VIRTUAL_ENCODERS 128  // Twister Firmware supports 4 Banks of 16 Encoders, each containing a virtual shift encoder (4x16x2=128)
	
	#define ENCODER_VALUE_SCALAR_DIRECT 100  // 100 values is 1 CC Value
	#define ENCODER_VALUE_SCALAR_EMULATION 178
	#define ENCODER_VALUE_SCALAR_DIRECT_FINE 25 // 25 values is 4 ticks per CC Value change
	#define ENCODER_VALUE_SCALAR_VELOCITY_MIN 25
	#define ENCODER_VALUE_SCALAR_VELOCITY_MAX 255

	#define ENCODER_RELATIVE_TICKS_RESPONSIVE 2 // 2x per tick is approximately 178/100, best we can do
	/*	Types: */
	
		typedef enum enc_control_type {
			ENCODER,
			SWITCH,
			SHIFT,
		} enc_control_type_t ; 

		// Encoder Switch Type Enum
		typedef enum enc_sw_type {
			CC_HOLD,
			CC_TOGGLE,
			NOTE_HOLD,
			NOTE_TOGGLE,
			ENC_RESET_VALUE,
			ENC_FINE_ADJUST,
			ENC_SHIFT_HOLD,
			ENC_SHIFT_TOGGLE,
		} enc_sw_action_type_t;
	
		// Encoder switch movement enum
		typedef enum switch_event {
			SW_UP,
			SW_DOWN,
			SW_HELD,
		} switch_event_t;

		// Encoder Switch MIDI Type Enum
		typedef enum midi_type {
			SEND_NOTE,
			SEND_CC,
			SEND_REL_ENC,
			SEND_NOTE_OFF = 3, // 20160615 - for MIDI Feedback only
			SEND_SWITCH_VEL_CONTROL = 3, // For 'Encoders' only, sends like send_cc, but also adjusts the button output velocity.
			SEND_REL_ENC_MOUSE_EMU_DRAG = 4, // For 'Encoders' only, sends like rel_enc, but tells MF Utility, to drag the mouse for controlling onscreen elements with the mouse
			SEND_REL_ENC_MOUSE_EMU_SCROLL = 5, // For 'Encoders' only, sends like rel_enc, but tells MF Utility, to drag the mouse for controlling onscreen elements with the mouse
		} midi_type_t;

		// Encoder Movement Type Enum
		typedef enum enc_move_type {
			DIRECT,
			EMULATION, // This is used for "responsive" as well
			VELOCITY_SENSITIVE_ENC
		} enc_move_type_t;
		
		// Encoder Indicator Display Type Enum
		typedef enum {
			DOT,
			BAR,
			BLENDED_BAR,
			BLENDED_DOT,
		} display_type_t;

		// Tag-Value table which holds the configuration for 1 encoder
		//#define ENC_CFG_SIZE 14
		#define ENC_CFG_SIZE 15
		#define ENC_REL_FINE_LIMIT 0x04 // how many 'ticks' per output when encoder is Relative and Fine
		typedef union {  // Each of these fields is designed to be written to directly from MIDI Sysex Data
			struct {     // - so you can only use 7-bits of these uint8_t's to store data.
				uint8_t			has_detent;
				uint8_t			movement;
				uint8_t			switch_action_type;
				uint8_t			switch_midi_channel;
				uint8_t			switch_midi_number;
				uint8_t			switch_midi_type;
				uint8_t			encoder_midi_channel;
				uint8_t			encoder_midi_number;
				uint8_t			encoder_midi_type;
				uint8_t			active_color;
				uint8_t			inactive_color;
				uint8_t			detent_color;
				uint8_t		    indicator_display_type;
				uint8_t			is_super_knob;		
				uint8_t			encoder_shift_midi_channel; // !Summer2016Update
			};
			uint8_t bytes[ENC_CFG_SIZE];
		} encoder_config_t;
		
		// !Summer2016Update: Encoder Shift Feedback 
		// - Input map was removed in favor of expanding encoder_settings table

		/* Constants */
		extern const uint16_t encoder_detent_limit_low;
		extern const uint16_t encoder_detent_limit_high;
		
		/* Variables */
		
		extern uint8_t indicator_value_buffer[4][16];
		extern encoder_config_t encoder_settings[64];
		// - overall, the use of input_map over an enlarged encoder_settings saves about 236 Bytes of RAM (624->960)
		// -- But logically, the use of encoder_settings is a much simpler and faster implementation

		/* Function Prototypes: */
		void get_encoder_config(uint8_t bank, uint8_t encoder, encoder_config_t *cfg_ptr);
		void save_encoder_config(uint8_t bank, uint8_t encoder, encoder_config_t *cfg_ptr);
		void factory_reset_encoder_config(void);
		
		void encoders_init(void);
		void process_encoder_input_rotary(uint8_t i, uint8_t virtual_encoder_id, uint8_t banked_encoder_id, uint16_t bit);

		#if VELOCITY_CALC_METHOD == VELOCITY_CALC_M_TPS_BLOCKS
			bool process_encoder_input_rotary_relative(uint8_t i, uint8_t virtual_encoder_id, uint8_t banked_encoder_id, int8_t new_value, uint16_t bit, uint16_t cycle_count);
			bool process_encoder_input_rotary_absolute(uint8_t i, uint8_t virtual_encoder_id, uint8_t banked_encoder_id, int8_t new_value, uint16_t bit, uint16_t cycle_count);
			bool process_encoder_input_rotary_detent(uint8_t i, uint8_t virtual_encoder_id, uint8_t banked_encoder_id, int8_t new_value, uint16_t cycle_count);
			bool process_encoder_input_rotary_deadzone(uint8_t i, uint8_t virtual_encoder_id, uint8_t banked_encoder_id, int8_t new_value, uint16_t cycle_count);
		#else
			bool process_encoder_input_rotary_relative(uint8_t i, uint8_t virtual_encoder_id, uint8_t banked_encoder_id, int8_t new_value, uint16_t bit);
			bool process_encoder_input_rotary_absolute(uint8_t i, uint8_t virtual_encoder_id, uint8_t banked_encoder_id, int8_t new_value, uint16_t bit);
			bool process_encoder_input_rotary_detent(uint8_t i, uint8_t virtual_encoder_id, uint8_t banked_encoder_id, int8_t new_value);
			bool process_encoder_input_rotary_deadzone(uint8_t i, uint8_t virtual_encoder_id, uint8_t banked_encoder_id, int8_t new_value);
		#endif


		void process_encoder_input_switch(uint8_t i, uint8_t virtual_encoder_id, uint8_t banked_encoder_id, uint16_t bit);
		void process_encoder_input(void);
		void update_encoder_display(void);
		void change_encoder_bank(uint8_t new_bank);
		uint8_t current_encoder_bank(void);
		void refresh_display(void);
		
		void process_element_midi(uint8_t channel, uint8_t type, uint8_t number, uint8_t value, uint8_t state);
		//void process_indicator_update(uint8_t idx, uint8_t value);
		void process_indicator_update(uint8_t idx, uint8_t value, uint8_t shifted); // !Summer2016Update: Shifted Encoder Value Feedback
		void process_sw_toggle_update(uint8_t idx, uint8_t value); // !Summer2016Update: Toggle State Feedback
		void process_sw_encoder_shift_update(uint8_t idx, uint8_t value); // !Summer2016Update: Shifted Encoder Switch: Toggle State Feedback
		void process_sw_rgb_update(uint8_t idx, uint8_t value);
		void process_sw_animation_update(uint8_t idx, uint8_t value);
		void process_encoder_animation_update(uint8_t idx, uint8_t value);
		void process_shift_update(uint8_t idx, uint8_t value);
		
		void run_shift_mode(uint8_t page);
		
		uint8_t scale_encoder_value(int16_t value);
		int16_t clamp_encoder_raw_value(int16_t value);
		
		bool encoder_is_in_detent(int16_t value);
		bool encoder_is_in_deadzone(int16_t value);
		bool encoder_is_in_shift_state(uint8_t bank, uint8_t encoder);
		bool encoder_midi_type_is_relative(uint8_t encoder);

#endif /* ENCODERS_H_ */ 
