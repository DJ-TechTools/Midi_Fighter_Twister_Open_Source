/*
 * sequencer_input.h
 *
 * Created: 10/14/2013 10:27:43 AM
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


#ifndef SEQUENCER_INPUT_H_
#define SEQUENCER_INPUT_H_

/*	Includes: */

	#include <asf.h>

	
	
	#include "sequencer.h"
	#include "sequencer_display.h"

	#include "midi.h"
	#include "encoders.h"

/*	Macros: */
	
/*	Types: */

	typedef struct {
		uint8_t clip_selection;
		uint8_t buffer_selection;
		uint8_t step_states[16];
	} slot_memory_t;

/* Variables */

	// Holds the raw encoder values 0 - 12700
	uint16_t sequencerRawValue[16];
	
	// Holds the detent counter value for the filter encoders
	int8_t  seq_detent_counter[16];
	
	// Used to store the current pattern settings for the memory recall mode 
	slot_memory_t slot_memory[4];
	
	extern int8_t   mod_level[4];
	
	extern uint8_t active_memory_slot;
	
	extern bool safe_to_write;
	extern uint8_t swing_amount;
		
/* Function Prototypes: */

	void process_sequencer_input(void);
	void process_seq_enc_buttons(uint8_t switch_idx, bool downpress);
	void process_seq_side_buttons(void);
	void do_alternate_functions(bool sw_down);
	void push_all_parameters(void);
	
	// Pattern Memory public functions
	uint16_t get_memory_slot_states(void);
	void pattern_memory_init(void);
	
	void load_pattern_memory(uint8_t memory_slot);
	void save_pattern_memory(uint8_t memory_slot);
	void delete_pattern_memory(uint8_t memory_slot);

#endif /* SEQUENCER_INPUT_H_ */