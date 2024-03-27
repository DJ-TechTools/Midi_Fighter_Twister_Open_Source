/*
 * SEQUENCE.c
 *
 * Created: 8/2/2013 12:18:50 PM
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

#include "sequencer.h"

/*
const uint8_t default_pattern[11][16] PROGMEM = {
		
		{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
		//{127,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
		{127,0,0,0,0,0,0,0,127,0,0,0,0,0,0,0},
		{127,0,0,0,127,0,0,0,127,0,0,0,127,0,0,0},
		
		{127,0,0,127,0,0,127,0,0,127,0,0,127,0,0,0},
		{127,0,0,127,0,0,127,0,127,0,0,127,0,0,127,0},
		{127,127,127,0,0,0,127,127,127,0,0,0,127,127,127,127},
		{127,0,127,0,127,0,127,0,127,0,127,0,127,0,127,0},
		
		{127,0,127,0,127,0,127,127,0,127,0,127,0,127,0,127},
		{127,0,127,127,0,127,127,0,127,0,127,127,0,127,127,0},
		{127,127,0,127,127,0,127,127,0,127,127,0,127,127,0,127},
		{127,127,127,127,127,127,127,127,127,127,127,127,127,127,127,127},
	};
*/	
// Ean's Patterns	
const uint8_t default_pattern[11][16] PROGMEM = {
		
		{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
		{127,0,0,0,127,0,0,0,127,0,0,0,127,0,0,0},
		{0,0,0,0,127,0,0,0,0,0,0,0,127,0,0,0},
		{0,0,0,0,0,0,0,0,0,85,0,127,0,0,127,0},
		
		{127,0,0,85,0,0,100,0,0,110,0,0,127,0,0,0},
		{20,85,0,127,0,0,127,0,0,0,127,0,0,35,85,85}, //5
		{85,127,127,0,0,0,110,127,100,0,0,0,0,85,85,85},
		
		{85,20,110,0,127,10,100,20,0,45,127,0,110,45,75,35}, //7
		{127,0,127,0,127,0,110,127,0,127,0,127,127,0,127,0}, //8
		{127,0,127,127,127,0,127,127,127,0,127,127,127,127,127,127},
		{85,127,100,127,85,127,85,127,85,127,85,127,110,127,100,127},
};


//static uint8_t tempo = 6;
bool start_pattern = false;

uint8_t truncate_value;

bool seq_traktor_mode;

seq_state_t seq_state;

void sequencer_init(void)
{	
	// Initiase the slot parameters to their defaults
	for(uint8_t i=0;i<4;++i)
	{
		set_slot_clip(i, 0);
		
		// Read in the default patterns
		for(uint8_t j=0;j<12;++j){
			for(uint8_t k=0;k<16;++k){
				pattern[i][j][k] = pgm_read_byte(&default_pattern[j][k]);
			}
		}
		
		slotSelectedBuffer[i] = 0;
		slotSettings[i].filter_on = false;
		slotSettings[i].filter    = 63;
		slotSettings[i].mute_on   = false;
		slotSettings[i].volume    = 127;
		slotSettings[i].fx_send   = false;		
	}

	selectedSlot = 0;
	//fxParameters.master_value = 127;
	//fxParameters.selection = 0;
	//fxParameters.currentFx = get_fx_parameters(0);
	
	// Initialize all arrays
	memset(sequencerRawValue, 0, 32);
	memset(SeqIndicatorValue, 0, 16);	
	memset(seq_detent_counter, 0, 4);
	
	// Initialize pattern memory
	pattern_memory_init();
	
	// Initialize non zero encoder variables 
	for(uint8_t i=0;i<4;++i){
		sequencerRawValue[i] = 0;
		SeqIndicatorValue[i] = 0;
		sequencerRawValue[i] = 0;
		SeqIndicatorValue[i] = 0;
		sequencerRawValue[i+12] = 6300;
		SeqIndicatorValue[i+12] = 63;
		sequencerRawValue[i+8] = 12700;
		SeqIndicatorValue[i+8] = 127;
	}
	
	truncate_value = 127;
	
	// Swing parameters - unused for now
	swing_enabled = false;
	swing_amount = 127;
	swing_selection = 0;
	
	// Initialize the sequencer display
	sequencerDisplayState = OFF;
	seqBeatColor = DEFAULT_CURSOR_COLOR;
	
	seq_traktor_mode = false;
	seq_state = DISABLED;
}

void play_or_stop_sequence(void)
{
	if(seq_state == DISABLED){
		// Arm the sequencer
		seq_state = WAIT_FOR_SYNC;
		midi_stream_raw_cc(SEQ_CHANNEL, SEQ_PLAY_CC, 127);
		
	} else {
		// Stop the sequencer
		seq_state = DISABLED;
		rythmIndex = 0;
		midi_stream_raw_cc(SEQ_CHANNEL, SEQ_PLAY_CC, 0);
	}
	
	
	/*
	
	if(!seq_is_in_playback){
		
		start_pattern = true;
		midi_stream_raw_cc(SEQ_CHANNEL, SEQ_PLAY_CC, 127);
		rythmIndex = getTickCount()/4;
		seq_is_in_playback = true;
		
		if (getTickCount() < 12){
			//rythmIndex = 24/6;// replace 6 with ticks per beat
		}
		
		// Wait for a stable clock
		if(get_counts_per_tick() && clock_is_stable())
		seq_is_in_playback = true;
		start_pattern = true;
		// Send Note On to host
		midi_stream_raw_cc(SEQ_CHANNEL, SEQ_PLAY_CC, 127);
		// If the trigger is late make sure we
		// start on the second step
		if (getTickCount() < 12){
			//rythmIndex = 24/6;// replace 6 with ticks per beat
		}*/ /*
	} else {
		midi_stream_raw_cc(SEQ_CHANNEL, SEQ_PLAY_CC, 0);
		start_pattern = false;
		seq_is_in_playback = false;
		rythmIndex = 0;
	}*/
}

void reset_sequence(void)
{
	rythmIndex = 0;
}

uint8_t rythmIndex = 0;
const uint8_t states[4] = {0,4,2,1};
const uint8_t truncate_steps[5] = {0,8,4,2,1};
const uint8_t factors[4] = {1,2,8,16};

const int8_t swing_adjustment[16][4] = {{0,1,0,1},
									   {0,3,0,3},
									   {0,5,0,5},
									   {0,8,0,8},
									   {0,11,0,11},
									   {0,13,0,13},
									   {0,0,-1,1},
									   {0,0,-3,3},
									   {0,0,-5,5},
									   {0,0,-8,8},
									   {0,0,-11,11},
									   {0,0,-13,13},
									   {0,0,0,0},
									   {0,0,0,0},
									   {0,0,0,0},
									   {1,-1,1,-1}};


uint8_t swing_selection;
bool swing_enabled;	

//static int16_t	current_swing_value = 0;
//static int16_t 	last_swing_value = 0;

void send_triggers(void){
	
	// This if statement can prolly be removed
	if (seq_state == PLAYBACK){
		
		// Schedule the next trigger
		//uint16_t ticks_per_beat = ((uint16_t)abs(get_counts_per_tick()*tempo));
		
		/*
		float swing_factor = 0;
		// If enabled add calculate swing factor
		if (swing_enabled){
			swing_factor = swing_amount/127.0f;
		}
		
		uint16_t time_for_next_trigger;
		if (rythmIndex == 0) {
			time_for_next_trigger = ticks_per_beat;
		} else {
			// Apply swing factor
			if(rythmIndex % 2){
				time_for_next_trigger = ticks_per_beat + (uint16_t)(ticks_per_beat * .16f * swing_factor);
			} else {
				time_for_next_trigger = ticks_per_beat - (uint16_t)(ticks_per_beat * .16f * swing_factor);
			}
		}
		
		swing_enabled = false;
		int16_t time_for_next_trigger;
		
		if(swing_enabled){
			current_swing_value = (int16_t)(ticks_per_beat * .03125f * (float)swing_adjustment[swing_selection][rythmIndex%4]);
			time_for_next_trigger = ticks_per_beat + current_swing_value - last_swing_value;
			last_swing_value = current_swing_value;
		} else {
			time_for_next_trigger = ticks_per_beat;
		}
		
		// After every 16 beats we wait to sync with the midi clock
		if(rythmIndex != 15){
			//schedule_task(send_triggers, time_for_next_trigger);
		}
		*/
		

		uint8_t step = rythmIndex; //% truncate_steps[4-(truncate_value/26)];
		
		// Debug
		// midi_stream_raw_cc(SEQUENCER_TEST_CHANNEL,SIXTEENTH_TRIGGER,rythmIndex);
		
		// Send any triggers
		uint16_t level;
		uint8_t note;
		
		//when scanning step 16 slots 1 & 2 never send their Note Off (Do send the note on)
		// Slot 3 works fine
		// Slot 4 Never sends a note off, however a note off of the sample selected for slot 4
		// is sent for 1 & 2
		
		
		for(uint8_t i=0;i<4;++i){			
			// Calculate the MIDI note number for the currently selected clip
			note = 36 + i + (get_slot_clip(i)*4);
			level = 0; // defaults to 0 which is off
			
			// If the beat repeat is enabled
			if ((mod_level[i] / 10) && (mod_level[i] > 0)) {
				
				uint8_t rate = mod_level[i] / 10;
				if (rate > 3) {rate = 3;}					
				if ((rythmIndex % states[rate]) == 0) {					
					level = 127;// (slotSettings[i].volume);
				}
				
			} else {
				// Get the state and velocity from the pattern
				if(get_step_state(i, step)) {
					level = get_step_state(i, step);						
				} 
			}
			int8_t prev_step = step - 1;
			prev_step = (prev_step == -1) ? 15 : prev_step;
			
			if(get_step_state(i, prev_step)){
				midi_stream_raw_note(SEQ_CHANNEL,note,false, 127);
			}
			
			// then if there is a note to send we send it!
			if (level) {	
				// send the slots volume setting (for Traktor)
				uint8_t velocity = (uint8_t)((level * slotSettings[i].volume) / 127);
				midi_stream_raw_note(SEQ_CHANNEL,note, true, velocity);
				midi_stream_raw_cc(SEQ_CHANNEL, SEQ_SLOT_VOL_OFFSET+i, velocity);
			}
		}

		// Flush USB endpoint now now to give best timing
		MIDI_Device_Flush(g_midi_interface_info);
		
		rythmIndex++;
		if(rythmIndex == 16){
			rythmIndex = 0;
			start_pattern = true;
		}
	}
}

void seq_midi_clock_handler(int8_t tick)
{
	// We always start play back on the first tick
	if (seq_state == WAIT_FOR_SYNC && tick == 0) {
		seq_state = PLAYBACK;
		send_triggers();
	} else if (seq_state == PLAYBACK){
	// If already in playback and the clock is stable we schedule the
	// next trigger one tick before it is due, this allows swing etc	
		if (clock_is_stable()){
			if (((tick+2) % 6) == 0) {
				schedule_task(schedule_trigger, 1);
			}
		} else {
	// Otherwise we just send it when we receive the tick
			if ((tick % 6) == 0) {
				send_triggers();
			}
		}
	}
}

/* Because interrupts are disabled when handling MIDI IN any scheduled tasks occur as soon
   as we enable the interrupts again, to work around this we schedule a task to schedule the 
   actual task
 */
void schedule_trigger(void)
{
	if (seq_traktor_mode){
		schedule_task(send_triggers,  get_counts_per_tick()/2);		
	} else {
		schedule_task(send_triggers, (3*get_counts_per_tick())/4);		
	}
}

// Returns the volume/play state for a given step of the specified slots selected pattern
uint8_t get_step_state(uint8_t slot, uint8_t step)
{
	return pattern[slot][slotSelectedBuffer[slot]][step];
}

// Sets the volume and state for a given step of the specified slots selected pattern
void set_step_state(uint8_t slot, uint8_t step, uint8_t volume)
{
	pattern[slot][slotSelectedBuffer[slot]][step] = volume;
}

// Sets the clip selection for the specified slot
void set_slot_clip(uint8_t slot, uint8_t clip)
{
	slot_clip[slot] = clip;
}

// Returns the clip selection for the specified slot 
uint8_t get_slot_clip(uint8_t slot)
{
	return slot_clip[slot];
}

// Forces MIDI clock sync for Traktor
void sync_clock(void){
	midi_stream_raw_cc(SEQ_CHANNEL, SEQ_ENABLE_CC, 127);
	midi_stream_raw_cc(SEQ_CHANNEL, SEQ_ENABLE_CC, 0);
}

/**
 * Shifts a pattern forwards or back 1 beat.
 * 
 * \param pattern_ptr [in] A pointer to the pattern we are editing
 *
 * \param direction [in] If true shift forwards, otherwise shift back
 *
 */

void shift_pattern(uint8_t slot, bool direction){
	
	uint8_t last_byte = 0;
	uint8_t next_byte = 0;
	
	if (direction){
			for(uint8_t j=0;j<16;++j){
				if (j==0){
					next_byte = pattern[slot][slotSelectedBuffer[slot]][0];
					pattern[slot][slotSelectedBuffer[slot]][0] = pattern[slot][slotSelectedBuffer[slot]][15];
					last_byte = next_byte;
				} else {
					next_byte = pattern[slot][slotSelectedBuffer[slot]][j];
					pattern[slot][slotSelectedBuffer[slot]][j] = last_byte;
					last_byte = next_byte;
				}
			}	
	} else {
			for(int8_t j=15;j>-1;--j){
				if (j==15){
					next_byte = pattern[slot][slotSelectedBuffer[slot]][15];
					pattern[slot][slotSelectedBuffer[slot]][15] = pattern[slot][slotSelectedBuffer[slot]][0];
					last_byte = next_byte;
					} else {
					next_byte = pattern[slot][slotSelectedBuffer[slot]][j];
					pattern[slot][slotSelectedBuffer[slot]][j] = last_byte;
					last_byte = next_byte;
				}
			}
		}
}



// Creates a randomized pattern in the specified slot

void random_pattern(uint8_t inSlot)
{
	for(uint8_t i=0;i<16;++i){
		pattern[inSlot][slotSelectedBuffer[inSlot]][i] = (random16() & 0x7F00)>>8;
	}
}


// Processes sequencer related MIDI input
static bool init = true;

void process_seq_midi(uint8_t cc_number, uint8_t cc_value)
{
	if((cc_number >= SEQ_MUTE_OFFSET) && (cc_number <= (SEQ_MUTE_OFFSET+3))){
		slotSettings[cc_number - SEQ_MUTE_OFFSET].mute_on = cc_value ? 0: 1;
		if (sequencerDisplayState == DEFAULT && get_op_mode() == sequencer){
			build_default_display();
		}
	} else if ((cc_number >= SEQ_FILTER_ON_OFFSET) && (cc_number <= (SEQ_FILTER_ON_OFFSET+3))){
		slotSettings[cc_number - SEQ_FILTER_ON_OFFSET].filter_on = cc_value ? 1 : 0;
		if (sequencerDisplayState == DEFAULT && get_op_mode() == sequencer){
			build_default_display();
		}
	} else if ((cc_number >= SEQ_SETTINGS_OFFSET) && (cc_number <= (SEQ_SETTINGS_OFFSET+15))) {
		uint8_t slot = (cc_number - SEQ_SETTINGS_OFFSET)%4;
		uint8_t setting = (cc_number - SEQ_SETTINGS_OFFSET)/4;
		switch (setting) {
			case 0:{
				slotSettings[slot].keylock = cc_value ? 1 : 0;
			}
			break;
			case 1:{
				slotSettings[slot].fx_send = cc_value ? 1: 0;
				if (sequencerDisplayState == DEFAULT && get_op_mode() == sequencer){
					build_default_display();
				}
			}
			break;
			case 2:{
				slotSettings[slot].monitor = cc_value ? 1 : 0;
			}
			break;
			case 3:{
				slotSettings[slot].punch_mode = cc_value ? 1 : 0;
			}
			break;
			
		}
	} else if ((cc_number == SEQ_ENABLE_CC)){
		if (init){
			// Set sequencer to traktor mode
			seq_traktor_mode = true;
			
			// Force into SEQ mode
			set_op_mode(sequencer);
			// Init sequencer display
			init_seq_display();
			init = false;
			// Force initial sync
			sync_clock();}
	} else if ((cc_number == SEQ_PLAY_CC)){
		if (cc_value){
			// Start playback
			if(seq_state == DISABLED){
				
				seq_state = WAIT_FOR_SYNC;
				
				
				/*
				// Wait for a stable clock
				if(get_counts_per_tick())
				seq_is_in_playback = true;
				start_pattern = true;
				
				// If the trigger is late make sure we 
				// start on the next step
				if (getTickCount() < 12){
					rythmIndex = 4;
				}*/
			}
		} else {
			// Stop playback
			seq_state = DISABLED;
			rythmIndex = 0;
			start_pattern = false;
		}
	}
}


// Returns a row control type given an encoder index
// added to make the sequencer code easier to read
rowTypes_t is_row_type(uint8_t idx)
{
	if((idx >= 0) && (idx <=3)){
		return clipSelection;	
	} else if ((idx >= 4) && (idx <=7)){
		return patternSelection;
	} else if((idx >= 8) && (idx <=11)){
		return volumeAdjust;
	} else if ((idx >= 12) && (idx <=15)){
		return filterAdjust;
	}	
	return false;
}

