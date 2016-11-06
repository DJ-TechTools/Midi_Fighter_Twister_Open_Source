/*
 * sequence.h
 *
 * Created: 8/2/2013 12:18:57 PM
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
#ifndef SEQUENCE_H_
#define SEQUENCE_H_

/*	Includes: */

	#include <asf.h>
	
	#include "sequencer_display.h"
	#include "sequencer_input.h"

	#include "display_driver.h"
	#include "midi.h"
	#include "encoders.h"

/*	Macros: */

	// Color Settings
	#define DEFAULT_CURSOR_COLOR    90
	#define MEM_CURSOR_COLOR        10
	#define FILTER_DETENT_COLOR    126
	#define FILTER_COLOR            15

	#define ACTIVE_COLOR	        10
	#define INACTIVE_COLOR	         0
	#define STEP_COLOR		        90
	#define SEQ_DOWN_BEAT_COLOR     61
	
	#define MUTE_COLOR                1
	#define FX_ON_COLOR			    120
	#define MEMORY_SLOT_FULL_COLOR	 62
	
	#define NO_CLOCK_COLOR		   0x50
	
	// Sequence control MIDI Settings
	#define SEQ_CHANNEL				  7
	#define SEQ_FX_CHANNEL            8
	
	#define SEQ_SLOT_VOL_OFFSET	     10
    #define SEQ_PLAY_CC			     14
	#define SEQ_ENABLE_CC			 15
	#define SEQ_MUTE_OFFSET			 20	
	#define SEQ_FILTER_ON_OFFSET	 24
	#define SEQ_FILTER_VALUE_OFFSET  28
	#define SEQ_SETTINGS_OFFSET		102
	#define SEQ_FX_CONTROL_CC       102

	
	#define SEQ_FX_PRESET_START      80

	// FX CC's
	#define SEQ_FX_DRYWET_CC		 65
	#define SEQ_FX_ONOFF_CC_1		 66
	#define SEQ_FX_ONOFF_CC_2		 67
	#define SEQ_FX_ONOFF_CC_3		 68
	#define SEQ_FX_ADJUST_CC_1		 69
	#define SEQ_FX_ADJUST_CC_2		 70
	#define SEQ_FX_ADJUST_CC_3		 71
	#define SEQ_FX_SELECT_CC_1		 72
	#define SEQ_FX_SELECT_CC_2		 73
	#define SEQ_FX_SELECT_CC_3		 74
	
/*	Types: */

	typedef enum displayMode{
		OFF,
		DEFAULT,
		PATTERN_MEMORY,
		PATTERN_EDIT,
		EFFECTS_SELECTION,
		EFFECTS_PATTERN_EDIT,
		SWING_PAGE,
	} displayMode_t;
	
	typedef enum rowTypes{
		clipSelection,
		patternSelection,
		volumeAdjust,
		filterAdjust,	
	} rowTypes_t;
	
	typedef enum {
		DISABLED,
		WAIT_FOR_SYNC,
		PLAYBACK,
	} seq_state_t;

	typedef struct {
		bool	keylock;
		bool	fx_send;
		bool	monitor;
		bool	punch_mode;
		bool	mute_on;
		bool	filter_on;
		uint8_t volume;
		uint8_t filter;
	} slotSettings_t;
	
/* Variables */

	extern bool seq_traktor_mode;
	
	extern seq_state_t seq_state;

	// The 12 default patterns
	const uint8_t default_pattern[11][16];

	// Holds the current sequencer interface state
	displayMode_t sequencerDisplayState;
	
	// Holds the current rhythm index or 'Step' the sequencer is at
	uint8_t rythmIndex;
	
	// Holds the currently selected slot (for Pattern Editing)
	uint8_t selectedSlot;
	
	// Holds the currently selected pattern for each slot
	uint8_t slotSelectedBuffer[4];
	
	// Holds the 12 pattern buffers for each slot
	uint8_t pattern[4][12][16];

	// Holds the currently selected clip for each slot
	uint8_t slot_clip[4];
	
	// Holds the slot settings states for each slot
	// Key-lock, FX Send, Monitor, & Punch Mode 
	slotSettings_t slotSettings[4];

	// Used to signal the start of playback
	extern bool start_pattern;
	
	// Holds the truncate amount
	extern uint8_t truncate_value;	
	extern const uint8_t truncate_steps[5];
	
	// The currently selected swing pattern
	extern uint8_t swing_selection;
	
	// Is swing enabled
	extern bool swing_enabled;
	
	void stop_playback(void);
	void reset_sequence(void);
	void test(void);
	void schedule_trigger(void);

/* Function Prototypes: */

	void sequencer_init(void);

	// Sends the midi triggers for the current rythm index (step)
	void send_triggers(void);
	
	// Handles the MIDI clock
	void seq_midi_clock_handler(int8_t tick);
	
	// Process incoming MIDI packets used by sequencer features
	void process_seq_midi(uint8_t cc_number, uint8_t cc_value);

	void set_step_state(uint8_t step, uint8_t slot, uint8_t volume);
	void set_slot_clip(uint8_t slot, uint8_t clip);

	uint8_t get_slot_clip(uint8_t slot);
	uint8_t get_step_state(uint8_t selected_pattern, uint8_t step);

	uint16_t get_slot_volume(uint8_t slot);
	
	// Forces clock sync
	void sync_clock(void);
	
	// Shifts the pattern for a slot forwards (true) or back 1 step 
	void shift_pattern(uint8_t slot, bool direction);
	
	// If true doubles the playback speed, if false halves it
	// void double_tempo(bool double_time);
	
	rowTypes_t is_row_type(uint8_t idx);
	
	void random_pattern(uint8_t inSlot);
	
	void play_or_stop_sequence(void);
	
	void stop_playback(void);
	
	void reset_sequence(void);

#endif /* INCFILE1_H_ */