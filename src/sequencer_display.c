/*
 * sequencer_display.c
 *
 * Created: 10/14/2013 10:31:25 AM
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

#include "sequencer_display.h"

void init_seq_display(void)
{
	switch (sequencerDisplayState) {
		case OFF:{

		}
		break;
		case DEFAULT:{
			build_default_display();
		}
		break;
		case PATTERN_MEMORY: {
			build_pattern_memory_display();
		}
		break;
		case PATTERN_EDIT:{
			build_pattern_edit_display();
		}
		break;
		//case EFFECTS_SELECTION:{
			//build_effects_selection_display();
		//}
		break;
	}
}

void build_default_display(void)
{
	// Get the display brightness values
	uint8_t rgb_brightness = pgm_read_byte(&brightnessMap[global_rgb_brightness]);
	uint8_t ind_brightness = pgm_read_byte(&brightnessMap[global_ind_brightness]);
	UNUSED(rgb_brightness);
	UNUSED(ind_brightness);

	// Initialize all indicators for the four slots
	for(uint8_t i=0;i<4;++i){
		
		// Dim the brightness if the slot is muted
		uint8_t rgb_brightness = !slotSettings[i].mute_on ? pgm_read_byte(&brightnessMap[global_rgb_brightness]) : 10;
		uint8_t ind_brightness = !slotSettings[i].mute_on ? pgm_read_byte(&brightnessMap[global_ind_brightness]) : 10;
		
		// If FX send is enabled for a slot we turn on the RGB of th clip selector encoder
		if(slotSettings[i].fx_send){
			set_encoder_rgb_level(i, FX_ON_COLOR, rgb_brightness);
		} else {
			set_encoder_rgb(i, 0);
		}
	
		// Draw the clip selection indicators
		set_indicator_pattern_level(i, 0x8000 >> get_slot_clip(i), ind_brightness);
		sequencerRawValue[i] = get_slot_clip(i)*1270;
		
		// Draw the pattern selection indicators
		set_indicator_pattern_level(i+4, 0x8000 >> slotSelectedBuffer[i], ind_brightness);
		sequencerRawValue[i+4] = slotSelectedBuffer[i] * 1270;

		// Draw the mute on/off indicators
		if(!slotSettings[i].mute_on){
			set_encoder_rgb_level(8+i, MUTE_COLOR, rgb_brightness);
		} else {
			set_encoder_rgb(8+i, 0);
		}
		
		// Draw the volume indicators
		set_encoder_indicator_level(8+i, slotSettings[i].volume, false, BAR, 0, ind_brightness);
		sequencerRawValue[8+i] = slotSettings[i].volume*100;
		
		// Draw the filter indicators
		if(slotSettings[i].filter_on){
			set_encoder_rgb_level(12+i, FILTER_COLOR, rgb_brightness);
			} else {
			set_encoder_rgb(12+i, 0);
		}

		// Draw the filter on/off indicators
		set_encoder_indicator_level(12+i, slotSettings[i].filter, true, BAR, FILTER_DETENT_COLOR, ind_brightness);
		sequencerRawValue[12+i] = slotSettings[i].filter*100;
	}
}

void build_pattern_memory_display(void)
{
	uint16_t bit = 0x01;
	uint16_t memory_slot_state = get_memory_slot_states();
	
	// Draw a display which indicates whether a mem slot is full
	// or empty by setting its RGB to a certain color
	for(uint8_t i=0;i<16;++i){
		if (bit & memory_slot_state){
			set_encoder_rgb(i, MEMORY_SLOT_FULL_COLOR);
		} else {
			set_encoder_rgb(i, 0);
		}
		set_encoder_indicator(i, 0, false, BAR, 0);
		sequencerRawValue[i] = 12700;
		bit <<=1;
	}
}

void build_pattern_edit_display(void)
{
	for(uint8_t i=0;i<16;++i){
		uint8_t state = get_step_state(selectedSlot, i);
		// Set the RGB to Red if step active, off if inactive
		if(state){
			set_encoder_rgb(i, ACTIVE_COLOR);
			} else {
			set_encoder_rgb(i, INACTIVE_COLOR);
		}
		// Set the encoder indicator to match the step level
		set_encoder_indicator(i, state, false, BAR, 0);
		sequencerRawValue[i] = state*100;
	}
}

void run_sequencer_display(void)
{
	static uint8_t idx=0;
	static uint8_t prev_seq_state = OFF;
	
	// If the sequencer state has changed since
	// last time rebuild the display
	if (sequencerDisplayState != prev_seq_state){
		init_seq_display();
	}
	prev_seq_state = sequencerDisplayState;
	
	switch (sequencerDisplayState) {
		case OFF:{
			set_encoder_indicator(idx, 0,0,0,0);
			set_encoder_rgb(idx, 0x50);
		}
		break;
		case DEFAULT:{
			run_default_display(idx);
		}
		break;
		case PATTERN_MEMORY:{
			run_pattern_memory_display(idx);
		}
		break;
		case PATTERN_EDIT:{
			run_pattern_edit_display(idx);
		}
		break;
	}
	// Increment the index for next time
	idx += 1;
	if(idx > 15){idx = 0;}
}

const uint16_t indicator_pattern[4] = {0xC000,0x3800,0x0700,0x00E0};

void run_default_display(uint8_t idx){

	//static uint8_t lastIndex;

	// First the indicator displays
	uint8_t currentValue = SeqIndicatorValue[idx];
	
	// Get the display brightness values
	uint8_t rgb_bright = pgm_read_byte(&brightnessMap[global_rgb_brightness]);
	uint8_t ind_bright = pgm_read_byte(&brightnessMap[global_ind_brightness]);
	uint8_t brightness;

	switch (is_row_type(idx)){
		
		// Draw the clip selection indicator
		case clipSelection:{
			
			brightness = !slotSettings[idx].mute_on ? ind_bright : 10;
			
			// If we are currently in playback of a recorded sequence draw the clip selection for this step
			set_indicator_pattern_level(idx, 0x8000 >> get_slot_clip(idx), brightness);
		}
		break;
		
		// Draw the pattern selection indicator
		case patternSelection: {
		
			brightness = !slotSettings[idx-4].mute_on ? ind_bright : 10;
		
			// If beat roll is active draw the beat roll display
			if (mod_level[idx-4]){
				uint8_t rate = mod_level[idx-4]/10;
				set_indicator_pattern_level(idx, indicator_pattern[rate], brightness);
			// Otherwise draw the current pattern selection
			} else {
				set_indicator_pattern_level(idx, 0x8000 >> slotSelectedBuffer[idx-4], brightness);
			}
			
			// If the current step is enabled for this pattern set the RGB on
			uint8_t current_step;
	
			if(rythmIndex == 0) {
				current_step = 15;
			} else {
				current_step = (rythmIndex - 1);
			}
									
			if(get_step_state(idx-4, current_step)){			
				brightness = !slotSettings[idx-4].mute_on ? rgb_bright : 10;
				if(clock_is_stable()){
					if(current_step == 0){
						set_encoder_rgb_level(idx, SEQ_DOWN_BEAT_COLOR, brightness);
					} else {
						set_encoder_rgb_level(idx, seqBeatColor, brightness);
					}
				} else {
					set_encoder_rgb_level(idx, 0x50, brightness);
				}
			} else {
				set_encoder_rgb(idx, 0);
			}
		}
		break;
		
		// Draw the slot volume indicator
		case volumeAdjust: {
			uint8_t brightness = !slotSettings[idx-8].mute_on ? 127 : 10;
			set_encoder_indicator_level(idx, currentValue, false, BLENDED_BAR, 0, brightness);
		}
		break;
		
		// Draw the slot filter indicator
		case filterAdjust: {
			uint8_t brightness = !slotSettings[idx-12].mute_on ? 127 : 10;
			set_encoder_indicator_level(idx, currentValue, true, BLENDED_BAR, FILTER_DETENT_COLOR, brightness);
		}
		break;
	}
	
	//lastIndex = rythmIndex;
}

void run_pattern_edit_display(uint8_t idx)
{
	uint8_t state;
	
	uint8_t current_step;
	
	if(rythmIndex == 0) {
		current_step = 15;
	} else {
		current_step = (rythmIndex - 1);
	}
	
	// Force the cursor to the first position if not in playback
	if (seq_state != PLAYBACK){
		current_step = 0;
	}

	if(idx == current_step){
		// Then draw the beat cursor
		set_encoder_rgb(idx, seqBeatColor);
		// And refresh the last position to prevent ghosting on the display
		uint8_t last = (idx == 0) ? 15 : (idx-1);
		state = get_step_state(selectedSlot, last);
		uint8_t color = state ? ACTIVE_COLOR : INACTIVE_COLOR;
		set_encoder_rgb(last, color);
	} else {
		// Otherwise draw the state color
		state = get_step_state(selectedSlot, idx);
		if(state){
			set_encoder_rgb(idx, ACTIVE_COLOR);
			} else {
			set_encoder_rgb(idx, INACTIVE_COLOR);
		}
	}
}

void run_pattern_memory_display(uint8_t idx)
{
	static uint8_t lastIndex;

	if ((rythmIndex != lastIndex) && active_memory_slot < 16 && active_memory_slot == idx){
		uint8_t divider = truncate_steps[4-(truncate_value/26)];
		uint8_t index = rythmIndex % divider;
		uint8_t spinner_position = index;
		if (spinner_position <= 11){
			uint16_t dot_pattern = 0x8000 >> (divider-1);
			dot_pattern |= 0xFFFF8000 >> spinner_position;
			set_indicator_pattern(active_memory_slot, dot_pattern);
		}
		lastIndex = rythmIndex;	
	}
}

uint16_t make_effect_indicator_pattern(uint8_t master_level, uint8_t step_level)
{
	// Draw a dot to indicate the master fx level settings,
	// With a bar to indicate the step fx level
	// LED		[1] [2] [3] [4] [5] [6] [7] [8] [9] [10] [11]
	// MASTER	 -   -   -   -   -   -   -   -   *   -    -
	// STEP      *   *   *   *   *   -   -   -   -   -    -
	// DISPLAY   *   *   *   *   *   -   -   -   *   -    -
			
	uint32_t bit_pattern = 0x00008000;
	uint8_t step = (master_level / 11.6f);
	bit_pattern >>= step;
	uint32_t step_pattern = 0xFFFF0000;
	uint16_t level = (step_level * master_level) /127;
	step = level / 11.6f;
	step_pattern >>= step;
	bit_pattern |= step_pattern;
	
	return (uint16_t)bit_pattern;
}
