/*
 * self_test.h
 *
 * Created: 11/18/2013 9:52:35 AM
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


#ifndef SELF_TEST_H_
#define SELF_TEST_H_

	#include <asf.h>
	#include "display_driver.h"
	#include "input.h"
	#include "config.h"

	// Checks if test needs to be performed
	bool check_self_test(void);

	// The two different tests
	bool elec_self_test(void);
	bool assembly_self_test(void);
	
	// Helper functions
	void fail_indicator(uint8_t element);
	void wait_for_input(uint8_t element);
	void pass_indicator(uint8_t element);
	uint8_t get_random_16(void);

#endif /* SELF_TEST_H_ */