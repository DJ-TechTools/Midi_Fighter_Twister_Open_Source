// Sysex Handling functions
//
//   Copyright (C) 2012 DJ Tech Tools
//
// DanielKersten 2011
//
// DJTT - MIDI Fighter Twister - Embedded Software License
// Copyright (c) 2016: DJ Tech Tools
// Permission is hereby granted, free of charge, to any person owning or possessing 
// a DJ Tech-Tools MIDI Fighter Twister Hardware Device to view and modify this source 
// code for personal use. Person may not publish, distribute, sublicense, or sell 
// the source code (modified or un-modified). Person may not use this source code 
// or any diminutive works for commercial purposes. The permission to use this source 
// code is also subject to the following conditions:
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, 
// INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,  FITNESS FOR A 
// PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT 
// HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION 
// OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE 
// SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#ifndef _SYSEX_H_INCLUDED
#define _SYSEX_H_INCLUDED

#include <stdint.h>
#include <util/delay.h>

#include "midi.h"
#include "constants.h"
#include "config.h"

#define STR_COMBINE(a,b) a##b
#define SYSEX_READ(n) if (index < SYSEX_MAX_PAYLOAD) {sysEx.data[index++] = STR_COMBINE(input_event.Data,n);} else {index = 0; continue;} do {} while (0)
#define SYSEX_END() sysEx.length = index; sysex_handle (&sysEx); index = 0

// SysEx constants -----------------------------------------------
#define SYSEX_MAX_PAYLOAD (MIDI_MAX_SYSEX - 5)

// SysEx types     -----------------------------------------------

// SysEx command handler function
typedef void (*SysExFn)(uint8_t, uint8_t*);

// SysEx functions -----------------------------------------------

// Install a new sysex message handler
#define sysex_install(cmd,fn) sysex_install_(cmd, (SysExFn)fn)
void sysex_install_ (uint8_t cmd, SysExFn fn);

// Handle a 3-byte start or continue message
void sysex_handle_3sc (MIDI_EventPacket_t* packet);
// Handle a 3-byte end message
void sysex_handle_3e (MIDI_EventPacket_t* packet);
// Handle a 2-byte end message
void sysex_handle_2e (MIDI_EventPacket_t* packet);
// Handle a 1-byte end message
void sysex_handle_1e (MIDI_EventPacket_t* packet);


#endif // _SYSEX_H_INCLUDED
