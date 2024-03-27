// Sysex Handling functions
//
//   Copyright (C) 2012 DJ Tech Tools
//
// DanielKersten 2011
//
// DJTT - MIDI Fighter Twister - Embedded Software License
// Copyright (c) 2016: DJ Tech Tools
// Permission is hereby granted, free of charge, to any person owning or possessing 
// a DJ Tech-Tools MIDI Fighter Twister Hardware Device to view and modify this source 
// code for personal use. Person may not publish, distribute, sublicense, or sell 
// the source code (modified or un-modified). Person may not use this source code 
// or any diminutive works for commercial purposes. The permission to use this source 
// code is also subject to the following conditions:
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, 
// INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,  FITNESS FOR A 
// PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT 
// HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION 
// OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE 
// SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


#include <sysex.h>

uint8_t sysex_buffer[MIDI_MAX_SYSEX];
enum {
	State_Begin = 0,    // Beginning of new message, state not yet known
	State_CheckMID,     // Need to verify 3rd byte of manufacturer ID
	State_Invalid,       // Message is not for us or malformed
	
	// Different types of messages to handle
	State_NonRealtime,  // Non Realtime Sysex message
	State_DJTT,         // Manufacturer ID verified as DJTT manufacturer ID
} sysex_state = State_Begin;

#define MAX_COMMAND 8
SysExFn sysExCommandMap[MAX_COMMAND] = {0,};

static void __attribute__((optimize("O0"))) sysex_handle (uint8_t length)
{
	if (sysex_state == State_DJTT && length > 0) {
		// This is a DJTT SysEx message
		
		// First byte is the command byte
		uint8_t command = sysex_buffer[0];
		// Make sure the command number is in range and a handler is installed
		if (command > 0 && command <= MAX_COMMAND && sysExCommandMap[command - 1] != 0) {
			// Run the command
			sysExCommandMap[command - 1](length - 1, &(sysex_buffer[1]) /* skip the command byte */);
		}
	}
	else if (sysex_state == State_NonRealtime) { // Universal Non Real Time
		if (length >= 2 && // Make sure there are at least enough bytes for this to be an identify request
		sysex_buffer[0] == 0x06 &&
		sysex_buffer[1] == 0x01) {
			// Device identify request; send a Device Identify Response
			// Need to find new method of sending device serial as this caused invalid sysex bytes.
			//uint8_t serialByte1 = (uint8_t)((G_EE_UNIT_NUMBER>>0)  & 0xFF);
			//uint8_t serialByte2 = (uint8_t)((G_EE_UNIT_NUMBER>>8)  & 0xFF);
			//uint8_t serialByte3 = (uint8_t)((G_EE_UNIT_NUMBER>>16) & 0xFF);
			//uint8_t serialByte4 = (uint8_t)((G_EE_UNIT_NUMBER>>24) & 0xFF);
			
			uint8_t payload[] = {0xf0, 0x7e, 0x7f, 0x06, 0x02,
				0x00, MANUFACTURER_ID >> 8, MANUFACTURER_ID & 0x7f,
				DEVICE_FAMILY,
				DEVICE_MODEL,
				(uint8_t)(((uint16_t)DEVICE_VERSION_YEAR) >> 8),
				DEVICE_VERSION_YEAR & 0x7f,
				DEVICE_VERSION_MONTH,
				DEVICE_VERSION_DAY,
				//serialByte1 & 0x7f,
				//serialByte2,
				//serialByte3,
				//serialByte4,
			0xf7};
			midi_stream_sysex(/* 17 bytes */ sizeof(payload), payload);
		}
	}
}


void sysex_install_ (uint8_t cmd, SysExFn fn)
{
	if (cmd > 0 && cmd <= MAX_COMMAND) {
		sysExCommandMap[cmd-1] = fn;
	}
}


bool sysex_is_reading = false;
uint8_t* sysex_ptr = NULL;

// calculate the next byte after the end of the sysex buffer.
const uint8_t* buffer_end = sysex_buffer + MIDI_MAX_SYSEX;

// Handle a 3-byte start or continue message
void sysex_handle_3sc (MIDI_EventPacket_t* packet)
{
	if (!sysex_is_reading) {
		// Start a new sysex block.
		sysex_is_reading = true;
		// restart the sysex pointer.
		sysex_ptr = sysex_buffer;
		
		// check to see whether this sysex block is for us.
		if (packet->Data1 == 0xf0 &&
		packet->Data2 == 0x7e &&
		(packet->Data3 == g_midi_sysex_channel ||
		packet->Data3 == 0x7f)) {
			// Non realtime sysex
			// yay, it's for us!
			sysex_state = State_NonRealtime;
			
		} else if (packet->Data1 == 0xf0 &&
		packet->Data2 == MIDI_MFR_ID_0 &&
		packet->Data3 == MIDI_MFR_ID_1) {
			// Might be for us...
			sysex_state = State_CheckMID;
			
			} else {
			// Its not for us
			sysex_state = State_Invalid;
		}
		
		} else if (sysex_state == State_CheckMID) {
		// Need to check third byte of manufacturer ID
		if (packet->Data1 == MIDI_MFR_ID_2 &&
		(sysex_ptr + 2) < buffer_end) { // Should be the first two bytes read into buffer, so can't really overflow here, but check anyway
			// Its for us!
			sysex_state = State_DJTT;
			*sysex_ptr++ = packet->Data2;
			*sysex_ptr++ = packet->Data3;
			
			
			} else {
			// Its not for us
			sysex_state = State_Invalid;
		}
		
		} else {
		// Sysex continues with three new bytes.
		if (sysex_state == State_Invalid) return; // Ignore until we get an end
		
		// check bounds before inserting anything.
		if ( (sysex_ptr + 3) < buffer_end ) {
			*sysex_ptr++ = packet->Data1;
			*sysex_ptr++ = packet->Data2;
			*sysex_ptr++ = packet->Data3;

			} else {
			// we overflowed the buffer.
			// this is a bad message, reject the rest.
			sysex_state = State_Invalid;
		}
	}
}

// Handle a 3-byte end message
void sysex_handle_3e (MIDI_EventPacket_t* packet)
{
	// 3-byte End of Sysex
	sysex_is_reading = false;
	
	if (sysex_state == State_CheckMID) {
		// Need to check third byte of manufacturer ID
		if (packet->Data1 == MIDI_MFR_ID_2 &&
		sysex_ptr + 2 < buffer_end) { // Should be the first two bytes read into buffer, so can't really overflow here, but check anyway
			// Its for us!
			sysex_state = State_DJTT;
			*sysex_ptr++ = packet->Data2;
			*sysex_ptr++ = packet->Data3;
			
			// Process the message
			sysex_handle((uint8_t)(sysex_ptr - sysex_buffer));
		}
		} else if (sysex_state != State_Invalid) {
		// check for buffer overflow
		if (sysex_ptr + 3 < buffer_end) {
			// NOTE: always going to be 0xF7 - so why bother?
			*sysex_ptr++ = packet->Data1;
			*sysex_ptr++ = packet->Data2;
			*sysex_ptr++ = packet->Data3;
			
			// Process the message
			sysex_handle((uint8_t)(sysex_ptr - sysex_buffer));
		}
	}
	
	// Reset state for next message
	sysex_state = State_Begin;
}

// Handle a 2-byte end message
void sysex_handle_2e (MIDI_EventPacket_t* packet)
{
	if (sysex_is_reading)
	{
		// 2-byte End of sysex
		sysex_is_reading = false;
		
		if (sysex_state != State_Invalid) {
			// check for buffer overflow
			if (sysex_ptr + 2 < buffer_end) {
				// NOTE: always going to be 0xF7 - so why bother?
				*sysex_ptr++ = packet->Data1;
				*sysex_ptr++ = packet->Data2;
				
				// Process the message
				sysex_handle((uint8_t)(sysex_ptr - sysex_buffer));
			}
		}
		
		// Reset state for next message
		sysex_state = State_Begin;
	}
	else{}
}

// Handle a 1-byte end message
void sysex_handle_1e (MIDI_EventPacket_t* packet)
{
	// Either a 1-byte System Common message or a 1-byte End Of Sysex.
	if (sysex_is_reading) {
		// finished reading sysex
		sysex_is_reading = false;
		
		if (sysex_state != State_Invalid) {
			// check for buffer overflow
			if (sysex_ptr + 1 < buffer_end) {
				// NOTE: always going to be 0xF7 - so why bother?
				*sysex_ptr++ = packet->Data1;
				
				// Process the message
				sysex_handle((uint8_t)(sysex_ptr - sysex_buffer));
			}
		}
		
		// Reset state for next message
		sysex_state = State_Begin;
		} else {
		// single byte System Common Message
	}
}

