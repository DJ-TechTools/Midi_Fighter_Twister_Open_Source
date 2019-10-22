/*
 * sequencer_input.c
 *
 * Created: 10/14/2013 10:26:55 AM
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

#include "sequencer_input.h"

bool shift_active = false;

uint16_t button_down_counter[4];
int8_t   mod_level[4]; 

uint8_t active_memory_slot = 17;

bool safe_to_write = false;

uint8_t swing_amount  = 0;
bool    swing_enabled = 0;

void process_sequencer_input(void)
{
	int16_t new_value;
	uint16_t bit = 0x0001;
	
	int8_t detent_size = 10;

	//static uint16_t prev_seq_switch_state = 0;
	//uint16_t seq_switch_state = update_encoder_switch_state();
	update_encoder_switch_state();
	
	for (uint8_t i=0;i<16;i++) {
		
		// First we check for movement on each encoder
		new_value = get_encoder_value(i);
		
		if (new_value) {

			// The filter row in the default view uses de-tents
			if ((i>11) && (i < 16) && (sequencerDisplayState == DEFAULT) && (encoder_is_in_detent(sequencerRawValue[i]))) {
				seq_detent_counter[i-12] += new_value;
				if (seq_detent_counter[i-12] > detent_size){
					sequencerRawValue[i] = encoder_detent_limit_high;
					//sequencerRawValue[i] = 6500;
				} else if (seq_detent_counter[i-12] < -detent_size) {
					sequencerRawValue[i] = encoder_detent_limit_low;
					//sequencerRawValue[i] = 6200;
				}
			} else if (encoder_is_in_deadzone(sequencerRawValue[i])) {
				// The encoder is in a end zone, only change the encoder value once we have
				// moved out of the dead zone.
				#define dead_zone_size 3
				if( ((sequencerRawValue[i] < 100) && (new_value > 0)) ||
					((sequencerRawValue[i] > 12600) && (new_value < 0)) ) {
					seq_detent_counter[i] += new_value;
				}
				
				if ((seq_detent_counter[i] > dead_zone_size) &&  (sequencerRawValue[i] < 100)) {
					sequencerRawValue[i] = 100;
					seq_detent_counter[i] = 0;
				} else if ((seq_detent_counter[i] < -dead_zone_size) && (sequencerRawValue[i] > 12600)) {
					sequencerRawValue[i] = 12600;
					seq_detent_counter[i] = 0;
				}
			} else {
				// Update & clamp the raw value
				// The sound & pattern selectors have different sensitivities
				if ((i < 4) && (sequencerDisplayState == DEFAULT)){
					//sequencerRawValue[i] += new_value*(SeqIndicatorValue[12]*2);
					sequencerRawValue[i] += new_value*(80);
				} else if ((i < 8) && (sequencerDisplayState == DEFAULT)){
					//sequencerRawValue[i] += new_value*(SeqIndicatorValue[13]*2);
					sequencerRawValue[i] += new_value*(100);
				} else {
					sequencerRawValue[i] += new_value*178;
				}
				sequencerRawValue[i] = clamp_encoder_raw_value(sequencerRawValue[i]);
			}
			
			// Translate the raw value into a MIDI value
			uint8_t control_change_value = (uint8_t)scale_encoder_value(sequencerRawValue[i]);

			// Update Sequencer Internal Parameters
			if (sequencerDisplayState == DEFAULT) {
					// Update settings
					switch (is_row_type(i)) {
						case clipSelection: {
							set_slot_clip(i,(uint8_t)(control_change_value/11.55f));	
							SeqIndicatorValue[i]=control_change_value;	
						}
						break;
						case patternSelection: {
							if(button_down_counter[i-4] > 0){
								// Hack to stop the raw value changing during override
								sequencerRawValue[i] -= new_value*178;
								// Another hack which helps detect a press to enter pattern edit
								// vs a press and turn to activate the beat roll.
								button_down_counter[i-4] += abs(new_value)*100;
								mod_level[i-4]+= new_value;
								if (mod_level[i-4] > 39){mod_level[i-4] =39;}
							} else {
								slotSelectedBuffer[i-4] = (uint8_t)(control_change_value/11.55f);
								// Update pattern selection indicator
								SeqIndicatorValue[i]=control_change_value;	
							}
						}
						break;
						case volumeAdjust: {
							// Update slot volume adjustment & indicator
							slotSettings[i-8].volume = control_change_value;
							SeqIndicatorValue[i]=(uint8_t)control_change_value;
							//midi_stream_raw_cc(SEQ_CHANNEL, SEQ_FILTER_VALUE_OFFSET + i , control_change_value);
						}
						break;
						case filterAdjust: {
							// Update filter indicator value
							slotSettings[i-12].filter = control_change_value;
							SeqIndicatorValue[i] = control_change_value;
							midi_stream_raw_cc(SEQ_CHANNEL, SEQ_FILTER_VALUE_OFFSET + (i-12) , control_change_value);
						}
						break;
					}					
			} else if (sequencerDisplayState == PATTERN_EDIT) {
				set_step_state(selectedSlot, i,  control_change_value);
				set_encoder_indicator(i, control_change_value, false, BAR, 0);
			} else if (sequencerDisplayState == PATTERN_MEMORY){
				if (i == active_memory_slot){
					truncate_value = control_change_value;
				}
			} 
		}
		
		// Process Encoder Switch actions
		if(bit & get_enc_switch_down()){
			process_seq_enc_buttons(i, true);
		} else if (bit & get_enc_switch_up()){
			process_seq_enc_buttons(i, false);
		}
		
		//Keep track of how long the pattern buttons have been down
		if ((i >= 4) && (i <= 7)){
			if (bit & get_enc_switch_state()){
				button_down_counter[i-4]++;
			} else {
				button_down_counter[i-4] = 0;
				mod_level[i-4]=0;
			}
		}			
		bit<<=1;
	}
}

void process_seq_enc_buttons(uint8_t switch_idx, bool downpress)
{	
	switch (sequencerDisplayState){
		case OFF:{}
		break;
		case DEFAULT:{
			// In default mode each row of switches has a different type of function
			switch (is_row_type(switch_idx)) {
				case clipSelection: {
					if (downpress) {
					// Toggle FX
						slotSettings[switch_idx].fx_send = slotSettings[switch_idx].fx_send ? 0 : 1;
												
						midi_stream_raw_cc(SEQ_CHANNEL, SEQ_SETTINGS_OFFSET+ switch_idx+4, 
												slotSettings[switch_idx].fx_send * 127);
						
						build_default_display();		
					}
				}
				break;
				case patternSelection: {
					// Enable pattern editing for that slot
					if (!downpress) {
						if(button_down_counter[switch_idx-4] < 2000)
						{
							selectedSlot = switch_idx-4;
							sequencerDisplayState = PATTERN_EDIT;
						} else if (button_down_counter[switch_idx-4]){
							build_default_display();
							truncate_value = 127;
						}
					}/* else {
						truncate_value = (uint8_t)(sequencerRawValue[switch_idx]/100);
					}*/
				}
				break;
				case volumeAdjust: {
					// Toggle the mute state and send the new state via MIDI
					if (downpress) {
						if (slotSettings[switch_idx-8].mute_on){
							midi_stream_raw_cc(SEQ_CHANNEL, SEQ_MUTE_OFFSET+(switch_idx-8), 127);
						} else {
							midi_stream_raw_cc(SEQ_CHANNEL, SEQ_MUTE_OFFSET+(switch_idx-8), 63);
						} 
					}
				}
				break;
				case filterAdjust: {
					// Send the filter on/off toggle midi out
					if (downpress) {
					
						if (slotSettings[switch_idx-12].filter_on){
							midi_stream_raw_cc(SEQ_CHANNEL, SEQ_FILTER_ON_OFFSET+(switch_idx-12), 63);
						} else {
							midi_stream_raw_cc(SEQ_CHANNEL, SEQ_FILTER_ON_OFFSET+(switch_idx-12), 127);
						}
					}
				}
				break;
			}
		}
		break;
		case PATTERN_MEMORY:{
			// In shift mode we simply a midi out message, we rely on midi input from Traktor to actually
			// change the control state.
			if(downpress){
				if (shift_active){
					// Shift + Encoder Button deletes that stored pattern
					delete_pattern_memory(switch_idx);
					if(active_memory_slot == switch_idx){
						active_memory_slot = 17;
					}
				} else {
					if(get_memory_slot_states() & (0x0001 << switch_idx)){
						// If the slot is full load the data
						load_pattern_memory(switch_idx);
						active_memory_slot = switch_idx;
						truncate_value = (uint8_t)(sequencerRawValue[switch_idx]/100);
					} else {
						// Otherwise save the current patterns to that slot
						save_pattern_memory(switch_idx);
						active_memory_slot = switch_idx;
						sequencerRawValue[switch_idx] = 12700;
						truncate_value = 127;
					}
				}
				build_pattern_memory_display();
			}
		}
		break;
		case PATTERN_EDIT:{	
			// In pattern edit mode the switch toggles the trigger state for that step	
			if (downpress) {
				if(!get_step_state(selectedSlot, switch_idx)){
					set_encoder_indicator(switch_idx, 127, false, BAR, 0);
					sequencerRawValue[switch_idx] = 12700;
					set_step_state(selectedSlot, switch_idx, 127);
				} else {
					set_encoder_indicator(switch_idx, 0, false, BAR, 0);
					sequencerRawValue[switch_idx] = 0;
					set_step_state(selectedSlot, switch_idx, 0);
				}
			}
		}
		break;
		
	}
}


// TODO update to use new side button read method
void  process_seq_side_buttons(void)
{
	if(seq_traktor_mode && sequencerDisplayState == OFF) {
		return;
	}	
	
	static uint8_t prev_seq_side_sw_state = 0;
	
	uint8_t bit = 0x01;
	uint8_t seq_side_sw_state = update_side_switch_state();
	
	for(uint8_t i = 0; i <6;++i) {
		
		if((seq_side_sw_state & bit) != (prev_seq_side_sw_state & bit)) {

			// The switch state changed so check if it went up or down
			bool sw_down;
			if (!(prev_seq_side_sw_state & bit)) {
				sw_down = true;
				} else {
				sw_down = false;
			}
			
			// Buttons 0 & 3 (LHS Button 1 & RHS Button 1) have the same action in ALL sequencer states
			switch (i){
				case 0:{
					if (sw_down){
						// Set play position to 0 without stopping playback
						reset_sequence();
					}
				}
				break;
				case 1:{
					switch (sequencerDisplayState){
						// Do alternate actions if in default State
						case DEFAULT:{
							do_alternate_functions(sw_down);
						}
						break;
						//Go back to Default on in all other states
						default:{
							if (!sw_down){
								sequencerDisplayState = DEFAULT;
							}
						}
						break;
					}
				}
				break;
				case 2:{
					// Sync to Traktor Clock & phase
					sync_clock();
				}
				break;
				case 3:{
					if(sw_down){
						if(shift_active){
							// Not Used
						} else {
							// Toggle Playback
							play_or_stop_sequence();
						}
					}	
				}
				break;
				case 4:{
					switch (sequencerDisplayState){
						case DEFAULT:{
							do_alternate_functions(sw_down);	
						}
						break;
						default:{
							if (!sw_down){
								sequencerDisplayState = DEFAULT;
							}
						}
						break;
					}
				}
				break;
				case 5:{
					switch (sequencerDisplayState){
						case PATTERN_MEMORY:{
							if(sw_down){
								shift_active = true;
							} else {
								shift_active = false;
							}
						}
						break;
						default:{
							sequencerDisplayState = PATTERN_MEMORY;
						}
						break;
					}
				}	
				break;
			}
		}
		bit <<=1;
	}
	prev_seq_side_sw_state = seq_side_sw_state;			
}

/**
 * Scans EEPROM to check if pattern data has been saved for each of the 16 memory slots
 * Returns a 16 bit wide bit field of the boolean result. LSB = Slot 1 (Encoder 1)
 * 
 * \return 
 */

static uint16_t seq_memory_slot_state = 0;
#define FX_SLOT_FULL_FLAG  0xA5
#define FX_SLOT_FULL_FLAG_OFFSET 45		//Note: This is easily broken by adding or decreasing
									    // the number of bytes used to save a pattern
void pattern_memory_init(void)
{
	// Calculate Initial Offset
	uint16_t addr = (SEQ_EEPROM_START_PAGE * EEPROM_PAGE_SIZE) + FX_SLOT_FULL_FLAG_OFFSET;
	
	uint16_t bit = 0x0001;
	
	seq_memory_slot_state = 0x0000;
	
	// We check the FX_SLOT_FULL_FLAG location for the fx slot full flag value of 0xA5
	for(uint8_t i=0;i<16;++i){
		if(eeprom_read(addr) == FX_SLOT_FULL_FLAG){
			seq_memory_slot_state |= bit;
		}
		bit <<=1;
		addr+= 64;
	}
	
}

uint16_t get_memory_slot_states(void){
	return seq_memory_slot_state;
}

void delete_pattern_memory(uint8_t memory_slot)
{
	cpu_irq_disable();
	seq_memory_slot_state &= ~(0x0001 << memory_slot);
	uint16_t addr = (EEPROM_PAGE_SIZE * (SEQ_EEPROM_START_PAGE + (memory_slot * 2))) + FX_SLOT_FULL_FLAG_OFFSET;
	eeprom_write(addr, 0xFF);	
	cpu_irq_enable();
}

void save_pattern_memory(uint8_t memory_slot)
{
	
	wdt_disable();
	
	uint8_t page_buffer[EEPROM_PAGE_SIZE];
	
	memset(&page_buffer, 0, EEPROM_PAGE_SIZE);
	
	uint8_t* buffer_ptr = page_buffer;
	
	uint8_t page_index = SEQ_EEPROM_START_PAGE + (memory_slot * 2);
		
	// First we write the four patterns into the page buffer, we compress the pattern velocity information
	// to four bits to keep the pattern memory footprint minimal
	for(uint8_t i=0;i<4;++i){
		for(uint8_t j=0;j<8;++j){
			*buffer_ptr++ = (uint8_t)((0x0F & (get_step_state(i,(j*2)) >> 3)) << 4) | 
									  (0x0F & (get_step_state(i, (j*2)+1) >> 3));
		}
	}
	
	// Now the page buffer is full so we can write this page to EEPROM
	nvm_eeprom_load_page_to_buffer(page_buffer);
	nvm_eeprom_atomic_write_page(page_index);
	
	// Reset the buffer_ptr
	buffer_ptr = page_buffer;
	
	for(uint8_t i=0;i<4;++i){
		*buffer_ptr++ = get_slot_clip(i);
	}
	
	for(uint8_t i=0;i<4;++i){
		// The Slot Filter On & Filter Level Settings, boolean filter on state is stored in 8th bit, with the 7 bit
		// filter level stored in the lower 7.
		*buffer_ptr++ = (uint8_t)((0x80 & (slotSettings[i].filter_on << 7)) | slotSettings[i].filter);
		// The Slot Mute On & Volume Level Settings, boolean mute on state is stored in 8th bit, with the 7 bit
		// volume level stored in the lower 7.
		*buffer_ptr++ = (uint8_t)((0x80 & (slotSettings[i].mute_on << 7)) | slotSettings[i].volume); 
	
	}
	
	// Now store the FX settings, the FX Send boolean states are stored in the upper 4 bits and the FX selection
	// is stored in the lower four.
	uint8_t fx_flags = 0;
	uint8_t bit = 0x01;
	
	for(uint8_t i=0;i<4;++i){
		fx_flags |= slotSettings[i].fx_send ? bit : 0;
		bit <<= 1;
	}

	*buffer_ptr++ = (fx_flags & 0x0F);

	// Write the memory slot full flag
	*buffer_ptr++ = FX_SLOT_FULL_FLAG;

	// Write the memory slot full flag
	seq_memory_slot_state |= (0x0001 << memory_slot);
	
	// Increment the page_index for the second page write
	page_index++;
	
	// Now the page buffer is full so we can write this page to EEPROM
	nvm_eeprom_load_page_to_buffer(page_buffer);
	nvm_eeprom_atomic_write_page(page_index);
	
	wdt_enable();
}

void load_pattern_memory(uint8_t memory_slot)
{
	uint16_t addr = EEPROM_PAGE_SIZE * (SEQ_EEPROM_START_PAGE + (memory_slot * 2));
	
	// Read memory
	for(uint8_t i=0;i<4;++i){
		// Load the stored patterns
		// Memory is ALLWAYS loaded into pattern buffer 0
		slotSelectedBuffer[i] = 0;
		
		for(uint8_t j=0;j<8;++j){
			uint8_t data = eeprom_read(addr++);
			
			set_step_state(i, (j*2), ((data & 0xF0)>>4)*8);
			set_step_state(i, (j*2)+1, (data & 0x0F)*8);
		}
	}
	
	// Load the slot clip selections
	for(uint8_t i=0;i<4;++i){
		set_slot_clip(i,eeprom_read(addr++));
	}
	
	bool state;
	// Load the slot settings
	for(uint8_t i=0;i<4;++i){
		
		// Filter Amount and State
		slotSettings[i].filter  = eeprom_read(addr) & 0x7F;
		// Because mapping uses toggle control we only send MIDI if the state changed
		state = (eeprom_read(addr++) & 0x80) ? true : false;
		if(slotSettings[i].filter_on != state){
			midi_stream_raw_cc(SEQ_CHANNEL, SEQ_FILTER_ON_OFFSET+i, 127);
		}
		
		// Volume & Mute State
		slotSettings[i].volume  = eeprom_read(addr) & 0x7F;
		
		state = (eeprom_read(addr++) & 0x80) ? true : false;
		if (slotSettings[i].mute_on != state){
			midi_stream_raw_cc(SEQ_CHANNEL, SEQ_MUTE_OFFSET+i, 127);
		}
		
	}
	
	uint8_t fx_flags = eeprom_read(addr++);
	uint8_t bit = 0x01;
	
	for(uint8_t i=0;i<4;++i){
		state = (fx_flags & bit) ? 1 : 0;
		midi_stream_raw_cc(SEQ_CHANNEL, SEQ_SETTINGS_OFFSET+i+4, 127*state);
		bit <<=1;
	}
}

void do_alternate_functions(bool sw_down)
{
	// Activate Shift - in shift mode any changes to the sound are
	// undone once shift is released.
	if (sw_down){
		if(shift_active){
			set_op_mode(normal);
			refresh_display();
			} else {
			shift_active = true;
			for(uint8_t i=0;i<4;++i){
				// Store the selected sounds
				slot_memory[i].clip_selection = get_slot_clip(i);
				// Store the buffer selection for the four slots
				slot_memory[i].buffer_selection = slotSelectedBuffer[i];
				// Store the actual step states for the four slots
				for(uint8_t j=0;j<16;++j){
					slot_memory[i].step_states[j] = get_step_state(i,j);
				}
			}
		}
		} else {
		shift_active = false;
		for(uint8_t i=0;i<4;++i){
			// Revert to the previously selected clip
			set_slot_clip(i, slot_memory[i].clip_selection);
			slotSelectedBuffer[i] = slot_memory[i].buffer_selection;
			for(uint8_t j=0;j<16;++j){
				// Restore the stored patterns
				set_step_state(i, j, slot_memory[i].step_states[j]);
			}
		}
		// Rebuild the display
		build_default_display();
	}
}

//  Sends all MIDI controlled parameters to the Software
//  MIDI Interface is flush before to ensure it does not become overloaded by the 18 messages we are about to send
void push_all_parameters(void){
	
	MIDI_Device_Flush(g_midi_interface_info);
	
	for(uint8_t i=0;i<4;++i){
		midi_stream_raw_cc(SEQ_CHANNEL, SEQ_MUTE_OFFSET+i, 127 * slotSettings[i].mute_on);
		midi_stream_raw_cc(SEQ_CHANNEL, SEQ_FILTER_ON_OFFSET+i, 127 * slotSettings[i].filter_on);
		midi_stream_raw_cc(SEQ_CHANNEL, SEQ_SETTINGS_OFFSET + i+4, 127 * slotSettings[i].fx_send);
		midi_stream_raw_cc(SEQ_CHANNEL,   SEQ_FILTER_VALUE_OFFSET + i , slotSettings[i].filter);
	}
}

