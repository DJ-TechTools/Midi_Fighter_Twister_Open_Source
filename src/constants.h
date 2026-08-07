/*
 * constants.h
 *
 * Created: 9/16/2013 4:34:41 PM
 * Author: Michael 
 *
 * DJTT - Midi Fighter Twister - Embedded Software License
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


#ifndef CONSTANTS_H_
#define CONSTANTS_H_

// Versioning: Version is Firmware Release Date
//#define DEVICE_VERSION_YEAR 	0x2014
//#define DEVICE_VERSION_MONTH	0x08
//#define DEVICE_VERSION_DAY	0x13
//#define DEVICE_VERSION  		((DEVICE_VERSION_YEAR << 16) | (DEVICE_VERSION_MONTH << 8) | DEVICE_VERSION_DAY)

#define DEVICE_VERSION_YEAR 	0x2026
#define DEVICE_VERSION_MONTH	0x08
#define DEVICE_VERSION_DAY		0x07
#define DEVICE_VERSION  		((DEVICE_VERSION_YEAR << 16) | (DEVICE_VERSION_MONTH << 8) | DEVICE_VERSION_DAY)

// 14bit device family ID, LSB first
#define DEVICE_FAMILY_LSB		0x05
#define DEVICE_FAMILY_MSB		0x00
#define DEVICE_FAMILY   		DEVICE_FAMILY_LSB, DEVICE_FAMILY_MSB

// 14bit device model ID, LSB first
#define DEVICE_MODEL_LSB		0x01
#define DEVICE_MODEL_MSB		0x00
#define DEVICE_MODEL    		DEVICE_MODEL_LSB, DEVICE_MODEL_MSB

// MIDI Constants ---------------------------------------------------------

// DJTechTools MIDI Manufacturer ID
// see http://www.midi.org/techspecs/manid.php

#define MIDI_MFR_ID_0           0x00
#define MIDI_MFR_ID_1           0x01
#define MIDI_MFR_ID_2           0x79
#define MANUFACTURER_ID			0x0179

// Channel & Offset Definitions -------------------------------------------

// Channel numbers start from ZERO! The corresponding user manual entries should be 1-based as they
// are in most user interfaces. (i.e. this number plus one)

#define ENCODER_ROTARY_CHANNEL	  0
#define ENCODER_SWITCH_CHANNEL	  1
#define SIDE_SWITCH_OFFSET        8
#define SHIFT_OFFSET             44
//#define ENCODER_CHANNEL	0 // Defined elsewhere
//#define SWITCH_CHANNEL	1 // Defined elsewhere
//#define ENCODER_ANIMATION_CHANNEL 5  //old hardcoded value
//#define DEF_MIDI_CHANNEL		3 // TWISTER DEFAULT SETTINGS Channel for changing banks
//#define ENCODER_SHIFTED_CHANNEL 4 // Defined elsewhere
//#define SWITCH_ANIMATION_CHANNEL  2 //old hardcoded value


// Debug & Test Harness Definitions ---------------------------------------
#define SEQUENCER_TEST_CHANNEL   15
#define CLOCK_TICK_RECEIVED		  0 
#define SIXTEENTH_TRIGGER		  1


// System Constants -------------------------------------------------------
#define EXTENDED_BANKS  // Comment out to use sequencer + 4 banks

#ifdef EXTENDED_BANKS
#define NUM_BANKS 8
#else
#define NUM_BANKS 4
#endif

// Alternate USB ID Selection Constant

#define USE_ALTERNATE_ID 0


// EEPROM Constants -------------------------------------------------------
#define EEPROM_LAYOUT			         8	//The EEPROM layout version

// EEPROM Memory Locations for configurable settings

#define EE_EEPROM_VERSION			0x0000	//The location we store the EEPROM LAYOUT VERSION
#define EE_MIDI_CHANNEL				0x0001  //The Base MIDI Channel
#define EE_SELF_TEST_FLAG			0x0002  //Internal self test flag

#define EE_BANK_SIDE_SW				0x0003	//Bank Side Setting
#define EE_SIDE_SW_1_FUNC			0x0004	//Side Switch 1 function 
#define EE_SIDE_SW_2_FUNC			0x0005  //Side Switch 2 function
#define EE_SIDE_SW_3_FUNC			0x0006  //Side Switch 3 function
#define EE_SIDE_SW_4_FUNC			0x0007  //Side Switch 4 function
#define EE_SIDE_SW_5_FUNC			0x0008  //Side Switch 5 function
#define EE_SIDE_SW_6_FUNC			0x0009  //Side Switch 6 function
#define EE_SUPER_KNOB_START			0x000A  //Super Knob Secondary CC start point
#define EE_SUPER_KNOB_END			0x000B  //Super Knob Secondary CC start point
#define EE_RGB_BRIGHTNESS			0x000C  //Global brightness setting for RGB
#define EE_IND_BRIGHTNESS           0x000D  //Global brightness setting for indicators
#define EE_COLOR_MAP				0x000E  //Switch between 2016 and 2026 color palettes
#define EE_ANIMATION_CHANNELS		0x000F	//Encoder and switch animation channels
#define EE_SLEEP_SETTINGS			0x0010  // Timeout (6 bits) + Animation (2 bits)
#define EE_BANK_ANIMATIONS_ENABLED	0x0011	// Bank change animations on/off

#define PACK_SLEEP_SETTINGS(timeout, anim)   (((timeout) & 0x3F) | (((anim) & 0x03) << 6))
#define GET_SLEEP_TIMEOUT(val)               ((val) & 0x3F)
#define GET_SLEEP_ANIMATION(val)             (((val) >> 6) & 0x03)

#define EE_ENC_SETTING_START		0x0020  //Start of encoder settings
#define EE_HAS_DETENT_OFFSET		0x0000  //Has Detent setting offset			    //
#define EE_MOVEMENT_OFFSET			0x0001  //Movement setting offset               //
#define EE_SW_ACT_TYPE_OFFSET		0x0002  //Switch Action Type offset				//
#define EE_SW_MIDI_CH_OFFSET		0x0003  //Switch MIDI Type & Number				//
#define EE_SW_MIDI_NUM_OFFSET		0x0004  //Switch MIDI Type & Number offset      //
#define EE_SW_MIDI_TYPE_OFFSET		0x0005  //Switch MIDI Type & Number offset      //
#define EE_ENC_MIDI_CH_OFFSET		0x0006  //Switch MIDI Type & Number             //
#define EE_ENC_MIDI_NUM_OFFSET		0x0007  //Switch MIDI Type & Number offset      //
#define EE_ENC_MIDI_TYPE_OFFSET		0x0008  //Switch MIDI Type & Number offset      //
#define EE_DETENT_COLOR_OFFSET		0x000B  //Detent Color offset                   //
#define EE_INDICATOR_TYPE_OFFSET	0x000C  //Indicator Style offset
#define EE_ACTIVE_COLOR_OFFSET		0x0002  //Active Color offset
#define EE_INACTIVE_COLOR_OFFSET	0x0003  //Inactive Color offset

#define ENC_EE_SIZE 8

#define DEV_SETTINGS_START_PAGE      0
#define ENC_SETTINGS_START_PAGE		 1
#define SEQ_EEPROM_START_PAGE		31

// Defaults -------------------------------------------------------------------
// System 
#define DEF_MIDI_CHANNEL			3

// Side Buttons
#define DEF_BANK_SIDE_SW			true
#define DEF_SIDE_SW_1_FUNC			CC_HOLD_SS
#define DEF_SIDE_SW_2_FUNC			GLOBAL_BANK_DOWN
#define DEF_SIDE_SW_3_FUNC			CC_HOLD_SS
#define DEF_SIDE_SW_4_FUNC			CC_HOLD_SS
#define DEF_SIDE_SW_5_FUNC			GLOBAL_BANK_UP
#define DEF_SIDE_SW_6_FUNC			CC_HOLD_SS

#define DEF_SUPER_START_VALUE		63
#define DEF_SUPER_END_VALUE			127
#define DEF_RGB_BRIGHTNESS			127
#define DEF_IND_BRIGHTNESS			127
#define DEF_COLOR_MAP				0x00
#define DEF_ENCODER_ANIMATION_CH    5
#define DEF_SWITCH_ANIMATION_CH     2

#define DEF_SLEEP_TIMEOUT			7		// Index 7 = 60 minutes
#define DEF_SLEEP_ANIMATION			1		// Default 0 = lights off, 1 = rainbow wave
#define DEF_SLEEP_SETTINGS			PACK_SLEEP_SETTINGS(DEF_SLEEP_TIMEOUT, DEF_SLEEP_ANIMATION)
#define DEF_BANK_ANIMATIONS_ENABLED	true



//Encoder
#define DEF_ENC_DETENT          false
#define DEF_ENC_MOVEMENT        DIRECT
#define DEF_SW_ACTION           CC_HOLD
#define DEF_SW_MIDI_TYPE		CC_HOLD			// Deprecated - should be REMOVED
#define DEF_ENC_MIDI_TYPE       SEND_CC			// Default to CC define as 0x00 for Note
#define DEF_ENC_CH		        0x00            // Enc def Ch = 0
#define DEF_SW_CH		        0x01            // SW  def Ch = 1
#define DEF_ENC_SHIFT_CH		0x04			// !Summer2016Update: encoder_shift_midi_channel -> Encoder shifted channel def
#define DEF_ACTIVE_COLOR		51
#define DEF_INACTIVE_COLOR      1

#define DEF_ACTIVE_COLOR_BANK1_CLASSIC 25
#define DEF_INACTIVE_COLOR_BANK1_CLASSIC 5
// Bank 2
#define DEF_ACTIVE_COLOR_BANK2_CLASSIC 25
#define DEF_INACTIVE_COLOR_BANK2_CLASSIC 84
// Bank 3
#define DEF_ACTIVE_COLOR_BANK3_CLASSIC 25
#define DEF_INACTIVE_COLOR_BANK3_CLASSIC 53
// Bank 4
#define DEF_ACTIVE_COLOR_BANK4_CLASSIC 25
#define DEF_INACTIVE_COLOR_BANK4_CLASSIC 13
// Bank 5
#define DEF_ACTIVE_COLOR_BANK5_CLASSIC 25
#define DEF_INACTIVE_COLOR_BANK5_CLASSIC 75
// Bank 6 Bank
#define DEF_ACTIVE_COLOR_BANK6_CLASSIC 25
#define DEF_INACTIVE_COLOR_BANK6_CLASSIC 67
// Bank 7 Bank
#define DEF_ACTIVE_COLOR_BANK7_CLASSIC 25
#define DEF_INACTIVE_COLOR_BANK7_CLASSIC 106
// Bank 8 Bank
#define DEF_ACTIVE_COLOR_BANK8_CLASSIC 25
#define DEF_INACTIVE_COLOR_BANK8_CLASSIC 44

// !Summer2016Update mark: Default Colors (last used in build 20160615)
//~ #define DEF_ACTIVE_COLOR_BANK1		25
//~ #define DEF_INACTIVE_COLOR_BANK1      0
//~ #define DEF_ACTIVE_COLOR_BANK2		69
//~ #define DEF_INACTIVE_COLOR_BANK2      0
//~ #define DEF_ACTIVE_COLOR_BANK3		100
//~ #define DEF_INACTIVE_COLOR_BANK3      0
//~ #define DEF_ACTIVE_COLOR_BANK4		88
//~ #define DEF_INACTIVE_COLOR_BANK4      0

#define DEF_DETENT_COLOR        63
#define DEF_INDICATOR_TYPE      BLENDED_BAR
#define DEF_IS_SUPER_KNOB       false

#endif /* CONSTANTS_H_ */
