/*
 *	jumptobootloader.c
 *
 *	Created: 9/17/2013 9:40:50 AM
 *  Author: Michael
 *  Based of code taken from Dean Camera's boot loader from application code example  
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
#include "jump_to_bootloader.h"

// This attribute ensures this variables is not initialized on reset
uint32_t Boot_Key ATTR_NO_INIT;

void(* start_bootloader)(void) = (void (*)(void))(BOOT_SECTION_START/2+0x1FC/2);

// This attribute ensures this function is called before main()
void Bootloader_Jump_Check(void) ATTR_INIT_SECTION(3);
void Bootloader_Jump_Check(void)
{
	// If the reset source was the boot loader and the key is correct, clear it and jump to the boot loader
	if (((reset_cause_get_causes() & CHIP_RESET_CAUSE_WDT)) && (Boot_Key == MAGIC_BOOT_KEY))
	{
		Boot_Key = 0;
		EIND = BOOT_SECTION_START>>17;//We are dealing with a 17 bit address here so need to load the MSbit into EIND
		start_bootloader();
	}
}

/**
 * This function disables the USB & interrupt system, then waits 2 seconds for
 * the USB detach to register with the host, following this the magic key is
 * written and a mcu reset is forced using the WDT.
 * 
 */
void Jump_To_Bootloader(void)
{	
	USB_Disable();
	
	cpu_irq_disable();

	for (uint8_t i = 0; i < 128; i++)
	_delay_ms(16);

	Boot_Key = MAGIC_BOOT_KEY;
	wdt_reset_mcu();
}
