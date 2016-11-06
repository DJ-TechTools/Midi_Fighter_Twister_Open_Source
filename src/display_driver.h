/*
 * display_driver.h
 *
 * Created: 6/28/2013 1:49:00 PM
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


#ifndef DISPLAY_DRIVER_H_
#define DISPLAY_DRIVER_H_

/*	Includes: */
	#include <asf.h>
	#include <math.h>
	#include <colorMap.h>
	
	#include "encoders.h"
	

/*	Macros: */
	#define ENABLE_MAX_LED_UPDATE_SPEED 1

	// DMA Constants
	#define DMA_CHANNEL	        0
	//#define DMA_BUFFER_SIZE    4096
	#define DMA_BUFFER_SIZE    3072
	#define DMA_FRAME_SIZE     32
	#define NUM_OF_FRAMES	   (DMA_BUFFER_SIZE/DMA_FRAME_SIZE)
	

	// Define Pin Names
	#define DISPLAY_EN		IOPORT_CREATE_PIN(PORTD, 0)
	#define DISPLAY_RST     IOPORT_CREATE_PIN(PORTD, 5)
	#define DISPLAY_LATCH   IOPORT_CREATE_PIN(PORTD, 4)
	#define SPI_DATA        IOPORT_CREATE_PIN(PORTD, 3)
	#define SPI_CLK         IOPORT_CREATE_PIN(PORTD, 1)

	// USART SPI Constants
	#define USART_SPI                   &USARTD0
	#define USART_SPI_BAUDRATE          4000000   // 4 MBps
	#define USART_SPI_MODE              0         // Sample on rising edge.
	#define USART_SPI_DATA_ORDER        1         // MSB First.
	
/*	Types: */
	
	// Structure which holds two LED patterns (bit masks) and their respective
	// brightness settings.
	typedef struct {
		uint16_t pattern_A;
		uint8_t  pattern_A_brightness;
		uint16_t pattern_B;
		uint8_t  pattern_B_brightness;
	} indicator_bit_mask_t;
		
		
/* Variables */

	// Config structure for DMA channel
	struct dma_channel_config	dmach_conf;	
	extern volatile uint8_t animation_counter;		
/* Function Prototypes: */

	void display_init(void);
	
	//static void display_frame_timer(void);
	
	//static void display_animation_timer(void);
	
	void display_disable(void);
	
	void display_enable(void);
	
	void clear_display_buffer(void);
	
	void build_rgb(uint8_t encoder, uint32_t color, uint8_t level);
	
	int build_indicator_pattern(indicator_bit_mask_t *result, uint8_t position, uint16_t type, 
								 bool has_detent, uint8_t detent_color);
	
	bool strobe_animation(uint8_t flash_rate);
	
	uint8_t pulse_animation(uint8_t pulse_rate);
	
	bool build_sparkles(void);
	
	uint8_t lerp(uint8_t high, uint8_t low, uint8_t t);
	
	uint16_t random16(void);
	
	
	// External Functions - these are what you should use to interact with the display
	void set_encoder_rgb(uint8_t encoder, uint8_t color);
	void set_encoder_rgb_level(uint8_t encoder, uint8_t color, uint8_t brightness);
	
	void set_encoder_indent(uint8_t encoder, uint8_t color_index);
	
	void set_encoder_indicator(uint8_t encoder, uint8_t position, bool has_detent, uint16_t type, 
								uint8_t detent_color);
	
	void set_encoder_indicator_level(uint8_t encoder, uint8_t position, bool has_detent, uint16_t type,
								      uint8_t detent_color, uint8_t brightness);							
								
	void set_indicator_pattern(uint8_t encoder, uint16_t pattern);
	void set_indicator_pattern_level(uint8_t encoder, uint16_t pattern, uint8_t brightness);
								
	void run_encoder_animation(uint8_t encoder, uint8_t bank, uint8_t animation, uint8_t color);
	
	void setting_confirmation_animation(uint32_t color);
	
	void run_sparkle(uint8_t count);
	
	void rainbow_demo(void);
	

	

#endif /* DISPLAY_DRIVER_H_ */

