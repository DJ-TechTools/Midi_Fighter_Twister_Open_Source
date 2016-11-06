/**
 * \file
 *
 * main.c: It all starts and ends here.
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

/**
 *	DRAW ASCII ART OF TWISTER ELEMENTS HERE
 *  ENCODERS ARE Referred to as ENC0 - ENC15
 *  SIDE SW ARE SW1 - SW6 
 *
 *
 */

/*
 * Include header files for all drivers that have been imported from
 * Atmel Software Framework (ASF).
 */
#include <asf.h>
#include <LUFA/Drivers/USB/USB.h>
#include <LUFA/Platform/Platform.h>

#include "display_driver.h"
#include "input.h"
#include "midi.h"
#include "encoders.h"
#include "side_switch.h"
#include "Descriptors.h"
#include "jump_to_bootloader.h"
#include "sequencer.h"
#include "self_test.h"

//#define DEMO 
	
void system_init(void);
void board_init(void);

bool watchdog_flag = false;

int main (void)
{	
	// Initialize all systems	
	system_init();
	config_init();
	load_config();
		
	input_init();
	midi_init();	
	encoders_init();
	side_switch_init();	
	display_init();
	sequencer_init();
	
	// Disable the display until we are connected and ready to start the 
	// start-up animation.
	display_disable();
	clear_display_buffer();
	
	// Wait 10 ms for all inputs to stabilize
	_delay_ms(10);
	
	// Check if we need to perform any self tests
	check_self_test();
	
	//Then check for the four corner boot loader input pattern
	if(update_encoder_switch_state() == 0x9009)
	{
		Jump_To_Bootloader();
	} else if (update_encoder_switch_state() == 0xF000){
		// Force factory test
		eeprom_write(EE_SELF_TEST_FLAG, 0xFF);
		check_self_test();
	}
	
	// Clear out any start up noise from the encoders
	for(uint8_t i=0;i<16;++i){
		get_encoder_value(i);
	}

	// If the USB connection is not configured within a certain window
	// we switch to using serial/legacy for MIDI
	schedule_task(set_midi_serial, 0xFFFF);

	// Main Loop
	do {
		
			// Force display to rainbow demo		
#ifdef DEMO
			if(update_encoder_switch_state() == 0x0001)
			{
				display_enable();
				while(true){
					rainbow_demo();
				}
			}
#endif		

			switch (get_op_mode()) {
				case normal:{
					// Process any encoder movements or changes to the switch state
					process_encoder_input();
			
					// Check for any change to the encoder state and refresh the display, because
					// redrawing any display is slow we only check and update 1 encoder per main loop
					#if ENABLE_MAX_LED_UPDATE_SPEED > 0
					 #warning LED Controllers are being Updated at a Higher Speed! This results in latency up to 8ms.
					 // !Summer2016Update: improve LED Update Times
					 // Performance Testing: Dual Animations running on Every Encoder. MIDI Feedback sent Constantly 1-message/ millisecond.
					 // - Target Range is a maximum of 8ms.
					 // < 1: latency = 2ms
					 // < 4: latency in range (4-6ms)
					 // < 6: latency in range (6-8ms)*
					 // < 8: latency in range (8-10ms)
					 // < 16:latency = (16-20ms)
					 for(uint8_t encoder_display_counter = 0; encoder_display_counter < 6; encoder_display_counter++)  
					 {
						update_encoder_display();
					 }
					#else
					 update_encoder_display();
					#endif
					// Now we have dealt with the encoders we check for side switch state changes
					// Side switches either send MIDI or carry out an action
					process_side_switch_input();
				}
				break;
				case shift1:{
					
					run_shift_mode(0);
					process_side_switch_input();	
				}
				break;
				case shift2:{
					
					run_shift_mode(1);
					process_side_switch_input();
				}
				break;
				case sequencer:{
					process_seq_side_buttons();
					process_sequencer_input();	
					run_sequencer_display();
				}
				break;
			}
			
			
			
			if(midi_is_usb())
			{
				MIDI_Device_USBTask(g_midi_interface_info);
				USB_USBTask();
				
				// If we are using the USB MIDI connection check for and process received MIDI packets
				MIDI_EventPacket_t ReceivedMIDIEvent;
				
				// For now we disable interrupts while processing incoming MIDI, this avoids missed clock
				// ticks. This could probably be solved more elegantly but it works
				PMIC.CTRL &= ~(PMIC_LOLVLEN_bm | PMIC_MEDLVLEN_bm);
				
				while (MIDI_Device_ReceiveEventPacket(g_midi_interface_info, &ReceivedMIDIEvent))
				{
					process_midi_packet(ReceivedMIDIEvent);
				}				
				PMIC.CTRL = PMIC_LOLVLEN_bm | PMIC_MEDLVLEN_bm | PMIC_HILVLEN_bm;
			} else {
				// Otherwise process any legacy MIDI packets
				PMIC.CTRL &= ~(PMIC_LOLVLEN_bm | PMIC_MEDLVLEN_bm);
				process_legacy_packet();
				PMIC.CTRL = PMIC_LOLVLEN_bm | PMIC_MEDLVLEN_bm | PMIC_HILVLEN_bm;
			}
			
		watchdog_flag = true;	
		
		// Reset the watch dog timer, dawg
		if (watchdog_flag)
		{
			wdt_reset();
			watchdog_flag = false;
		}
		
	} while (1);
}

/** Initializes ASF drivers */
void system_init()
{	
	/* Start the PLL to multiply the 2MHz RC oscillator to 32MHz and switch the CPU core to run from it */
	XMEGACLK_StartPLL(CLOCK_SRC_INT_RC2MHZ, 2000000, F_CPU);
	XMEGACLK_SetCPUClockSource(CLOCK_SRC_PLL);

	/* Start the 32MHz internal RC oscillator and start the DFLL to increase it to 48MHz using the USB SOF as a reference */
	XMEGACLK_StartInternalOscillator(CLOCK_SRC_INT_RC32MHZ);
	XMEGACLK_StartDFLL(CLOCK_SRC_INT_RC32MHZ, DFLL_REF_INT_USBSOF, F_USB);

	PMIC.CTRL = PMIC_LOLVLEN_bm | PMIC_MEDLVLEN_bm | PMIC_HILVLEN_bm;

	USB_Init();
}


/** Event handler for the library USB Connection event. */
void EVENT_USB_Device_Connect(void)
{

}

/** Event handler for the library USB Disconnection event. */
void EVENT_USB_Device_Disconnect(void)
{
	
}

/** Event handler for the library USB Configuration Changed event. */
void EVENT_USB_Device_ConfigurationChanged(void)
{
	bool ConfigSuccess = true;

	ConfigSuccess &= MIDI_Device_ConfigureEndpoints(g_midi_interface_info);

	//LEDs_SetAllLEDs(ConfigSuccess ? LEDMASK_USB_READY : LEDMASK_USB_ERROR);
	
	if (ConfigSuccess){
		cancel_task();	// disable scheduled switch to legacy mode
		display_enable();
		// Display the start up animation
		run_sparkle(6);
		// Build the display
		set_op_mode(normal);
		refresh_display();
		wdt_set_timeout_period(WDT_TIMEOUT_PERIOD_64CLK);
		wdt_enable();
	} else {

	}
}

/** Event handler for the library USB Control Request reception event. */
void EVENT_USB_Device_ControlRequest(void)
{
	MIDI_Device_ProcessControlRequest(g_midi_interface_info);
}


/** Event handler for the library USB device suspend event. */
/*
void EVENT_USB_Device_Suspend(void)
{

}
*/

/** Event handler for the library USB device wake up event. */
/*
void EVENT_USB_Device_Wakeup(void)
{

}
*/
