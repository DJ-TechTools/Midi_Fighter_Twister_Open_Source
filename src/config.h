/*
 * config.h
 *
 * Created: 9/16/2013 4:31:44 PM
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

#ifndef CONFIG_H_
#define CONFIG_H_

	/*	Includes: */
		#include <asf.h>
		#include <string.h>
	
		#include "sysex.h"
		#include "eeprom.h"
		#include "jump_to_bootloader.h"
		
		// Include all objects which require config
		#include "side_switch.h"
		#include "encoders.h"
	
	/*	Macros: */
	
		// SysEx command constants
			
		#define SYSEX_COMMAND_PUSH_CONF    0x1
		#define SYSEX_COMMAND_PULL_CONF    0x2
		#define SYSEX_COMMAND_SYSTEM       0x3
		#define SYSEX_COMMAND_BULK_XFER    0x4
		
	/* Typedefs: */
		
		#define GLOBAL_TABLE_SIZE 12
		// The global table holds the Twister global settings
		typedef union {
			struct {
				// Message data                TAG
				uint8_t midiChannel;        
				uint8_t sideIsBanked;
				uint8_t sideFunc1;
				uint8_t sideFunc2;
				uint8_t sideFunc3;
				uint8_t sideFunc4;
				uint8_t sideFunc5;
				uint8_t sideFunc6;
				uint8_t superStart;
				uint8_t superEnd;
				uint8_t rgb_brightness;
				uint8_t ind_brightness;
			};
			uint8_t bytes[GLOBAL_TABLE_SIZE];
		} global_tvtable_t;	
		
		// TOOD MOVE TO GLOBAL SETTINGS STRUCTURE
		uint8_t global_super_knob_start;
		uint8_t global_super_knob_end;
		uint8_t global_rgb_brightness;
		uint8_t global_ind_brightness;
		uint8_t midi_system_channel;
	/* Function Prototypes: */
	
		void config_init (void);
		void load_config(void);
		void send_config_data (void);
		void config_factory_reset(void);
		
	
	/*	Inline Functions: */
	
		/**
		 * Writes an 8 bit value to EEPROM
		 * If your code uses program space access in interrupts you must disable those
		 * interrupts before calling this function
		 *
		 * \param [in] address	The location to write to
		 *
		 * \param [in] data		The data to write (8 Bit)
		 *
		 */
		 static inline void eeprom_write(uint16_t address, uint8_t data)
		 {
			nvm_eeprom_write_byte(address, data);
		 }
		 
		 /**
		  * Reads an 8-bit value from EEPROM memory.
		  * If your code uses program space access in interrupts you must disable those
		  * interrupts before calling this function
		  * 
          * \param [in]	The address to read from
          *
          */
         static inline uint8_t eeprom_read(uint16_t address)
          {	
			return nvm_eeprom_read_byte(address);	
		  }
		  
		 /**
		  * If the input data is valid (data < 0x80) save that data to the sepcified 
		  * setting, if it is invalid do nothing.
		  * 
		  * \param [in]	data
		  *
		  * \param [in] setting	
		  * 
		  * \return true if saved false if not saved
		  */
		 
		 static inline bool save_data_if_valid(uint8_t data, uint8_t *setting){
			 if (data > 0x7F){
				 return false;
			 } else {
				 *setting = data;
			 }
			 return true;
		 }
		 
		  

#endif /* CONFIG_H_ */