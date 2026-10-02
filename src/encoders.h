/*
 * encoders.h
 *
 * Created: 6/28/2013 1:48:45 PM
 *  Author: Michael
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

#ifndef ENCODERS_H_
#define ENCODERS_H_

	/*	Includes: */
	
		#include <asf.h>
		#include <midi.h>
		#include <input.h>
		#include <constants.h>
		#include <config.h>
		
	/*	Macros: */

	/* Constants: */
	#define PHYSICAL_ENCODERS 16  // 16 Encoders are placed on the Midi Fighter Twister Hardware
	//#define BANKED_ENCODERS 64 // essentially virtual encoders but not including 'shifted' encoders.
	//#define BANKED_ENCODER_MASK 0x3F // For Determining banked encoder id from the virtual encoder id
	//#define VIRTUAL_ENCODERS 128  // Twister Firmware supports 4 Banks of 16 Encoders, each containing a virtual shift encoder (4x16x2=128)
	// Bank mode is a runtime choice, so NUM_BANKS is a variable and not a
	// constant. Refer to constants.h. The macro keeps the name, because the
	// approximately 70 sites that use these three values are all loop bounds
	// or index arithmetic. No site does a #if or a case label on them, so
	// no site changes.
	//
	// Where you must have a compile time bound, for example an array size,
	// use MAX_BANKS.
	extern uint8_t g_num_banks;
	#define NUM_BANKS g_num_banks
	#define BANKED_ENCODERS (NUM_BANKS * PHYSICAL_ENCODERS)
	// This mask is correct only because 4 and 8 are both powers of two. If you
	// add a third bank count that is not a power of two, this breaks quietly.
	#define BANKED_ENCODER_MASK (BANKED_ENCODERS - 1)
	#define VIRTUAL_ENCODERS (NUM_BANKS * PHYSICAL_ENCODERS * 2)
	
	#define ENCODER_VALUE_SCALAR_DIRECT 100  // 100 values is 1 CC Value
	#define ENCODER_VALUE_SCALAR_EMULATION 178
	#define ENCODER_VALUE_SCALAR_DIRECT_FINE 25 // 25 values is 4 ticks per CC Value change
	#define ENCODER_VALUE_SCALAR_VELOCITY_MIN 25
	// A Mackie fader resolves 14 bits, so the 7 bit floor above throws away
	// resolution the host can see. 4 gives ~3175 steps over the travel, and the
	// fast end of the velocity curve is unchanged.
	#define ENCODER_VALUE_SCALAR_MCU_FADER_MIN 4
	// Pitch bend is 0..16383 where raw_encoder_value is 0..12700.
	#define MCU_FADER_MAX 16383
	// Fader touch, which a DAW needs to write automation in Touch and Latch
	// modes. Note 104 is strip 1 and 112 is the master, so 104 plus the strip
	// channel. All touch notes are on channel 1.
	#define MCU_TOUCH_NOTE_BASE 0x68
	#define MCU_TOUCH_CHANNEL 0
	#define MCU_TOUCH_NONE 0xFF
	// Scans of stillness before the touch is released, approximately 240ms at
	// 1.21ms per scan. encoder_is_active() releases after 100, which drops the
	// touch during a pause in a gesture.
	#define MCU_TOUCH_HOLD_SCANS 200
	// Half width of the fader detent, in raw units. The center detent spans 210,
	// so both feel the same. A width in raw units is also speed independent: a
	// fast sweep steps over the window, as it does over a real detent.
	#define MCU_DETENT_WINDOW_RAW 105
	// Surface state the host reports with button lamps, all notes on channel 1.
	// Flip swaps the V-Pots and the faders, so a flipped fader is no longer
	// volume and a 0 dB detent would be wrong.
	#define MCU_FLIP_NOTE 50
	#define MCU_ASSIGN_FIRST 40
	#define MCU_ASSIGN_IO 40
	#define MCU_ASSIGN_SENDS 41
	#define MCU_ASSIGN_PAN 42
	#define MCU_ASSIGN_PLUGINS 43
	#define MCU_ASSIGN_LAST 45
	#define MCU_ASSIGN_UNKNOWN 0xFF
	// V-Pot movement is sign and magnitude, not the binary offset the other
	// relative modes use: bit 6 is the direction and bits 0-5 the tick count.
	#define MCU_VPOT_DIRECTION 0x40
	#define MCU_VPOT_TICK_MAX 63
	// The host answers a V-Pot on its own CC plus 0x20, so V-Pot 16 rings on 48.
	#define MCU_VPOT_RING_OFFSET 0x20
	#define MCU_VPOT_CC_FIRST 0x10	// V-Pot 1. The eight strips run to 0x17
	// Ring modes the host sends in bits 5-4 of the ring byte.
	#define MCU_RING_DOT 0
	#define MCU_RING_BOOST_CUT 1
	#define MCU_RING_WRAP 2
	#define MCU_RING_SPREAD 3
	// Mackie lamp states, the velocity of the note the host sends back. Solid is
	// 127 in practice, but anything above a blink is taken as solid.
	#define MCU_LAMP_OFF		0
	#define MCU_LAMP_BLINK		1

	// Blink rate for MCU_BUTTON, as a value for run_encoder_animation()'s RGB
	// strobe. strobe_animation() reads bit 8 minus the rate of animation_counter,
	// so the period is 2^(9 - rate) counts. Free running the counter advances
	// every 18.4ms (255 frame timer passes of 72us), which puts rate 4 at 588ms,
	// close to the 2Hz a Mackie surface blinks at. Under MIDI clock the counter
	// advances every third clock tick, where rate 6 is a quarter note.
	#define MCU_BLINK_RATE_FREE		4
	#define MCU_BLINK_RATE_CLOCKED	6

	// The eight channel strips.
	#define MCU_STRIPS 8

	// A V-Pot does a different job in each assignment mode, and its press resets
	// whatever that job is, so the color shows the mode. Indices into colorMap7,
	// far enough apart to tell one from another.
	// The two color maps hold different palettes at the same index, so each map
	// needs its own set. colorMap7 is a hue wheel; colorMap64 matches the MF64.
	// An entry stores blue, green then red, so it reads backward from an RGB
	// literal. These indices were picked from the map and not from the source.
	#define MCU_COLOR_MODE_IO			112	// purple
	#define MCU_COLOR_MODE_SENDS		43	// green
	#define MCU_COLOR_MODE_PAN			1	// blue
	#define MCU_COLOR_MODE_PLUGINS		72	// orange
	#define MCU_COLOR_MODE_IO_64		49	// purple
	#define MCU_COLOR_MODE_SENDS_64		21	// green
	#define MCU_COLOR_MODE_PAN_64		45	// blue
	#define MCU_COLOR_MODE_PLUGINS_64	9	// orange
	// Mute shows the encoder's own inactive color, so a muted strip goes quiet
	// rather than taking a color of its own.
	// Mid travel, where a flipped fader on the pan assignment detents instead.
	#define MCU_CENTER_RAW 6350
	#define ENCODER_VALUE_SCALAR_VELOCITY_MAX 255

	//#define ENCODER_RELATIVE_TICKS_RESPONSIVE 2 // 2x per tick is approximately 178/100, best we can do
	#define ENCODER_RELATIVE_TICKS_RESPONSIVE 1 // 2x per tick is approximately 178/100, best we can do
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
			ENC_RESET_VALUE_INV,
			ENC_FINE_ADJUST,
			ENC_SHIFT_HOLD,
			ENC_SHIFT_TOGGLE,
			PC_RESEND = 9, // On press, re-sends Bank Select and Program Change for a program knob
			SW_ACTION_NONE = 10, // The switch does nothing. 10 held the Mackie 0 dB learn, which is gone
			MCU_BUTTON = 11,    // Sends like NOTE_HOLD, but reads the note that comes back as a Mackie lamp
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
			SEND_REL_ENC_MOUSE_EMU_DRAG = 4, // For 'Encoders' only, sends like rel_enc, but tells MF Utility, to drag the mouse for controlling on-screen elements with the mouse
			SEND_REL_ENC_MOUSE_EMU_SCROLL = 5, // For 'Encoders' only, sends like rel_enc, but tells MF Utility, to drag the mouse for controlling on-screen elements with the mouse
			SEND_PROGRAM_CHANGE = 6, // For 'Encoders' only, sends Bank Select from the page's CC0/CC32 knobs on the same channel, then Program Change
			SEND_MCU_FADER = 7,      // For 'Encoders' only, a Mackie Control fader: 14 bit pitch bend on the strip's channel
			SEND_MCU_VPOT = 8,       // For 'Encoders' only, a Mackie Control V-Pot: relative CC out, ring feedback in
			SEND_NONE = 9,           // For 'Encoders' only, the rotation does nothing
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
			SPREAD_BAR,
			NUM_DISPLAY_TYPES, // this must always be the last entry for stored types
			// Runtime only, for a V-Pot in the boost or cut ring mode. It draws from
			// the center like the detent styles, but without the detent dot that
			// blanks the ring at 63 and 64. It is never stored, so it needs no room
			// in the 2 bit EEPROM field.
			CENTER_BAR
		} display_type_t;

		// Tag-Value table which holds the configuration for 1 encoder
		#define ENC_CFG_SIZE 15
		#define ENC_REL_FINE_LIMIT 0x04 // how many 'ticks' per output when encoder is Relative and Fine
		#define ENC_REL_OUTPUT_MIN 0x01 // Relative binary offset floor (64-63), clamps the CCW output so it cannot wrap
		#define ENC_REL_OUTPUT_MAX 0x7F // Relative binary offset ceiling (64+63), clamps the CW output so it cannot wrap
		// Velocity sensitive step mapping. The relative encoder path and Native Mode
		//   both use it, so the two cannot drift apart. 'base' is the 1..256
		//   multiplier that convert_ticks_per_scan_to_value_multiplier() returns.
		#define ENC_REL_VELOCITY_STEP_MAX 17
		#define ENC_REL_VELOCITY_STEP(base) (1 + (((base) >> 4) & 0x1F)) // 1..17
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
				//uint8_t			reset_value;
			};
			uint8_t bytes[ENC_CFG_SIZE];
		} encoder_config_t;
		

		/* Constants */
		extern const uint16_t encoder_detent_limit_low;
		extern const uint16_t encoder_detent_limit_high;
		
		/* Variables */
		
		//extern uint8_t indicator_value_buffer[4][16];
		//extern encoder_config_t encoder_settings[64];
		// These point into bank_arena. Refer to encoders_alloc(). The arena
		// holds the storage for the live bank mode, so the size is available at
		// run time only. Every use is arr[i] or arr[bank][enc], which reads
		// the same through a pointer, so no use site changes.
		extern uint8_t (*indicator_value_buffer)[16];
		extern encoder_config_t *encoder_settings;
		// - overall, the use of input_map over an enlarged encoder_settings saves about 236 Bytes of RAM (624->960)
		// -- But logically, the use of encoder_settings is a much simpler and faster implementation

		/* Function Prototypes: */
		void get_encoder_config(uint8_t bank, uint8_t encoder, encoder_config_t *cfg_ptr);
		void save_encoder_config(uint8_t bank, uint8_t encoder, encoder_config_t *cfg_ptr);
		void factory_reset_encoder_config(void);
		// Resets a range of encoder settings pages only. The count starts at the
		// first such page. A change of bank mode uses this to give defaults to
		// the pages that the new mode will read. 4 pages make one bank.
		void factory_reset_encoder_pages(uint8_t first_page, uint8_t last_page);
		
		// Divides bank_arena for the given bank count. Call one time at boot,
		// before anything reads or writes bank scaled storage.
		void encoders_alloc(uint8_t banks);
		// The part of the arena that the live bank mode does not use. The
		// sequencer puts its large data here in 4 bank mode.
		uint8_t* bank_arena_spare(uint16_t *size);

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

		#if VELOCITY_CALC_METHOD > VELOCITY_CALC_M_NONE
			// Converts encoder speed (ticks per scan cycle) into a 1..256 value multiplier.
			uint16_t convert_ticks_per_scan_to_value_multiplier(uint8_t tick_count, uint16_t cycles_count);
		#endif

		void process_encoder_input_switch(uint8_t i, uint8_t virtual_encoder_id, uint8_t banked_encoder_id, uint16_t bit);
		void process_encoder_input(void);
		void update_encoder_display(void);
		void change_encoder_bank(uint8_t new_bank);
		uint8_t current_encoder_bank(void);
		void refresh_display(void);
		
		// Mackie fader feedback. Pitch bend has no number, so the channel selects.
		void process_pitchbend_midi(uint8_t channel, uint16_t bend);
		// Tracks Flip and the assignment mode from the host's lamp feedback.
		void mcu_observe_feedback_note(uint8_t channel, uint8_t note, uint8_t velocity, bool note_on);
		void process_element_midi(uint8_t channel, uint8_t type, uint8_t number, uint8_t value, uint8_t state);
		void process_indicator_update(uint8_t idx, uint8_t value, uint8_t shifted);
		void process_sw_toggle_update(uint8_t idx, uint8_t value);
		void process_sw_encoder_shift_update(uint8_t idx, uint8_t value);
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