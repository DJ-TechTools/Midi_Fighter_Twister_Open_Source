/*
 * encoders.c
 *
 * Created: 6/28/2013 1:48:45 PM
 *  Author: Michael
 *
 *  Encoder Management & Control 
 *  This takes care of interfacing between the hardware input (encoders & switches)
 *  and the MIDI output. Because updating the display is such a processor intensive
 *  activity we avoid updating the display unless it should have changed. This is 
 *  achieved by buffers which hold the previous state of all elements, Encoder Value,
 *  RGB color setting, RGB animation setting for the current bank. Each main loop cycle
 *  the prev buffer is compared with the MIDI buffer of the selected bank and any changes
 *  are added to the display update cue. 1 display update task is performed per main loop
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



#include <encoders.h>

// Constants
const uint16_t encoder_detent_limit_low = 6240; // 6240 - ok for all modes (6250 causes high res mode to hit '64' on the way up)
const uint16_t encoder_detent_limit_high = 6450; // 6450 - ok for all modes, (note: scale_encoder_value adds 50 to these "raw" values in velocity calc)

// Locals

uint8_t indicator_value_buffer[NUM_BANKS][16];	 // Holds the 7 bit indicator value

uint8_t switch_color_buffer[NUM_BANKS][16];      // Holds the switch color setting 

uint8_t switch_animation_buffer[NUM_BANKS][16];  // Holds the switch animation setting
uint8_t encoder_animation_buffer[NUM_BANKS][16];  // Holds the encoder animation setting

uint16_t enc_switch_midi_state[NUM_BANKS][16];	 // Holds the switch states

uint16_t enc_switch_toggle_state[NUM_BANKS];	 // 16-Encoders Per Bank/ 1-bit per Encoder
							// toggle state is not tied directly 
							// to the MIDI state we use this to track.
												 
uint16_t switch_color_overide[NUM_BANKS];		 // Bit field to track if color override
												 // was received for switch color
												 
uint16_t enc_indicator_overide[NUM_BANKS];       // Bit field to track if indicator override
												 // was received for indicator
												 
											
// Shift Mode Variables

uint16_t shift_mode_switch_state[2];			// Bit field to track the switch state for											    
uint16_t shift_mode_midi_override[2];		    // Bit field to track the midi state for the
												// two shift pages

// - this is necessary because some parameters are shared between 'shifted' and 'non-shifted' encoders.

int8_t encoder_detent_counter[PHYSICAL_ENCODERS]; 
//~ input_map_t input_map[BANKED_ENCODERS]; // !Summer2016Update: Removed in favor of expanding encoder_settings
int16_t  raw_encoder_value[VIRTUAL_ENCODERS]; // !Summer2016Update: Expanded to store values for all banks and shifted encoders
// - indexed by Virtual Encoder ID
// --- 0-15: Encoders Bank 1 (Unshifted)
// --- 16-31: Encoders Bank 2 (Unshifted)
// --- 32-47: Encoders Bank 3 (Unshifted)
// --- 48-63: Encoders Bank 4 (Unshifted)
// --- 64-79: Encoders Bank 1 (Shifted)
// --- 80-95: Encoders Bank 2 (Shifted)
// --- 96-111: Encoders Bank 3 (Shifted)
// --- 112-127: Encoders Bank 4 (Shifted)

static uint8_t encoder_bank = 0;
static int8_t g_detent_size;
static int8_t g_dead_zone_size;
//static encoder_config_t encoder_settings[PHYSICAL_ENCODERS];
encoder_config_t encoder_settings[BANKED_ENCODERS];
//static encoder_config_t encoder_settings_transfer_buffer[1];

// Private Functions
uint8_t get_virtual_encoder_id (uint8_t bank, uint8_t encoder_id);
void encoderConfig(encoder_config_t *settings);
bool switch_value_from_encoder_value_enabled(uint8_t banked_encoder_idx);
uint8_t adjust_switch_value_by_encoder_value(uint8_t banked_encoder_idx, uint8_t value, bool state);

void send_element_midi(enc_control_type_t type, uint8_t banked_encoder_index, uint8_t value, bool state);
void send_encoder_midi(uint8_t banked_encoder_idx, uint8_t value, bool state, bool shifted);
//void send_encoder_pitchbend(uint8_t banked_encoder_idx, uint16_t value, bool shifted);
uint8_t scale_encoder_value(int16_t value);
int16_t clamp_encoder_raw_value(int16_t value);
bool encoder_is_in_detent(int16_t value);
bool color_overide_active(uint8_t bank, uint8_t encoder);
// - Encoder Banks
bool encoder_maps_match(uint8_t this_banked_encoder_id, uint8_t that_banked_encoder_id);
void transfer_this_encoder_value_to_other_banks(uint8_t current_bank, uint8_t encoder_id);
void transfer_encoder_values_to_other_banks(uint8_t current_bank);
// - Animation Related
bool animation_is_encoder_indicator(uint8_t animation_value);
bool animation_is_switch_rgb(uint8_t animation_value);
bool animation_buffer_conflict_exists(uint8_t bank, uint8_t encoder);

#if VELOCITY_CALC_METHOD > VELOCITY_CALC_M_NONE 
	uint16_t convert_ticks_per_scan_to_value_multiplier(uint8_t tick_count, uint16_t cycles_count);
	void send_midi_velocity_sensitive_encoder(uint8_t encoder_id, uint16_t output_value);
#endif


/** 
 *  Initializes all encoder buffers and EEPROM settings
**/

uint8_t get_virtual_encoder_id (uint8_t bank, uint8_t encoder_id){
	uint8_t virtual_encoder_id = encoder_id + bank*16;
	uint8_t this_bank = bank;
	if (encoder_is_in_shift_state(this_bank, encoder_id))
		{virtual_encoder_id += 64;}
	return virtual_encoder_id;
}

// encoder_maps_match: Compares MIDI Mapping Parameters common to both 'shift' and 'unshifted' states.
// - returns true if the mappings match, otherwise returns false.
bool encoder_maps_match(uint8_t this_banked_encoder_id, uint8_t that_banked_encoder_id) {
	if (encoder_settings[this_banked_encoder_id].encoder_midi_number != encoder_settings[that_banked_encoder_id].encoder_midi_number)
		{ return false; }
	if (encoder_settings[this_banked_encoder_id].encoder_midi_type != encoder_settings[that_banked_encoder_id].encoder_midi_type)
		{ return false; }
	if (encoder_settings[this_banked_encoder_id].encoder_midi_type == SEND_REL_ENC || encoder_settings[this_banked_encoder_id].encoder_midi_type == SEND_REL_ENC_MOUSE_EMU_DRAG || encoder_settings[this_banked_encoder_id].encoder_midi_type == SEND_REL_ENC_MOUSE_EMU_SCROLL) // Don't Transfer values for relative encoders
		{ return false; }
	return true;
}

void transfer_this_encoder_value_to_other_banks(uint8_t current_bank, uint8_t encoder_id){
	uint8_t this_banked_encoder_id = current_bank * PHYSICAL_ENCODERS + encoder_id;
	for (uint8_t that_bank = 0; that_bank < NUM_BANKS; that_bank++) {
		wdt_reset(); // reset the watchdog timer, to keep controller from resetting while processing these updates
		if (that_bank == current_bank) { continue; } // We don't allow this feature within the same bank (too much processing for a frivolous feature)
			
		for (uint8_t that_encoder = 0; that_encoder < PHYSICAL_ENCODERS; that_encoder++) {
			uint8_t that_banked_encoder_id = that_bank*PHYSICAL_ENCODERS + that_encoder;
			if (encoder_maps_match(this_banked_encoder_id, that_banked_encoder_id)) {
				// Mappings Match! (only midi channel needs checked now)
				bool current_shift_state = encoder_is_in_shift_state(that_bank, that_encoder); 
				// - Check the non-shifted channel
				if (encoder_settings[this_banked_encoder_id].encoder_midi_channel == encoder_settings[that_banked_encoder_id].encoder_midi_channel) {
					// Transfer the Value
					raw_encoder_value[that_banked_encoder_id] = raw_encoder_value[this_banked_encoder_id];
					// Update Display if applicable				
					if (!current_shift_state) { // If Value is currently on display, update the display
						indicator_value_buffer[that_bank][that_encoder] = indicator_value_buffer[current_bank][encoder_id];
					}
				}
				// - Check the shifted channel
				if (encoder_settings[this_banked_encoder_id].encoder_shift_midi_channel == encoder_settings[that_banked_encoder_id].encoder_shift_midi_channel) {
					// Transfer the Value (note: must translate banked_encoder_id to virtual_encoder_id)
					raw_encoder_value[that_banked_encoder_id+BANKED_ENCODERS] = raw_encoder_value[this_banked_encoder_id+BANKED_ENCODERS];
					// Update Display if applicable				
					if (current_shift_state) { // If Value is currently on display, update the display
						indicator_value_buffer[that_bank][that_encoder] = indicator_value_buffer[current_bank][encoder_id];
					}
				
				}
				
			}
		}
	}
}

void transfer_encoder_values_to_other_banks(uint8_t current_bank){
	
	for (uint8_t this_encoder = 0; this_encoder < PHYSICAL_ENCODERS; this_encoder++){
		transfer_this_encoder_value_to_other_banks(current_bank, this_encoder);
	}
}

// !Summer2016Update: Removed input_map in favor of expanding encoder_settings table
//~ void sync_input_map_to_output_map(uint8_t encoder_id)  // Accepted Encoder IDs are: 0-63 (16-encoders per each of the 4-banks)
//~ {
	//~ uint16_t addr = (ENC_SETTINGS_START_PAGE * 32);	// !Summer2016Update mark: eeprom address of Encoder Settings //of input_map
	//~ addr+= ENC_EE_SIZE*encoder_id;
	//~ input_map[encoder_id].sw_midi_channel  = (eeprom_read(addr)>> 4) & 0x0F;
	//~ input_map[encoder_id].sw_midi_number   =  eeprom_read(addr+1) & 0x7F;
	//~ input_map[encoder_id].ind_midi_channel = (eeprom_read(addr+6)>> 4) & 0x0F;
	//~ input_map[encoder_id].ind_midi_number  =  eeprom_read(addr+7) & 0x7F;
	//~ input_map[encoder_id].ind_shifted_midi_channel = (eeprom_read(addr+5)>> 4) & 0x0F;
//~ }

void encoders_init(void)
{
	// Read in all the encoder settings for all banks
	// - in to the encoder_settings RAM Table
	//for(uint8_t i=0;i<16;++i){
	for(uint8_t i=0;i<BANKED_ENCODERS;++i){
		uint8_t this_bank = i/16;
		uint8_t this_phys_encoder = i%16;
		get_encoder_config(this_bank, this_phys_encoder, &encoder_settings[i]);
		//get_encoder_config(encoder_bank, i, &encoder_settings[i]);
	}
	
	// Build all encoder color state buffer banks
	// To do this efficiently we read the values directly from
	// EEPROM rather than reading the entire config structure
	// Load encoder settings for the currently selected bank
	
	uint16_t addr = (ENC_SETTINGS_START_PAGE * 32) + EE_INACTIVE_COLOR_OFFSET;

	for (uint8_t i = 0; i<NUM_BANKS;++i){
		for(uint8_t j=0;j<16;++j){
			switch_color_buffer[i][j] = eeprom_read(addr);
			addr+= ENC_EE_SIZE;
		}
	}
	
	// Initialize raw encoder values for Bank 1
	
	// Initialize Per-Bank related variables
	for (uint8_t i = 0; i<NUM_BANKS;++i){ 
		// !Summer2016Update: Moved this encoder_value init, so it initializes ALL values.
		enc_switch_toggle_state[i] = 0x0000;
	}	 
	 
	// Initialize Per-Physical Encoder related variables
	for (uint8_t i = 0; i<PHYSICAL_ENCODERS;++i){ 
		encoder_detent_counter[i] = 0;
		enc_switch_toggle_state[encoder_bank] = 0;
	}

	// Initialize Per-Banked Encoder related variables 
	for (uint8_t i = 0; i<BANKED_ENCODERS;++i){ 
		if (encoder_settings[i].has_detent) {
			raw_encoder_value[i] = 6300;
			raw_encoder_value[i+BANKED_ENCODERS] = 6300; // Also set value of the 'shifted encoder')
		} else {
			raw_encoder_value[i] = 0;
			raw_encoder_value[i+BANKED_ENCODERS] = 0; // Also set the value of the 'shifted encoder')
		}
	}
	
	// Initialize MIDI input map for all encoders	
	// !Summer2016Update mark: sync_input_map_to_output_map 
	// - (added and then removed in favor of expanding encoder_settings)
	//~ for (uint8_t i=0;i<(NUM_BANKS*16);++i){
		//~ sync_input_map_to_output_map(i);
	//~ }
		
	// Set the de-tent size	TODO THIS SHOULD BE A SETTING
	g_detent_size = 8;	
	g_dead_zone_size = 2;	
}

/**
 *	Reads the configuration data for a given encoder and writes it to the table
 *  specified by cfg_ptr. See header file for a map of the EEPROM layout for 
 *	encoder settings
 * 
 * \param [in] bank		The bank of the encoder we are getting the settings for
 *
 * \param [in] encoder	The index of encoder we are getting the settings for
 *
 * \param [in] cfg_ptr	Pointer to the table to load the settings into
 */

void get_encoder_config(uint8_t bank, uint8_t encoder, encoder_config_t *cfg_ptr)
{
	uint16_t addr = (ENC_SETTINGS_START_PAGE * 32) + (bank * 128) + (encoder * 8);
	
	uint8_t buffer[8];
	
	// We need to disable interrupts while interacting with the EEPROM drivers
	cpu_irq_disable();
	nvm_eeprom_read_buffer(addr, buffer, 8);
	cpu_irq_enable();
	
	// Expand compressed settings
	cfg_ptr->switch_action_type		= buffer[0] & 0x0F;
	cfg_ptr->switch_midi_type		= 0;//(buffer[0] >> 1) & 0x01;
	cfg_ptr->switch_midi_channel	= (buffer[0] >> 4) & 0x0F;
	cfg_ptr->switch_midi_number		= buffer[1] & 0x7F;
	cfg_ptr->active_color			= buffer[2];
	cfg_ptr->inactive_color			= buffer[3];
	cfg_ptr->detent_color			= buffer[4] & 0x7F;
	cfg_ptr->has_detent				= (buffer[4] >> 7) & 0x01;
	cfg_ptr->indicator_display_type = buffer[5] & 0x03;
	cfg_ptr->movement				= (buffer[5] >> 2) & 0x03;
	cfg_ptr->encoder_shift_midi_channel = (buffer[5] >> 4) & 0x0F; // !Summer2016Update: Shifted Encoder MIDI Channel
	cfg_ptr->encoder_midi_type		= buffer[6] & 0x07; // !Spring2019Update: Added Switch Velocity Control and Mouse Emulation
	cfg_ptr->encoder_midi_channel   = (buffer[6] >> 4) & 0x0F;
	cfg_ptr->encoder_midi_number	= buffer[7] & 0x7F;
	cfg_ptr->is_super_knob          = (buffer[7] >> 7) & 0x01;
}

// !review: this may not be correct
// !revision: less overhead for animations, by removing eeprom reads
//void get_encoder_config_locally(uint8_t bank, uint8_t encoder, encoder_config_t *cfg_ptr) {
	//uint8_t virtual_encoder_id = get_virtual_encoder_id(bank, encoder);
	//uint8_t banked_encoder_id = virtual_encoder_id & BANKED_ENCODER_MASK;
	//cfg_ptr = &encoder_settings[banked_encoder_id];
//}

/**
 * Takes a table of new configuration data for a given encoder and saves all 
 * valid settings to EEPROM. Valid settings values are 0 - 127. Settings with
 * value above 127 will be ignored. When passing configuration data to this 
 * function any setting which is not being updated must have its value set to
 * 0x80 or above, otherwise it will be overwritten. See header file for a map
 * of the EEPROM layout of encoder settings
 * 
 * \param bank [in]		The bank of the encoder
 *
 * \param encoder [in]	The index of the encoder
 *
 * \param encoder [in]	The tag table containing the new settings
 * 
 */
void save_encoder_config(uint8_t bank, uint8_t encoder, encoder_config_t *cfg_ptr)
{	
	// Record the new setting to RAM first
	uint8_t virtual_encoder_id = get_virtual_encoder_id (bank, encoder);
	encoder_settings[virtual_encoder_id] = *cfg_ptr;
	
	// Create a tempory page_buffer;
	uint8_t page_buffer[EEPROM_PAGE_SIZE];
	
	// Each page contains setting for four encoders, so to avoid overwriting the
	// data for the other 3 encoders we have to read in their setting first.
	uint16_t page_address = (ENC_SETTINGS_START_PAGE * 32) + (bank * 128) + ((encoder/4)*32);
	nvm_eeprom_read_buffer(page_address, page_buffer, EEPROM_PAGE_SIZE);
	
	uint8_t* buffer_ptr = page_buffer;
	buffer_ptr += (8 * (encoder % 4));
	
	// Switch Action, Switch MIDI type & Switch MIDI channel are saved
	// in the first byte
	if (cfg_ptr->switch_action_type < 0x80){
		*buffer_ptr &= ~0x0F; 
		*buffer_ptr |= cfg_ptr->switch_action_type & 0x0F;
	}

	if (cfg_ptr->switch_midi_channel < 0x80){
		*buffer_ptr &= ~0xF0;
		*buffer_ptr |= (0xF0 & ((cfg_ptr->switch_midi_channel - 1) << 4));
	}
	buffer_ptr++;  // Full
	
	// Switch MIDI number is saved in the second byte
	if (cfg_ptr->switch_midi_number < 0x80){
		*buffer_ptr = cfg_ptr->switch_midi_number;
	}
	buffer_ptr++;  // 1-bit open
	
	// Active and Inactive colors are saved in the third and fourth bytes	
	if (cfg_ptr->active_color < 0x80){
		*buffer_ptr = cfg_ptr->active_color;
	}
	buffer_ptr++;  // 1-bit open
		
	if (cfg_ptr->inactive_color < 0x80){
		*buffer_ptr = cfg_ptr->inactive_color;
	}
	buffer_ptr++;  // 1-bit open
	
	// Has de-tent and de-tent color are saved in the 5th byte
	if (cfg_ptr->has_detent < 0x80){
		*buffer_ptr &= ~0x80;
		*buffer_ptr |= (0x80 & (cfg_ptr->has_detent << 7));
	}	
	if (cfg_ptr->detent_color < 0x80){
		*buffer_ptr &= ~0x7F;
		*buffer_ptr |=  (0x7F & cfg_ptr->detent_color);
	}
	buffer_ptr++; // Full
	
	// Encoder Indicator & Movement type are save in the 6th byte
	if (cfg_ptr->indicator_display_type < 0x80){
		*buffer_ptr &= ~0x03;
		*buffer_ptr |= (0x03 & cfg_ptr->indicator_display_type);
	}	
	if (cfg_ptr->movement < 0x80){
		*buffer_ptr &= ~0x0C;
		*buffer_ptr |= (0x0C & (cfg_ptr->movement << 2));
	}
	if (cfg_ptr->encoder_shift_midi_channel < 0x80){ // !Summer2016Update shifted encoders midi channel
		*buffer_ptr &= ~0xF0;
		*buffer_ptr |= (0xF0 & (cfg_ptr->encoder_shift_midi_channel << 4));
	}
	buffer_ptr++; // Full
	
	// Encoder MIDI Type & MIDI channel are saved in the 7th byte
	if (cfg_ptr->encoder_midi_type < 0x80){
		*buffer_ptr &= ~0x07; // !Spring2019Update: Added Switch Velocity Control and Mouse Emulation
		*buffer_ptr |= (0x07 & cfg_ptr->encoder_midi_type); // !Spring2019Update: Added Switch Velocity Control and Mouse Emulation
	}
	if (cfg_ptr->encoder_midi_channel < 0x80){
		*buffer_ptr &= ~0xF0;
		*buffer_ptr |= (0xF0 & ((cfg_ptr->encoder_midi_channel - 1) << 4));
	}
	buffer_ptr++;	// 2-bits Open 0xC0
	
	// Encoder MIDI number & is super knob flag are saved in the 8th byte
	if (cfg_ptr->encoder_midi_number < 0x80){
		*buffer_ptr &= ~0x7F;
		*buffer_ptr |= cfg_ptr->encoder_midi_number;
	}
	if (cfg_ptr->is_super_knob < 0x80){
		*buffer_ptr &= ~0x80;
		*buffer_ptr |= (0x80 & (cfg_ptr->is_super_knob << 7));
	}
	buffer_ptr++;  // Full
	
	uint8_t page_index = ENC_SETTINGS_START_PAGE + (4 * bank) + (encoder / 4);
	
	// Save the updated page buffer back to EEPROM
	cpu_irq_disable();
	nvm_eeprom_load_page_to_buffer(page_buffer);
	nvm_eeprom_atomic_write_page(page_index);
	cpu_irq_enable();

	// !Summer2016Update: Match the newly changed input_map to match the output_map saved in eeprom
	// - removed in favor of expanding encoder_map
	//~ sync_input_map_to_output_map(bank*16 + encoder); // !Summer2016Update: midi_input_map bugfix
}

/**
 * Returns all encoder configuration data to the factory defaults  
**/
void factory_reset_encoder_config(void)
{
	// Create a table of the default settings common to all encoders
	encoder_config_t enc_default;
			
	enc_default.has_detent = DEF_ENC_DETENT;
	enc_default.detent_color = DEF_DETENT_COLOR;
	enc_default.active_color = DEF_ACTIVE_COLOR;      // !Summer2016Update: Overwritten later in this function by active_colors[]
	enc_default.inactive_color = DEF_INACTIVE_COLOR;  // !Summer2016Update: Overwritten later in this function by inactive_colors[]
	enc_default.movement = DEF_ENC_MOVEMENT;
	enc_default.indicator_display_type = DEF_INDICATOR_TYPE;
	enc_default.switch_action_type = DEF_SW_ACTION;
	enc_default.switch_midi_channel = DEF_SW_CH;
	enc_default.encoder_midi_channel = DEF_ENC_CH;
	enc_default.encoder_midi_type = DEF_ENC_MIDI_TYPE;
	enc_default.is_super_knob = DEF_IS_SUPER_KNOB;
	enc_default.encoder_midi_number = 0;
	enc_default.switch_midi_number = 0;
	enc_default.encoder_shift_midi_channel = DEF_ENC_SHIFT_CH; // !Summer2016Update: Shifted Encoders MIDI Channel
	 
	// !Summer2016Update active/inactive colors modified to be fixed per bank
	uint8_t active_colors[NUM_BANKS] = {DEF_ACTIVE_COLOR_BANK1, DEF_ACTIVE_COLOR_BANK2, DEF_ACTIVE_COLOR_BANK3, DEF_ACTIVE_COLOR_BANK4};
	uint8_t inactive_colors[NUM_BANKS] = {DEF_INACTIVE_COLOR_BANK1, DEF_INACTIVE_COLOR_BANK2, DEF_INACTIVE_COLOR_BANK3, DEF_INACTIVE_COLOR_BANK4};

	/* We then fill a page buffer with the compressed settings for 4 encoders
	   (each encoder has 8 bytes of data allocated for its setting) */
	uint8_t page_buffer[EEPROM_PAGE_SIZE];
	memset(&page_buffer, 0, EEPROM_PAGE_SIZE);
	
	uint8_t* buffer_ptr = page_buffer;
	
	uint8_t data_byte = 0;
	
	//uint8_t bit;
	
	// Set up a Template Memory Page - (One Column of 4-Encoders) - with the default settings
	// - buffer_ptr will be the actual table passed to the EEPROM Write Routine
	// - It will be modded for each group of four encoders
	for(uint8_t i=0;i<4;++i) {
		// Switch Settings are saved in the first and second bytes
		data_byte  = (enc_default.switch_action_type & 0x0F);
		data_byte |= (0xF0 & (enc_default.switch_midi_channel << 4));		
		
		*buffer_ptr++ = data_byte;
		
		*buffer_ptr++ = enc_default.switch_midi_number+i;
		
		// Active and Inactive colors are saved in the third and fourth bytes
		// *buffer_ptr++ = enc_default.active_color+(i*2);
		// *buffer_ptr++ = enc_default.inactive_color+(i*2);
		// *buffer_ptr++ = enc_default.active_color;
		// *buffer_ptr++ = enc_default.inactive_color;
		*buffer_ptr++ = 127;// active_colors[0];
		*buffer_ptr++ = 127;// inactive_colors[0];
		
		// Has de-tent and de-tent color are saved in the 5th byte
		*buffer_ptr++ =  (0x80 & (enc_default.has_detent << 7)) | 
						  enc_default.detent_color;
		
		// Encoder Indicator & Movement type are save in the 6th byte (and now encoder_shift_midi_channel)
		data_byte  = (0x03 & enc_default.indicator_display_type);
		data_byte |= (0x0C & (enc_default.movement << 2));
		data_byte |= (0xF0 & (enc_default.encoder_shift_midi_channel << 4)); // !Summer2016Update: shifted encoders midi channel
		*buffer_ptr++ = data_byte;
		
		// Encoder MIDI Type & MIDI channel are saved in the 7th byte
		data_byte  = (0x07 & enc_default.encoder_midi_type); // !Spring2019Update: Switch Velocity Control and Mouse Emulation
		data_byte |= (0xF0 & (enc_default.encoder_midi_channel << 4));
		*buffer_ptr++ = data_byte;
		
		// Encoder MIDI number is saved in the 8th byte
		data_byte = (0x7f & (enc_default.encoder_midi_number+i));
		data_byte |= (0x80 & (enc_default.is_super_knob << 7));
		*buffer_ptr++ = data_byte;
		
	}
	
	uint8_t page_index = ENC_SETTINGS_START_PAGE;
	
	// For Each Memory Page
	// - Modify the Template, and write the page
	for(uint8_t i=0;i<4*NUM_BANKS;++i){		// 4*NUM_BANKS, aka. NUM_ROWS_PER_BANK*NUM_BANKS 
		// for each Row in each Bank modify the default settings
		/* We now save the buffer to EEPROM, then increment the settings 
		   which are unique to each encoder ie MIDI number/pitch and repeat 
		   until we have saved all 64 encoders data */
		
		// !Summer2016Update: Modify Default On/Off colors by bank rather than per encoder.
		uint8_t color_index = i >> 2;
		uint8_t active_color = active_colors[color_index];  // Changes with each Bank
		uint8_t inactive_color = inactive_colors[color_index]; // Changes with each bank
		for(uint8_t j=0;j<4;++j){  // For each Data Page (Corresponds to 1 - Horizontal Row of Encoders)
			page_buffer[(j*8)+2] = active_color; // Adjust Active Color  (All Rows should be the same for THIS BANK)
			page_buffer[(j*8)+3] = inactive_color; // Adjust Inactive Color (ALL Rows should be the same for THIS BANK)
			// 8 = Settings Size
		}

		// Write the Page
		cpu_irq_disable();
		nvm_eeprom_load_page_to_buffer(page_buffer);
		nvm_eeprom_atomic_write_page(page_index);
		cpu_irq_enable();
		
		// Modify Mapping Parameters for the next item.
		for(uint8_t j=0;j<4;++j){  // For each Data Page (Corresponds to 1 - Horizontal Row of Encoders)
			page_buffer[(j*8)+1] += 4; //Increment SW MIDI Pitch
			page_buffer[(j*8)+7] += 4; //Increment ENC MIDI Number			
		}
		page_index++;
	}
}
//void adjust_
#if VELOCITY_CALC_METHOD > VELOCITY_CALC_M_NONE 

uint16_t convert_ticks_per_scan_to_value_multiplier(uint8_t tick_count, uint16_t cycles_count) {
	float multiplier = velocity_calc_slope*(float)(tick_count)/(float)(cycles_count) + velocity_calc_offset;
	if (multiplier > VELOCITY_CALC_MAX_MULTIPLIER) {
		multiplier = VELOCITY_CALC_MAX_MULTIPLIER;
	} else if (multiplier < VELOCITY_CALC_MIN_MULTIPLIER) {
		multiplier = VELOCITY_CALC_MIN_MULTIPLIER;
	}
	return (uint16_t)(multiplier);
}

void send_midi_velocity_sensitive_encoder(uint8_t encoder_id, uint16_t output_value) { // DEPRECATED?
	#if VELOCITY_CALC_METHOD == VELOCITY_CALC_M_TPS_BLOCKS
		uint8_t midi_channel = 0x0F;
		uint16_t multiplier = convert_ticks_per_scan_to_value_multiplier(1, output_value+1); // !review: '1' no guarantee only 1 tick
		midi_stream_raw_pitchbend(midi_channel, multiplier);
	#endif
}
#endif


// ======================== Process Encoder Input Functions======================
/**
 * Contains the main encoder task, this checks encoder hardware for change
 * then translates this change into a MIDI value based on the encoders
 * settings, if the MIDI value changes then the updated value is sent
 * and saved into the MIDI state buffer.
 */                                                                
void process_encoder_input(void)  // MIDI Output: Digital Inputs -> Encoders (Output)
{
	uint16_t bit = 0x0001;
	// Update the current encoder switch states
	update_encoder_switch_state();
	
	for (uint8_t i=0;i<16;i++) {
		uint8_t virtual_encoder_id = get_virtual_encoder_id(encoder_bank, i);
		uint8_t banked_encoder_id = virtual_encoder_id & BANKED_ENCODER_MASK;
		process_encoder_input_rotary(i, virtual_encoder_id, banked_encoder_id, bit);  // First we check for movement on each encoder
		process_encoder_input_switch(i, virtual_encoder_id, banked_encoder_id, bit); // Then we check the movement of each switch
		bit <<= 1;
	}
}
void process_encoder_input_rotary(uint8_t i, uint8_t virtual_encoder_id, uint8_t banked_encoder_id, uint16_t bit) { // i = encoder_id
	int16_t new_value;
	#if VELOCITY_CALC_METHOD == VELOCITY_CALC_M_TPS_BLOCKS
	uint16_t cycle_count;
	#endif
	//uint8_t virtual_encoder_id, banked_encoder_id;

	#if VELOCITY_CALC_METHOD == VELOCITY_CALC_M_TPS_BLOCKS
	cpu_irq_disable(); // disable interrupts to ensure we don't lose any data by reading midstream.
	new_value = get_encoder_value(i);  // !review: int8 return is being assigned to int16
	cycle_count = get_encoder_cycle_count(i);
	cpu_irq_enable();
	#else
	new_value = get_encoder_value(i);  // !review: int8 return is being assigned to int16
	#endif

	if (new_value) { // if Encoder Has Moved
		// --- Detents and Deadzones
		#if VELOCITY_CALC_METHOD == VELOCITY_CALC_M_TPS_BLOCKS
		if (process_encoder_input_rotary_detent(i, virtual_encoder_id, banked_encoder_id, new_value, cycle_count)) { }
		#else
		if (process_encoder_input_rotary_detent(i, virtual_encoder_id, banked_encoder_id, new_value)) { }
		#endif
		// everything has been done in 'pei_detent', if that function returned true
		#if VELOCITY_CALC_METHOD == VELOCITY_CALC_M_TPS_BLOCKS
		else if (process_encoder_input_rotary_deadzone(i, virtual_encoder_id, banked_encoder_id, new_value, cycle_count)) {}
		#else
		else if (process_encoder_input_rotary_deadzone(i, virtual_encoder_id, banked_encoder_id, new_value)) {}
		#endif
		// everything has been done in 'pei_deadzone', if that function returned true
		else { // Encoder Event Occurred, and Encoder is not in a Detent or Deadzone.
			// TODO TIDY THIS ALL UP - NEED TO RETHINK FUNCTION TOPOGRAPHY FOR ENCODERS *?
			#if VELOCITY_CALC_METHOD == VELOCITY_CALC_M_TPS_BLOCKS
			if(process_encoder_input_rotary_relative(i, virtual_encoder_id, banked_encoder_id, new_value, bit, cycle_count)) {}
			// everything has been done in 'pei_relative', if that function returned true
			#else
			if(process_encoder_input_rotary_relative(i, virtual_encoder_id, banked_encoder_id, new_value, bit)) {}
			// everything has been done in 'pei_relative', if that function returned true
			#endif

			else { // The encoder is not relataive nor in any detent or end zone
				#if VELOCITY_CALC_METHOD == VELOCITY_CALC_M_TPS_BLOCKS
				process_encoder_input_rotary_absolute(i, virtual_encoder_id, banked_encoder_id, new_value, bit, cycle_count);
				// everything has been done in 'pei_absolute', if that function returned true
				#else
				process_encoder_input_rotary_absolute(i, virtual_encoder_id, banked_encoder_id, new_value, bit);
				// everything has been done in 'pei_absolute', if that function returned true
				#endif
			}
		}
	}

}

#if VELOCITY_CALC_METHOD == VELOCITY_CALC_M_TPS_BLOCKS
bool process_encoder_input_rotary_detent(uint8_t i, uint8_t virtual_encoder_id, uint8_t banked_encoder_id, int8_t new_value, uint16_t cycle_count)
#else
bool process_encoder_input_rotary_detent(uint8_t i, uint8_t virtual_encoder_id, uint8_t banked_encoder_id, int8_t new_value)
#endif
{
	if (!encoder_settings[banked_encoder_id].has_detent) {
		//midi_stream_raw_cc(9,99,1);
		return false;
	} else if (!encoder_is_in_detent(raw_encoder_value[virtual_encoder_id])) {
		//midi_stream_raw_cc(9,99,2);
		return false;
	}


	encoder_detent_counter[i] += new_value;
	if (encoder_detent_counter[i] > g_detent_size){ // Exiting detent (CW)
		//raw_encoder_value[virtual_encoder_id] = 6500;
		raw_encoder_value[virtual_encoder_id] = encoder_detent_limit_high;
		//midi_stream_raw_cc(9,99,11);
	} else if (encoder_detent_counter[i] < -g_detent_size) { // Exiting detent (CCW)
		//raw_encoder_value[virtual_encoder_id] = 6200;
		raw_encoder_value[virtual_encoder_id] = encoder_detent_limit_low;
		//midi_stream_raw_cc(9,99,12);
	} else { // Still In Detent
		//midi_stream_raw_cc(9,99,13);
		// Patch: Ensure the Detent LED is set even for higher resolution modes, like velocity sensitivity
		indicator_value_buffer[encoder_bank][i] = 63;
	}
	return true;
}

// - Rotary Sub Function
#if VELOCITY_CALC_METHOD == VELOCITY_CALC_M_TPS_BLOCKS
  bool process_encoder_input_rotary_deadzone(uint8_t i, uint8_t virtual_encoder_id, uint8_t banked_encoder_id, int8_t new_value, uint16_t cycle_count)
#else
  bool process_encoder_input_rotary_deadzone(uint8_t i, uint8_t virtual_encoder_id, uint8_t banked_encoder_id, int8_t new_value)
#endif
{
	uint16_t value = raw_encoder_value[virtual_encoder_id];
	if (encoder_settings[banked_encoder_id].encoder_midi_type == SEND_REL_ENC || encoder_settings[banked_encoder_id].encoder_midi_type == SEND_REL_ENC_MOUSE_EMU_DRAG || encoder_settings[banked_encoder_id].encoder_midi_type == SEND_REL_ENC_MOUSE_EMU_SCROLL) {
		return false;
	} else if (!encoder_is_in_deadzone(value)) {
		return false;
	}
	// The encoder is in a end zone, only change the encoder value
	//   once we have moved out of the dead zone.
	if( ((raw_encoder_value[virtual_encoder_id] < 100) && (new_value > 0)) ||
	((raw_encoder_value[virtual_encoder_id] > 12600) && (new_value < 0)) ) {
		
		encoder_detent_counter[i] += new_value;
	}
	// - Allow movement to get us out of the dead zone, if we've traveled enough.
	if ((encoder_detent_counter[i] > g_dead_zone_size) &&
	(raw_encoder_value[virtual_encoder_id] < 100)) { // CW
		
		raw_encoder_value[virtual_encoder_id] = 100;
		encoder_detent_counter[i] = 0;
		
	} else if ((encoder_detent_counter[i] < -g_dead_zone_size) &&
	(raw_encoder_value[virtual_encoder_id] > 12600)) {  // CCW
		
		raw_encoder_value[virtual_encoder_id] = 12600;
		encoder_detent_counter[i] = 0;
	}
	return true;
}


#if VELOCITY_CALC_METHOD == VELOCITY_CALC_M_TPS_BLOCKS
  bool process_encoder_input_rotary_relative(uint8_t i, uint8_t virtual_encoder_id, uint8_t banked_encoder_id, int8_t new_value, uint16_t bit, uint16_t cycle_count)
#else
  bool process_encoder_input_rotary_relative(uint8_t i, uint8_t virtual_encoder_id, uint8_t banked_encoder_id, int8_t new_value, uint16_t bit)
#endif
{
	if (!(encoder_settings[banked_encoder_id].encoder_midi_type == SEND_REL_ENC || encoder_settings[banked_encoder_id].encoder_midi_type == SEND_REL_ENC_MOUSE_EMU_DRAG || encoder_settings[banked_encoder_id].encoder_midi_type == SEND_REL_ENC_MOUSE_EMU_SCROLL)) {
		return false;
	}

	// Calculate Output Value 
	// new_value will be 1 or -1 if it gets here(though technically it could hold values higher)
	uint8_t output_value;// = 64 + new_value;	// Relative: Bin Offset
	// Case A: Relative Encoder Fine Adjustments
	if (encoder_settings[banked_encoder_id].switch_action_type == ENC_FINE_ADJUST && get_enc_switch_state() & bit) { // !Summer2016Update: Relative Encoder Fine Adjustment Added
		static int16_t relative_fine_value[PHYSICAL_ENCODERS] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};  // Counter
		relative_fine_value[i] += new_value;
		output_value = 64;
		while (relative_fine_value[i] >= 4)  { // Adjust for Clockwise Output -> 4=ENC_REL_FINE_LIMIT
			relative_fine_value[i] -= 4;
			output_value++;
		}
		while (relative_fine_value[i] <= -4)  { // Adjust for CounterClockwise Output -> 4=ENC_REL_FINE_LIMIT
			relative_fine_value[i] += 4;
			output_value--;
		}
	}
	else {	// Case B: Normal Relative Encoders
		#if VELOCITY_CALC_METHOD == VELOCITY_CALC_M_NONE
			output_value = 64 + new_value;	// Relative: Bin Offset
		#elif VELOCITY_CALC_METHOD == VELOCITY_CALC_M_TPS_BLOCKS
			if(encoder_settings[banked_encoder_id].movement == VELOCITY_SENSITIVE_ENC) {
				int8_t tick_count = new_value < 0 ? -1*new_value : new_value;
				//uint16_t cycle_count = encoder_event_cycle_counts[i];
				uint16_t base_multiplier = convert_ticks_per_scan_to_value_multiplier(tick_count , cycle_count);
				int16_t scaled_mult = 1 + ((base_multiplier >> 4) & 0x1F); // 0-16
				output_value = 64 + new_value*scaled_mult;	// Relative: Bin Offset
			} else {
				output_value = 64 + new_value;	// Relative: Bin Offset
			}
				
		#endif
	}

	//uint8_t output_value = (uint8_t)(new_value) & 0x7F;	// Relative: Two's Complement
	uint8_t midi_channel;
	uint8_t this_bank = encoder_bank;
	if (!encoder_is_in_shift_state(this_bank, i)){ // !Summer2016Update Shifted Encoders Midi Channel
		midi_channel = encoder_settings[banked_encoder_id].encoder_midi_channel;
	} else{
		midi_channel = encoder_settings[banked_encoder_id].encoder_shift_midi_channel;
		// !revision use the unused switch midi number: midi_number = encoder_settings[banked_encoder_id].switch_midi_number ? 20190806
		//midi_channel = encoder_settings[banked_encoder_id].switch_midi_channel;
	}
	// Send Message
	// Relative output does not need to call 'Send Encoder' as it just always outputs 'CC' 
	if (output_value != 64) { // !Summer2016Update: Relative Encoder Fine Adjustment Bugfix, disallow non-moving
		midi_stream_raw_cc(midi_channel, 
					encoder_settings[banked_encoder_id].encoder_midi_number, 
					output_value);	
	}
	return true;
}

#if VELOCITY_CALC_METHOD == VELOCITY_CALC_M_TPS_BLOCKS
  bool process_encoder_input_rotary_absolute(uint8_t i, uint8_t virtual_encoder_id, uint8_t banked_encoder_id, int8_t new_value, uint16_t bit, uint16_t cycle_count) {
	uint16_t base_multiplier;
	int16_t scaled_value;
	bool fine_adj = (encoder_settings[banked_encoder_id].switch_action_type == ENC_FINE_ADJUST && get_enc_switch_state() & bit);
	
	// Calculate Value adjustment
	if(encoder_settings[banked_encoder_id].movement == VELOCITY_SENSITIVE_ENC) { // Velocity Sensitive
		int8_t tick_count = new_value < 0 ? -1*new_value : new_value;
		base_multiplier = convert_ticks_per_scan_to_value_multiplier(tick_count , cycle_count);
		if (fine_adj || base_multiplier < ENCODER_VALUE_SCALAR_VELOCITY_MIN) { // Velocity Sensitive-Fine
			base_multiplier = ENCODER_VALUE_SCALAR_VELOCITY_MIN;  // TODO: 14-bit will have a min for 14-bit and 7-bit operations
		} else if (base_multiplier > ENCODER_VALUE_SCALAR_VELOCITY_MAX) {
			base_multiplier = ENCODER_VALUE_SCALAR_VELOCITY_MAX;  // TODO: 14-bit will have a min for 14-bit and 7-bit operations
		}
	}
	else if (fine_adj) { // Standard-Fine
		base_multiplier = ENCODER_VALUE_SCALAR_DIRECT_FINE; 
	}
	else if(encoder_settings[banked_encoder_id].movement == DIRECT) {  // Standard, 1 CC value per tick
		base_multiplier = ENCODER_VALUE_SCALAR_DIRECT;
	} else  {  // Quick Moving, 1.78 CC values per tick
		base_multiplier = ENCODER_VALUE_SCALAR_EMULATION; 
	}
	// Calculate Value to write into raw_encoder_value
	scaled_value = new_value*base_multiplier;

	// Update the Stored raw_encoder_value
	raw_encoder_value[virtual_encoder_id] += scaled_value;
	raw_encoder_value[virtual_encoder_id] = clamp_encoder_raw_value(raw_encoder_value[virtual_encoder_id]);
	// Translate the raw value into a MIDI value, send & save the new value
	uint8_t control_change_value = scale_encoder_value(raw_encoder_value[virtual_encoder_id]);
	//send_element_midi(SWITCH, i, control_change_value, true);
	uint8_t this_bank = encoder_bank;// banked_encoder_id >> 4; // !review !test this_bank
	bool shifted = encoder_is_in_shift_state(this_bank, i);
	send_encoder_midi(banked_encoder_id, control_change_value, true, shifted); // !Summer2016Update: Shifted Encoders now always output as Encoders
	indicator_value_buffer[encoder_bank][i]=control_change_value;
	return true;
  }
#else
  bool process_encoder_input_rotary_absolute(uint8_t i, uint8_t virtual_encoder_id, uint8_t banked_encoder_id, int8_t new_value, uint16_t bit) {
	int16_t scaled_value;
		
	if (encoder_settings[banked_encoder_id].switch_action_type == ENC_FINE_ADJUST &&
		get_enc_switch_state() & bit) {
		// Fine adjust sensitivity, 1 pulse = 1/4 a CC step	
		scaled_value = new_value*ENCODER_VALUE_SCALAR_DIRECT_FINE; 
		
	} else if(encoder_settings[banked_encoder_id].movement == DIRECT) {
			// Standard sensitivity, 1 pulse = 1 CC step
			scaled_value = new_value*ENCODER_VALUE_SCALAR_DIRECT; 
			
	} else  {
			// Emulated sensitivity, 270 degress rotation = 127 CC steps
			scaled_value = new_value*ENCODER_VALUE_SCALAR_EMULATION;   
			//scaled_value = new_value*200;   
	}
	// Update the Stored raw_encoder_value
	raw_encoder_value[virtual_encoder_id] += scaled_value;
	raw_encoder_value[virtual_encoder_id] = clamp_encoder_raw_value(raw_encoder_value[virtual_encoder_id]);
	// Translate the raw value into a MIDI value, send & save the new value 	
	uint8_t control_change_value = scale_encoder_value(raw_encoder_value[virtual_encoder_id]);
	//send_element_midi(SWITCH, i, control_change_value, true);
	uint8_t this_bank = encoder_bank;// banked_encoder_id >> 4; // !review !test this_bank
	bool shifted = encoder_is_in_shift_state(this_bank, i);
	send_encoder_midi(banked_encoder_id, control_change_value, true, shifted); // !Summer2016Update: Shifted Encoders now always output as Encoders
	indicator_value_buffer[encoder_bank][i]=control_change_value;
	return true;
}
#endif

void process_encoder_input_switch(uint8_t i, uint8_t virtual_encoder_id, uint8_t banked_encoder_id, uint16_t bit) {


	if (bit & get_enc_switch_down() || bit & get_enc_switch_up()) {
	// If the switch state has changed due its action
		switch (encoder_settings[banked_encoder_id].switch_action_type)
		{
			case CC_TOGGLE:{}
			// cascade to the next case ... NO BREAK ON PURPOSE
			case NOTE_TOGGLE:{
				if(bit & get_enc_switch_down()) {
					// Toggle the MIDI state
					enc_switch_midi_state[encoder_bank][i] = enc_switch_midi_state[encoder_bank][i] ? 
																0 : 127;
					// Update the display
					if (!color_overide_active(encoder_bank,i)) {
						switch_color_buffer[encoder_bank][i] = enc_switch_midi_state[encoder_bank][i] ?
						encoder_settings[banked_encoder_id].active_color : encoder_settings[banked_encoder_id].inactive_color;
					}
					// And send any MIDI
					send_element_midi(SWITCH, banked_encoder_id, enc_switch_midi_state[encoder_bank][i],
					enc_switch_midi_state[encoder_bank][i]);
				}
			}
			break;
			
			case CC_HOLD:{
			}
			// cascade to the next case ... NO BREAK ON PURPOSE
			case NOTE_HOLD:{
				if (bit & get_enc_switch_down()) {
					enc_switch_midi_state[encoder_bank][i] = 127;
				} else {
					enc_switch_midi_state[encoder_bank][i] = 0;
				}
				// Update the display
				if (!color_overide_active(encoder_bank,i)) {
					switch_color_buffer[encoder_bank][i] = enc_switch_midi_state[encoder_bank][i] ?
					encoder_settings[banked_encoder_id].active_color : encoder_settings[banked_encoder_id].inactive_color;
				}
				// And send any MIDI
				send_element_midi(SWITCH, banked_encoder_id, enc_switch_midi_state[encoder_bank][i],
				enc_switch_midi_state[encoder_bank][i]);
			}
			break;
			
			/*	The fine adjust action is handled in the encoder code, the
				LED does not change state with this action */
			case ENC_FINE_ADJUST:{}
			break;	
			
			case ENC_RESET_VALUE:{
				// Reset the encoder value
				uint8_t control_change_value; // = scale_encoder_value(raw_encoder_value[virtual_encoder_id]);
				if (encoder_settings[banked_encoder_id].has_detent){
					raw_encoder_value[virtual_encoder_id] = 6300;
					control_change_value = scale_encoder_value(raw_encoder_value[virtual_encoder_id]);
				} else {
					raw_encoder_value[virtual_encoder_id] = 0;
					control_change_value = scale_encoder_value(raw_encoder_value[virtual_encoder_id]);
				}
				// Send the updated encoder value
				encoder_detent_counter[i] = 0;
				send_element_midi(ENCODER, banked_encoder_id, control_change_value, true);  // Last Encoder call to 'send_element_midi', all others upgraded to 'send_encoder_midi'
				indicator_value_buffer[encoder_bank][i]=control_change_value;
				// Then send the switch action
				if (bit & get_enc_switch_down()) {						
					send_element_midi(SWITCH, banked_encoder_id , 127, true);
				} else if (bit & get_enc_switch_up()){
					send_element_midi(SWITCH, banked_encoder_id , 0, false);
				}
			}
			break;
			case ENC_SHIFT_HOLD:{
				if (bit & get_enc_switch_down()){
					// Update the display with the active color
					if (!color_overide_active(encoder_bank,i)) { // 20160622 - Allow midi feedback override of RGB Indicator.
						switch_color_buffer[encoder_bank][i] = encoder_settings[banked_encoder_id].active_color;
					}
					//switch_color_buffer[encoder_bank][i] = encoder_settings[banked_encoder_id].active_color;
					
					// Update the indicator display with the shift value
					indicator_value_buffer[encoder_bank][i]=(uint8_t)(raw_encoder_value[virtual_encoder_id]/100);
					// Send a Note On
					//midi_stream_raw_note(encoder_settings[banked_encoder_id].switch_midi_channel, // Update 20160622 - send according to switch settings
					midi_stream_raw_cc(encoder_settings[banked_encoder_id].switch_midi_channel, // Update 20160622 - send according to switch settings
					encoder_settings[banked_encoder_id].switch_midi_number,
					//midi_stream_raw_note(encoder_settings[banked_encoder_id].encoder_midi_channel,
					//encoder_settings[banked_encoder_id].encoder_midi_number,
					//true,
					127);
					
				} else if (bit & get_enc_switch_up()) {
					// Update the display the inactive color
					if (!color_overide_active(encoder_bank,i)) { // 20160622 - Allow midi feedback override of RGB Indicator.
						switch_color_buffer[encoder_bank][i] = encoder_settings[banked_encoder_id].inactive_color;
					}
					//switch_color_buffer[encoder_bank][i] = encoder_settings[banked_encoder_id].inactive_color;
					
					// Update the indicator display with the base encoder value
					indicator_value_buffer[encoder_bank][i]=(uint8_t)(raw_encoder_value[virtual_encoder_id]/100);
					// Send a Note Off
					//midi_stream_raw_note(encoder_settings[banked_encoder_id].switch_midi_channel, // Update 20160622 - send according to switch settings
					midi_stream_raw_cc(encoder_settings[banked_encoder_id].switch_midi_channel, // Update 20160622 - send according to switch settings
					encoder_settings[banked_encoder_id].switch_midi_number,
					//midi_stream_raw_note(encoder_settings[banked_encoder_id].encoder_midi_channel,
					//encoder_settings[banked_encoder_id].encoder_midi_number,
					//false,
					0);
				}
			}
			break;
			
			case ENC_SHIFT_TOGGLE:{
				if (bit & get_enc_switch_down()){
					// Toggle state
					enc_switch_toggle_state[encoder_bank] ^= bit;
					virtual_encoder_id = get_virtual_encoder_id(encoder_bank, i); // !Summer2016Update Re-Read virtual_encoder_id
													// - now that toggle_state has been updated
					// - this case is change indicator display before change value access to back to the 'non-shifted encoder'		
					if (bit & enc_switch_toggle_state[encoder_bank]){
						// Update the display the active color
						if (!color_overide_active(encoder_bank,i)) { // 20160622 - Allow midi feedback override of RGB Indicator.
							switch_color_buffer[encoder_bank][i] = encoder_settings[banked_encoder_id].active_color;
						}
						//switch_color_buffer[encoder_bank][i] = encoder_settings[banked_encoder_id].active_color;

						// Update the indicator display with the shift value
						indicator_value_buffer[encoder_bank][i]=(uint8_t)(raw_encoder_value[virtual_encoder_id]/100);
						
						// Send a CC
						// !Summer2016Update: Encoder Shift Switches now output CCs on Switch Channel like other special function switches
						//midi_stream_raw_note(encoder_settings[banked_encoder_id].switch_midi_channel,
						midi_stream_raw_cc(encoder_settings[banked_encoder_id].switch_midi_channel,
						encoder_settings[banked_encoder_id].switch_midi_number,
						//true,
						127);
						
					} else {
						// Update the display the inactive color
						if (!color_overide_active(encoder_bank,i)) { // 20160622 - Allow midi feedback override of RGB Indicator.
							switch_color_buffer[encoder_bank][i] = encoder_settings[banked_encoder_id].inactive_color;
						}
						//switch_color_buffer[encoder_bank][i] = encoder_settings[banked_encoder_id].inactive_color;
						
						// Update the indicator display with the base encoder value
						indicator_value_buffer[encoder_bank][i]=(uint8_t)(raw_encoder_value[virtual_encoder_id]/100);
						
						// Send a CC
						// !Summer2016Update: Encoder Shift Switches now output CCs on Switch Channel like other special function switches
						//midi_stream_raw_note(encoder_settings[banked_encoder_id].switch_midi_channel,
						midi_stream_raw_cc(encoder_settings[banked_encoder_id].switch_midi_channel, 
						encoder_settings[banked_encoder_id].switch_midi_number,
						//false,
						0);
						
					}
				}
			}
			break;
		}
	}
}
// ======================== Process Encoder Input Functions==================END=



/**
 * Shift mode is an alternate operation mode which is accessed by the side buttons
 * In shift mode the encoders do not do anything, only the switches have effect.
 * All 11 position LEDs are turned on and the RGB set to white if active.  
 * 
 * \param page [in]	0, 1 Which shift page is being accessed
 */
void run_shift_mode(uint8_t page){
	
	static uint8_t idx = 0x00;
	
	// First we check the encoder switch states and send any MIDI
	update_encoder_switch_state();
	uint16_t bit = 0x01;
	
	//uint16_t sw_down = get_enc_switch_down();
	
	for(uint8_t i=0;i<16;++i){
		if (bit & get_enc_switch_down()) {
			// Switch was just pressed
			midi_stream_raw_note(midi_system_channel, SHIFT_OFFSET + (i+(page*16)) , true, 127);
			// Set state if override is not active
			if(!(shift_mode_midi_override[page] & bit)){
				shift_mode_switch_state[page] |= bit;
			}
			
		} else if (bit & get_enc_switch_up()) {
			// Switch was just released
			//send_element_midi(SHIFT, i+(page*16), 0, false);
			midi_stream_raw_note(midi_system_channel, SHIFT_OFFSET + (i+(page*16)), false, 0);
			// Clear state if override is not active
			if(!(shift_mode_midi_override[page] & bit)){
				shift_mode_switch_state[page] &= ~bit;
			}
		}
		bit <<=1;
	}
		
	// The we update the display.
	if (shift_mode_switch_state[page] & (0x01<<idx)){
		// Set the LEDs on
		set_encoder_rgb(idx, 0);
		set_encoder_indicator(idx,127, false, BAR, 0);
	} else {
		set_encoder_rgb(idx, 0);
		set_encoder_indicator(idx,0, false, BAR, 0);
	}
	
	// Increment the encoder index for next time
	idx += 1;
	if(idx > 15){idx = 0;}
}

//void send_encoder_pitchbend(uint8_t midi_channel, uint16_t value, bool state, bool shifted) {
//	
//}

void send_encoder_midi(uint8_t banked_encoder_idx, uint8_t value, bool state, bool shifted)
{
	uint8_t midi_channel = shifted ? encoder_settings[banked_encoder_idx].encoder_shift_midi_channel: encoder_settings[banked_encoder_idx].encoder_midi_channel;
	// !revision resuse unused switch midi number for shifts: midi_number = shifted? encoder_midi_num/ switch_midi_num 20190806 
	// Sending encoder as note is not useful so this can likely be simplified
	// once incremental messages are added
	if (encoder_settings[banked_encoder_idx].encoder_midi_type == SEND_CC || encoder_settings[banked_encoder_idx].encoder_midi_type == SEND_SWITCH_VEL_CONTROL)
	{
		midi_stream_raw_cc(midi_channel,
		encoder_settings[banked_encoder_idx].encoder_midi_number,
		value);
		//if (encoder_settings[banked_encoder_idx].is_super_knob && (value >= global_super_knob_start)) {
		if (encoder_settings[banked_encoder_idx].is_super_knob) {

			float step = 1/(((float)(global_super_knob_end - global_super_knob_start)) / 127.0f);
					
			uint16_t secondary_value = 0;
			if (value >= global_super_knob_start) {
				secondary_value = ((uint16_t)(step * (float)(value - global_super_knob_start)));
			}

			// Clamp to 127
			secondary_value = secondary_value > 127 ? 127 : secondary_value;

			MIDI_Device_Flush(g_midi_interface_info);
					
			//midi_stream_raw_cc(encoder_settings[banked_encoder_idx].encoder_midi_channel,
			midi_stream_raw_cc(midi_channel, 
			encoder_settings[banked_encoder_idx].encoder_midi_number+64,
			(uint8_t)secondary_value);		
		}			
	} else if (encoder_settings[banked_encoder_idx].encoder_midi_type == SEND_NOTE) {
		midi_stream_raw_note(midi_channel,
		encoder_settings[banked_encoder_idx].encoder_midi_number,
		true,
		value);
	}
}

bool switch_value_from_encoder_value_enabled(uint8_t banked_encoder_idx) {
	if (encoder_settings[banked_encoder_idx].encoder_midi_type == SEND_SWITCH_VEL_CONTROL) {
		return true;
	} else {
		return false;
	}
}

uint8_t adjust_switch_value_by_encoder_value(uint8_t banked_encoder_idx, uint8_t value, bool state) {
	if (!state) {
		return value;
	} else if (!switch_value_from_encoder_value_enabled(banked_encoder_idx)) {
		return value;
	} else {
		uint8_t enc_val = scale_encoder_value(raw_encoder_value[banked_encoder_idx]); // virtual encoder id not used, because shift buttons have no value
		return enc_val;
	}
}


/**
 * Sends MIDI for a given encoder or switch. Because MIDI can only be SENT
 * for encoder of the current bank this function only accepts an index range
 * of 0 - 15
 * 
 * \param type  [in]	The type of control, encoder or switch
 * 
 * \param index [in]	The index of the control (0 - 15)
  * 
 * \param state [in]	The Note On/Off state for note messages
 * 
 * \param value [in]	The new value of the control
 * 
 */

void send_element_midi(enc_control_type_t type, uint8_t banked_encoder_idx, uint8_t value, bool state)
{
	//if(banked_encoder_idx>15){
	if(banked_encoder_idx >= BANKED_ENCODERS){
		return;
	}
	
	switch (type){
		case ENCODER:{
			// Sending encoder as note is not useful so this can likely be simplified
			// once incremental messages are added
			if (encoder_settings[banked_encoder_idx].encoder_midi_type == SEND_CC || encoder_settings[banked_encoder_idx].encoder_midi_type == SEND_SWITCH_VEL_CONTROL)
			{
				midi_stream_raw_cc(encoder_settings[banked_encoder_idx].encoder_midi_channel,
										   encoder_settings[banked_encoder_idx].encoder_midi_number,
						                   value);
				//if (encoder_settings[banked_encoder_idx].is_super_knob && (value >= global_super_knob_start)) {
				if (encoder_settings[banked_encoder_idx].is_super_knob) {
					float step = 1/(((float)(global_super_knob_end - global_super_knob_start)) / 127.0f);
					
					uint16_t secondary_value = 0;
					if (value >= global_super_knob_start) {
						secondary_value = ((uint16_t)(step * (float)(value - global_super_knob_start)));
					}

					// Clamp to 127
					secondary_value = secondary_value > 127 ? 127 : secondary_value;

					MIDI_Device_Flush(g_midi_interface_info);
						
					midi_stream_raw_cc(encoder_settings[banked_encoder_idx].encoder_midi_channel,
										encoder_settings[banked_encoder_idx].encoder_midi_number+64,
										(uint8_t)secondary_value);
				
				} 
	
			} else if (encoder_settings[banked_encoder_idx].encoder_midi_type == SEND_NOTE) {
				midi_stream_raw_note(encoder_settings[banked_encoder_idx].encoder_midi_channel, 
								     encoder_settings[banked_encoder_idx].encoder_midi_number, 
								     true,
									 value);
			} else if (encoder_settings[banked_encoder_idx].encoder_midi_type == SEND_REL_ENC  || encoder_settings[banked_encoder_idx].encoder_midi_type == SEND_REL_ENC_MOUSE_EMU_DRAG || encoder_settings[banked_encoder_idx].encoder_midi_type == SEND_REL_ENC_MOUSE_EMU_SCROLL){ 
				midi_stream_raw_cc(encoder_settings[banked_encoder_idx].encoder_midi_channel,
										   encoder_settings[banked_encoder_idx].encoder_midi_number,
										   value);
			}
		}
		break;
		case SWITCH:{		
			if (encoder_settings[banked_encoder_idx].switch_action_type == CC_HOLD ||
				encoder_settings[banked_encoder_idx].switch_action_type == CC_TOGGLE ||
				encoder_settings[banked_encoder_idx].switch_action_type == ENC_SHIFT_HOLD ||
				encoder_settings[banked_encoder_idx].switch_action_type == ENC_SHIFT_TOGGLE ||
				encoder_settings[banked_encoder_idx].switch_action_type == ENC_RESET_VALUE){
					
				uint8_t adj_val = adjust_switch_value_by_encoder_value(banked_encoder_idx, value, state);
				//midi_stream_raw_cc(encoder_settings[banked_encoder_idx].switch_midi_channel, 
				//		encoder_settings[banked_encoder_idx].switch_midi_number,
				//		adj_val);
		
				uint8_t midi_channel = encoder_settings[banked_encoder_idx].switch_midi_channel;
				uint8_t midi_number = encoder_settings[banked_encoder_idx].switch_midi_number;
				midi_stream_raw_cc(midi_channel, midi_number, adj_val);
				// Allow direct Bank Changes from appropriately matched switches: 20190807
				if (value) { // only change bank on 'press' not 'release'
					if (midi_channel == midi_system_channel) {
						if (midi_number < NUM_BANKS) {
							uint8_t i = banked_encoder_idx & 0x0F; // NUM_ENCODERS_SINGLE_BANK_MASK
							// Turn LED Off, leads to hung 'on' indicators leds if you don't...
							// Reset the MIDI state
							enc_switch_midi_state[encoder_bank][i] = 0;
							// Reset the display state to 'inactive
							if (!color_overide_active(encoder_bank,i)) {
								switch_color_buffer[encoder_bank][i] = encoder_settings[banked_encoder_idx].inactive_color;
							}
							// Actually change the bank
							change_encoder_bank(midi_number);
						}
					}
				}		   
			} else if (encoder_settings[banked_encoder_idx].switch_action_type == NOTE_HOLD ||
						encoder_settings[banked_encoder_idx].switch_action_type == NOTE_TOGGLE) {
					
				uint8_t adj_val = adjust_switch_value_by_encoder_value(banked_encoder_idx, value, state);
				midi_stream_raw_note(encoder_settings[banked_encoder_idx].switch_midi_channel, 
						encoder_settings[banked_encoder_idx].switch_midi_number, 
						state,
						adj_val);						   
			} 
		}
		break;
		default:{

		}
		break;
	}	
}



/**
 * Takes a midi message and translates it to its related state buffer
 * 
 * \param channel [in]	The MIDI channel 
  * 
 * \param channel [in]	The MIDI control type CC/NOTE 
 * 
 * \param channel [in]	The MIDI note / control number
  * 
 * \param channel [in]	The value 
  * 
 * \param channel [in]	The boolean state (ignored if not a note)
 * 
 */

#define ENABLE_DUPLICATE_INPUT_MAPPINGS 1

void process_element_midi(uint8_t channel, uint8_t type, uint8_t number, uint8_t value, uint8_t state) // Midi Feedback - Main Routine
{
	// If the incoming midi is in the system channel then its mapping is fixed 
	if (channel == midi_system_channel) {
		// Fixed for notes for now
		if ((type == SEND_NOTE) || (type == SEND_NOTE_OFF)){
			if ((number >= SHIFT_OFFSET) && (number <= SHIFT_OFFSET+32)){
				number = number - SHIFT_OFFSET;
				uint8_t index  = number % 16;
				uint8_t bank   = number / 16;
				// Set the corresponding overide bit
				shift_mode_midi_override[bank] |= (0x0001 << index);
				if (value){
					shift_mode_switch_state[bank] |= (0x0001 << index);
					} else {
					shift_mode_switch_state[bank] &= ~(0x0001 << index);
				}
			}
		} 
	} else {
		// Otherwise the input is re mappable so scan through the input map for a match
		for(uint8_t i=0;i<64;++i){
			// Search the input map for a match
			if(encoder_settings[i].encoder_midi_number == number){  // !revision: reuse unused switch midi number for shifted encoder midi number
				uint8_t output_type = encoder_settings[i].encoder_midi_type;
				// Check Encoder Mapping for a Match
				if(encoder_settings[i].encoder_midi_channel == channel){
					// Matched to an encoder indicator
					// - !Summer2016Update: MIDI Type Filtering for MIDI Feedback
					if(type == SEND_CC){  
						if (output_type == SEND_CC || output_type == SEND_SWITCH_VEL_CONTROL || output_type == SEND_REL_ENC || output_type == SEND_REL_ENC_MOUSE_EMU_DRAG || output_type == SEND_REL_ENC_MOUSE_EMU_SCROLL) // Ensure MIDI Type Matches
						{
							process_indicator_update(i, value, 0); // !Summer2016Update: 0 = non-shifted encoder
						}
					}
					else { // Other Valid types that reach here are SEND_NOTE and SEND_NOTE_OFF, encoders treat them the same
						if (output_type == SEND_NOTE)
						{
							process_indicator_update(i, value, 0); // !Summer2016Update: 0 = non-shifted encoder		
						}
					}
					// !Summer2016Update: To allow for duplicate mappings across banks, we must continue to check for duplicate mappings
					// - Same goes for all other statements.
					#if ENABLE_DUPLICATE_INPUT_MAPPINGS == 0
					return;
					#endif
				} else if(encoder_settings[i].encoder_shift_midi_channel == channel){
					// Matched to an shifted encoder's indicator
					// - !Summer2016Update: MIDI Type Filtering for MIDI Feedback
					if(type == SEND_CC){ 
						if (output_type == SEND_CC || output_type == SEND_SWITCH_VEL_CONTROL || output_type == SEND_REL_ENC || output_type == SEND_REL_ENC_MOUSE_EMU_DRAG){
							process_indicator_update(i, value, 1); // !Summer2016Update: 1 = shifted encoder
						}
					}
					else{ // Other Valid types that reach here are SEND_NOTE and SEND_NOTE_OFF, encoders treat them the same
						if (output_type == SEND_NOTE) {
							process_indicator_update(i, value, 1); // !Summer2016Update: 1 = shifted encoder							
						}
					}
					// 2016: Must continue to check for duplicate mappings to allow proper operation of a Master Knob that is the same in all four banks
					#if ENABLE_DUPLICATE_INPUT_MAPPINGS == 0
					return;
					#endif
				}
			} 
			// Check Switch Mapping for a Match
			if (encoder_settings[i].switch_midi_number == number){
				if(encoder_settings[i].switch_midi_channel == channel){
					// Matched to an encoder switch
					uint8_t action_type = encoder_settings[i].switch_action_type;
					// Check for Message type Match
					// - !Summer2016Update: MIDI Type Filtering for MIDI Feedback
					if ((action_type == NOTE_HOLD) || (action_type == NOTE_TOGGLE)){
						if (type == SEND_NOTE){
							process_sw_rgb_update(i, value);
							process_sw_toggle_update(i, value); // !Summer2016Update: Switch Toggle State Feedback
							#if ENABLE_DUPLICATE_INPUT_MAPPINGS == 0
							return;
							#endif
						}
						else if (type == SEND_NOTE_OFF){
							// !review: handle value 64 to use Ableton's Ability to light up unpopulated clips
							// - if so, should light white
							process_sw_rgb_update(i, 0);
							process_sw_toggle_update(i, 0);							
							//process_sw_rgb_update(i, value);
							//process_sw_toggle_update(i, value);
							#if ENABLE_DUPLICATE_INPUT_MAPPINGS == 0
							return;
							#endif							
						}
					} else {
						// All other Switch Output Types will output a CC Message, and should expect to receive one in return.
						if (type == SEND_CC){
							process_sw_rgb_update(i, value);
							process_sw_toggle_update(i, value); // !Summer2016Update: Switch Toggle State Feedback
							//if ((action_type == ENC_SHIFT_HOLD) || (action_type == ENC_SHIFT_TOGGLE)){
								// Note: ENC_SHIFT_HOLD can't support software toggling of encoder shift state
								// - this is because HOLD type buttons are continually checked at a hardware level
								// - adding this feature would take a lot of work/testing for little benefit.
							//if (action_type == ENC_SHIFT_TOGGLE){
							//	process_sw_encoder_shift_update(i, value);
							//}
							#if ENABLE_DUPLICATE_INPUT_MAPPINGS == 0
							return;
							#endif
						}
					}
				} else if (channel == ENCODER_ANIMATION_CHANNEL) { 
					// Note: Animations are executed, so long as the number matches the switch
					// - This allows backward compatibility with a very lenient earlier protocol for twister (2014 builds)
					// Matched to encoder switch animation
					process_encoder_animation_update(i, value);
					#if ENABLE_DUPLICATE_INPUT_MAPPINGS == 0
					return;
					#endif
				} else if (channel == SWITCH_ANIMATION_CHANNEL) {  // 2016: Dual software animation channels update
					// Matched to encoder switch animation
					process_sw_animation_update(i, value);
					#if ENABLE_DUPLICATE_INPUT_MAPPINGS == 0
					return;
					#endif
				}
			}
		}
	}
}

// Midi Feedback - Encoder Value Indicator Displays
// value: MIDI Value (7-bit)
// shifted: 	0 = Incoming messages is referencing base encoder mapping
// 		1 = Incoming message is referencing shifted encoder mapping
//void process_indicator_update(uint8_t idx, uint8_t value)
void process_indicator_update(uint8_t idx, uint8_t value, uint8_t rx_msg_shifted_mapping) // !Summer2016Update: Added MIDI Feedback for Shifted Encoders
{		
	uint8_t bank = idx / 16;
	uint8_t encoder = idx % 16;
	//uint16_t mask = 0x0001 << encoder;
	uint8_t current_shift_state = encoder_is_in_shift_state(bank, encoder); 
	uint8_t virtual_encoder_id = idx;// can't use get_virtual_encoder_id(bank, encoder) because we may be accessing encoder outside of current shift state.
	virtual_encoder_id += rx_msg_shifted_mapping ? BANKED_ENCODERS:0; // Increment virtual_encoder_id to enable addressing of rx_msg_shifted_mapping encoders (if applicable)
	
	// !review: should be able to merge cases here, now that raw_encoder_value has been expanded
	// Update the raw value if bank is active, and the encoder is not currently moving
	if (bank == current_encoder_bank()) {
		if ( !encoder_is_active(encoder)  ||  encoder_midi_type_is_relative(encoder) ) {
			int16_t raw_value= ((uint16_t)value)*100;
			raw_encoder_value[virtual_encoder_id] = raw_value;
			if (current_shift_state == rx_msg_shifted_mapping) { // If Value is currently on display, update the display
				indicator_value_buffer[bank][encoder] = value;
			}
		}	
	} else {
	     // otherwise update the value buffer
		int16_t raw_value= ((uint16_t)value)*100;
		raw_encoder_value[virtual_encoder_id] = raw_value;
		if (current_shift_state == rx_msg_shifted_mapping) { // If Value is currently on display, update the display
			indicator_value_buffer[bank][encoder] = value;
		}
	}
}

void process_sw_rgb_update(uint8_t idx, uint8_t value) // Midi Feedback - Switch State Indicators (RGB LEDs)
{
	uint8_t bank = idx / 16;
	uint8_t encoder = idx % 16;
	
	// Accept either note or CC for color control
	if (value == 0){
		// Disable the color over ride
		switch_color_overide[bank] &= ~(0x01<<encoder);
		switch_color_buffer[bank][encoder] = encoder_settings[idx].inactive_color;
	} else if (value >0 && value < 126) { // Exclude 126 as we don't allow user to set color to white
		// Enable the override and set the color to value
		switch_color_overide[bank] |= (0x01<<encoder);
		switch_color_buffer[bank][encoder] = value;
	} else {
		// Read Directly from RAM
		// uint8_t banked_encoder_id = idx;
		// Enable the override and set the color to active
		switch_color_overide[bank] |= (0x01<<encoder);
		switch_color_buffer[bank][encoder] = encoder_settings[idx].active_color;
	}
}

// Midi Feedback - Switch Stored Toggle State (RGB LEDs) - !Summer2016Update
void process_sw_toggle_update(uint8_t idx, uint8_t value) 
{
	uint8_t bank = idx / 16;
	uint8_t encoder = idx % 16;
	//uint8_t action_type = encoder_settings[i].switch_action_type;
	//enc_switch_toggle_state  // toggle_state is "used to track" midi_state
	enc_switch_midi_state[bank][encoder] = value ? 127:0;	// midi_state appears to be the proper variable for modification here
	/*if (action_type == ENC_SHIFT_TOGGLE){ 
		// SHIFT_TOGGLE encoders also use enc_switch_toggle_state, updated it
		uint16 bit = 0x01 << encoder;
		if (value){
			enc_switch_toggle_state[bank] |= bit;
		}
		else {
			enc_switch_toggle_state[bank] &= bit;
		}
		// Now change the active encoder based on this change

	}
	else if (action_type == ENC_SHIFT_HOLD){
		// Change active encoder
		
	}*/
}

// Midi Feedback - Switch Stored Toggle State for Shift Encoder Toggle Switches (also updates Encoder Value Indicator LEDS
// - !Summer2016Update
void process_sw_encoder_shift_update(uint8_t idx, uint8_t value){
	uint8_t bank = idx / 16;
	uint8_t encoder = idx % 16;
	uint16_t bit = 0x01 << encoder; // for updating enc_switch_toggle_state
			
	// SHIFT_TOGGLE encoders also use enc_switch_toggle_state, so we'll update it (this is different from enc_switch_midi_state)
	if (value){
		enc_switch_toggle_state[bank] |= bit;
	}
	else {
		enc_switch_toggle_state[bank] &= ~bit;
	}
	
	// Update Encoder Value Display
	// - Get virtual encoder number so we know which value to display (shifted or unshifted)
	// - this must stay after the update of enc_switch_toggle_state!
	uint8_t virtual_encoder_id = idx;
	if (encoder_is_in_shift_state(bank, encoder))
		{virtual_encoder_id += 64;}
	indicator_value_buffer[bank][encoder]=(uint8_t)(raw_encoder_value[virtual_encoder_id]/100); // update display buffer
}

void process_sw_animation_update(uint8_t idx, uint8_t value)
{
	uint8_t bank = idx / 16;
	uint8_t encoder = idx % 16;
	switch_animation_buffer[bank][encoder] = value;
}

void process_encoder_animation_update(uint8_t idx, uint8_t value)	// !Summer2016Update: dual animations
{
	uint8_t bank = idx / 16;
	uint8_t encoder = idx % 16;
	encoder_animation_buffer[bank][encoder] = value;
}

void process_shift_update(uint8_t idx, uint8_t value)
{
	
}

/**
 * 	For each iteration of the main loop we compare the current MIDI state with the
 *  previous state, if changed we update that display element.
 *
**/

static uint8_t prevIndicatorValue[16];
static uint8_t prevSwitchColorValue[16];
static uint8_t prevEncoderAnimationValue[16];
static uint8_t prevSwAnimationValue[16];

// - Switch Animations 1-48, 127
bool animation_is_switch_rgb(uint8_t animation_value) { // !Summer2016Update: Dual Animations - Identify to Eliminate Conflicts
	if (!animation_value) {return false;}
	else if (animation_value < 49 || animation_value == 127) {return true;}
	else {return false;}
}

// - Indicator Animations 49-96
bool animation_is_encoder_indicator(uint8_t animation_value) { // !Summer2016Update: Dual Animations - Identify to Eliminate Conflicts
	if (animation_value < 49 || animation_value == 127) {return false;}
	else if (animation_value < 97) {return true;}
	else {return false;}
}

// Detect if two Animations Buffers are both attempting to animate the same item
// - Switch Animations 1-48, 127
// - Indicator Animations 49-96
bool animation_buffer_conflict_exists(uint8_t bank, uint8_t encoder) {
	uint8_t this_animation = switch_animation_buffer[bank][encoder];
	uint8_t that_animation = encoder_animation_buffer[bank][encoder];
	
	if (!this_animation || !that_animation) { // Case: this or that animation is OFF
		return false;
	} else if (this_animation > 127 || that_animation > 127) { // Case: this or that animation is invalid
		return false;
	}else if (this_animation < 49 || this_animation == 127 ) { // Case: this_animation is a Switch animation
		if (that_animation < 49 || that_animation == 127){  // Case: Both are switch animations
			return true;
		} else {  // Case: one is switch, one is not
			return false;
		}
	} else if (this_animation < 97 ) { // if we get here, this_animation cannot be 0,1-48, or 127+ (must be 49-126)
		if (that_animation > 48 && that_animation < 97) { // Case: both are indicator animations
			return true;
		}
		else { // Case: one is a indicator animation, one is not
			return false;
		}
	} else { // Case: this_animation is invalid
		return false;
	}
}

void update_encoder_display(void)
{	
	static uint8_t idx = 0;
	
//#define FORCE_UPDATE
#ifdef FORCE_UPDATE
	#warning FORCE UPDATE is Enabled, may impede some LED Operations
	set_encoder_indicator(idx, indicator_value_buffer[encoder_bank][idx], encoder_settings[idx].has_detent,
	encoder_settings[idx].indicator_display_type,
	encoder_settings[idx].detent_color);
	set_encoder_rgb(idx, switch_color_buffer[encoder_bank][idx]);
#endif 
	
	// First the indicator display
	uint8_t currentValue = indicator_value_buffer[encoder_bank][idx];
	uint8_t banked_encoder_idx = idx + encoder_bank*PHYSICAL_ENCODERS;				
	if (currentValue != prevIndicatorValue[idx]) {
		set_encoder_indicator(idx, currentValue, encoder_settings[banked_encoder_idx].has_detent,
								encoder_settings[banked_encoder_idx].indicator_display_type,
								encoder_settings[banked_encoder_idx].detent_color);
		prevIndicatorValue[idx] = currentValue;
	}
	
	// Next the RGB display
	currentValue = switch_color_buffer[encoder_bank][idx];
	if (currentValue != prevSwitchColorValue[idx]) {
		set_encoder_rgb(idx, currentValue);
		prevSwitchColorValue[idx] = currentValue;
	}
	
	// Finally if animation is active for this encoder run the animation
	// If it has just changed from active to inactive reset the RGB to its
	// pre-animation state
	
	// !Summer2016Update: Dual Animations 
	// - created encoder_animation_buffer, which double-uses run_encoder_animation

	// rmerrill 2023-08-23
	// we ignore animation types from the "wrong" buffer now.
	// encoder_animation_buffer's values are ignored unless they apply to encoder
	// likewise with switch_animation_buffer.

	currentValue = encoder_animation_buffer[encoder_bank][idx];
	if (currentValue && animation_is_encoder_indicator(currentValue)){
		run_encoder_animation(idx, encoder_bank, currentValue, switch_color_buffer[encoder_bank][idx]);
		prevEncoderAnimationValue[idx] = currentValue;
	} else if (prevEncoderAnimationValue[idx]) {
		set_encoder_indicator(idx, indicator_value_buffer[encoder_bank][idx], encoder_settings[banked_encoder_idx].has_detent,
				encoder_settings[banked_encoder_idx].indicator_display_type,
				encoder_settings[banked_encoder_idx].detent_color);
		prevEncoderAnimationValue[idx] = 0;
	}

	// Run Switch Animation, or set the indicator to the last set color.
	currentValue = switch_animation_buffer[encoder_bank][idx];
	if (currentValue && animation_is_switch_rgb(currentValue)) {
		run_encoder_animation(idx, encoder_bank, currentValue, switch_color_buffer[encoder_bank][idx]);
		prevSwAnimationValue[idx] = currentValue;
	}  else if (prevSwAnimationValue[idx]) {  // Animation Just Ended
		set_encoder_rgb(idx, switch_color_buffer[encoder_bank][idx]);
		prevSwAnimationValue[idx] = 0;
	}
	
	// Increment the encoder index for next time
	idx += 1;
	if(idx > 15){idx = 0;}
}

/**
 * Rebuilds the entire display, this is called when the unit powers up
 * or when changing banks.
 * 
 * \param[in] new_bank		The bank number we are changing to.
 * 
 */
void change_encoder_bank(uint8_t new_bank) // Change Bank
{
	// Prepare the state buffers for the new bank
	transfer_encoder_values_to_other_banks(encoder_bank);

	for(uint8_t i =0;i<16;++i){
		uint8_t old_virtual_encoder_id = get_virtual_encoder_id(encoder_bank, i);
		uint8_t new_virtual_encoder_id = get_virtual_encoder_id(new_bank, i);
		
		// Save previous raw values
		indicator_value_buffer[encoder_bank][i] = raw_encoder_value[old_virtual_encoder_id] / 100;
		
		// Update raw values for new bank 
		// - !Summer2016Update: Removed in favor of expansion of raw_encoder_value to include all banks and shifted encoders
		//raw_encoder_value[i] = indicator_value_buffer[new_bank][i] * 100;
		// Save current raw shift states for previous bank 
		//if((encoder_settings[i].switch_action_type == ENC_SHIFT_HOLD) || 
		   //(encoder_settings[i].switch_action_type == ENC_SHIFT_TOGGLE)){
			   //enc_switch_midi_state[encoder_bank][i] = raw_shift_encoder_value[i] / 100;
		//}

		// Set the prev values to -1 which forces a display update
		prevIndicatorValue[i] = -1;
		prevSwitchColorValue[i] = -1;	
		
		// Read in all the encoder settings for the current bank
		// - !Summer2016Update: Removed in favor of expanding encoder_settings to include all banks 
		//~ get_encoder_config(new_bank, i, &encoder_settings[i]);  
		
		// Update raw shift states for new bank
		// - !Summer2016Update: Removed in favor of expansion of raw_encoder_value to include all banks and shifted encoders
		//if((encoder_settings[i].switch_action_type == ENC_SHIFT_HOLD) || 
		   //(encoder_settings[i].switch_action_type == ENC_SHIFT_TOGGLE)){
			   //raw_shift_encoder_value[i] = enc_switch_midi_state[new_bank][i] * 100;
		   //}
		indicator_value_buffer[new_bank][i] = raw_encoder_value[new_virtual_encoder_id] / 100;

		/* !Summer2016Update: Removed Double use of enc_switch_midi_state by expanding of raw_encoder_value table
		 * // Check to see if encoder is in a shift state, and if so update its indicator
		if (encoder_is_in_shift_state(new_bank, i)){
			indicator_value_buffer[new_bank][i] = enc_switch_midi_state[new_bank][i];
		}*/		
	} 
	
	encoder_bank = new_bank;                                                 
}

/**
 * Returns the current encoder bank index
 * 
 * \return	The current encoder bank index
 */
uint8_t current_encoder_bank(void)
{
	return encoder_bank;
}

/**
 * Convenience function which forces a refresh of the encoder display and values
 */
void refresh_display(void){
	change_encoder_bank(current_encoder_bank());
}

uint8_t scale_encoder_value(int16_t value)
{
	uint8_t scaled_value;
	// Detent Checking removed for encoders without 'detent checking enabled' at user request 6/22/2016
	// - because doing that is redundant, and causes value '64' to not be output
	// -- if users want a detent, they will enable the center 'detent' feature in the MF Utility, which still functions.
	scaled_value = (uint8_t) ((value+50)/100);
	//~ if (encoder_is_in_detent(value))
	//~ {
		//~ scaled_value = 63;
	//~ } else {
		//~ scaled_value = (uint8_t) ((value+50)/100);
	//~ }
	return scaled_value;
}

int16_t clamp_encoder_raw_value(int16_t value)
{
	if (value < 0){value = 0;}
	if (value > 12700){value = 12700;}
	return value;
}



bool encoder_is_in_detent(int16_t value)
{
	//if (value > 6200 && value < 6500) // values of 62 do not trigger the 'detent' led routines, so we need to be sure value is at least 6300
	//if (value >= 6300 && value < 6500)
	if (value > encoder_detent_limit_low && value < encoder_detent_limit_high) 
	{									// - so to get value '64' to output in velocity sensing mode, we need to get down this low.
		return true;
	}
	return false;
}



bool encoder_is_in_deadzone(int16_t value)
{
	if (value > 12699 || value < 1){
		return true;
	}	
	return false;	
}

/**
 * Returns the color over ride state for a given encoder 
 * 
 * \param bank [in]		The encoders bank
 *
 * \param encoder [in]  The encoder
 * 
 * \return true if over ride is active
 */

bool color_overide_active(uint8_t bank, uint8_t encoder)
{
	uint16_t bit = 0x01 << encoder;
	bool active = false;
	active = (bit & switch_color_overide[bank]) ? true : false; 
	return active;
}

/**
 * Returns true if encoder is in a shift state
 */
bool encoder_is_in_shift_state(uint8_t bank, uint8_t encoder)
{
	uint8_t banked_encoder_idx = encoder + bank*PHYSICAL_ENCODERS;				
	uint16_t bit = 0x0001 << encoder;

	/*if ((encoder_settings[encoder].switch_action_type == ENC_SHIFT_HOLD ||
		 encoder_settings[encoder].switch_action_type == ENC_SHIFT_TOGGLE) &&
	   ((get_enc_switch_state() & bit) || (enc_switch_toggle_state[bank] & bit)))*/ 
	if ( ((encoder_settings[banked_encoder_idx].switch_action_type == ENC_SHIFT_HOLD)  && (get_enc_switch_state() & bit)) ||
		 ((encoder_settings[banked_encoder_idx].switch_action_type == ENC_SHIFT_TOGGLE) && (enc_switch_toggle_state[bank] & bit)) ) 
	{
		return true;
	} else  {
		return false;
	}
}
// This function assumes bank == current bank
bool encoder_midi_type_is_relative(uint8_t encoder) 
{
	uint8_t banked_encoder_idx = encoder + encoder_bank*PHYSICAL_ENCODERS;				
	if (encoder_settings[banked_encoder_idx].encoder_midi_type == SEND_REL_ENC || encoder_settings[banked_encoder_idx].encoder_midi_type == SEND_REL_ENC_MOUSE_EMU_DRAG || encoder_settings[banked_encoder_idx].encoder_midi_type == SEND_REL_ENC_MOUSE_EMU_SCROLL){
		return true;
	}
	else{
		return false;
	}
}
