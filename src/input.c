/*
 * input.c
 *
 * Created: 6/30/2013 2:19:30 PM
 *  Author: Michael
 *  Input handles the reading of the 6 side buttons and 16 encoders with
 *  integrated switches . 
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

#include <input.h>

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

static uint32_t ms_timer;

// Buffers used to debounce the switch states;
uint16_t enc_switch_debounce_buffer[SWITCH_DEBOUNCE_BUFFER_SIZE];
uint8_t side_switch_debounce_buffer[SWITCH_DEBOUNCE_BUFFER_SIZE];

// The previous switch states
uint16_t g_enc_prev_switch_state;
uint8_t  g_side_prev_switch_state;

// The state variables can be used to determine the switch states - DUH
// if a bit is set the switch is closed, otherwise it is open
uint16_t g_enc_switch_state;
uint8_t  g_side_switch_state;
	
// The up/down variables can be used to determine if the switch's have 
// changed state since the last time switch_calc was called
uint16_t g_enc_switch_up;
uint16_t g_enc_switch_down;
uint8_t  g_side_switch_up;
uint8_t  g_side_switch_down;

// The relative movement of the 16 encoders
int8_t  encoder_state[16];  // the number of encoder steps since last time get_encoder_value was called

// The previous encoder pin states
uint16_t encoder_cha_state_prev = 0;
uint16_t encoder_chb_state_prev = 0;
uint8_t encoder_inactive_counter[16];
// ===== Velocity Calculation Method ==================

#if VELOCITY_CALC_METHOD == VELOCITY_CALC_M_TPS_BLOCKS
int8_t encoder_last_movement[16];
uint16_t encoder_event_cycle_counts[16];
#endif


// - If velocity is less than 10 ticks per second, multiplier = 1
// - If velocity is greater than 10 ticks per second, mutliplier = 1 + multiplier adjustment
// -- multiplier adjustment is a number between 0-255.

// --- Method 1: Calculate Velocity based on cycles between movement 'events', debounce events that change direction too quickly
// ------ We collect packets through interrupts while the mainloop runs. To handle cycles with multiple events, just sum the events together.
// ------ TPS Blocks


// - Velocity Measurements (ms per tick)
// -- Ticks per 360-degrees: 96  
// -- Encoder Poll Time (interrupt): measured as 1.21ms
// -- Very slow turn: 200-500ms (1 multiplier)  (2 to 5 ticks-per-second)
// -- slow turn: 200-100ms (1 multiplier)  		(5 to 10 ticks-per-second)
// -- med turn: ~50ms							(20 ticks-per-second)
// -- quick turn: ~25ms							(40 ticks-per-second)
// -- super fast turn: 1-4ms per tick 			(250-1000 ticks-per-second)
// ---- (1/2 turn (48-ticks) should span full range maybe 2/3rds (64-ticks))
// ---- 7-bit: 2.67 multiplier (1/2), 2.00 multiplier (2/3) 
// ---- 14-bit: 341.3 muliplier (1/2), 256 multiplier (2/3)

#if VELOCITY_CALC_METHOD > VELOCITY_CALC_M_NONE 
const float velocity_calc_slope = (VELOCITY_CALC_MAX_MULTIPLIER-VELOCITY_CALC_MIN_MULTIPLIER)/(VELOCITY_CALC_TPS_MAX-VELOCITY_CALC_TPS_MIN);
const float velocity_calc_offset = -1*(VELOCITY_CALC_MIN_MULTIPLIER + 
				(VELOCITY_CALC_MAX_MULTIPLIER-VELOCITY_CALC_MIN_MULTIPLIER)/(VELOCITY_CALC_TPS_MAX-VELOCITY_CALC_TPS_MIN)*VELOCITY_CALC_TPS_MIN);
#endif
// ===== Velocity Calculation Method ==============END=

static uint16_t timer_cca_value = 125;
//static uint16_t timer_ccb_value = 0;

void do_task(void);

/* Input_init initializes the input hardware. The six side buttons are
 * connected to port A pins 0 - 5, and the 16 encoders with integrated 
 * push switches are read from 6 74HC165 shift registers connected on 
 * port C pins 0 - 2.
 *
 * Side Switch Pin Connections
 *
 * Side Switch 1 = PA.5, Side Switch 2 = PA.4, Side Switch 3 = PA.3
 * Side Switch 4 = PA.2, Side Switch 5 = PA.1, Side Switch 6 = PA.0
 * 
 * Encoder Shift Register Pin Connections
 *
 * ENC_LATCH = PC.0, ENC_CLK = PC.1, ENC_DATA_IN = PC.2
 *
 */
 
void input_init(void)
{
	//Initialize Side Switch Pins as inputs
	ioport_set_pin_dir(SIDE_SW1, IOPORT_DIR_INPUT);
	ioport_set_pin_dir(SIDE_SW2, IOPORT_DIR_INPUT);
	ioport_set_pin_dir(SIDE_SW3, IOPORT_DIR_INPUT);
	ioport_set_pin_dir(SIDE_SW4, IOPORT_DIR_INPUT);
	ioport_set_pin_dir(SIDE_SW5, IOPORT_DIR_INPUT);
	ioport_set_pin_dir(SIDE_SW6, IOPORT_DIR_INPUT);
		
	//Enable internal pull-up resistors on side switch input pins
	ioport_set_pin_mode(SIDE_SW1, IOPORT_MODE_PULLUP);
	ioport_set_pin_mode(SIDE_SW2, IOPORT_MODE_PULLUP);
	ioport_set_pin_mode(SIDE_SW3, IOPORT_MODE_PULLUP);
	ioport_set_pin_mode(SIDE_SW4, IOPORT_MODE_PULLUP);
	ioport_set_pin_mode(SIDE_SW5, IOPORT_MODE_PULLUP);
	ioport_set_pin_mode(SIDE_SW6, IOPORT_MODE_PULLUP);
	
	//Initialize pins for reading encoder shift registers
	ioport_set_pin_dir(ENC_LATCH, IOPORT_DIR_OUTPUT);
	ioport_set_pin_dir(ENC_CLK, IOPORT_DIR_OUTPUT);
	ioport_set_pin_dir(ENC_DATA, IOPORT_DIR_INPUT);
	
	//Leave latch high and the clk low
	ioport_set_pin_level(ENC_LATCH, true);
	ioport_set_pin_level(ENC_CLK, false); 
	
	//Clear out the state variables
	memset(encoder_state, 0x00, 16);
	
	#if VELOCITY_CALC_METHOD == VELOCITY_CALC_M_TPS_BLOCKS
		memset(encoder_last_movement, 0x00, 16); // 0 is "unknown movement"
		memset(encoder_event_cycle_counts, 0xFF, 32); // memset works in units of uint8_t so double the members for a uint16_t array 0xFF -> 255ms+
	#endif
	
	
	g_enc_prev_switch_state  = 0;
	g_enc_switch_state		 = 0;
	g_enc_switch_up			 = 0;
	g_enc_switch_down        = 0;
	
	g_side_switch_state      = 0;
	g_side_prev_switch_state = 0;
	g_side_switch_up		 = 0;
	g_side_switch_down       = 0;


	// Timer Initialization ---------------------------------------------------
	
	/** The Input Driver uses timer counter compare A interrupt to ensure the 
	 *	inputs are scanned every 1.28 ms. (1/32x10^6) x 1024 x 40
	 *  To support the timing requirements of the sequencer and other functions
	 *  access to the second CC channel is provided, allowing scheduling of
	 *  events.
	 */
	
	#define INPUT_SCAN_RATE 40
	
	// Enable Timer 1 
	tc_enable(&TCC1);
	// Set Timer 1 overflow callback to display_frame_timer()
	tc_set_cca_interrupt_callback(&TCC1, encoder_scan);
	tc_set_ccb_interrupt_callback(&TCC1, do_task);
	// Set Timer 1 waveform mode to count up
	tc_set_wgm(&TCC1, TC_WG_NORMAL);
	// Set Timer 1 CCA match value to 40 (1mS)
	tc_write_cc(&TCC1, TC_CCA, INPUT_SCAN_RATE);
	// Set Timer 1 CCA interrupt level to low
	tc_set_cca_interrupt_level(&TCC1, TC_INT_LVL_HI);  // Interrupt Priority: Buttons 1
	// Disable the CCB interrupt
	tc_set_ccb_interrupt_level(&TCC1, TC_INT_LVL_OFF); // Interrupt Priority: Buttons 2 (inert) (used by schedule_task)
	// Enable interrupts
	cpu_irq_enable();
	// Set the Timer 1 clock source to system / 1024
	tc_write_clock_source(&TCC1, TC_CLKSEL_DIV1024_gc);
	
	ms_timer = 0;
	
	ioport_set_pin_dir(DEBUG_PIN, IOPORT_DIR_OUTPUT);
}

/**
 * Callback function for the task scheduler. This function is called when
 * the specified amount of time for a scheduled task has passed. It disables
 * the CCB interrupt then calls the scheduled task.
 * 
 */

static void (*scheduled_task) (void);
static bool task_is_scheduled = false;

void do_task(void)
{
	tc_set_ccb_interrupt_level(&TCC1, TC_INT_LVL_OFF); // Interrupt Priority: Buttons 2 (inert)
	task_is_scheduled = false;
	scheduled_task();
}

/**
 * Schedules a single task to occur after a certain amount of time. 
 *
 * The longest schedule time is 2^16 x 40 uS = 2.621 seconds
 * 
 * \param task [in] pointer to a function, must be of void type
 *
 * \param time [in] time increments of 40 uS
 * 
 * \return true if scheduled
 */

bool schedule_task(void (*task)(void), uint16_t time)
{
	// Only schedule a task if the scheduler is idle
	if (!task_is_scheduled){
		
		task_is_scheduled = true;
		scheduled_task = task;

		uint16_t current_counter_value = tc_read_count(&TCC1);
	
		uint16_t compare_value = current_counter_value + time;
	
		// Write the compare value into its register and enable the interrupt
		tc_write_cc(&TCC1, TC_CCB, compare_value);
		tc_set_ccb_interrupt_level(&TCC1, TC_INT_LVL_MED);	// Interrupt Priority: Buttons 2 (ACTIVE)
		
		return true;
	} else {
		return false;
	}
}

bool cancel_task() 
{
	task_is_scheduled = false;
	tc_set_ccb_interrupt_level(&TCC1, TC_INT_LVL_OFF);	// Interrupt Priority: Buttons 2 (inert)
	return true;
}


/**
 * Returns a 32 bit counter, this counter is incremented ever 1ms by the input
 * scan routine. This is NOT accurate (CLK is +/- 1.5%)
 *
 * \return ms_timer value
 */

uint32_t get_ms_timer(void)
{
	return ms_timer;
}

/*	
 *  encoder_scan scans the encoder pins and converts any pin state changes to 
 *  relative movements which are stored in the encoder_state array.
 */
static uint8_t enc_switch_buffer_pos = 0;
// #define ENCODER_INTERPRET_VERSION_ORIG 0 
// #define ENCODER_INTERPRET_VERSION_STATE_TABLE 1
// #define ENCODER_INTERPRET_VERSION ENCODER_INTERPRET_VERSION_STATE_TABLE

// #if ENCODER_INTERPRET_VERSION == ENCODER_INTERPRET_VERSION_STATE_TABLE
// Receives 4 bit value, [0]:last chan a, [1]: last chan b, [2]: current chan a, [3]: current chan b
// Returns: 0 = idle, -127 = Ambiguous, 1 = Increment Once, -1 = Decrement Once
const int8_t EncoderActionByState[16] = {	
	0, -1, 1, -127,
	1, 0, -127, -1,
	-1, -127, 0, 1,
	-127, 1, -1, 0
};
// #endif

void encoder_scan(void)   // MIDI Output: Digital Inputs -> Encoders (Read Pins)
{
	// Increment the timer compare value
	timer_cca_value += INPUT_SCAN_RATE;
	tc_write_cc(&TCC1, TC_CCA, timer_cca_value);
	// Increment the ms_timer count
	ms_timer++;
	
	// Latch the encoder data into the shift registers, latching data also 
	// presents first bit to ENC_DATA so no need to clk in the first bit
	ioport_set_pin_level(ENC_LATCH, true);
	uint16_t current_enc_switch_state = 0;
	uint16_t bit   = 0x8000;
	
	// Read the 16 Switch States, set the corresponding bit in switch_state if 
	// the switch is pressed.
	for (uint8_t i = 0; i < 16; ++i) {
		ioport_set_pin_level(ENC_CLK, false);// CLK falling edge does nothing
		current_enc_switch_state |= ioport_get_pin_level(ENC_DATA) ? 0 : bit;
		bit >>= 1;
		ioport_set_pin_level(ENC_CLK, true); //CLK is active on the rising edge
	}
	
	// Add the current state to the circular debounce buffer, and increment the 
	// buffer pos.
	enc_switch_debounce_buffer[enc_switch_buffer_pos] = current_enc_switch_state;
	enc_switch_buffer_pos = (enc_switch_buffer_pos + 1) % SWITCH_DEBOUNCE_BUFFER_SIZE;
	uint16_t encoder_cha_state = 0;
	uint16_t encoder_chb_state = 0;
	bit   = 0x8000;
	
	// Read the encoder channel a and b states for the 16 encoders
	for (uint8_t i = 0; i < 16;++i) {
		ioport_set_pin_level(ENC_CLK, false);
		encoder_chb_state |= ioport_get_pin_level(ENC_DATA) ? bit : 0;
		ioport_set_pin_level(ENC_CLK, true);
		ioport_set_pin_level(ENC_CLK, false);
		encoder_cha_state |= ioport_get_pin_level(ENC_DATA) ? bit : 0;
		bit >>=1;
		ioport_set_pin_level(ENC_CLK, true);
	}
	
	// Leave the encoder register latch low now we are done reading
	ioport_set_pin_level(ENC_LATCH, false);
	
	// Process the encoder channel state data
	bit = 0x00001;
	for (uint8_t i = 0; i < 16;++i) {
		//#if ENCODER_INTERPRET_VERSION == ENCODER_INTERPRET_VERSION_STATE_TABLE
		// Calculate the index for the EncoderActionByState table
		uint8_t this_encoder_state = ((encoder_cha_state & bit) ? 0x04 : 0x00) | ((encoder_chb_state & bit) ? 0x08 : 0x00) |
					 ((encoder_cha_state_prev & bit) ? 0x01 : 0x00) | ((encoder_chb_state_prev & bit) ? 0x02 : 0x00);
		int8_t encoder_action = EncoderActionByState[this_encoder_state]; // Get Encoder Action for current state
		if (encoder_action == 0 || encoder_action <= -127) { // Encoder is Idle
			// TODO: Handle the -127 'ambiguous' state more gracefully (presuming direction by momentum for example)
			if ( encoder_inactive_counter[i] < ENCODER_INACTIVE_THRESHOLD ){ // For determining when an encoder is inactive
				encoder_inactive_counter[i]++;
			}

		} //else if (enocder_action <= -127) { // Encoder is in an ambiguous state
		//} 
		else if (encoder_action < 0) { // Event! Moving CCW
			// encoder event table: mark event type +store event cycle count + increment event counter
			#if VELOCITY_CALC_METHOD == VELOCITY_CALC_M_TPS_BLOCKS
				uint16_t cycle_count = encoder_inactive_counter[i] + 1;
				int8_t last_move = encoder_last_movement[i]; 
				if (cycle_count >= ENCODER_DEBOUNCE_CYCLE_TIMEOUT) { // event spacing was reasonable, allow to travel freely in either direction
					// Add event to the tally
					encoder_state[i]--;
					encoder_event_cycle_counts[i] += cycle_count; // cycles for this event to occur.
					encoder_last_movement[i] = -1;
				} else if (last_move == -1) { // Moving fast but in a consistent direction
					// Add event to the tally
					encoder_state[i]--;
					encoder_event_cycle_counts[i] += cycle_count; // cycles for this event to occur.
					//redundant encoder_last_movement[i] = -1;
				} else { // moving fast in a different direction
					// Reject Event, but change direction to allow subsequent events to pass if in same direction
					encoder_last_movement[i] = -1;
				}
				encoder_inactive_counter[i] = 0; // clear the inactive counter for all states
			#else // original code
				encoder_state[i]--; 
				encoder_inactive_counter[i] = 0; // clear the inactive counter
			#endif
		} else { // Event! Moving CW
			// !mark encoder event table: mark event type +store event cycle count + increment event counter
			#if VELOCITY_CALC_METHOD == VELOCITY_CALC_M_TPS_BLOCKS
				uint16_t cycle_count = encoder_inactive_counter[i] + 1;
				int8_t last_move = encoder_last_movement[i];
				if (cycle_count >= ENCODER_DEBOUNCE_CYCLE_TIMEOUT) { // event spacing was reasonable, allow to travel freely in either direction
					// Add event to the tally
					encoder_state[i]++;
					encoder_event_cycle_counts[i] += cycle_count; // cycles for this event to occur.
					encoder_last_movement[i] = 1;
				} else if (last_move == 1) { // Moving fast but in a consistent direction
					// Add event to the tally
					encoder_state[i]++;
					encoder_event_cycle_counts[i] += cycle_count; // cycles for this event to occur.
					//redundant encoder_last_movement[i] = 1;
				} else { // moving fast in a different direction
					// Reject Event, but change direction to allow subsequent events to pass
					encoder_last_movement[i] = 1;
				}
				encoder_inactive_counter[i] = 0; // clear the inactive counter for all states
			#else // original code
				encoder_state[i]++;
				encoder_inactive_counter[i] = 0; // clear the inactive counter
			#endif
		}
		bit <<= 1;
		// endif ENCODER_INTERPRET_VERSION == ENCODER_INTERPRET_VERSION_STATE_TABLE
		//#elif ENCODER_INTERPRET_VERSION == ENCODER_INTERPRET_VERSION_ORIG
		//#endif
	}
	// Store current state for future comparisons
	encoder_cha_state_prev = encoder_cha_state;
	encoder_chb_state_prev = encoder_chb_state;
}

/**
 * Returns number of encoder steps since last time this function was called
 */
int8_t get_encoder_value(uint8_t encoder)
{
	int8_t return_value = encoder_state[encoder];
	encoder_state[encoder] = 0;
	return return_value;
}
#if VELOCITY_CALC_METHOD == VELOCITY_CALC_M_TPS_BLOCKS
uint16_t get_encoder_cycle_count(uint8_t encoder) {
	uint16_t return_value = encoder_event_cycle_counts[encoder];
	encoder_event_cycle_counts[encoder] = 0;
	return return_value;
}
#endif

/**
 * Scans the encoder switch registers and returns the de-bounced 
 * enc_switch_state global variable.
 */
uint16_t update_encoder_switch_state(void)
{
	// De-bounce the encoder switch's by ORing the columns of the buffer
	// Any switch down even is immediately recognized, however it will take 10 samples
	// for a switch design to be recognized
	g_enc_switch_state = 0;
	
	for(uint8_t i=0;i<SWITCH_DEBOUNCE_BUFFER_SIZE;++i) {
		g_enc_switch_state |= enc_switch_debounce_buffer[i];
	}
	
	// If a bit has changed, and it is 1 in the current state, it's a KeyUp.
	g_enc_switch_up = (g_enc_prev_switch_state ^ g_enc_switch_state) & g_enc_prev_switch_state;
	// If a bit has changed and it was 1 in the previous state, it's a KeyDown.
	g_enc_switch_down = (g_enc_prev_switch_state ^ g_enc_switch_state) & g_enc_switch_state;
	// Demote the current state to history.
	g_enc_prev_switch_state = g_enc_switch_state;
	
	return g_enc_switch_state;
}

uint16_t get_enc_switch_state(void)
{
	return g_enc_switch_state;
}

uint16_t get_enc_switch_down(void)
{
	return g_enc_switch_down;
}

uint16_t get_enc_switch_up(void)
{
	return g_enc_switch_up;
}

/**
 * Scans the side switch pins and returns the de- bounced 
 * side_switch_state global variable.
 */

static uint8_t side_switch_buffer_pos = 0;

uint16_t update_side_switch_state(void)
{
	uint8_t current_side_switch_state = 0;
	uint8_t bit   = 0x0001;
	
	// Read in the side switch pin states
	// This code is ugly but the side switch naming convention does not
	// match the pin order so this corrects for that
	
	if(!ioport_get_pin_level(SIDE_SW1)){
		current_side_switch_state |= bit;
	}
	bit <<= 1;
	if(!ioport_get_pin_level(SIDE_SW2)){
		current_side_switch_state |= bit;
	}
	bit <<= 1;
	if(!ioport_get_pin_level(SIDE_SW3)){
		current_side_switch_state |= bit;
	}
	bit <<= 1;
	if(!ioport_get_pin_level(SIDE_SW4)){
		current_side_switch_state |= bit;
	}
	bit <<= 1;
	if(!ioport_get_pin_level(SIDE_SW5)){
		current_side_switch_state |= bit;
	}
	bit <<= 1;
	if(!ioport_get_pin_level(SIDE_SW6)){
		current_side_switch_state |= bit;
	} 
	
	// Add the current state to the de-bounce buffer
	side_switch_debounce_buffer[side_switch_buffer_pos] = current_side_switch_state;
	side_switch_buffer_pos = (side_switch_buffer_pos + 1) % SWITCH_DEBOUNCE_BUFFER_SIZE;
	
	// De-bounce the encoder switch's by ORing the columns of the buffer
	// Any switch down even is immediately recognized, however it will take 10 samples
	// for a switch up to be recognized
	g_side_switch_state = 0;
	
	for(uint8_t i=0;i<SWITCH_DEBOUNCE_BUFFER_SIZE;++i) {
		g_side_switch_state |= side_switch_debounce_buffer[i];
	} 
	
	// If a bit has changed, and it is 1 in the current state, it's a KeyDown.
	g_side_switch_up = (g_side_prev_switch_state ^ g_side_switch_state) & g_side_prev_switch_state;
	// If a bit has changed and it was 1 in the previous state, it's a KeyUp.
	g_side_switch_down = (g_side_prev_switch_state ^ g_side_switch_state) & g_side_switch_state;
	// Demote the current state to history.
	g_side_prev_switch_state = g_side_switch_state;

	return g_side_switch_state;
}

uint16_t get_side_switch_state(void)
{
	return g_side_switch_state;
}

uint16_t get_side_switch_down(void)
{
	return g_side_switch_down;
}

uint16_t get_side_switch_up(void)
{
	return g_side_switch_up;
}

bool encoder_is_active(uint8_t enc_idx)
{
	if (encoder_inactive_counter[enc_idx] >= ENCODER_INACTIVE_THRESHOLD){
		return false;
	}
	return true;
}




