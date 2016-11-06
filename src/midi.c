/*
 * midi.c
 *
 *	Created: 7/16/2013 12:06:05 PM
 *  Author: Michael Mitchell
 * 
 *  Contains all MIDI handling functions for the Midi Fighter Twister
 *  Uses some code taken from Robin Greens midi library for the Midi Fighter Classic 
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
#include "midi.h"
//#include "display_driver.h"

static midi_port_type_t midi_port_mode;
bool midi_clock_enabled = false;
uint8_t g_midi_sysex_channel = 5;

uint16_t calculate_bpm(void);
uint8_t get_bpm(void);
int32_t update_clock_counter(void);

// Interface object for the high level LUFA MIDI Class Drivers. This gets
// passed into every MIDI call so it can potentially keep track of many
// interfaces. The Midi fighter only needs the one.
static USB_ClassInfo_MIDI_Device_t g_midi_interface =
{
	.Config =
	{
		.StreamingInterfaceNumber = 1,
		.DataINEndpoint           =
		{
			.Address          = MIDI_STREAM_IN_EPADDR,
			.Size             = MIDI_STREAM_EPSIZE,
			.Banks            = 1,
		},
		.DataOUTEndpoint          =
		{
			.Address          = MIDI_STREAM_OUT_EPADDR,
			.Size             = MIDI_STREAM_EPSIZE,
			.Banks            = 1,
		},
	},
};

// FIFO descriptor for legacy MIDI FIFO buffer
fifo_desc_t fifo_desc;

USB_ClassInfo_MIDI_Device_t* g_midi_interface_info;

// Setup USART options for Legacy MIDI
static usart_serial_options_t usart_options = {
	.baudrate   = USART_SERIAL_BAUDRATE,
	.charlength = USART_SERIAL_CHAR_LENGTH,
	.paritytype = USART_SERIAL_PARITY,
	.stopbits	= USART_SERIAL_STOP_BIT
};


// MIDI System Functions ------------------------------------------------------

// Initialize the MIDI system
void midi_init(void)
{
    // Set up the global LUFA MIDI class interface pointer.
    g_midi_interface_info = &g_midi_interface;
	
	// Initialize the MIDI port control Pins
	ioport_set_pin_dir(MIDI_EN, IOPORT_DIR_OUTPUT);
	ioport_set_pin_dir(MIDI_SEL, IOPORT_DIR_OUTPUT);
	ioport_set_pin_dir(MIDI_TX, IOPORT_DIR_OUTPUT);
	ioport_set_pin_dir(MIDI_RX, IOPORT_DIR_INPUT);
	
	// Enable the MIDI port switch
	ioport_set_pin_level(MIDI_EN, false);

	// Initialize the midi port type to USB
	midi_port_mode = USB_CONNECTION;	
	ioport_set_pin_level(MIDI_SEL, false);		
}


// Maximum task scheduling window is ~2 seconds, this is to short to reliably
// detect no USB connection, because of this we use a counter variable allow
// a longer window of ~8 seconds
static uint8_t counts = 0;

/* Sets MIDI to use serial (legacy) */
void set_midi_serial(void)
{
	if (counts >= 3)	{
		
		midi_port_mode = SERIAL_CONNECTION;
		USB_Disable();
		display_enable();
		// Display the start up animation
		run_sparkle(6);
		// Build the display
		set_op_mode(normal);
		refresh_display();

		wdt_set_timeout_period(WDT_TIMEOUT_PERIOD_64CLK);
		wdt_enable();
	
		// Select the legacy connection and enable the UART ...
		usart_serial_init(USART_SERIAL, &usart_options);
		ioport_set_pin_level(MIDI_SEL, true);
		
		// ... and init the FIFO buffer
		static uint8_t midi_fifo_buffer[MIDI_FIFO_BUFFER_LENGTH];
		fifo_init(&fifo_desc, midi_fifo_buffer, MIDI_FIFO_BUFFER_LENGTH);
		
		// Finally enable the USARTE0 Rx interrupt at low priority
		USARTE0_CTRLA |= USART_RXCINTLVL0_bm;
	
	} else {
		schedule_task(set_midi_serial, 0xFFFF);
		counts++;
	}
}

/**
 *  Returns true if using USB MIDI, false if using Legacy MIDI
**/
bool midi_is_usb(void)
{
	if(midi_port_mode == USB_CONNECTION)
	{
		return true;
	} else {
		return false;
	}
}


/* Send a 16 bit value as a song position pointer message for easy debugging */
void debug_16_bit_value(uint16_t value)
{
	uint8_t data[3];
	data[0] = 0xF2;
	data[2] = (uint8_t)(value & 0x3F);
	data[1] = (uint8_t)((value >> 8) & 0x3F);

	midi_stream_sysex(3,&data);
}

/**
 * Calculates the average number of counts between clock ticks 
 * 
 * \return The current average
 */

static volatile uint16_t prev_count;
static volatile int32_t average;
static volatile bool clock_stable;

int32_t update_clock_counter(void)
{
	uint16_t new_count = tc_read_count(&TCC1);
	
	int16_t delta = 0;
	
	// Check for overflow 
	if (new_count > prev_count) {
		delta = ((new_count - prev_count) - average)/8;		
	} else {
		delta = ((abs(((2^16)-prev_count)-1)+new_count) - average)/8;
	}
	
	average += delta;
	//debug_16_bit_value(abs(average));

	// We only allow the clock to become registered as stable on the 2nd tick
	// otherwise we can get a race condition where the clock becomes stable after
	// the point at which it schedules triggers.
	if((abs(delta) < 5) && (getTickCount() == 1)){
		clock_stable = true;
		if(seq_traktor_mode && sequencerDisplayState == OFF){
			sequencerDisplayState = DEFAULT;
		}
	} else {
		//clock_stable = false;
	}
	
	//debug_16_bit_value(abs(delta));
	
	// Store current count for next time
	prev_count = new_count;
	
	return average;
}

uint16_t get_counts_per_tick(void)
{
	return average;
}

bool clock_is_stable(void)
{
	if (clock_stable){
		return true;
	} 
	return false;
}

static int8_t tick_counter = 0;

// Handler for Midi Clock tick, this counts from 0 to 23
// The MIDI USB CABLE HAS CRAZY COCK JITTER - this causes timing issues
void midi_clock(void)
{
	// !Summer2016Update: midi_clock animation
	uint16_t counts = update_clock_counter();
	seq_midi_clock_handler(tick_counter);

	// If not enabled enable MIDI Clock for animations 
	// - !Summer2016Update: midi clock for animations
	if(!midi_clock_enabled){animation_counter=0;midi_clock_enable(true);}

	// Increment the tick counter
	tick_counter+=1;
	if(tick_counter >= 24) {
		tick_counter = 0;
	}

	// MIDI Clock for Animations Servicing - !Summer2016Update: midi clock for animations
	if (tick_counter % 3 == 2) // Increment animation_counter every 3rd clock tick.
	{
		animation_counter += 1;
	}
}

void midi_clock_enable(bool state) // !Summer2016Update: midi clock for animations
{
	if (state) 
	{
		midi_clock_enabled =  true;
	}
	else       
	{
		midi_clock_enabled =  false;
	}
}


// Returns the current tick count value
uint8_t getTickCount(void){
	return tick_counter;
}

// Handle real time start messages
void real_time_start(void)
{
	tick_counter = 0;
	average = 0;
	prev_count = 0;
	// Todo: Send Note Offs for any active notes ..
	clock_stable = false;
	if (seq_state == PLAYBACK){
		seq_state == WAIT_FOR_SYNC;
	}
}

// Handle real time stop messages
void real_time_stop(void)
{
	// Reset the sequencer position
	reset_sequence();
	// Cancel any schedules sequencer tasks
	cancel_task();
}

// MIDI Packet Functions ------------------------------------------------------

void midi_stream_raw_note(const uint8_t channel,
                          const uint8_t pitch,
                          const bool onoff,
                          const uint8_t velocity)
{
    uint8_t command = ((onoff)? 0x90 : 0x80);
    MIDI_EventPacket_t midi_event;
	midi_event.Event       = MIDI_EVENT(0, command);
    midi_event.Data1       = command | (channel & 0x0f); // 0..15
    midi_event.Data2       = pitch & 0x7f;   // 0..127
    midi_event.Data3       = velocity & 0x7f; // 0..127
	if (midi_is_usb()){
		int error_code = MIDI_Device_SendEventPacket(g_midi_interface_info, &midi_event);
		if (error_code){
			int i = 0 + error_code;
		}
		
	} else {
		MIDI_send_legacy_packet(&midi_event);
	}
}


void midi_stream_raw_cc(const uint8_t channel,
                        const uint8_t cc,
                        const uint8_t value)
{
    const uint8_t command = 0xb0;  // the control change command
    MIDI_EventPacket_t midi_event;
    midi_event.Event       = MIDI_EVENT(0, command);  
	midi_event.Data1       = command | (channel & 0x0f); // 0..15
    midi_event.Data2       = cc & 0x7f;   // 0..127
    midi_event.Data3       = value & 0x7f;  // 0..127

	if (midi_is_usb()) {
		MIDI_Device_SendEventPacket(g_midi_interface_info, &midi_event);
	} else {
		MIDI_send_legacy_packet(&midi_event);
	}
}

// Append a SysEx Event to the currently selected USB Endpoint. If
// the endpoint is full it will be flushed.
//
//  length       Number of bytes in the message.
//  data         SysEx message data buffer.
//
void midi_stream_sysex (const uint8_t length, uint8_t* data)
{
	//  Assign this MIDI event to cable 0.
	// const uint8_t midi_virtual_cable = 0;
	MIDI_EventPacket_t midi_event;
	
	//     0x2 = 2-byte System Common
	//     0x3 = 3-byte System Common
	//     0x4 = 3-byte Sysex starts or continues
	//     0x5 = 1-byte System Common or Sysex ends
	//     0x6 = 2-byte Sysex ends
	//     0x7 = 3-byte Sysex ends

	//midi_event.CableNumber = midi_virtual_cable << 4;

	uint8_t num = length;// + 1;
	bool first = true;
	while (num > 3) {
		midi_event.Event     = 0x4;
		if (first) {
			first = false;
			midi_event.Data1       = *data++;
			} else {
			midi_event.Data1       = *data++;
		}
		midi_event.Data2       = *data++;
		midi_event.Data3       = *data++;
		MIDI_Device_SendEventPacket(g_midi_interface_info, &midi_event);
		num -= 3;
	}
	if (num) {
		midi_event.Event      = 0x5;
		midi_event.Data1        = *data++;
		midi_event.Data2        = 0;
		midi_event.Data3        = 0;
		if (num == 2) {
			midi_event.Event  = 0x6;
			midi_event.Data2    = *data++;
			} else if (num == 3) {
			if (first) {
				midi_event.Event     = 0x3;
				} else {
				midi_event.Event  = 0x7;
			}
			midi_event.Data2    = *data++;
			midi_event.Data3    = *data++;
		}
		MIDI_Device_SendEventPacket(g_midi_interface_info, &midi_event);
	}
}

/**
 *	Processes USB or legacy (serial) midi packet events
 *	Input
 *  input_event:	The MIDI event to process
 *
**/
void process_midi_packet(MIDI_EventPacket_t input_event)
{
	// Parse the USB-MIDI packet to see what it contains
	switch (input_event.Event) {
		case 0xF :
		{
			// This is a single byte Real Time message.
			switch (input_event.Data1) {
				case 0xF8 :
					// Midi Clock Event
					// We turn off the input interrupt while processing MIDI clock
					PMIC.CTRL &= ~(PMIC_HILVLEN_bm);
					midi_clock_enable(true); // Midi Clock for Animations
					midi_clock();  
					PMIC.CTRL |= PMIC_HILVLEN_bm;
					break;
				case 0xFA :
					// Start Event
					real_time_start();
					midi_clock_enable(true); // Midi Clock for Animations
				break;
				case 0xFC :
					// Stop Event
					real_time_stop();
					midi_clock_enable(false); // Midi Clock for Animations
				break;
			}
		}
		break;
		case 0x9 :
		{
			// A NoteOn event was found, if the Channel is within the
			// correct range update the stored velocity
			uint8_t channel = input_event.Data1 & 0x0f;
			uint8_t note = input_event.Data2;
			uint8_t velocity = input_event.Data3;
			
			if (channel == midi_system_channel){
				if (note < 4){
					if (velocity == 127){
						change_encoder_bank(note);
					}
				}
			} 
			process_element_midi(channel, SEND_NOTE, note, velocity, true);	
		}
		break;
		case 0x8 :  
		{
			// A Note Off event, so record a zero in the MIDI
			// keystate. Yes, a note off can have a "velocity",
			// but we're relying on the keystate to be zero when
			// we have a note off, otherwise the LEDs won't match
			// the state when we come to calculate them.
			uint8_t channel = input_event.Data1 & 0x0f;
			uint8_t note = input_event.Data2;
			uint8_t velocity = input_event.Data3;

			process_element_midi(channel, SEND_NOTE_OFF, note, velocity, false);
		}
		break;
		case 0xB:
		{
			// A Control Change event, if the channel is in the 
			// correct range then store its value
			uint8_t channel = input_event.Data1 & 0x0f;
			uint8_t cc_number = input_event.Data2;
			uint8_t cc_value  = input_event.Data3;

			process_element_midi(channel, SEND_CC, cc_number, cc_value, true);
			
			if (channel == midi_system_channel){
				if (cc_number < 4){
					if (cc_value == 127){
						change_encoder_bank(cc_number);
					}
				}
			}
			
			// Handle Sequencer Controls in here
			if (channel == SEQ_CHANNEL){
				process_seq_midi(cc_number, cc_value);
			}
		}
		break;
		case 0x4 :
		{
			// 3 byte sysex start or continue
			sysex_handle_3sc(&input_event);
		}
		break;
		case 0x5 :
		{
			// One byte sysex end or system common
			sysex_handle_1e(&input_event);
		}
		break;
		case 0x6 :
		{
			// 2 byte sysex end
			sysex_handle_2e(&input_event);
		}
		break;
		case 0x7 :
		{
			// 3 byte sysex end
			sysex_handle_3e(&input_event);
		}
		break;
		default:
		// do nothing.
		break;
	} // end USB-MIDI packet parse
}


// USB MIDI Functions ---------------------------------------------------------

// Legacy MIDI Functions ------------------------------------------------------

/**
 *  Sends a packet of MIDI data on the serial port
 *  Inputs:		MIDI packet to send
**/
void MIDI_send_legacy_packet(const MIDI_EventPacket_t* const Event)
{
	uint8_t length = sizeof(MIDI_EventPacket_t);
	uint8_t *ptr = Event;
	usart_serial_write_packet(USART_SERIAL, ptr, length);
};

/** 
 *  Triggered by Rx Data ready interrupt
 *  Writes the received byte into the FIFO buffer
**/
ISR (USARTE0_RXC_vect)
{
	uint8_t received_byte;
	usart_serial_getchar(USART_SERIAL, &received_byte); 
	fifo_push_uint8(&fifo_desc, received_byte);
}

/**
 *	Checks for bytes in the MIDI FIFO buffer then parses these bytes into
 *  a valid MIDI packet. Once a valid packet has been built it is passed
 *  to process_midi_packet for processing
 *
**/

void process_legacy_packet(void)
{
	static MIDI_EventPacket_t midi_packet;
	static uint8_t bytes_processed = 0;
	uint8_t buffer_byte = 0;
	
	while(!fifo_is_empty(&fifo_desc)){
		// Get a byte to process
		fifo_pull_uint8(&fifo_desc, &buffer_byte);
		
		switch (bytes_processed)
		{
			// We are not currently building a packet so check for a command
			case 0:
			{
				if (buffer_byte & 0xF0){
					// Save the command nibble into the event parameter
					// The upper nibble of the event parameter is used by USB MIDI
					// packets to store the cable number parameter
					midi_packet.Event = (buffer_byte >> 4) & 0x0F;
					// Save the first byte into Data1 and clear Data2 & Data3
					midi_packet.Data1 = buffer_byte;
					midi_packet.Data2 = 0x00;
					midi_packet.Data3 = 0x00;
					// Check if this is a one byte message, if so process it
					if (midi_packet.Event == MIDI_COMMAND_SYSEX_END_1BYTE ||
					midi_packet.Event == MIDI_1BYTE) {
						process_midi_packet(midi_packet);
						} else {
						// Otherwise keep building the message
						bytes_processed++;
					}
				}
			}
			break;
			// We are building a packet and this is the second byte
			case 1:
			{
				// Check if this is a 2 byte message, if so process it
				if (midi_packet.Event == MIDI_COMMAND_SYSEX_2BYTE ||
				midi_packet.Event == MIDI_COMMAND_SYSEX_END_2BYTE) {
					midi_packet.Data2 = buffer_byte;
					process_midi_packet(midi_packet);
					bytes_processed=0;
					
					} else {
					// Otherwise keep building the message
					midi_packet.Data2 = buffer_byte;
					bytes_processed++;
				}
			}
			break;
			// We are building a packet and this is the third and final byte
			case 2:
			{
				midi_packet.Data3 = buffer_byte;
				process_midi_packet(midi_packet);
				bytes_processed=0;
			}
			break;
		}
	}
}


// ----------------------------------------------------------------------------
