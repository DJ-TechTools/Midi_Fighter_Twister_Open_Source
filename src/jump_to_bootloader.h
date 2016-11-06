/*
 * jumptobootloader.h
 *
 * Created: 9/17/2013 9:41:07 AM
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


#ifndef JUMPTOBOOTLOADER_H_
#define JUMPTOBOOTLOADER_H_

	/* Includes: */
	#include <asf.h>
	#include <LUFA/Common/Common.h>
	#include <LUFA/Drivers/USB/USB.h>
	
	/* Macros: */
	
	#define MAGIC_BOOT_KEY            0xDC42ACCA
	
	/* Function Prototypes: */
	
	void Bootloader_Jump_Check(void) ATTR_INIT_SECTION(3);
	void Bootloader_Jump_Check(void);
	void Jump_To_Bootloader(void);
	
#endif /* JUMPTOBOOTLOADER_H_ */