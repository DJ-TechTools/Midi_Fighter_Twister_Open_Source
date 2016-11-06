/*
 *  self_test.c
 *
 *  Created: 11/18/2013 9:52:13 AM
 *  Author: Michael Mitchell
 *
 *  To include the factory test procedure into a build you must define
 *  FACTORY_CODE as well as either PCB_TEST or ASSEMBLY_TEST. PCB_TEST
 *  is the test run during PCB assembly and tests all LEDs and inputs
 *  whereas ASSEMBLY_TEST just checks all buttons as a quick post 
 *  assembly check. 
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

#include "self_test.h"

//#define FACTORY_CODE
//#define PCB_TEST
//#define ASSEMBLY_TEST

#if defined(PCB_TEST) && defined(ASSEMBLY_TEST)
#error "Cannot define both PCB & assembly test"
#endif

#define WHITE      (uint32_t) 0xCFFFA0

#define PCB_TEST_PASSED_FLAG      0xA5
#define ASSEMBLY_TEST_PASSED_FLAG 0x5A

#define TEST_TIME_OUT			  5000
#define MAX_SWITCH_DOWN           1000

/**
 * There are two different self tests that are performed during manufacture
 * The first is performed at the PCB assembly level and tests the functionality
 * of all LEDs, encoders, and switches. The second is performed following product 
 * assembly. Once a test has passed a flag is written to EEPROM to signal that the
 * test does not need to be performed again. This function check for the flag and
 * performs the test if the flag is not present. Select which test is to be run by 
 * setting FACTORY CODE and either ASSEMBLY_TEST or PCB_TEST above.
 */

bool check_self_test(void)
{
#ifdef FACTORY_CODE

		uint8_t flag = eeprom_read_byte(EE_SELF_TEST_FLAG);
		
#if defined(PCB_TEST)
	
		// If this is PCB level test code then check for the pcb test flag
		// If it is present then jump to the boot loader, otherwise we perform the test
		// writing the pcb test flag on success.
		if (flag != PCB_TEST_PASSED_FLAG){
			if (elec_self_test()){
				// Write the test passed flag
				eeprom_write(EE_SELF_TEST_FLAG, PCB_TEST_PASSED_FLAG);
				// Set display to all green and flash three times to signal pass
				for(uint8_t i=0;i<16;++i){
					build_rgb(i, 0x00FF00, false);
				}
				for(uint8_t i=0;i<3;++i){
					_delay_ms(500);
					display_disable();
					_delay_ms(500);
					display_enable();
				}
				// Clear the display and enter bootloader mode
				clear_display_buffer();
				Jump_To_Bootloader();
			}
		} else {
			Jump_To_Bootloader();
		}

#elif defined(ASSEMBLY_TEST)

		// If this is PCB level test code then check for the assembly test flag
		// If it is present then return to the main code loop, otherwise we perform the test
		// writing the assembly test flag on success.
		if (flag != ASSEMBLY_TEST_PASSED_FLAG){
			if (assembly_self_test()){
				eeprom_write(EE_SELF_TEST_FLAG, ASSEMBLY_TEST_PASSED_FLAG);
			} 
			// Force a Reset
			wdt_set_timeout_period(WDT_TIMEOUT_PERIOD_16CLK);
			wdt_enable();
			while(true){};
		} else {
			return true;
		}

#endif
	// We should never reach here if self test is enabled
	return false;
#endif		
	return true;
}


/*
 * A self test function for PCB level factory use. This performs a series of hardware
 * tests which require test operator interaction. Once the function of all hardware 
 * has been verified then the function returns true.
 *
 * \return true if passed false/no return if failed
 */


// TODO Make error indication more meaningful 
bool elec_self_test(void)
{
	// Enable display
	display_enable();
	
	bool test_passed = false;
	
	uint16_t bit = 0x0001;
	
	uint16_t allread_tested = 0x0000;

	// Initialize by check all switches are open
	for(uint8_t i=0;i<16;++i){
		if(update_encoder_switch_state() & bit) {
			fail_indicator(i);
		}
		bit <<= 1;
	}
	
	bit = 0x0001;
	
	for(uint8_t i=0;i<6;++i){
		if(update_side_switch_state() & bit) {
			fail_indicator(i+16);
		}
		bit <<= 1;
	}
	
	bit = 0x0001;
	
	// First test encoder switch function & RGB segments
	// We randomly step through each encoder, setting its RGB segment
	// white. The test operator pushes
	// the encoder switch to confirm white is present. If there is an
	// issue with any of the colors then white is not possible. This
	// does not test for short between cathode.
	
	uint8_t enc = 0;
	
	while(allread_tested != 0xFFFF){
		
		while (allread_tested & (0x0001 << enc)){
			enc = get_random_16();
		}
		
		clear_display_buffer();
		build_rgb(enc, WHITE, false);
		wait_for_input(enc);
		pass_indicator(enc);
		
		// Tests each color individually
		/*
		for(uint8_t j=0;j<3;++j){
			clear_display_buffer();
			build_rgb(enc, test_colors[j], false);
			wait_for_input(enc);
		}
		*/
	
		allread_tested |= 0x0001 << enc;
	}
	
	// Next test the encoder position indicators
	// We randomly step through each encoder, setting all white
	// LEDs on, the test operator pushes the encoder
	// switch to confirm all LEDs are functional

	enc = 0;
	allread_tested = 0;
	
	while(allread_tested != 0xFFFF){
		
		while (allread_tested & (0x0001 << enc)){
			enc = get_random_16();
		}
		
		clear_display_buffer();
		set_encoder_indicator(enc, 127, false, BAR, false);
		wait_for_input(enc);
		pass_indicator(enc);
		
		allread_tested |= 0x0001 << enc;
	}
	
	// Now we test the encoder inputs. We step through
	// each encoder and wait for its value to move from 0
	// to 20. If either encoder inputs have a fault the value
	// will never pass 1.
	for(uint8_t i=0;i<16;++i){
		clear_display_buffer();
		get_encoder_value(i); // discard any old encoder movements
		int16_t encoder_value = 2;
		uint16_t count = 0;	
		while(encoder_value < 135) {
			//Wait for the encoder to reach a value of 30
			encoder_value += get_encoder_value(i)*3;
			int16_t display_value = encoder_value;
			if(display_value > 127){
				display_value = 127;
			}
			set_encoder_indicator(i, (uint8_t)display_value, false, DOT, false);
			_delay_ms(1);
			count++;
			if(count > 12000){// actually time around 5 seconds, indicator set is slow..
				fail_indicator(i);
			}
		}
		pass_indicator(i);
	}

	// Now we test the de-tent LEDs. We randomly step through the 16 encoders
	// setting each to purple, waiting for operator confirmation and then moving
	// to the next.
	
	enc = 0;
	allread_tested = 0;
	 
	// Test Blue
	while(allread_tested != 0xFFFF){
		
		while (allread_tested & (0x0001 << enc)){
			enc = get_random_16();
		}
		
		clear_display_buffer();
		set_encoder_indicator(enc, 63, true, BAR, 63);
		wait_for_input(enc);
		pass_indicator(enc);
		
		allread_tested |= 0x0001 << enc;
	}
	
	// Finally we test the side switches, we set the adjacent encoder
	// indicator LEDs on and wait for the operator to press the side 
	// switch
	
	uint8_t side_pattern[6][2] = {{0,4},{4,8},{8,12},{3,7},{7,11},{11,15}}; 
		
	for(uint8_t i=0;i<6;++i){
		uint8_t element1 = side_pattern[i][0];
		uint8_t element2 = side_pattern[i][1];
		clear_display_buffer();
		set_encoder_indicator(element1, 127, false, BAR, false);
		set_encoder_indicator(element2, 127, false, BAR, false);
		wait_for_input(i+16);
		pass_indicator(i+16);
	}
	
	clear_display_buffer();
	/*
	uint8_t red = 0;
	uint8_t blue = 0;
	uint8_t green = 0;
	
	
	while(!(update_side_switch_state() & 0x12)){
		red += get_encoder_value(0); // discard any old encoder movements
		green += get_encoder_value(1); // discard any old encoder movements
		blue += get_encoder_value(2); // discard any old encoder movements
		if ( red < 0) {red =0;}
		if (blue < 0){blue = 0;}
		if (green < 0){green =0;}
		
		uint32_t color = 0;
		color = color | (red*2);
		color = (color << 8) | (green*2);
		color = (color << 8) | (blue*2);

		build_rgb(0, color, 255);
		build_rgb(1, color, 255);
		build_rgb(2, color, 255);
		set_encoder_indicator(0, red, false,BAR, 0);
		set_encoder_indicator(1, green, false,BAR, 0);
		set_encoder_indicator(2, blue, false,BAR, 0);
	}
	*/
	for(uint8_t i=0;i<16;++i){
		set_encoder_indicator(i, 127, false, BAR, false);
		set_encoder_rgb(i, 127);
	}
	
	while(!(update_side_switch_state() & 0x12)){
		
	}
	
	// If passed all tests flash all RGB green then write the flag and reset
	clear_display_buffer();
	
	return true;
}



/**
 * Assembly level self test, tests all buttons by turning on
 * adjacent LEDs and waiting for the operator to press the button
 */

bool assembly_self_test(void)
{
	// Enable display
	display_enable();
	
	// Set each encoder RGB LED to White and wait for operator to push enc switch
	for(uint8_t i=0;i<16;++i){
		clear_display_buffer();
		build_rgb(i, WHITE, false);
		wait_for_input(i);
	}
	
	// Finally we test the side switches, we set the adjacent encoder
	// indicator LEDs on and wait for the operator to press the side
	// switch
	
	uint8_t side_pattern[6][2] = {{0,4},{4,8},{8,12},{3,7},{7,11},{11,15}};
	
	for(uint8_t i=0;i<6;++i){
		uint8_t element1 = side_pattern[i][0];
		uint8_t element2 = side_pattern[i][1];
		clear_display_buffer();
		set_encoder_indicator(element1, 127, false, BAR, false);
		set_encoder_indicator(element2, 127, false, BAR, false);
		wait_for_input(i+16);
		pass_indicator(i+16);
	}
	
	// If passed all tests flash all RGB green then write the flag and reset
	clear_display_buffer();
	
	for(uint8_t i=0;i<16;++i){
		build_rgb(i, 0x00FF00, false);
	}
	
	for(uint8_t i=0;i<3;++i){
		_delay_ms(500);
		display_disable();
		_delay_ms(500);
		display_enable();
	}
	
	clear_display_buffer();
	display_disable();
	
	return true;
}

// Displays a flashing RED display pattern to indicate failure
// This function never returns
void fail_indicator(uint8_t element)
{
	clear_display_buffer();
	
	if (element < 16)
	{
		// If the fault is for an encoder element we set its red LEDs on
		build_rgb(element, 0xFF0000, false);
		set_encoder_indicator(element, 63, true, BAR, 0);
	} else {
		// If the fault is for a side switch element we set the red LEDs
		// of the closest two encoders on.
		uint8_t side_pattern[6][2] = {{0,4},{4,8},{8,12},{3,7},{7,11},{11,15}}; 
		uint8_t element1 = side_pattern[element-16][0];
		uint8_t element2 = side_pattern[element-16][1];
		
		build_rgb(element1, 0xFF0000, false);
		set_encoder_indicator(element1, 63, true, BAR, 0);
		build_rgb(element2, 0xFF0000, false);
		set_encoder_indicator(element2, 63, true, BAR, 0);
	}
	
	while(true){
		_delay_ms(500);
		display_disable();
		_delay_ms(500);
		display_enable();
	}
}


// Flashes the related RGB LEDs green briefly to confirm test passed
void pass_indicator(uint8_t element)
{
	clear_display_buffer();
	if ( element < 16) {
		build_rgb(element, 0x00FF00, false);	
	} else {
		uint8_t side_pattern[6][2] = {{0,4},{4,8},{8,12},{3,7},{7,11},{11,15}}; 
		uint8_t element1 = side_pattern[element-16][0];
		uint8_t element2 = side_pattern[element-16][1];
		build_rgb(element1, 0x00FF00, false);
		build_rgb(element2, 0x00FF00, false);
	}
	_delay_ms(250);	
	clear_display_buffer();
}

// Waits for a button press for a given element
// If the Press to Release time period exceeds 1 second
// this function will not return. This checks the pull up 
// resistors are present, as typical symptom of this failure
// is normal falling edge for press, but a delay following the 
// switch release and the rising edge on the switch input 
void wait_for_input(uint8_t element)
{
	uint16_t bit = 0x0001;
	uint16_t count = 0;
	
	if (element < 16){
		bit = bit << element;
		while(!(update_encoder_switch_state() & bit)) {
			// Wait for the switch down press, fail if this
			// takes longer than 5 seconds.
			_delay_ms(1);
			count++;
			if (count > TEST_TIME_OUT){
				fail_indicator(element);
			}	
		}
		count = 0;
		while(update_encoder_switch_state() & bit) {
			// Wait for switch release, fail if this takes longer
			// than 1 second
			_delay_ms(1);
			count++;
			if (count > MAX_SWITCH_DOWN){
				fail_indicator(element);
			}	
		}
	} else {
		bit = bit << (element-16);
		while(!(update_side_switch_state() & bit)) {
			// Wait for the switch down press, fail if this
			// takes longer than 5 seconds.
			_delay_ms(1);
			count++;
			if (count > TEST_TIME_OUT){
				fail_indicator(element);
			}	
		}
		count = 0;
		while(update_side_switch_state() & bit) {
			// Wait for switch release, fail if this takes longer
			// than 1 second
			_delay_ms(1);
			count++;
			if (count > MAX_SWITCH_DOWN){
				fail_indicator(element);
			}	
		}
	}
	
}

// Returns a random value between 0 and 15
uint8_t get_random_16(void)
{
	uint16_t g_seed16 = tc_read_count(&TCC1);
	
	g_seed16 ^= (g_seed16 << 13);
	g_seed16 ^= (g_seed16 >> 9);
	g_seed16 ^= (g_seed16 << 7);
	
	return (g_seed16 & 0x0F);		
}