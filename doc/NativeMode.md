# Native mode documentation
The native mode enables extended bidirectional communication via MIDI for advanced integration with other software:
- User control input can be received via MIDI control change (CC) messages. (see [Knob/button control input messages](#knobbutton-control-input-messages))
- The native mode active state as well as the visual state can be remotely configured via well-defined CC/SysEx messages. (see [Remote setup messages](#remote-setup-messages))

**Please notice the following**:
- **You have to set the native mode to active first before you can make use of any of these features!** This can be done via SysEx (see below) or by pressing the two **bottom side buttons** together, which toggles native mode on/off.
- It makes sense to deactivate native mode after use to return the device to normal operation.
- All native-mode-specific configuration data sent to the device is (intentionally) lost after disconnecting the Midi Fighter Twister.
- **All native mode control messages use the system channel** (default 4), in a CC block of their own (64-101). Normal bank traffic never uses that block, so native mode messages are never ambiguous with it. See [The system channel](#the-system-channel).

## Knob/button control input messages
Knob/button control input can be received from the Midi Fighter Twister via well-defined CC messages. The CC message components can be interpreted as follows:
Message component|Interpretation
--|--
Channel|Always the system channel (default 4)
Data1|Interaction type and control index combined, see below
Data2|Control value

Every message arrives on the same channel; the **CC number** identifies both the kind of interaction and which control it came from:
CC number (Data1)|Interaction type|Control index assignment|Control value interpretation
--:|--|--|--
64-79|Knob twist|CC 64 + index; 0-15: row by row|64-*n*: to the left / 64+*n*: to the right
80-95|Knob press|CC 80 + index; 0-15: row by row|127: pressed / 0: released
96-101|Side button press|CC 96 + index; 0-2: left side / 3-5: right side|127: pressed / 0: released

For knob twist, *n* is the step size. It is **1** unless the knob input mode is velocity sensitive (see [Set knob input mode](#native-mode-sysex-messages)). By default a knob sends only 63 or 65. In velocity sensitive mode, *n* increases with turn speed up to 17, and the values go from 47 to 81. A relative encoder in velocity sensitive mode follows the same curve in a normal bank, outside native mode.

Here are some examples for control input CC messages and their interpretation:
Status (channel)|Data1|Data2 (decimal)|Interpretation
--:|--:|--:|--
0xB3 (4)| 0x40 (64)| 63|Top left knob turned to the left
0xB3 (4)| 0x53 (83)| 127|Top right knob pressed down
0xB3 (4)| 0x62 (98)| 0|Bottom left side button released

These status bytes use the **default** system channel of 4. If the user changes the system channel, the status byte changes with it. The CC numbers do not change.

## Remote setup messages
The native mode active state and the LED states of the Midi Fighter Twister can be remotely set via a mixture of CC and SysEx messages. This section contains all necessary details to send indicator positions via CC and to compose native mode SysEx messages for the Midi Fighter Twister.

### Indicator position CC messages
Send the indicator position to the device on the **system channel**, with **CC 64-79** (CC 64 + knob index). These are the same numbers that the knobs send for twist, in the opposite direction. The position value is from 0 to 127. Native mode ignores CC numbers outside 64-79, and their normal handlers get them.

Here are some examples:

Action|Status (channel)|Data1|Data2 (decimal)
--|--:|--:|--:
Set top left knob indicator position to 100%|0xB3 (4)| 0x40 (64)| 127
Set top right knob indicator position to 0%|0xB3 (4)| 0x43 (67)| 0

### The system channel
Native mode uses the system channel, so an integration must know its value. The user can change the system channel, and the default is 4.

**On connect**, request the device configuration:

    F0 00 01 79 02 00 F7

The device replies with a list of tag and value pairs. The value for tag `0x00` is the system channel, **sent 1-based**. Subtract 1 to get the wire channel. Search for the tag, and do not use a fixed offset in the response.

**When it changes**, the device tells you without a request. After each configuration push, the device sends the same configuration response, with the new system channel. It is sufficient to listen for that message. It is not necessary to poll.

**On factory reset** the device enumerates again over USB. Open the port again and request the configuration.

SysEx messages have no MIDI channel. A change of the system channel cannot break this path.

### Native mode SysEx messages

Native mode SysEx messages in general are composed as follows:
SysEx start|Manufacturer ID|Native mode|Command ID|Content|SysEx end
--:|--:|--:|:--:|:--:|--:
0xF0|0x000179|0x06|\<*commandId*\>|\<*content*\>|0xF7

The \<*commandId*\> bytes define the setting whose value is set using the \<*content*\> bytes. The \<*commandId*\> assignments and basic structure of the corresponding \<*content*\> HEX bytes are defined as follows:
\<*commandId*\>|Setting|\<*content*\> HEX bytes
--:|--|:--:
0x00|Set native mode active|*aa*
0x0100|Set knob indicator configuration|*ii ttddcc*
0x0101|Set knob LED color|*ii rrggbb*
0x0102|Set knob input mode|*ii mm00ss*

Here the individual bytes \<*content*\> HEX bytes are two-letter literals:
\<*content*\> HEX bytes|Parameter|Possible values
--:|--|:--:
*aa*|Active|0: inactive / 1: active
*ii*|Knob index|0-15: row by row
*tt*|Indicator type|0: dot / 1: bar / 2: blended bar / 3: spread bar
*dd*|Detent|0: no detent / 1: has detent
*cc*|Detent color|0-127: crossfade between red (0) and blue (127), ignored if no detent
*rrggbb*|RGB LED color|Each component (R/G/B): 0-127
*mm*|Input mode|0: raw detents / 1: velocity sensitive
*ss*|Maximum step|0: firmware default (17) / 1-17: cap the velocity step, ignored if *mm* is 0

The *00* byte in *ii mm00ss* is reserved. Always send it as zero. Velocity sensitive mode changes only the step size of each twist message, and never the number of messages. A host that already adds up 63 and 65 needs no protocol change. Use *ss* to keep fast turns in a range that your parameter can accept.

Here are some useful example SysEx messages:
Action|SysEx HEX bytes
--|--
Enter native mode|F0 000179 **06 00 01** F7
Leave native mode|F0 000179 **06 00 00** F7
Set top left knob RGB LED color to red |F0 000179 **06 0101 00 7F0000** F7
Set top right knob RGB LED color to green |F0 000179 **06 0101 03 007F00** F7
Setup top left knob indicator as bar without detent|F0 000179 **06 0100 00 010000** F7
Setup top left knob indicator as dot with blue detent|F0 000179 **06 0100 00 00017F** F7
Make top left knob velocity sensitive|F0 000179 **06 0102 00 010000** F7
Make top left knob velocity sensitive, capped at 3 steps|F0 000179 **06 0102 00 010003** F7
Return top left knob to raw detents|F0 000179 **06 0102 00 000000** F7
