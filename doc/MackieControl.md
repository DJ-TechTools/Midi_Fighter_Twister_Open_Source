# Mackie Control

**What to know before you start:**

- Your DAW must have a Mackie Control surface, with the Twister set as its input and its output.
- The Twister has no screen. On macOS, the Midi Fighter Utility can show the display on your computer. Refer to The display.

## Setup

1. In the Midi Fighter Utility, set the encoders you want. Refer to Encoder types.
2. In your DAW, add a Mackie Control surface. Set its input port and its output port to `Midi Fighter Twister`.

In Ableton Live this is Preferences, then Tempo & IDI, then a Control Surface slot. In Logic Pro it is Control Surfaces, then Setup.

A DAW claims the port that it uses for a control surface. The switches play notes when a second program makes a copy of the port. Close that program.

### The display

The Midi Fighter Utility can show the Mackie display: the names and values of the eight strips, in two rows. This is on macOS only.

The DAW sends the display text to the output of the surface. The Twister cannot show it and discards it. The utility shows it when the DAW sends to the utility, and the utility sends everything on to the Twister.

1. In the Midi Fighter Utility, select **View**, then **Mackie Display**, or press **Cmd+Shift+M**. The display window opens, and the utility makes a MIDI port called `MFU Twister MCU`.
2. In your DAW, set the input port of the Mackie Control surface to `Midi Fighter Twister`.
3. Set the output port to `MFU Twister MCU`.
4. Select the surface again, or change the bank, so that the DAW sends the display text.

Keep the utility open. When you close it, or turn off the display, the lights, rings and faders on the Twister no longer follow the DAW. The controls still work. To go back to the setup without the display, set the output port to `Midi Fighter Twister` again.

The display window stays in front of other windows. To change this, select **View**, then **Mackie Display Always on Top**, or press **Cmd+Option+T**. Drag the window to move it.

The utility stops the display while it updates the firmware, and while it sends or reads your settings. The Twister cannot follow the DAW then. After, the display comes back if it was open. Change the bank, or move a control, to update the Twister.

With more than one Twister, the utility sends to the Twister you select in the utility.

## Encoder types

The utility has two output types for Mackie Control. Both are for encoders.

Type|What the encoder becomes
--|--
Mackie fader|A channel fader, in 14 bit
Mackie V-Pot|The rotary above a channel fader, which the DAW assigns

### Faders

The MIDI channel selects the strip.

Channel|Strip
--|--
1 to 8|Strips 1 to 8
9|The master

Set the movement type to velocity sensitive. A fader gains from it twice. The step follows how fast you turn, and the slow end is four times finer than a 7 bit CC gives. A slow turn then has about 3175 steps across the range. A fast turn sweeps the whole range in half a turn. Direct movement throws both away.

The MIDI number does nothing for a fader. The channel alone selects the strip.

Turn on the detent for a detent at the center. It applies only while Flip is on and the DAW is in the pan assignment. A flipped fader holds a pan then, and a pan centers. 

### V-Pots

Set the MIDI channel to 1. The MIDI number selects the strip: 16 for strip 1, up to 23 for strip 8.

The DAW owns the ring. It sends the pattern and the position, so the indicator style does nothing. A pan draws from the center, a send fills from the left, and a plugin parameter uses the style the DAW picks.

The encoder color follows the assignment, so a row of V-Pots tells you what it controls.

Assignment|Color
--|--
I/O|Purple
Sends|Green
Pan|Blue
Plug-ins|Orange

The active color and the inactive color do nothing for a V-Pot, because the assignment sets the color.

The detent color does something: it is the center LED at 12 o'clock, which the DAW commands. Logic lights it for a centered pan and Ableton never lights it.

## Buttons

One action joins the list in the utility.

Action|What the switch does
--|--
Mackie button|Sends a Mackie button, and takes its light from the DAW

Give the switch a note from the tables below, on channel 1. The DAW decides when the switch lights:

Light|What you see
--|--
Off|The inactive color
Blink|The active color, which flashes
On|The active color

A Mackie button does not light when you press it. The DAW lights it, so a button that the DAW ignores stays dark.

## Global settings

Setting|Effect
--|--
Detent size|How far you turn to leave a detent
Dead zone size|How far you turn to leave the end of a range
Disable shift switch MIDI|Stops the MIDI from a shift switch, and gives its number to the shifted encoder

## The controls

Give a switch one of these notes, on channel 1. Rotation is not in this list, because an encoder sends rotation from its type and not from a note.

The names are the ones the protocol uses. An empty cell means that the DAW ignores the control, or that its manual does not say.

### One for each strip

Add the strip number, 0 to 7, to the base note.

Base|Control|Ableton|Logic
--|--|--|--
0|Record arm|Arms the track. Ctrl inverts the exclusive arm preference|Arms the strip. Option disarms all. Shift toggles input monitoring
8|Solo|Solos the track. Ctrl inverts the exclusive solo preference|Solos the strip. Option clears every solo. Sets pre and post in a Send view
16|Mute|Mutes the track|Mutes the strip. Option unmutes all. Bypasses in an EQ or Send view
24|Select|Selects the track|Selects the strip. Shift adds to the selection. **Option sets the volume to 0 dB**. Shift and Option make a track
32|V-Select|Resets the V-Pot parameter. Steps a quantized parameter. Selects the device on the device page|
104|Fader touch|The surface sends this. The firmware sends it for you, so do not assign it|As Ableton

### Assignment

Note|Ableton name|Logic name|Ableton|Logic
--|--|--|--|--
40|I/O|Track|V-Pots control the input and output routing|Press once to edit one parameter across the strips. Press again to edit many parameters of one strip
41|Sends|Send|V-Pots control the sends|V-Pots control the sends
42|Pan|Pan and Surround|V-Pots control the pan. This is the mode Ableton starts in|V-Pots control the pan
43|Plug-ins|Plug-in|V-Pots select a device, then control its parameters|V-Pots control the plug-ins
44|EQ|EQ|Previous page of plugin parameters|V-Pots control the channel EQ
45|Dynamics|Instrument|Next page of plugin parameters|V-Pots control the instrument

The two hosts disagree most here. Notes 44 and 45 are page controls in Ableton and they select a V-Pot assignment in Logic. Hold a Logic assignment button to get a shortcut menu on the LCD, which the Twister cannot show.

Exactly one of 40 to 43 lights at a time. Notes 44 and 45 light only while a page exists in that direction, and they do nothing outside the plug-ins mode.

### Strips and banks

Note|Control|Ableton|Logic
--|--|--|--
46|Bank left|Moves the eight strips down by eight tracks. Shift goes to the first track|Moves down by eight. Option goes to the first set
47|Bank right|Moves them up by eight. Shift goes to the last track|Moves up by eight. Option goes to the last set
48|Channel left|Moves them down by one track|Option goes to the first set
49|Channel right|Moves them up by one track|Option goes to the last set
50|Flip|Swaps what the faders and the V-Pots control|Mirrors the V-Pots onto the faders. Shift swaps them. Control parks the faders at zero
51|Edit|Swaps the strips between the tracks and the return tracks|

Ableton reads Shift for the first and last track. Logic reads Option. A user who moves between the two hosts needs both modifiers.

### Modifiers

Hold a modifier and press another control. A modifier and the control it changes must be in the same bank, because a bank change takes the other control away.

Note|Control
--|--
70|Shift
71|Option
72|Ctrl
73|Alt

Ableton reads Shift on notes 46 to 49 and on Play, and Ctrl on Record arm, Solo and Play. It reads Option and Alt only on the jog wheel and the cursor keys.

### Transport

Note|Control|Ableton|Logic
--|--|--|--
91|Rewind|Rewinds|Rewinds. Press again while it rewinds to go faster
92|Fast forward|Runs the transport forward|Runs forward. Press again to go faster
93|Stop|Stops. Lights while stopped|Stops playback, recording and every other transport function
94|Play|Plays. Shift continues or stops. Ctrl plays the selection|Plays from the playhead. Shift pauses
95|Record|Arms the recording|Records on the selected strip, if the strip is armed

In Logic the Marker and Nudge modes change notes 91 and 92: they then move between markers, or move the selection by the nudge value.

### Jog and cursor

Note|Control|Ableton|Logic
--|--|--|--
96|Cursor up|Moves the selection. Alt changes what it moves|Selects the parameter, or shifts the page or slot. With Zoom on, changes the zoom
97|Cursor down|Moves the selection|As above
98|Cursor left|Moves the selection|As above
99|Cursor right|Moves the selection|As above
100|Zoom|Changes what the jog wheel does. This is the one control Ableton makes blink|Changes the four cursor keys into zoom controls
101|Scrub|Changes what the jog wheel does|

The jog wheel is CC 60 on channel 1, and it uses the same relative form as a V-Pot. To get a jog wheel, set an encoder to Mackie V-Pot and set its MIDI number to 60. The firmware gives it no mode color, because it belongs to no strip.

### Automation and edits

Note|Control|Ableton
--|--|--
74|Automation on|
75|Automation record|
76|Undo|Undoes
77|Automation snapshot|
78|Automation touch|
79|Redo|Redoes
80|Cancel|
81|Enter|

### Markers

Note|Control|Ableton
--|--|--
82|Marker|
83|Mixer|
84|Marker, previous|
85|Marker, next|
86|Loop|
87|Punch in|
88|Punch out|
89|Home|
90|End|

### The rest

Note|Control
--|--
52|Name and value display
53|Timecode and beats display
54 to 69|Function keys 1 to 16
102 and 103|Foot switches A and B

## What the Twister does not use

The Twister reads these and discards them.

- Meters, which the host sends as channel pressure.
- The LCD text, the timecode display and the assignment display. The Twister has no screen.

The Twister answers the Mackie device query. It does not answer the firmware version query, and it must not. A host that reads a firmware version treats the surface as a Mackie Control Pro. That changes which messages the host sends.
