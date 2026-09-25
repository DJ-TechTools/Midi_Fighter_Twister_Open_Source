/*
 * calibration.h
 *
 * RGB white balance calibration for the Midi Fighter Twister.
 *
 * DJTT - Midi Fighter Twister - Embedded Software License
 * Copyright (c) 2026: DJ TechTools
 * Permission is hereby granted, free of charge, to any person owning or possessing 
 * a DJ TechTools Midi Fighter Twister Hardware Device to view and modify this source 
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


#ifndef CALIBRATION_H_
#define CALIBRATION_H_

	/* Includes: */
	#include <asf.h>

	#include "constants.h"

	/* Function Prototypes: */

	// Reads the stored calibration from EEPROM into the display driver, or the
	// defaults if the block is absent or its checksum fails. A format 1 block is
	// converted in RAM.
	void calibration_init(void);

	// Runs the calibration editor from the setup menu. It blocks until the user
	// presses Back. It edits RAM only, and returns true if the user changed a value.
	bool calibration_edit(void);

	// Writes the calibration in RAM to EEPROM.
	void calibration_commit(void);

#endif /* CALIBRATION_H_ */
