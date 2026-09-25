/*
 *	config.c
 *
 *	Created: 9/16/2013 4:31:34 PM
 *  Authors: Daniel Kersten, Michael Mitchell
 *
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

#include "config.h"
#include "native_mode.h"
#include "display_driver.h"
#include "calibration.h"
#include "setup_mode.h"

uint8_t global_super_knob_start;
uint8_t global_super_knob_end;
uint8_t global_bank_animations_enabled;
uint8_t global_detent_size;
uint8_t global_dead_zone_size;



// Because the utility relays does not differ between the tags for global and encoder
// settings we have hacked this function, the first 10 tags are global, then 11 - 29 are
// reserved for encoder. 30 + is global again

#define GLOBAL_TAG_LOW_UPPER 10
#define GLOBAL_TAG_HIGH_LOWER 30
#define ENCODER_RESV_RANGE (GLOBAL_TAG_HIGH_LOWER - GLOBAL_TAG_LOW_UPPER)

void global_tv_table_decode(global_tvtable_t*, uint8_t*, uint8_t); // -Wmissing-prototype
void global_tv_table_decode(global_tvtable_t* table, uint8_t* buffer, uint8_t size)
{
    uint8_t idx = 0;
    uint8_t tag;
    while (idx < size - 1) {
        tag = buffer[idx++];
		// Hack to avoid messy utility changes
        if (tag < GLOBAL_TAG_LOW_UPPER) {
            table->bytes[tag] = buffer[idx];
        } else if (tag > GLOBAL_TAG_HIGH_LOWER && tag - ENCODER_RESV_RANGE - 1 < GLOBAL_TABLE_SIZE){
			table->bytes[tag-ENCODER_RESV_RANGE - 1] = buffer[idx];
		}
        ++idx;
    }
}

/**
 * Stores the detent and dead zone sizes and makes them live. The encoder code
 * reads the globals on each movement, thus no reset is necessary. Values out of
 * range are clamped.
 */
void detent_settings_save(uint8_t detent_size, uint8_t dead_zone_size)
{
	global_detent_size    = min(detent_size, MAX_DETENT_SIZE);
	global_dead_zone_size = min(dead_zone_size, MAX_DEAD_ZONE_SIZE);
	eeprom_write(EE_DETENT_SETTINGS, PACK_DETENT_SETTINGS(global_detent_size, global_dead_zone_size));
}

static void sysExCmdPushConfig (uint8_t length, uint8_t* buffer)
{
	
	// First disable watch dog as EEPROM is slow
	wdt_disable();
	
    global_tvtable_t config = {{0}};
    global_tv_table_decode(&config, buffer, length);

    // Write global settings, encoder settings are handled by bulk Xfer
	eeprom_write(EE_MIDI_CHANNEL, config.midiChannel - 1);
	
	eeprom_write(EE_BANK_SIDE_SW, config.sideIsBanked);
	eeprom_write(EE_SIDE_SW_1_FUNC, config.sideFunc1);
	eeprom_write(EE_SIDE_SW_2_FUNC, config.sideFunc2);
	eeprom_write(EE_SIDE_SW_3_FUNC, config.sideFunc3);
	eeprom_write(EE_SIDE_SW_4_FUNC, config.sideFunc4);
	eeprom_write(EE_SIDE_SW_5_FUNC, config.sideFunc5);
	eeprom_write(EE_SIDE_SW_6_FUNC, config.sideFunc6);
	eeprom_write(EE_SUPER_KNOB_START, config.superStart);
	eeprom_write(EE_SUPER_KNOB_END, config.superEnd);	
	eeprom_write(EE_RGB_BRIGHTNESS, config.rgb_brightness);
	eeprom_write(EE_IND_BRIGHTNESS, config.ind_brightness);
	eeprom_write(EE_COLOR_MAP, config.colorMap);
	uint8_t enc_ch = config.enc_animChannels > 0 ? config.enc_animChannels - 1 : DEF_ENCODER_ANIMATION_CH;
	uint8_t sw_ch  = config.sw_animChannels  > 0 ? config.sw_animChannels  - 1 : DEF_SWITCH_ANIMATION_CH;
	eeprom_write(EE_ANIMATION_CHANNELS, PACK_ANIM_CHANNELS(enc_ch, sw_ch));
	eeprom_write(EE_SLEEP_SETTINGS, PACK_SLEEP_SETTINGS(config.sleepTimeout, config.sleepAnimation));
	eeprom_write(EE_BANK_ANIMATIONS_ENABLED, config.bankAnimationsEnabled);
	// Utilities that predate these tags send nothing, which decodes as 0: keep the live value
	detent_settings_save(config.detentSize   > 0 ? config.detentSize - 1   : global_detent_size,
	                     config.deadZoneSize > 0 ? config.deadZoneSize - 1 : global_dead_zone_size);
	real_time_start;
	reset_idle_timer();
	setting_confirmation_animation(0x00FF00);
		
	// Load the new settings from EEPROM
    load_config();
	// Re-init encoder settings (that aren't saved in EEPROM, must stay after load_config)
	encoders_init();
	// Return the config to the utility
    send_config_data();
	// Rebuild the display 
	refresh_display();
	
	// Enable watchdog again
	//wdt_enable();
}

void send_config_data (void)
{
	cpu_irq_disable();
	
	side_sw_settings_t* side_cfg = get_side_switch_config();
	uint8_t sleep_settings = eeprom_read(EE_SLEEP_SETTINGS);
	
	cpu_irq_enable();
	
    uint8_t payload[] = {0xf0, 0x00, MANUFACTURER_ID >> 8, MANUFACTURER_ID & 0x7f,
                                SYSEX_COMMAND_PULL_CONF,	
                                0x1, // 0x0 = request, 0x1 = response
                                0 , midi_system_channel + 1,    // Not used ....
								1 , side_cfg->side_is_banked,
								2 , side_cfg->sw_action[0],
								3 , side_cfg->sw_action[1],
								4 , side_cfg->sw_action[2],
								5 , side_cfg->sw_action[3],
								6 , side_cfg->sw_action[4],
								7 , side_cfg->sw_action[5],
								8 , global_super_knob_start,
								9 , global_super_knob_end,
								31, global_rgb_brightness,
								32, global_ind_brightness,
								33, eeprom_read(EE_COLOR_MAP),
								34, GET_ENC_ANIM_CHANNEL(global_animation_channels) + 1,
								35, GET_SW_ANIM_CHANNEL(global_animation_channels) + 1,
								36, GET_SLEEP_TIMEOUT(sleep_settings),
								37, GET_SLEEP_ANIMATION(sleep_settings),
								38, global_bank_animations_enabled,
								39, global_detent_size + 1,
								40, global_dead_zone_size + 1,

                                0xf7};
								
    midi_stream_sysex(sizeof(payload), payload);
}

static void sysExCmdPullConfig (uint8_t length, uint8_t* buffer)
{
    // Change settings
    if (length > 0 && *buffer == 0x0) { // Received request
        send_config_data();
    }
}

void sysExCmdSystem (uint8_t, uint8_t*); // -Wmissing-prototype
/**
 * Shows a confirmation, then resets the unit.
 *
 * The system commands that change a boot time setting use this. The MIDI input
 * handler disables interrupts, thus this turns them on again. Without this, the
 * display does not show the confirmation.
 *
 * \param color [in]	24 bit confirmation color
 */
static void system_confirm_and_reset(uint32_t color)
{
	PMIC.CTRL = PMIC_LOLVLEN_bm | PMIC_MEDLVLEN_bm | PMIC_HILVLEN_bm;
	setting_confirmation_animation(color);

	USB_Disable();
	// Wait for USB disconnect to register on the host
	_delay_ms(2000);

	wdt_set_timeout_period(WDT_TIMEOUT_PERIOD_16CLK);
	wdt_enable();

	while(true){};
}

void sysExCmdSystem (uint8_t length, uint8_t* buffer)
{
    if (length == 0) return;
    
    switch (*buffer) {
    case 0:
        {
            // The MF Twister has no menu mode
        }
        break;
    case 1:
        {
            // Boot-loader mode
			wdt_disable();
			USB_Disable();
			// Wait USB disconnect to register on the host
			_delay_ms(2000);
            Jump_To_Bootloader();
        }
        break;
    case 2:
        {
			wdt_disable();	
            // Factory reset EEPROM
            config_factory_reset();
			// Interrupts are disabled during MIDI input processing
			// so we need to turn them back on to see display updates.
			PMIC.CTRL = PMIC_LOLVLEN_bm | PMIC_MEDLVLEN_bm | PMIC_HILVLEN_bm;
			setting_confirmation_animation(0x00FFFF);
			
			USB_Disable();	
			// Wait for USB disconnect to register on the host
			_delay_ms(2000);
			
			// Force a Reset
			wdt_set_timeout_period(WDT_TIMEOUT_PERIOD_16CLK);
			wdt_enable();
			
			while(true){};
        }
        break;
    case 3:
        {
			// Set the USB device number, 0 to NUM_DEVICE_IDS-1. The product ID
			// and the product string both follow it, thus the unit resets and
			// the host sees a different device.
			//
			// On macOS the new MIDI port sometimes comes back but does not pass
			// data, and the user must then disconnect the cable. This occurred one
			// time in four identity changes. Thus the host must not expect the port
			// to be usable immediately. A change of bank mode keeps the product ID,
			// and the port always came back.
			if (length < 2) { return; }
			if (buffer[1] >= NUM_DEVICE_IDS) { return; }

			wdt_disable();
			identity_set(buffer[1]);
			system_confirm_and_reset(0x0000FF);
        }
        break;
    case 4:
        {
			// Set the bank mode. The value is the bank count itself, 4 or 8. This
			// keeps the command easy to read on the wire.
			//
			// bank_mode_set() also gives defaults to the EEPROM pages that the
			// new mode reads. The reset is necessary, because encoders_alloc()
			// divides bank_arena at boot only.
			if (length < 2) { return; }
			if ((buffer[1] != MIN_BANKS) && (buffer[1] != MAX_BANKS)) { return; }
			if (buffer[1] == g_num_banks) { return; }

			wdt_disable();
			bank_mode_set(buffer[1]);
			system_confirm_and_reset(0xFF8000);
        }
        break;
    case 5:
        {
			// Read back the device number and the bank mode.
			//
			// Byte 0x0 is a request and 0x1 is a response, as in the get device
			// ID command. Thus the unit does not answer a response that comes
			// back to it.
			if ((length >= 2) && (buffer[1] != 0x00)) { return; }

			uint8_t payload[] = {
				0xF0, 0x00, MANUFACTURER_ID >> 8, MANUFACTURER_ID & 0x7F,
				SYSEX_COMMAND_SYSTEM,
				5,
				0x1,					// 0x0 = request, 0x1 = response
				g_device_identity,		// 0 to NUM_DEVICE_IDS-1
				NUM_DEVICE_IDS,
				g_num_banks,			// MIN_BANKS or MAX_BANKS
				MAX_BANKS,
				0xF7
			};

			midi_stream_sysex(sizeof(payload), payload);
        }
        break;
    default:
        break;
    }
}

/**********
Bulk Transfer Protocol:
    Bulk transfer sysex protocol is designed to support:
        - Multiple transfer modes (push, pull)
        - Multi-part transfer messages (each part payload has a maximum size of 24 bytes)
        - Maximum of 255 parts; maximum transfer size = 6KB
        - Up to 256 transfer "destinations" (identified by the tag)

    SysEx message format of a bulk transfer message:
    0xf0 0x0 0x1 0x79 0x4 CMD TAG CMD-SPECIFIC 0xf7
        CMD:        0   Push
                    1   Pull Request
        TAG:        A byte identifying the bulk data transfer
        Push:
            0xf0 0x0 0x1 0x79 0x4 0x0 TAG PART TOTAL SIZE PAYLOAD 0xf7
                PART:       The part number (1-based! Part 0 is not a valid part number)
                TOTAL:      Total number of parts
                SIZE:       Size of payload (must be 24 or lower)
                PAYLOAD:    SIZE number of bytes
        Pull:
            0xf0 0x0 0x1 0x79 0x4 0x1 TAG 0xf7
    If TAG os 0x0, then the format is instead:
    0xf0 0x0 0x1 0x79 0x4 CMD 0x0 TAG.0 TAG.1 CMD-SPECIFIC 0xf7
    Where TAG.0 and TAG.1 make up a two byte tag. Combined total number of available tags is therefore 65791


This implementation is hard coded to support only a subset of the protocol:

		0x1   Bank 1 Encoder 1 Configuration Data
		0x2   Bank 1 Encoder 2 Configuration Data .. and so on 
		
NOTE: Binary data must either avoid setting the MSB, or encode octets as packed septets, as MIDI will interpret octets with the MSB set as special SysEx commands.

**********/
//__attribute__((optimize("O0")))
static void sysExCmdBulkXfer(uint8_t length, uint8_t* buffer) // Process/ParseSysexMessage (incoming)
{   
    if (length > 2) {
		
        uint8_t command = *buffer++; // Push or pull
        uint8_t sysex_tag = *buffer++; // What is being transferred
        if (sysex_tag == 0) sysex_tag = NUM_BANKS * 16;
        
        if (command == 0) { // PUSH (Change/Update Settings)
            if (length > 5) {
                if (sysex_tag == 0x0) return; // Extended tags not supported
                // Reject out-of-range tags. The PULL path below already does this;
                // without it here a stray tag walks bank/encoder past the end of
                // encoder_settings[] and pushes EEPROM writes outside the encoder pages.
                if (sysex_tag > (NUM_BANKS*16)) return;
                uint8_t part = *buffer++; // Transfers may consist of multiple parts
                if (part == 0) return; // Invalid part number
                uint8_t total = *buffer++;
                UNUSED(total);
                uint8_t size = *buffer++;
                if (size > length - 5) return; // Not enough data to support payload
                
               // buffer+=1;
		// Encoder and Accompanying Push Switch are together consider one object of 'config[]'
		uint8_t bank    = (sysex_tag-1)/16;  // (starting at object 1) 16 Objects per Bank
		uint8_t encoder = (sysex_tag-1)%16;  // Encoder+Push Switch ID

		encoder_config_t config = {{0}};     // encoder_config_t is output map+ all settings

		// First we set all bytes to invalid values, this avoids
		// saving any settings which where not included in this xfer
		for(uint8_t i=0;i<ENC_CFG_SIZE;++i){
			config.bytes[i] = 0x80; // 8th bit is never set for valid data
		}

		// Now we decode the buffer
		uint8_t idx = 0;
		uint8_t tag;
		while (idx < size - 1) {  // While not hitting the last byte (0xF7)
			tag = buffer[idx++] - 10; //Match Utility Tag's  // (Sysex Tag 10) -> (Firmware Tag 0)
			if (tag < ENC_CFG_SIZE) {
				config.bytes[tag] = buffer[idx];
			}
			++idx;
		}

		// And save the new settings to eeprom
		save_encoder_config(bank, encoder, &config);

		// Disable the display until all config data is received
		display_disable();
		// Reset the watchdog timer to avoid a reset while processing the sysex.
		wdt_reset();
            }
            
        } else if (command == 1) { // PULL  (Request/Get Settings)
	
			uint8_t bank = 0;
			uint8_t encoder = 0;
			
			// If the sysex tag is invalid reply with an empty message
			if (sysex_tag > 0 && sysex_tag <= ((NUM_BANKS*16))) {
				bank = (sysex_tag-1) / 16;
				encoder = (sysex_tag-1) % 16;
			}
			
			// Read in the requested config data and add the tag values
			encoder_config_t enc_cfg;
			get_encoder_config(bank, encoder, &enc_cfg);
			
			// Ensure order matches encoder_settings_t structure order
			// The tags do not need to be consecutive, but it makes it 
			// easier to check for errors when debugging
			
			uint8_t config_data[] = { 10 , enc_cfg.has_detent,
									  11, enc_cfg.movement, // movement: aka sensitivity
									 12, enc_cfg.switch_action_type,
									 13, enc_cfg.switch_midi_channel+1,
									 14, enc_cfg.switch_midi_number,
									 15, 0x00,//enc_cfg.switch_midi_type,   //!Summer2016Update enc_cfg.encoder_shift_midi_channel
									 16, enc_cfg.encoder_midi_channel+1,
									 17, enc_cfg.encoder_midi_number,
									 18, enc_cfg.encoder_midi_type,
									 19, enc_cfg.active_color,
								     20, enc_cfg.inactive_color,
									 21, enc_cfg.detent_color,	
									 22, enc_cfg.indicator_display_type,
									 23, enc_cfg.is_super_knob,
									 24, enc_cfg.encoder_shift_midi_channel+1, // !Summer2016Update
									};
				
			// Total number of bytes to transfer
			uint8_t bytes_remaining = sizeof(config_data);
			//uint8_t cfg_total = bytes_remaining/2;
			// If the syex_tag was out of scope reply with no data
			if (sysex_tag < 1 || sysex_tag > (NUM_BANKS*16)) {bytes_remaining = 0;}
			// Total number of parts in transfer
			uint8_t total = (bytes_remaining / 24)+1;
			uint8_t index=0;
				
			for (uint8_t part=1; part<=total; ++part) {
				// Size, in bytes, of current part
				uint8_t size = bytes_remaining > 24 ? 24 : bytes_remaining;
				bytes_remaining -= 24;
				// Message template
				uint8_t tag_out = (sysex_tag == (NUM_BANKS*16)) ? 0 : sysex_tag;

				uint8_t payload[] = {0xf0, 0x00, MANUFACTURER_ID >> 8, MANUFACTURER_ID & 0x7f,
								SYSEX_COMMAND_BULK_XFER,
								0x0, // Command: 0x0 = push, 0x1 = pull
								tag_out,
								part, // Part 'part' of 'total'
								total,
								size, // 24 bytes maximum size
								0xf7,0xf7,0xf7,0xf7,0xf7,0xf7,0xf7,0xf7,0xf7,0xf7,0xf7,0xf7,
								0xf7,0xf7,0xf7,0xf7,0xf7,0xf7,0xf7,0xf7,0xf7,0xf7,0xf7,0xf7,0xf7};
									
				// Copy the data into the payload
				for (uint8_t idx=10; idx < size+10; ++idx) {
					payload[idx] = config_data[index++];
					
				}
                
				// Send the message
				midi_stream_sysex(11 + size, payload);
				MIDI_Device_Flush(g_midi_interface_info);
				// Reset the watchdog timer to avoid a reset while processing the sysex.
				wdt_reset();
			}
			
		}
    }
}

static void sysExCmdNativeMode(uint8_t length, uint8_t* buffer)
{
	native_mode_handle_sysex_command(--length, buffer);
}

static void sysExCmdGetDeviceId(uint8_t length, uint8_t* buffer)
{
	if (length > 0 && buffer[0] == 0x0) {
		uint8_t addrs[8] = {0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x10, 0x12};
		uint8_t payload[] = {
			0xF0, 0x00, MANUFACTURER_ID >> 8, MANUFACTURER_ID & 0x7F,
			SYSEX_COMMAND_GET_DEVICE_ID,
			0x1,                                     // 0x0 = request, 0x1 = response
			0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00,
			0xF7
		};
		for (uint8_t i = 0; i < 8; i++) {
			payload[6 + i] = nvm_read_production_signature_row(addrs[i]);
		}
		midi_stream_sysex(sizeof(payload), payload);
	}
}

void config_init(void)
{
  // Install SysEx command handlers
  sysex_install(SYSEX_COMMAND_PUSH_CONF, sysExCmdPushConfig);
  sysex_install(SYSEX_COMMAND_PULL_CONF, sysExCmdPullConfig);
  sysex_install(SYSEX_COMMAND_SYSTEM,    sysExCmdSystem);
  sysex_install(SYSEX_COMMAND_BULK_XFER, sysExCmdBulkXfer);
  sysex_install(SYSEX_COMMAND_GET_DEVICE_ID, sysExCmdGetDeviceId);
  sysex_install(SYSEX_COMMAND_NATIVE_MODE, sysExCmdNativeMode);
	  

	
	// If our EEPROM layout has changed, reset everything.
	if (eeprom_read(EE_EEPROM_VERSION) != EEPROM_LAYOUT) {
		config_factory_reset();
	}
}

void load_config(void)
{
	cpu_irq_disable();
	
	// Load system settings
	midi_system_channel = eeprom_read(EE_MIDI_CHANNEL);
	
	// Load side button settings
	side_sw_settings_t side_sw_cfg;
	
	side_sw_cfg.side_is_banked = eeprom_read(EE_BANK_SIDE_SW);
	side_sw_cfg.sw_action[0]   = eeprom_read(EE_SIDE_SW_1_FUNC);
	side_sw_cfg.sw_action[1]   = eeprom_read(EE_SIDE_SW_2_FUNC);
	side_sw_cfg.sw_action[2]   = eeprom_read(EE_SIDE_SW_3_FUNC);
	side_sw_cfg.sw_action[3]   = eeprom_read(EE_SIDE_SW_4_FUNC);
	side_sw_cfg.sw_action[4]   = eeprom_read(EE_SIDE_SW_5_FUNC);
	side_sw_cfg.sw_action[5]   = eeprom_read(EE_SIDE_SW_6_FUNC);
	
	global_super_knob_start	   = eeprom_read(EE_SUPER_KNOB_START);
	global_super_knob_end      = eeprom_read(EE_SUPER_KNOB_END);
	global_rgb_brightness      = eeprom_read(EE_RGB_BRIGHTNESS);
	global_ind_brightness      = eeprom_read(EE_IND_BRIGHTNESS);
	colorMap_init();
	global_animation_channels = eeprom_read(EE_ANIMATION_CHANNELS);
	uint8_t sleep_settings = eeprom_read(EE_SLEEP_SETTINGS);
	uint8_t timeout_index = GET_SLEEP_TIMEOUT(sleep_settings);
	if (timeout_index > 7) timeout_index = 0;
	sleep_timeout_minutes = sleep_timeout_map[timeout_index];
	sleep_animation_type  = GET_SLEEP_ANIMATION(sleep_settings);
	global_bank_animations_enabled = eeprom_read(EE_BANK_ANIMATIONS_ENABLED);
	// Added without an EEPROM layout bump, so an unwritten 0xFF gives the defaults
	uint8_t detent_settings = eeprom_read(EE_DETENT_SETTINGS);
	global_detent_size    = GET_DETENT_SIZE(detent_settings);
	global_dead_zone_size = GET_DEAD_ZONE_SIZE(detent_settings);
	if (global_detent_size > MAX_DETENT_SIZE)       global_detent_size    = DEF_DETENT_SIZE;
	if (global_dead_zone_size > MAX_DEAD_ZONE_SIZE) global_dead_zone_size = DEF_DEAD_ZONE_SIZE;
	
	side_switch_config(&side_sw_cfg);
	
	cpu_irq_enable();

	// White balance trim. It is outside the block above, by design. The trim has
	// its own EEPROM page and magic, and is not part of the layout versioned
	// settings. Thus a factory reset and a new EEPROM_LAYOUT both keep it.
	calibration_init();

	// Runtime USB identity. It must be live before the unit services the host.
	// This call site makes sure of it, because load_config() runs before the
	// first USB_USBTask().
	identity_init();
}

// This could be re-written to use config structures
// with defaults saved in PROGEM
void config_factory_reset(void)
{
	cpu_irq_disable();
	
	eeprom_write(EE_EEPROM_VERSION, EEPROM_LAYOUT);
	
	// System Settings
	
	eeprom_write(EE_MIDI_CHANNEL, DEF_MIDI_CHANNEL);
	
	// Side Switch Settings
	
	eeprom_write(EE_BANK_SIDE_SW,   DEF_BANK_SIDE_SW);
	eeprom_write(EE_SIDE_SW_1_FUNC, DEF_SIDE_SW_1_FUNC);
	eeprom_write(EE_SIDE_SW_2_FUNC, DEF_SIDE_SW_2_FUNC);
	eeprom_write(EE_SIDE_SW_3_FUNC, DEF_SIDE_SW_3_FUNC);
	eeprom_write(EE_SIDE_SW_4_FUNC, DEF_SIDE_SW_4_FUNC);
	eeprom_write(EE_SIDE_SW_5_FUNC, DEF_SIDE_SW_5_FUNC);
	eeprom_write(EE_SIDE_SW_6_FUNC, DEF_SIDE_SW_6_FUNC);
	eeprom_write(EE_SUPER_KNOB_START, DEF_SUPER_START_VALUE);
	eeprom_write(EE_SUPER_KNOB_END, DEF_SUPER_END_VALUE);
	eeprom_write(EE_RGB_BRIGHTNESS, DEF_RGB_BRIGHTNESS);
	eeprom_write(EE_IND_BRIGHTNESS, DEF_IND_BRIGHTNESS);
	eeprom_write(EE_COLOR_MAP, DEF_COLOR_MAP);
	eeprom_write(EE_ANIMATION_CHANNELS, PACK_ANIM_CHANNELS(DEF_ENCODER_ANIMATION_CH, DEF_SWITCH_ANIMATION_CH));
	eeprom_write(EE_SLEEP_SETTINGS, DEF_SLEEP_SETTINGS);
	eeprom_write(EE_BANK_ANIMATIONS_ENABLED, DEF_BANK_ANIMATIONS_ENABLED);
	eeprom_write(EE_DETENT_SETTINGS, PACK_DETENT_SETTINGS(DEF_DETENT_SIZE, DEF_DEAD_ZONE_SIZE));

	// A factory reset keeps the bank mode, as it keeps the device identity,
	// because the user selected it. It does not keep this flag. The flag
	// records that banks 5 to 8 hold real settings, and after this reset they
	// do not.
	eeprom_write(EE_BANK_HI_INIT, 0xFF);
	
	cpu_irq_enable();
	
	// Encoder Settings
	factory_reset_encoder_config();
}

