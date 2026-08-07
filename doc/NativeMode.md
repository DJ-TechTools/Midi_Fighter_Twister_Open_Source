# Native mode documentation
The native mode enables extended bidirectional communication via MIDI for advanced integration with other software:
- User control input can be received via MIDI control change (CC) messages. (see [Knob/button control input messages](#knobbutton-control-input-messages))
- The native mode active state as well as the visual state can be remotely configured via well-defined CC/SysEx messages. (see [Remote setup messages](#remote-setup-messages))

**Please notice the following**:
- **You have to set the native mode to active first (via SysEx) before you can make use of any of these features!**
- It makes sense to deactivate native mode after use to return the device to normal operation.
- All native-mode-specific configuration data sent to the device is (intentionally) lost after disconnecting the Midi Fighter Twister.

## Knob/button control input messages
Knob/button control input can be received from the Midi Fighter Twister via well-defined CC messages. The CC message components can be interpreted as follows:
Message component|Interpretation
--|--
Channel|Interaction type
Data1|Control index
Data2|Control value

It depends on the type of interaction on how to interpret the control indices and values:
Channel|Interaction type|Control index assignment|Control value interpretation
--:|--|--|--
1|Knob twist|0-15: row by row|63: to the left / 65: to the right
2|Knob press|0-15: row by row|127: pressed / 0: released
3|Side button press|0-2: left side / 3-5: right side|127: pressed / 0: released

Here are some examples for control input CC messages and their interpretation:
Status (channel)|Data1|Data2 (decimal)|Interpretation
--:|--:|--:|--
0xB0 (1)| 0x00| 63|Top left knob turned to the left
0xB1 (2)| 0x03| 127|Top right knob pressed down
0xB2 (3)| 0x02| 0|Bottom left side button released

## Remote setup messages
The native mode active state and the LED states of the Midi Fighter Twister can be remotely set via a mixture of CC and SysEx messages. This section contains all necessary details to send indicator positions via CC and to compose native mode SysEx messages for the Midi Fighter Twister.

### Indicator position CC messages
Indicator position can be sent via CC on channel 1. The position value is  ranged from 0 to 127.

Here are some examples:

Action|Status (channel)|Data1|Data2 (decimal)
--|--:|--:|--:
Set top left knob indicator position to 100%|0xB0 (1)| 0x00| 127
Set top right knob indicator position to 0%|0xB0 (1)| 0x03| 0

### Native mode SysEx messages

Native mode SysEx messages in general are composed as follows:
SysEx start|Manufacturer ID|Native mode|Command ID|Content|SysEx end
--:|--:|--:|:--:|:--:|--:
0xF0|0x000179|0x05|\<*commandId*\>|\<*content*\>|0xF7

The \<*commandId*\> bytes define the setting whose value is set using the \<*content*\> bytes. The \<*commandId*\> assignments and basic structure of the corresponding \<*content*\> HEX bytes are defined as follows:
\<*commandId*\>|Setting|\<*content*\> HEX bytes
--:|--|:--:
0x00|Set native mode active|*aa*
0x0100|Set knob indicator configuration|*ii ttddcc*
0x0101|Set knob LED color|*ii rrggbb*

Here the individual bytes \<*content*\> HEX bytes are two-letter literals:
\<*content*\> HEX bytes|Parameter|Possible values
--:|--|:--:
*aa*|Active|0: inactive / 1: active
*ii*|Knob index|0-15: row by row
*tt*|Indicator type|0: dot / 1: bar / 2: blended bar / 3: blended dot
*dd*|Detent|0: no detent / 1: has detent
*cc*|Detent color|0-127: crossfade between red (0) and blue (127), ignored if no detent
*rrggbb*|RGB LED color|Each component (R/G/B): 0-127

Here are some useful example SysEx messages:
Action|SysEx HEX bytes
--|--
Enter native mode|F0 000179 **05 00 01** F7
Leave native mode|F0 000179 **05 00 00** F7
Set top left knob RGB LED color to red |F0 000179 **05 0101 00 7F0000** F7
Set top right knob RGB LED color to green |F0 000179 **05 0101 03 007F00** F7
Setup top left knob indicator as bar without detent|F0 000179 **05 0100 00 010000** F7
Setup top left knob indicator as dot with blue detent|F0 000179 **05 0100 00 00017F** F7
