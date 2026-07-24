/*
 * colorMap.c
 *
 * Created: 8/13/2013 10:01:48 AM
 *  Author: Michael
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

#include "colorMap.h"
#include "constants.h"
#include "eeprom.h"


// Color maps are stored in Blue Green Red format

#if 0 // We now precompute exponents

const uint8_t colorMap7[128][3] PROGMEM = { // Original release colormap
					{0, 0, 0},	// 0
					{255, 0, 0},	// 1 - Blue
					{255, 21, 0},	// 2 - Blue (Green Rising)
					{255, 34, 0},
					{255, 46, 0},
					{255, 59, 0},
					{255, 68, 0},
					{255, 80, 0},
					{255, 93, 0},
					{255, 106, 0},
					{255, 119, 0},
					{255, 127, 0},
					{255, 140, 0},
					{255, 153, 0},
					{255, 165, 0},
					{255, 178, 0},
					{255, 191, 0},
					{255, 199, 0},
					{255, 212, 0},
					{255, 225, 0},
					{255, 238, 0},
					{255, 250, 0},	// 21 - End of Blue's Reign 
					{250, 255, 0},  // 22 - Green (Blue Fading)
					{237, 255, 0},
					{225, 255, 0},
					{212, 255, 0},
					{199, 255, 0},
					{191, 255, 0},
					{178, 255, 0},
					{165, 255, 0},
					{153, 255, 0},
					{140, 255, 0},
					{127, 255, 0},
					{119, 255, 0},
					{106, 255, 0},
					{93, 255, 0},
					{80, 255, 0},
					{67, 255, 0},
					{59, 255, 0},
					{46, 255, 0},
					{33, 255, 0},
					{21, 255, 0},
					{8, 255, 0},
					{0, 255, 0},	// 43 - Green
					{0, 255, 12},	// 44 - Green/ Red Rising
					{0, 255, 25},
					{0, 255, 38},
					{0, 255, 51},
					{0, 255, 63},
					{0, 255, 72},
					{0, 255, 84},
					{0, 255, 97},
					{0, 255, 110},
					{0, 255, 123},
					{0, 255, 131},
					{0, 255, 144},
					{0, 255, 157},
					{0, 255, 170},
					{0, 255, 182},
					{0, 255, 191},
					{0, 255, 203},
					{0, 255, 216},
					{0, 255, 229},
					{0, 255, 242},
					{0, 255, 255},	// 64 - Green + Red (Yellow)
					{0, 246, 255},	// 65 - Red, Green Fading
					{0, 233, 255},
					{0, 220, 255},
					{0, 208, 255},
					{0, 195, 255},
					{0, 187, 255},
					{0, 174, 255},
					{0, 161, 255},
					{0, 148, 255},
					{0, 135, 255},
					{0, 127, 255},
					{0, 114, 255},
					{0, 102, 255},
					{0, 89, 255},
					{0, 76, 255},
					{0, 63, 255},
					{0, 55, 255},
					{0, 42, 255},
					{0, 29, 255},
					{0, 16, 255},
					{0, 4, 255},	// 85 - End Red/Green Fading
					{4, 0, 255},	// 86 - Red/ Blue Rising
					{16, 0, 255},
					{29, 0, 255},
					{42, 0, 255},
					{55, 0, 255},
					{63, 0, 255},
					{76, 0, 255},
					{89, 0, 255},
					{102, 0, 255},
					{114, 0, 255},
					{127, 0, 255},
					{135, 0, 255},
					{148, 0, 255},
					{161, 0, 255},
					{174, 0, 255},
					{186, 0, 255},
					{195, 0, 255},
					{208, 0, 255},
					{221, 0, 255},
					{233, 0, 255},
					{246, 0, 255},
					{255, 0, 255},	// 107 - Blue + Red
					{255, 0, 242},	// 108 - Blue/ Red Fading
					{255, 0, 229},
					{255, 0, 216},
					{255, 0, 204},
					{255, 0, 191},
					{255, 0, 182},
					{255, 0, 169},
					{255, 0, 157},
					{255, 0, 144},
					{255, 0, 131},
					{255, 0, 123},
					{255, 0, 110},
					{255, 0, 97},
					{255, 0, 85},
					{255, 0, 72},
					{255, 0, 63},
					{255, 0, 50},
					{255, 0, 38},
					{255, 0, 25},	// 126 - Blue-ish
					{225, 255, 255}	// 127 - White 
};

#else

const uint8_t colorMap7[128][3] PROGMEM = { // Classic pre 2026 color map
					{0, 0, 0},		// 0
					{255, 0, 0},
					{255, 0, 0},
					{255, 1, 0},
					{255, 3, 0},
					{255, 6, 0},
					{255, 9, 0},	// 6
					{255, 14, 0},
					{255, 20, 0},
					{255, 28, 0},
					{255, 37, 0},
					{255, 44, 0},
					{255, 56, 0},	// 12
					{255, 71, 0},
					{255, 85, 0},
					{255, 103, 0},
					{255, 123, 0},
					{255, 137, 0},
					{255, 160, 0},	// 18
					{255, 186, 0},
					{255, 214, 0},
					{255, 242, 0},
					{250, 255, 0},
					{237, 255, 0},
					{225, 255, 0},
					{212, 255, 0},	// 25
					{199, 255, 0},
					{191, 255, 0},
					{178, 255, 0},
					{165, 255, 0},
					{153, 255, 0},
					{140, 255, 0},  // 31
					{127, 255, 0},
					{119, 255, 0},
					{106, 255, 0},
					{93, 255, 0},
					{80, 255, 0},
					{67, 255, 0},	// 37
					{59, 255, 0},
					{46, 255, 0},
					{33, 255, 0},
					{21, 255, 0},
					{8, 255, 0},
					{0, 255, 0},
					{0, 255, 0},	// 44
					{0, 255, 0},
					{0, 255, 0},
					{0, 255, 0},
					{0, 255, 0},
					{0, 255, 0},
					{0, 255, 0},	// 50
					{0, 255, 2},
					{0, 255, 3},
					{0, 255, 6},
					{0, 255, 9},
					{0, 255, 14},
					{0, 255, 22},	// 56
					{0, 255, 33},
					{0, 255, 47},
					{0, 255, 60},
					{0, 255, 81},
					{0, 255, 111},
					{0, 255, 148},
					{0, 255, 196},	// 63
					{0, 255, 255},
					{0, 233, 255},
					{0, 203, 255},
					{0, 176, 255},
					{0, 153, 255},
					{0, 130, 255},	// 69
					{0, 117, 255},
					{0, 98, 255},
					{0, 80, 255},
					{0, 65, 255},
					{0, 52, 255},
					{0, 44, 255},	// 75
					{0, 34, 255},
					{0, 25, 255},
					{0, 18, 255},
					{0, 12, 255},
					{0, 7, 255},
					{0, 5, 255},	// 81
					{0, 2, 255},
					{0, 1, 255},
					{0, 0, 255},
					{0, 0, 255},
					{4, 0, 255},
					{16, 0, 255},
					{29, 0, 255},	// 88
					{42, 0, 255},
					{55, 0, 255},
					{63, 0, 255},
					{76, 0, 255},
					{89, 0, 255},
					{102, 0, 255},	// 94
					{114, 0, 255},
					{127, 0, 255},
					{135, 0, 255},
					{148, 0, 255},
					{161, 0, 255},
					{174, 0, 255},	// 100
					{186, 0, 255},
					{195, 0, 255},
					{208, 0, 255},
					{221, 0, 255},
					{233, 0, 255},
					{246, 0, 255},
					{255, 0, 255},	// 107
					{255, 0, 219},
					{255, 0, 196},
					{255, 0, 170},
					{255, 0, 148},
					{255, 0, 133},
					{255, 0, 127},	// 113
					{255, 0, 111},
					{250, 0, 111},
					{237, 0, 83},
					{225, 0, 60},
					{212, 0, 47},
					{199, 0, 32},	// 119
					{199, 0, 22},
					{191, 0, 16},
					{178, 0, 15},
					{165, 0, 14},
					{153, 0, 13},
					{140, 0, 12},
					{127, 0, 11},
					{255, 255, 255}	// 127
};
const uint8_t colorMap64[128][3] PROGMEM = { // 2026 colormap to match MF64 palette
					{0,0,0}, // 0 - black
					{30,30,30}, // 1 - dark gray
					{127,127,127}, // 2 - gray
					{255,255,255}, // 3 - white
					{76,76,255}, // 4 - bright red
					{0,0,255}, // 5 - pure red
					{0,0,127}, // 6 - dim red
					{0,0,30}, // 7 - very dark red
					{108,189,255}, // 8 - orange
					{0,84,255}, // 9 - bright orange
					{0,29,89}, // 10 - dark orange
					{0,27,39}, // 11 - very dark yellow
					{76,255,255}, // 12 - bright yellow
					{0,255,255}, // 13 - bright yellow
					{0,89,89}, // 14 - dark yellow
					{0,25,25}, // 15 - very dark yellow
					{76,255,136}, // 16 - bright green
					{0,255,84}, // 17 - bright green
					{0,89,28}, // 18 - dark green
					{0,43,20}, // 19 - very dark green
					{76,255,76}, // 20 - bright green
					{0,255,0}, // 21 - bright green
					{0,127,0}, // 22 - green
					{0,25,0}, // 23 - very dark green
					{94,255,76}, // 24 - bright green
					{25,255,0}, // 25 - bright green
					{13,89,0}, // 26 - dark green
					{2,25,0}, // 27 - very dark green
					{136,255,76}, // 28 - bright green
					{86,246,40}, // 29 - bright green
					{31,104,11}, // 30 - green
					{19,36,0}, // 31 - very dark teal
					{183,255,76}, // 32 - bright teal
					{153,255,0}, // 33 - bright teal
					{43,89,0}, // 34 - dark green
					{18,25,0}, // 35 - very dark teal
					{255,195,76}, // 36 - bright blue
					{255,169,0}, // 37 - bright blue
					{82,65,0}, // 38 - dark cyan
					{25,16,0}, // 39 - very dark blue
					{255,136,76}, // 40 - bright blue
					{255,85,0}, // 41 - bright blue
					{29,89,0}, // 42 - dark green
					{25,8,0}, // 43 - very dark blue
					{255,76,76}, // 44 - bright blue
					{255,0,0}, // 45 - bright blue
					{127,0,0}, // 46 - blue
					{30,0,0}, // 47 - very dark blue
					{255,76,135}, // 48 - bright purple
					{255,0,84}, // 49 - bright purple
					{100,0,25}, // 50 - dark purple
					{48,0,15}, // 51 - very dark purple
					{255,76,255}, // 52 - bright magenta
					{255,0,255}, // 53 - bright magenta
					{89,0,89}, // 54 - dark magenta
					{25,0,25}, // 55 - very dark magenta
					{135,76,255}, // 56 - bright pink
					{84,0,255}, // 57 - bright pink
					{29,0,89}, // 58 - dark pink
					{19,0,34}, // 59 - very dark pink
					{0,21,255}, // 60 - bright red
					{0,53,153}, // 61 - orange
					{0,81,121}, // 62 - yellow
					{0,100,67}, // 63 - dark yellow-green
					{0,57,3}, // 64 - dark green
					{53,87,0}, // 65 - dark teal
					{127,84,0}, // 66 - blue
					{255,0,0}, // 67 - bright blue
					{79,69,0}, // 68 - dark cyan
					{204,0,37}, // 69 - purple
					{127,127,127}, // 70 - gray
					{32,32,32}, // 71 - dark gray
					{0,0,255}, // 72 - bright red
					{45,255,189}, // 73 - bright yellow-green
					{6,237,175}, // 74 - bright yellow-green
					{9,255,100}, // 75 - bright green
					{0,139,16}, // 76 - green
					{135,255,0}, // 77 - bright teal
					{255,169,0}, // 78 - bright blue
					{255,42,0}, // 79 - bright blue
					{255,0,63}, // 80 - bright purple
					{255,0,122}, // 81 - bright purple
					{126,26,178}, // 82 - pink
					{0,33,64}, // 83 - dark orange
					{0,74,255}, // 84 - bright orange
					{6,225,136}, // 85 - green
					{21,255,114}, // 86 - bright green
					{0,255,0}, // 87 - bright green
					{38,255,59}, // 88 - bright green
					{113,255,89}, // 89 - green
					{204,255,56}, // 90 - bright teal
					{255,138,91}, // 91 - blue
					{198,81,49}, // 92 - blue
					{233,127,135}, // 93 - light blue
					{255,29,211}, // 94 - bright magenta
					{93,0,255}, // 95 - bright pink
					{0,127,255}, // 96 - bright orange
					{0,76,185}, // 97 - orange
					{0,255,144}, // 98 - bright green
					{7,93,131}, // 99 - yellow
					{0,43,57}, // 100 - dark yellow
					{16,76,20}, // 101 - dark green
					{56,80,13}, // 102 - dark teal
					{42,21,21}, // 103 - very dark blue
					{90,32,22}, // 104 - dark blue
					{28,60,105}, // 105 - orange
					{10,0,168}, // 106 - red
					{61,81,222}, // 107 - red
					{28,106,216}, // 108 - orange
					{38,225,255}, // 109 - bright yellow
					{47,225,158}, // 110 - green
					{15,181,103}, // 111 - green
					{48,30,30}, // 112 - very dark blue
					{107,255,220}, // 113 - yellow-green
					{189,255,128}, // 114 - light green
					{255,153,154}, // 115 - light blue
					{255,102,142}, // 116 - purple
					{64,64,64}, // 117 - gray
					{117,117,117}, // 118 - gray
					{255,255,224}, // 119 - off-white
					{0,0,160}, // 120 - red
					{0,0,53}, // 121 - dark red
					{0,208,26}, // 122 - green
					{0,66,7}, // 123 - dark green
					{0,76,185}, // 124 - orange
					{0,49,63}, // 125 - dark yellow
					{0,95,179}, // 126 - orange
					{2,21,75} // 127 - dark orange
};

#endif

const uint8_t (*activeColorMap)[3] = colorMap7;

void colorMap_init(void) {
	if (eeprom_read(EE_COLOR_MAP) == 1) {
		activeColorMap = colorMap64;
		} else {
		activeColorMap = colorMap7;
	}
}

/* Exponential look up table (e^(n*.0381)) for brightness curves  */
const uint8_t brightnessMap[128] PROGMEM = 	{1,1,1,1,1,1,1,1,1,1,1,2,2,2,2,2,2,
						2,2,2,2,2,2,2,2,3,3,3,3,3,3,3,3,4,
						4,4,4,4,4,4,5,5,5,5,5,6,6,6,6,6,7,
						7,7,8,8,8,8,9,9,9,10,10,11,11,11,12,
						12,13,13,14,14,15,16,16,17,17,18,19,
						20,20,21,22,23,24,25,25,26,28,29,30,
						31,32,33,35,36,37,39,40,42,43,45,47,
						49,51,53,55,57,59,61,64,66,69,71,74,
						77,80,83,86,90,93,97,100,104,108,113,
						117,122,127};
											
/* 32 Value exponential (e^(n*0.1515)) look up table for MIDI in control of RGB brightness */											
const uint8_t animationBrightnessMap[32] PROGMEM = 	{1,1,2,2,2,2,3,3,4,5,5,6,7,8,
							10,11,13,15,18,21,24,28,33,
							38,44,51,60,70,81,94,110,127};
