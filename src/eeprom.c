/*
 *  eeprom.c
 *
 *  Created: 9/17/2013 10:02:25 AM
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
 *
 */ 

#include "eeprom.h"

// EEPROM Access Functions ----------------------------------------------------

// EEPROM Setting Functions ---------------------------------------------------

/**
 * Set up the EEPROM system for use and read out the settings into the
 * global values.
 *
 * This includes checking the layout version and, if the version tag written
 * to the EEPROM doesn't match this software version we're running, we reset
 * the EEPROM values to their default settings. 
 * 
 */
void eeprom_init(void)
{
	// If our EEPROM layout has changed, reset everything.
	if (eeprom_read(EE_EEPROM_VERSION) != EEPROM_LAYOUT) {
		eeprom_factory_reset();
	}
	
}


/**
 * Resets all EEPROM settings to factory default 
 */

void eeprom_factory_reset(void)
{
	// Set the eeprom layout version
	eeprom_write(EE_EEPROM_VERSION, EEPROM_LAYOUT);
	
	// Write the setting defaults
	eeprom_write(EE_MIDI_CHANNEL, 0);
}

/**
 * Saves all current system settings to EEPROM 
 */
void eeprom_save_settings(void)
{
	
}