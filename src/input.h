/*
 * input.h
 *
 * Created: 6/30/2013 2:19:41 PM
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
#ifndef INPUT_H_
#define INPUT_H_

/* Includes: */
	#include <asf.h>
	#include <display_driver.h>
	
/* Macros: */

	#define SWITCH_DEBOUNCE_BUFFER_SIZE	10
	
	#define ENCODER_INACTIVE_THRESHOLD 100

	// Input Pin Definitions
	#define SIDE_SW6		IOPORT_CREATE_PIN(PORTA, 5)
	#define SIDE_SW5		IOPORT_CREATE_PIN(PORTA, 4)
	#define SIDE_SW4		IOPORT_CREATE_PIN(PORTA, 3)
	#define SIDE_SW3		IOPORT_CREATE_PIN(PORTA, 0)
	#define SIDE_SW2		IOPORT_CREATE_PIN(PORTA, 1)
	#define SIDE_SW1		IOPORT_CREATE_PIN(PORTA, 2)
	
	#define ENC_LATCH       IOPORT_CREATE_PIN(PORTC, 0)
	#define ENC_CLK			IOPORT_CREATE_PIN(PORTC, 1)
	#define ENC_DATA        IOPORT_CREATE_PIN(PORTC, 2)
	
	#define DEBUG_PIN       IOPORT_CREATE_PIN(PORTC, 6)
	
	#define D_LAT 0x01
	#define D_CLK 0x02
	#define D_DATA 0x03
	
/* Global Variables */

/* Function Prototypes */

	void input_init(void);
	
	void encoder_scan(void);
	int8_t get_encoder_value(uint8_t encoder);
	
	uint16_t update_encoder_switch_state(void);

	uint16_t update_side_switch_state(void);
	
	bool encoder_is_active(uint8_t enc_idx);
	
	uint32_t get_ms_timer(void);
	
	bool schedule_task(void (*task)(void), uint16_t time);
	bool cancel_task(void);
	
	uint16_t get_enc_switch_state(void);
	uint16_t get_enc_switch_down(void);
	uint16_t get_enc_switch_up(void);	
	uint16_t get_side_switch_state(void);
	uint16_t get_side_switch_down(void);
	uint16_t get_side_switch_up(void);

#endif /* INPUT_H_ */