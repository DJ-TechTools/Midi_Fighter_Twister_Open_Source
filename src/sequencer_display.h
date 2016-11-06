/*
 * sequencer_display.h
 *
 * Created: 10/14/2013 10:32:32 AM
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


#ifndef SEQUENCER_DISPLAY_H_
#define SEQUENCER_DISPLAY_H_

/*	Includes: */

	#include <asf.h>
	
	#include "sequencer.h"
	#include "sequencer_input.h"
	
	#include "display_driver.h"

/*	Macros: */
	
/*	Types: */

/* Variables */

	// Holds the color setting for the beat cursor
	uint8_t seqBeatColor;
	
	// Holds the color settings of the 16 RGB segments
	uint8_t SeqSwitchColor[16];
	
	// Holds the indicator values of the 16 rotary indicators
	uint8_t SeqIndicatorValue[16];
		
/* Function Prototypes: */

	void init_seq_display(void);
	
	void build_default_display(void);
	void build_pattern_edit_display(void);
	void build_pattern_memory_display(void);
	
	void run_sequencer_display(void);	
	void run_default_display(uint8_t idx);
	void run_pattern_edit_display(uint8_t idx);
	void run_pattern_memory_display(uint8_t idx);
	
	uint16_t make_effect_indicator_pattern(uint8_t master_level, uint8_t step_level);

#endif /* SEQUENCER_DISPLAY_H_ */