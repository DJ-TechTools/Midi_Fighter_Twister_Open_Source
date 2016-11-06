/*
 *  midi.h
 *
 *  Created: 6/28/2013 1:47:01 PM
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

#ifndef _MIDI_H_INCLUDED
#define _MIDI_H_INCLUDED

/*	Includes: */
	#include <asf.h>
	#include <stdbool.h>
	#include <stdint.h>
	#include <string.h>
	#include <avr/io.h>
	#include <stdlib.h>
	
	#include <LUFA/Drivers/USB/USB.h>
	#include <LUFA/Platform/Platform.h>
	#include <Descriptors.h>
	
	#include "encoders.h"
	#include "sysex.h"
	#include "sequencer.h"

/*	Macros: */

	// Define Midi Port Control Pins
	#define MIDI_EN		IOPORT_CREATE_PIN(PORTE, 0)
	#define MIDI_SEL    IOPORT_CREATE_PIN(PORTE, 1)
	
	// Define Legacy MIDI USART Pins
	#define MIDI_TX		IOPORT_CREATE_PIN(PORTE, 3)
	#define MIDI_RX     IOPORT_CREATE_PIN(PORTE, 2)
	
	// Legacy MIDI UASRT definitions
	#define USART_SERIAL                     &USARTE0
	#define USART_SERIAL_BAUDRATE            30670		//Should be 31250 ....
	#define USART_SERIAL_CHAR_LENGTH         USART_CHSIZE_8BIT_gc
	#define USART_SERIAL_PARITY              USART_PMODE_DISABLED_gc
	#define USART_SERIAL_STOP_BIT            true
	
	// Legacy MIDI FIFO Buffer definitions
	#define MIDI_FIFO_BUFFER_LENGTH			64
	
	// MIDI command definition missing from LUFA
	#define MIDI_1BYTE 0xF
	
	/*	Types: */
	
	// Type define for a USB-MIDI event packet.
	typedef struct USB_MIDI_EventPacket
	{
		unsigned char Command     : 4; // MIDI command number
		unsigned char CableNumber : 4; // Virtual cable number
		uint8_t Data1;                 // First byte of data in the MIDI event
		uint8_t Data2;                 // Second byte of data in the MIDI event
		uint8_t Data3;                 // Third byte of data in the MIDI event
	} USB_MIDI_EventPacket_t;

	typedef enum midi_port_type {
		USB_CONNECTION,
		SERIAL_CONNECTION,
	} midi_port_type_t;

	// MIDI global variables -------------------------------------------------------
	extern USB_ClassInfo_MIDI_Device_t* g_midi_interface_info;

	// Remove these globals - DOWN WITH GLOBALIZATION 
	#define MIDI_MFR_ID_0           0x00
	#define MIDI_MFR_ID_1           0x01
	#define MIDI_MFR_ID_2           0x79
	#define MANUFACTURER_ID       0x0179

	#define MIDI_MAX_NOTES	         127      // Number of notes to track.
	#define MIDI_MAX_SYSEX		     127

	// The number of channels we track MIDI input for
	#define NUM_OF_CH				  4

	extern uint8_t g_midi_sysex_channel;

	extern bool g_midi_sysex_is_reading;
	extern bool g_midi_sysex_is_valid;
	extern char* g_midi_sysex_ptr;
	extern char g_midi_sysex_buffer[MIDI_MAX_SYSEX];
	
	// MIDI function prototypes ----------------------------------------------------

	void	midi_init(void);

	void	midi_flush(void);

	void	midi_stream_raw_note(const uint8_t channel,
								 const uint8_t pitch,
								 const bool onoff,
								 const uint8_t velocity);

	void	midi_stream_raw_cc(const uint8_t channel,
							   const uint8_t cc,
							   const uint8_t value);

	void	midi_stream_sysex (const uint8_t length, uint8_t* data);

	// MIDI Real time message handlers
	void	midi_clock_tick(void);
	void	real_time_start(void);
	void    real_time_stop(void);
	
	// MIDI Clock for Animations
	extern bool midi_clock_enabled;  
	void midi_clock_enable(bool state);

	uint16_t get_counts_per_tick(void);	
	bool clock_is_stable(void);

	// Sequencer Only
	uint8_t getTickCount(void);

	bool midi_is_usb(void);
	
	void set_midi_serial(void);
	
	void MIDI_send_legacy_packet(const MIDI_EventPacket_t* const Event);

	void process_midi_packet(MIDI_EventPacket_t input_event);
	
	void process_legacy_packet(void);

#endif // _MIDI_H_INCLUDED
