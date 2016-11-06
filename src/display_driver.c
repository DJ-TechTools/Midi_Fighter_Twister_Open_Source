/*
 * display_driver.c
 *
 * Created: 6/28/2013 1:48:45 PM
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


/** Display Driver Source Code
 *  This files contains all routines for controlling the Midi Fighter 
 *  Twister display. The display is made up of 176 white, 16 RGB and
 *  a further 16 Red/Blue dual color LEDs. 
**/

#include <display_driver.h>

/* Variables: */
static uint8_t display_frame_buffer[DMA_BUFFER_SIZE]; 
volatile uint8_t animation_counter;
volatile uint8_t display_frame_index;
volatile uint8_t tick;

// Array which holds the 7 current bit color value for the RGB segments
static uint8_t rgb_color_setting[16];


/*Function Prototypes: */
static void display_frame_timer(void);
static void display_animation_timer(void);

/** Initialization Function for the display driver, this sets up the 
 *  DMA controller, configures USARTD0 in SPI Master Mode, and configures 
 *  Timer0 to provide a 80 uS interrupt.
**/

void display_init(void)
{
	// Set ALL LEDs off
	memset(&display_frame_buffer, 0xFF, sizeof(display_frame_buffer));
	
	// DMAC Initialization ----------------------------------------------------
	
	// Make sure DMA configuration is set to zero (default values)
	memset(&dmach_conf, 0, sizeof(dmach_conf));
	
	// Configure the burst length and transfer count
	// Since we are writing to the USART Data register the burst size is 1 Byte
	// Each frame consists of 32 1 byte bursts giving a frame size of 32 bytes
	dma_channel_set_burst_length(&dmach_conf, DMA_CH_BURSTLEN_1BYTE_gc);
	dma_channel_set_transfer_count(&dmach_conf, DMA_FRAME_SIZE);
	
	// Configure DMA Ch in single shot mode, only 1 byte transfered per trigger
	dma_channel_set_single_shot(&dmach_conf);
	
	// Configure the DMA Ch to increment the source address after each byte is 
	// transferred.
	dma_channel_set_src_dir_mode(&dmach_conf, DMA_CH_SRCDIR_INC_gc);
	
	// Configure the DMA Ch to never reload the source address, we handle this 
	// manually.
	dma_channel_set_src_reload_mode(&dmach_conf, DMA_CH_SRCRELOAD_NONE_gc);

	// Configure the DMA Ch destination address mode as fixed
	dma_channel_set_dest_dir_mode(&dmach_conf, DMA_CH_DESTDIR_FIXED_gc);
	// Configure the DMA Ch to never reload the destination address, this is 
	// fixed
	dma_channel_set_dest_reload_mode(&dmach_conf, DMA_CH_DESTRELOAD_NONE_gc);
	
	// Configure the display frame buffer array as the start address for the 
	// transfer
	dma_channel_set_source_address(&dmach_conf, 
								  (uint16_t)(uintptr_t)display_frame_buffer);
								  
	// Configure the UART Data register as the destination address for the 
	// transfer
	dma_channel_set_destination_address(&dmach_conf, 
								       (uint16_t)(uintptr_t)&USARTD0.DATA);
	
	// Configure USART D0 Data Register Empty as the trigger source for 
	// transfers
	dma_channel_set_trigger_source(&dmach_conf, 
								   DMA_CH_TRIGSRC_USARTD0_DRE_gc);
	
	// Initialize the DMA module
	dma_enable();
	
	// Save the Channel configuration to the DMA module
	dma_channel_write_config(DMA_CHANNEL, &dmach_conf);
	
	dma_set_double_buffer_mode(0x01);
	
	// USART Initialization ---------------------------------------------------
	
	/** The Display Driver utilizes USARTD0 configured in SPI MASTER MODE as it 
	 *  is not possible to use DMA with the SPI peripheral hardware in master 
	 *	mode. */ 
	
	// Create a structure to hold the configuration options
	static usart_spi_options_t USART_SPI_OPTIONS = {
		.baudrate = USART_SPI_BAUDRATE,
		.spimode = USART_SPI_MODE,
		.data_order = USART_SPI_DATA_ORDER,
	};
	
	// Initialize the USART as an SPI 
	usart_init_spi(USART_SPI, &USART_SPI_OPTIONS);
	
	// Initialize the USART XCLK & TXD Pins as outputs
	ioport_set_pin_dir(SPI_DATA, IOPORT_DIR_OUTPUT);
	ioport_set_pin_dir(SPI_CLK,  IOPORT_DIR_OUTPUT);
	
	// Timer Initialization ---------------------------------------------------
	
	/** The Display Driver uses a timer driven interrupt to initialize sending 
	 *  each frame via a DMA transaction. This interrupt occurs every 40 uS to
	 *	give a display refresh rate of 40 uS X 127 = 196.85 Hz
	 *  We achieve a 72 uS period by dividing the 32 MHz CLK by 256 and setting
	 *  the overflow count to 9.
	 */
	
	#define DISPLAY_FRAME_TIMER_PERIOD 9
	#define DISPLAY_ANIMATION_TIMER_PERIOD 127
	 
	// Enable Timer 0 
	tc_enable(&TCC0);
	// Set Timer 0 compare A callback to display_frame_timer()
	tc_set_cca_interrupt_callback(&TCC0, display_frame_timer);
	// Set Timer 0 compare B callback to animation_timer();
	tc_set_ccb_interrupt_callback(&TCC0, display_animation_timer);	
	// Set Timer 0 CCA match value to 9 (72 uS)
	tc_write_cc(&TCC0, TC_CCA, DISPLAY_FRAME_TIMER_PERIOD);
	// Set Timer 0 CCB match value to 126 (~1 mS)
	tc_write_cc(&TCC0, TC_CCB, DISPLAY_ANIMATION_TIMER_PERIOD);	
	// Set Timer 0 CCB interrupt level to high
	tc_set_cca_interrupt_level(&TCC0, TC_INT_LVL_LO);
	// Set Timer 0 CCB interrupt level to low
	tc_set_ccb_interrupt_level(&TCC0, TC_INT_LVL_LO);
	
	// Set Timer 0 waveform mode to count up
	tc_set_wgm(&TCC0, TC_WG_NORMAL);

	// Enable interrupts
	cpu_irq_enable();
	// Set the Timer 0 clock source to system / 256
	tc_write_clock_source(&TCC0, TC_CLKSEL_DIV256_gc);
	
	// Display Control Pins Initialization --------------------------------------------------------

	// Initialize IOPort Driver
	ioport_init();
	
	// Initialize Display Driver Control Pins
	ioport_set_pin_dir(DISPLAY_EN, IOPORT_DIR_OUTPUT);
	ioport_set_pin_dir(DISPLAY_RST, IOPORT_DIR_OUTPUT);
	ioport_set_pin_dir(DISPLAY_LATCH, IOPORT_DIR_OUTPUT);
	
	// Disable Display Driver register outputs until valid data is present
	ioport_set_pin_level(DISPLAY_EN,1);

	// Hard Reset the display driver registers
	ioport_set_pin_level(DISPLAY_RST, 0);
	ioport_set_pin_level(DISPLAY_RST, 1);
	
	// Leave Display_Latch Low
	ioport_set_pin_level(DISPLAY_LATCH, 0);
	
	// Initialize the animation tick and animation counter
	uint8_t animation_counter = 0;
	static uint8_t tick = 0;
	
	// Finally initialize the frame counter
	display_frame_index = 0;
}

/**
 *  Enables Display by setting the /OE pin of the drives low and enabling Timer 0 
 */
void display_enable(void)
{
	ioport_set_pin_level(DISPLAY_EN, 0);	
}

/**
 * Disables Display by setting the /OE pin of the drives high and disabling timer 0
 */
void display_disable(void)
{
	ioport_set_pin_level(DISPLAY_EN, 1);	
}

/**
 * Clears the display buffer
 */

void clear_display_buffer(void){
	memset(&display_frame_buffer, 0xFF, sizeof(display_frame_buffer));
}


/**
 * Display Animation Timer interrupt callback function. This is triggered about 
 * every millisecond and decrements the animation frame counters.
 */

volatile uint16_t animation_frames_remaining = 0;

void display_animation_timer(void)
{
	// Increment the timer compare value
	tc_write_cc(&TCC0, TC_CCB, DISPLAY_ANIMATION_TIMER_PERIOD + tc_read_count(&TCC0));
	
	if (animation_frames_remaining > 0){
		animation_frames_remaining--;
	}
}


/** Interrupt callback function. This is triggered every 80 uS by overflow  
 *  of Timer0. This function latches the last frame into the output stage of
 *  of the 74HC595 registers then starts DMA transfer of the next frame and
 *	resets the DMA source address at the end of each display cycle.
**/

static void display_frame_timer(void)
{
	// Increment the timer compare value
	tc_write_cc(&TCC0, TC_CCA, DISPLAY_FRAME_TIMER_PERIOD + tc_read_count(&TCC0));
	// Latch last frame to display driver shift register Outputs
	ioport_set_pin_level(DISPLAY_LATCH, 1);
	// Leave display_latch low
	ioport_set_pin_level(DISPLAY_LATCH, 0);
	// Enable the DMA Channel to start the transaction
	dma_channel_enable(DMA_CHANNEL);
	// Increment transaction counter
	display_frame_index += 1;
	
	// Check to see if we are at the end of the display buffer, then reset DMA 
	// source address to the start of the display buffer 
	if(display_frame_index >= (NUM_OF_FRAMES-1))
	{
		display_frame_index = 0;
		//Wait for the last DMA transaction to complete.
		while (dma_channel_is_busy(DMA_CHANNEL)){};
		dma_channel_write_source(DMA_CHANNEL, (uint16_t)(uintptr_t)display_frame_buffer);								
	}
	
	if(!midi_clock_enabled) // !Summer2016Update midi_clock animations
	{
		tick += 1;
		if(tick == 255){
			animation_counter +=1;
			tick = 0;
		}
	}
}

/**
 *  Set the indicator display of 11 white and 1 red/blue for a given encoder
 *  Inputs:
 *  Encoder	 -	The encoder to set the display for (0 - 15)
 *  Position -	The scaled position of the pointer (0 - 127)
**/
void set_encoder_indicator(uint8_t encoder, uint8_t position, bool has_detent, uint16_t type, 
							uint8_t detent_color)
{
	uint8_t ind_brightness = pgm_read_byte(&brightnessMap[global_ind_brightness]);
	set_encoder_indicator_level(encoder, position, has_detent, type, detent_color, ind_brightness);
}

// Allows the indicator to be set with a specified brightness setting
void set_encoder_indicator_level(uint8_t encoder, uint8_t position, 
								 bool has_detent, uint16_t type,
								 uint8_t detent_color, uint8_t brightness)
{
	// Do nothing if position is invalid
	if (position > 127 || position < 0) {
		return;
	}
	
	// Structure to store the display pattern data
	indicator_bit_mask_t bit_masks;
	
	if (build_indicator_pattern(&bit_masks, position, type, has_detent, detent_color)){
		
		float brightness_coeff = brightness/127.0f;
		
		bit_masks.pattern_A_brightness = (uint8_t)(bit_masks.pattern_A_brightness * brightness_coeff);
		bit_masks.pattern_B_brightness = (uint8_t)(bit_masks.pattern_B_brightness * brightness_coeff);
		
		uint8_t mask_A_upper_byte = (uint8_t)(((bit_masks.pattern_A) >> 8) & 0x00FF);
		uint8_t mask_A_lower_byte = (uint8_t)(((bit_masks.pattern_A)     ) & 0x00E3);
		uint8_t mask_B_upper_byte = (uint8_t)(((bit_masks.pattern_B) >> 8) & 0x00FF);
		uint8_t mask_B_lower_byte = (uint8_t)(((bit_masks.pattern_B)     ) & 0x00E3);
		
		uint8_t *ptr = display_frame_buffer;
		
		// Calculate initial buffer address offset for this encoder
		ptr += ((15-encoder)*2);
		
		for (uint8_t i=0;i<NUM_OF_FRAMES;++i)  // NUM_OF_FRAMES = 96 (MIDI Fighter Twister)
		{
			// Clear old data
			ptr[0] |= 0xE3;
			ptr[1]  = 0xFF;

			// Write Mask A
			if ((i*(127/NUM_OF_FRAMES)) < bit_masks.pattern_A_brightness) { // !revision: 127/NUM_OF_FRAMES is constant, why do this math?
				ptr[0] &= ~mask_A_lower_byte;
				ptr[1] &= ~mask_A_upper_byte;
			}
			// Write Mask B
			if ((i*(127/NUM_OF_FRAMES)) < bit_masks.pattern_B_brightness) { // !revision: 127/NUM_OF_FRAMES is constant, why do this math?
				ptr[0] &= ~mask_B_lower_byte;
				ptr[1] &= ~mask_B_upper_byte;
			}
			// Jump to next frame
			ptr += 32;
		}
		
		} else {
		// Building the display failed so return
		return;
	}
}

/** 
 * Takes a variety of indicator display settings as input and builds two 16 
 * bit masks with individual brightness settings as output. These masks set the 
 * brightness of the 11 white LEDs and the Red Blue de-tent Indicator color
 * given for the given display type and de tent settings.
 *
 * Inputs: 
 * result:      A pointer to an inidcator_bit_frame struct to store the result
 * position:	The 7bit encoder indicator position (0 - 127)
 * type:		The type of display to build
 * has_detent	If the encoder uses a virtual de tent
 * detent_color The de tent indicator color setting for the indicator
 *
 * Output:		1 if valid result
**/ 
 
int build_indicator_pattern(indicator_bit_mask_t *result, 
							uint8_t position, 
							uint16_t type, 
							bool has_detent, 
							uint8_t detent_color)
{
	int8_t		dot_count;
	uint32_t	bit_mask;
	int8_t		frac;
	bool		is_blended = false;
	bool        is_bar_display     = false;
	float       remainder = 0;
	
	if (type == BLENDED_BAR){// || type == BLENDED_DOT_DISPLAY) {
		is_blended = true;
	}
	if (type ==  BAR || type == BLENDED_BAR) {
		is_bar_display = true;
	}
	
	if (has_detent) {	
		// 
		if (position == 63 || position == 64 ) {
			// The encoder is in its detent position, set the detent indicator
			// to its color and return.
			result->pattern_A = 0x0001;
			result->pattern_B = 0x0002;
			result->pattern_A_brightness = (uint8_t)(detent_color);
			result->pattern_B_brightness = (uint8_t)(0x7F - (detent_color));
			return 1;
		} else {
			bit_mask = 0x0400;
			if(is_blended) {
				dot_count = (int8_t)((position - 63) / 12.7f);
				remainder = fmodf(position - 63, 12.7f);        
				frac =  (uint8_t)fabs((remainder * 10));
			} else {
				uint8_t center_point = (position > 63) ? 63 : 64;
				dot_count = (int8_t)((position - center_point) / 15.9f);
				remainder = fmodf(position - center_point, 15.9f);        
				frac =  (uint8_t)fabs((remainder * 8));
				//Not blended detent display does not use 12 o clock white LED
				if (remainder < 0){
					dot_count -= 1;
				} else {
					dot_count +=1;
				}
			}	
		}
			 
	} else {
		
		if (is_blended) {
			dot_count = (int8_t)(position / 11.5f);	
			remainder = fmodf(position, 11.5f);         
			frac =  (uint8_t)(remainder * 11);
			bit_mask = 0x10000;
		}
		else {
			dot_count = (int8_t)(position / 11.65f);
			bit_mask = 0x8000;
			frac = 0;
		}
		
		if (position == 0) {
			bit_mask = 0;
		}
		
	}
	
	if (is_bar_display) {
	// Build a bar bit mask	
		int8_t count = dot_count;
		if (dot_count >= 0) {
			while (count) {
				bit_mask |= bit_mask >> 1;
				count--;
			}
		} else if (dot_count < 0) {
			while (count) {
				bit_mask |= bit_mask << 1;
				count++;
			}
		}
	} else {
	// Build a dot bit mask	
		if (dot_count >= 0) {
			bit_mask = bit_mask >> dot_count;
		} else if (dot_count < 0) {
			bit_mask = bit_mask << dot_count*-1;
		}
	}
	
	// Store the bit masks and set their respective brightness levels
	result->pattern_B = (uint16_t)(bit_mask);
	
	// TODO NEED TO GET THE BLENDED DOT DISPLAY WORKING NICE ... leading dot does not fade in
	if ((remainder > 0) && is_blended) {
		result->pattern_A = (uint16_t)(bit_mask | (bit_mask >> 1));
		result->pattern_A_brightness = frac;
		result->pattern_B_brightness = 127 - frac;
			
	} else if ((remainder < 0) && is_blended) {
		result->pattern_A = (uint16_t)(bit_mask | (bit_mask << 1));
		result->pattern_A_brightness = frac;
		result->pattern_B_brightness = 127 - frac;
	}
	else
	{
		result->pattern_A = 0;
		result->pattern_A_brightness = 0;
	}
	
	if (type != BLENDED_DOT) {
		result->pattern_B_brightness = 127;	
	}

	return 1;	
}

/** Builds RGB color bit patterns in the display frame buffer
 *  Inputs:
 *  encoder	- which encoder to set RGB color for
 *	color	- 32 bit color value containing 8 bit RGB information
 *  level   - if not 0 scales the color brightness between 1 - 255 
**/

void build_rgb(uint8_t encoder, uint32_t color, uint8_t level)
{

	float red_pow = 5.0;
	float green_pow = 2.50f;
	float blue_pow = 1.0f;
	
	uint8_t red_byte = (uint8_t)((color >> 16) & 0xFF);
	red_byte = 255 * pow( (((float)red_byte)/255.0f) , (red_pow));
	
	uint8_t green_byte = (uint8_t)((color >> 8) & 0xFF);
	green_byte = 255 * pow( (((float)green_byte)/255.0f) , (green_pow));
	
	uint8_t blue_byte =  (uint8_t)(color & 0xFF);
	blue_byte = 255 * pow( (((float)blue_byte)/255.0f) , (blue_pow));

	if (level) {
		// Dim the color to the specified level
		red_byte = (red_byte * (level-1)) >> 8;
		green_byte = (green_byte * (level-1) ) >> 8;
		blue_byte = (blue_byte * (level-1)) >> 8;
	}
	
	uint8_t *ptr = display_frame_buffer;
	
	// Calculate initial byte offset
	ptr += ((15-encoder)*2);
	
	for (uint8_t i=0;i<NUM_OF_FRAMES;++i)
	{
		// Set RGB bits to "OFF" first
		*ptr |= 0x1C;		
		// Covert to 8 Bit space
		uint8_t value = (i << 1)*(127/NUM_OF_FRAMES);
		//uint8_t value = (i << 1);
		
		if (blue_byte > value){
			*ptr &= ~0x04; 
		}
		if (red_byte > value){
			*ptr &= ~0x08;
		}
		if (green_byte > value){
			*ptr &= ~0x10;
		}
		ptr += 32;
	}
}

/**
 * Draws the supplied bit pattern on the 11 white LEDs for a given encoder
 * 
 *	LED		[1][2][3][4][5][6][7][8][9][10][11][na][na][na][na]
 *	Bit       16 15 14 13 12 11 10 9  8  7   6   5  4  3  2  1 
 *				where 1 = on and 0 = off
 * 
 * \param encoder [in]	The encoder to draw the pattern for
 * 
 * \param pattern [in]	The bit pattern for the encoder
 *
 * \return 
 */

void set_indicator_pattern(uint8_t encoder, uint16_t pattern)
{
	uint8_t ind_brightness = pgm_read_byte(&brightnessMap[global_ind_brightness]);
	set_indicator_pattern_level(encoder, pattern, ind_brightness);
}

// Allows the indicator pattern to be set with a specified brightness setting.
void set_indicator_pattern_level(uint8_t encoder, uint16_t pattern, uint8_t brightness)
{
	// Calculate initial byte offset in frame buffer
	uint8_t *ptr = display_frame_buffer;
	
	ptr += ((15-encoder)*2);
	
	uint8_t pattern_uper_byte = (uint8_t)(pattern >> 8);
	uint8_t pattern_lower_byte = (uint8_t)(pattern & 0xFF);
	
	// Iterate through and build the bit patterns for the 128 frames
	for (uint8_t i=0;i<NUM_OF_FRAMES;++i)
	{
		ptr[0] |= 0xE0;
		ptr[1] |= 0xFF;
		if(i < brightness){
			ptr[0] &= ~(0xE0 & pattern_lower_byte);
			ptr[1]  = (0xFF & ~pattern_uper_byte);
		}
		ptr += 32;
	}
}


/** Sets RGB to active, inactive, or override color for a given encoder
 *  Inputs:
 *  encoder			- Which encoder to set RGB color for
 *  color			- The color setting (0-127)
**/
void set_encoder_rgb(uint8_t encoder, uint8_t color)
{
	uint8_t rgb_brightness = pgm_read_byte(&brightnessMap[global_rgb_brightness]);
	set_encoder_rgb_level(encoder, color, rgb_brightness);
}

/*
 * Sets encoder RGB to a given color and brightness
 * Added for use by certain sequencer display states
 * TODO - Add this feature to the set_encoder_rgb function
 * and update all uses of set_encoder_rgb to include 
 * a brightness setting
 */
void set_encoder_rgb_level(uint8_t encoder, uint8_t color, uint8_t brightness)
{
	rgb_color_setting[encoder] = color;
	uint32_t rgb_color = pgm_read_dword(&colorMap7[rgb_color_setting[encoder]][0]);
	build_rgb(encoder, rgb_color, brightness);
}

/** Sets Indent Red Blue led for a given encoder
 *  Inputs:
 *  encoder - which encoder to set indent color for
 *  color_index - 7 bit color setting
 */
void set_encoder_indent(uint8_t encoder, uint8_t color_index)
{
	uint8_t red_byte = (uint8_t)(0xFF - (color_index*2));
	uint8_t blue_byte =  (uint8_t)((color_index*2) - 0xFF);
	
	uint8_t *ptr = display_frame_buffer;
	
	// Calculate initial byte offset
	ptr += ((15-encoder)*2);
	
	for (uint8_t i=0;i<NUM_OF_FRAMES;++i)
	{
		// Set RGB bits to "OFF" first
		*ptr |= 0x03;		
		// Covert to 8 Bit space
		//uint8_t value = i << 1;
		uint8_t value = (i << 1)*(127/NUM_OF_FRAMES);
		if (blue_byte > value){
			*ptr &= ~0x01; 
		}
		if (red_byte > value){
			*ptr &= ~0x02;
		}
		ptr += 32;
	}
}

/** Builds various MIDI controlled animations for the RGB segments
 *  Inputs:
 *  encoder   - which encoder to set indent animation for
 *  animation - animation settings (0 - 127)
 */
void run_encoder_animation(uint8_t encoder, uint8_t bank, uint8_t animation, uint8_t color)
{
	// 0 is not a valid animation setting
	if (!animation) {
		return;
	}
	
	if ((animation > 0) && (animation < 9)) {
		
		// RGB Strobe Animation
		if (!strobe_animation(animation)) {
			build_rgb(encoder, 0, false);
		} else {
			uint32_t color = pgm_read_dword(&colorMap7[rgb_color_setting[encoder]][0]);
			build_rgb(encoder, color, false);
		}
		
	} else if ((animation > 8) && (animation < 17)) {
		
		// RGB Pulse Animation
		uint8_t level = pulse_animation(animation - 8);
		uint32_t color = pgm_read_dword(&colorMap7[rgb_color_setting[encoder]][0]);
		build_rgb(encoder, color, (uint8_t)(level));	
			
	} else if ((animation > 16) && (animation < 49)) {	
		
		// RGB Dimming	
		uint32_t color = pgm_read_dword(&colorMap7[rgb_color_setting[encoder]][0]);
		// Replace with a look up table used for ALL DIMING
		uint8_t level = pgm_read_byte(&animationBrightnessMap[animation-17]);
		build_rgb(encoder, color, (uint8_t)(level*2));
		
	} else if ((animation > 48) && (animation < 57)) {
		// Read Directly from RAM		
		uint8_t banked_encoder_id = encoder + bank*PHYSICAL_ENCODERS;
		//encoder_config_t enc_cfg = encoder_settings[banked_encoder_id];

		// Indicator Strobe Animation
		if (!strobe_animation(animation-48)) {
			set_encoder_indicator_level(encoder, indicator_value_buffer[bank][encoder], encoder_settings[banked_encoder_id].has_detent,
			encoder_settings[banked_encoder_id].indicator_display_type,
			encoder_settings[banked_encoder_id].detent_color, 0);
			} else {
			set_encoder_indicator_level(encoder, indicator_value_buffer[bank][encoder], encoder_settings[banked_encoder_id].has_detent,
			encoder_settings[banked_encoder_id].indicator_display_type,
			encoder_settings[banked_encoder_id].detent_color, 255);
		}

		// Read from EEPROM
		//encoder_config_t enc_cfg;
		//get_encoder_config(bank, encoder, &enc_cfg); // !revision: lessen overhead of encoder animations, no need to read from EEPROM anymore with expanded encoder_settings
		//
		// Indicator Strobe Animation
		//if (!strobe_animation(animation-48)) {
			//set_encoder_indicator_level(encoder, indicator_value_buffer[bank][encoder], enc_cfg.has_detent,
										//enc_cfg.indicator_display_type,
										//enc_cfg.detent_color, 0);
		//} else {
			//set_encoder_indicator_level(encoder, indicator_value_buffer[bank][encoder], enc_cfg.has_detent,
										//enc_cfg.indicator_display_type,
										//enc_cfg.detent_color, 255);
		//}
		
	} else if ((animation > 56) && (animation < 65)) {
		// Read Directly from RAM
		uint8_t banked_encoder_id = encoder + bank*PHYSICAL_ENCODERS;
		uint8_t level = (uint8_t)(pulse_animation(animation - 55));
		set_encoder_indicator_level(encoder, indicator_value_buffer[bank][encoder], encoder_settings[banked_encoder_id].has_detent,
		encoder_settings[banked_encoder_id].indicator_display_type,
		encoder_settings[banked_encoder_id].detent_color, level);

		// Read from EEPROM
		// Indicator Pulse Animation
		//encoder_config_t enc_cfg;
		//get_encoder_config(bank, encoder, &enc_cfg); // !revision: lessen overhead of encoder animations, no need to read from EEPROM anymore with expanded encoder_settings
		//uint8_t level = (uint8_t)(pulse_animation(animation - 55));
		//set_encoder_indicator_level(encoder, indicator_value_buffer[bank][encoder], enc_cfg.has_detent,
									//enc_cfg.indicator_display_type,
									//enc_cfg.detent_color, level);
		
	} else if ((animation > 64) && (animation < 97)) {
		// Read Directly from RAM
		uint8_t banked_encoder_id = bank*PHYSICAL_ENCODERS + encoder;
		// Indicator Dimming Animation
		uint8_t level = (uint8_t)(2 * pgm_read_byte(&animationBrightnessMap[animation-65]));
		set_encoder_indicator_level(encoder, indicator_value_buffer[bank][encoder], encoder_settings[banked_encoder_id].has_detent,
		encoder_settings[banked_encoder_id].indicator_display_type,
		encoder_settings[banked_encoder_id].detent_color, level);

		// Read From EEPROM
		//encoder_config_t enc_cfg;
		//get_encoder_config(bank, encoder, &enc_cfg); // !revision: lessen overhead of encoder animations, no need to read from EEPROM anymore with expanded encoder_settings
		//
		//// Indicator Dimming Animation
		//uint8_t level = (uint8_t)(2 * pgm_read_byte(&animationBrightnessMap[animation-65]));
		//set_encoder_indicator_level(encoder, indicator_value_buffer[bank][encoder], enc_cfg.has_detent,
									//enc_cfg.indicator_display_type,
									//enc_cfg.detent_color, level);
	
	} else if (animation == 127) {
		
		// Rainbow state
		const float rgb_freq = .049f;
		uint8_t rgb_step =(uint8_t)(animation_counter);
		
		uint32_t red_level = (uint8_t)(sin(rgb_step*rgb_freq)*126)+128;
		rgb_step += 85;
		uint32_t green_level = (uint8_t)(sin(rgb_step*rgb_freq)*126)+128;
		rgb_step += 85;
		uint32_t blue_level = (uint8_t)(sin(rgb_step*rgb_freq)*126)+128;
		
		uint32_t color = (uint32_t)(red_level << 16) + (green_level << 8) + (blue_level);
		build_rgb(encoder, color, 0);
	}
	
}


/** Returns a boolean for 8 different flash rates driven from the animation counter
 *  Inputs:
 *  Flash_rate	- which flash rate state we are checking for
 *  Output		- boolean of specified flash rate state
 */
bool strobe_animation(uint8_t flash_rate)
{
	if (animation_counter & (0x01<<(8-flash_rate))) {
		return true;
	}
	else {
		return false;
	}
}

/** Returns an amplitude value for 8 different pulse rates
 *  Inputs:
 *  Pulse_rate	- which pulse rate we are generating amplitude for
 *  Output		- amplitude of the specified rate at this point in time
 *
 *  NOTE:		- Having trouble linking? Make sure you are linking libm.a
 *                This is not linked by default in some versions of ASF
 */ 
uint8_t pulse_animation(uint8_t pulse_rate)
{
	const float rgb_freq = .04927f;
	static uint8_t rgb_step;
	//original_code: rgb_step  = (uint8_t)(((animation_counter<<5)>>(8-pulse_rate)) & 0xFF);
	if(!midi_clock_enabled){ // !Summer2016Update: midi_clock_animations
		rgb_step  = (uint8_t)(((animation_counter<<5)>>(8-pulse_rate)) & 0xFF);
	} 
	else{
		rgb_step  = (uint8_t)(((animation_counter<<5)>>(8-pulse_rate)) & 0xFF); // !review: << 5 is an estimate
	}

	uint8_t level = (uint8_t)(sin(rgb_step*rgb_freq)*126)+128;

	return level;
}

/**
 * A basic settings received animation
 * 
 */
void setting_confirmation_animation(uint32_t color)
{
	#define COLOR	0x00FF00
	
	// Clear display buffer
	clear_display_buffer();
	
	// Ensure the display interrupts are enabled
	//PMIC.CTRL = PMIC_LOLVLEN_bm;
	
	// And the display timer is running
	//display_enable();
	PMIC.CTRL |= ( PMIC_LOLVLEN_bm);
	
	// make sure the display is enabled
	display_enable();
	
	double freq = ((3.14159)/127);

	static int8_t step1 = 0;
	static int8_t step2 = - 42;
	static int8_t step3 = - 84;
	static int8_t step4 = -127;
	
	uint8_t level1;
	uint8_t level2;
	uint8_t level3;
	uint8_t level4;
	
	for(uint8_t j=0;j<255;++j){
	
		if (step1 > -1){
			level1 = (uint8_t)(255*sin(step1*freq))+1;
		} else {
			level1 = 1;
		}
		
		if (step2 > -1){
			level2 = (uint8_t)(255*sin(step2*freq))+1;
		} else {
			level2 = 1;
		}
		if (step3 > -1){
			level3 = (uint8_t)(255*sin(step3*freq))+1;
		} else {
			level3 = 1;
		}
		if (step4 > -1){
			level4 = (uint8_t)(255*sin(step4*freq))+1;
		} else {
			level4 = 1;
		}
	
		build_rgb(0, color, level1);
		build_rgb(1, color, level1);
		build_rgb(2, color, level1);
		build_rgb(3, color, level1);
		
		build_rgb(4, color, level2);
		build_rgb(5, color, level2);
		build_rgb(6, color, level2);
		build_rgb(7, color, level2);
		
		build_rgb(8, color, level3);
		build_rgb(9, color, level3);
		build_rgb(10, color, level3);
		build_rgb(11, color, level3);
		
		build_rgb(12, color, level4);
		build_rgb(13, color, level4);
		build_rgb(14, color, level4);
		build_rgb(15, color, level4);
		
		step1+=1;
		step2+=1;
		step3+=1;
		step4+=1;
		
		Delay_MS(1);
		
	}
}



/**
 * Runs the 'Sparkle' start up routine
 * 
 * \param count [in]	The number of sparkles
 * 
 * \return 
 */

static uint8_t sparkle_count = 0;
static uint8_t sparkle_intensity[16];

// Sparkle Start and End colors in RGB format
static uint8_t sparkle_start_color[3] = {0x00,0x00,0xFF};
static uint8_t sparkle_end_color[3]	= {0xFE,0x00,0xD3};
	
void run_sparkle(uint8_t count)
{
	sparkle_count = count;
	
	uint8_t prev_sparkle_intensity[16];
	
	for(uint8_t i=0;i<16;++i){
		sparkle_intensity[i] = 0;
		prev_sparkle_intensity[i] = 0;
	}
	
	// Initialize some LEDs as on
	sparkle_intensity[random16() & 0xf] = 0xff;
	
	animation_frames_remaining = 16 + (random16() & 0x7f);  // between 4..20

	while (build_sparkles())
	{
		uint32_t rb;
		uint32_t gb;
		uint32_t bb;
		
		for(uint8_t i=0;i<16 ;++i){
			
			if (sparkle_intensity[i] != prev_sparkle_intensity[i]){		
				
				uint8_t t = sparkle_intensity[i] << 1;
		
				if (sparkle_intensity[i] > 127) {
					rb = lerp(sparkle_start_color[0], sparkle_end_color[0], t);
					gb = lerp(sparkle_start_color[1], sparkle_end_color[1], t);
					bb = lerp(sparkle_start_color[2], sparkle_end_color[2], t);
				} else {
					rb = lerp(sparkle_end_color[0], 0, t);
					gb = lerp(sparkle_end_color[1], 0, t);
					bb = lerp(sparkle_end_color[2], 0, t);
				}
				
				uint32_t new_color = ((rb << 16) & 0xFF0000) | ((gb << 8) & 0xFF00) | (bb & 0xFF);
				
				build_rgb(i, new_color, 0xFE);
				
				// Store the value for the next comparison
				prev_sparkle_intensity[i] = sparkle_intensity[i];
			}
		}
		
	}	
}

bool build_sparkles(void)
{
	bool active = false;
	
	// Scan through the remaining sparkles and decrease
	// their brightness randomly 25% of the time
	for(uint8_t i=0;i<16;++i){
		if(sparkle_intensity[i] > 0 ) {
			active = true;
			if ((random16() & 0x3F) == 0) {
				--sparkle_intensity[i];
			}
		}
	}
	
	// Check if we are finished
	if (sparkle_count == 0 && !active){
		return false;
	}
	
	if ((sparkle_count > 0) && (animation_frames_remaining == 0)) {
		// Add a new sparkle, but only to 'dead' pixels
		uint8_t new = random16() & 0xf;
		
		if (!sparkle_intensity[new]){
			sparkle_intensity[new] = 0xff;
			sparkle_intensity[random16() & 0xf] = 0xff;
			// set time to generate next sparkle to random between 16 & 127 ms
			animation_frames_remaining = 16 + (random16() & 0x7F);
			// decrement the sparkle count
			--sparkle_count;
		}
	}
	
	return active;
}


/** Draws a rainbow display on the RGB segments. */
void rainbow_demo(void)
{
	static uint8_t idx=0;
	static uint8_t colorIndex = 0;
	
	if (colorIndex > 126){
		colorIndex = 1;
	}
	if (idx >15){
		idx = 0;
	}
	
	int8_t color = colorIndex + idx;
	
	if (color < 0){
		color = color + 127;
	}
	
	set_encoder_rgb(idx,color);
	
	idx+=1;
	colorIndex +=1;
	Delay_MS(150);
}


// Linear interpolation between two values.
// 8-bit fixed point values where 0 = 0.0f and 255 = 1.0f
//
uint8_t lerp(uint8_t high, uint8_t low, uint8_t t)
{
	int16_t temp = t * (high-low);
	return low + (temp >> 8);
}

// Valid seeds for the random number generators.
//
static uint16_t g_seed16 = 36243;

// Mathematical functions -----------------------------------------------------

// Generate pseudorandom numbers, based on "Xorshift RNGs", George
// Marsaglia, 2003
//
//    http://www.jstatsoft.org/v08/i14/paper
//

// Return a 16-bit pseudorandom value.
uint16_t random16(void)
{
	g_seed16 ^= (g_seed16 << 13);
	g_seed16 ^= (g_seed16 >> 9);
	g_seed16 ^= (g_seed16 << 7);
	return g_seed16;
}

