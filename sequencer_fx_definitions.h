/*
 * sequencer_fx_definitions.h
 *
 * Created: 12/4/2013 10:34:07 AM
 *  Author: Michael
 */ 


#ifndef SEQUENCER_FX_DEFINITIONS_H_
#define SEQUENCER_FX_DEFINITIONS_H_


/*	Includes: */

	#include <asf.h>

/*	Macros: */

	#define no_Effect			0
	#define Delay				1
	#define Reverb				2
	#define Flanger				3
	#define Flanger_Pulse		4
	#define Flanger_Flux		5
	#define Gater				6
	#define Beatmasher_2		7
	#define Delay_T3			8
	#define Filter_LFO			9
	#define Filter_Pulse		10
	#define Filter				11
	#define Filter92_LFO		12
	#define Filter92_Pulse      13
	#define Filter92			14
	#define Phaser				15
	#define Phaser_Flux			16
	#define Reverse_Grain		17
	#define Turntable_FX		18
	#define Iceverb				19
	#define Reverb_T3           20
	#define Ringmodulator		21
	#define Digital_Lofi		22
	#define Mulholland_Drive	23
	#define Transpose_Strech    24
	#define Beatslicer			25
	#define Formant_Filter		26
	#define Peak_Filter         27
	#define Tape_Delay			28
	#define Ramp_Delay			29
	#define Auto_Bouncer		30
	#define Bouncer				31

/*	Types:	*/

	typdef struct {
		
		uint8_t effectSelection_1;
		uint8_t effectSelection_2;
		uint8_t effectSelection_3;
		
		uint8_t effectSequence_1[16];
		uint8_t effectSequence_2[16];
		uint8_t effectSequence_3[16];
	
	} fxSequence_t;

#endif /* SEQUENCER_FX_DEFINITIONS_H_ */