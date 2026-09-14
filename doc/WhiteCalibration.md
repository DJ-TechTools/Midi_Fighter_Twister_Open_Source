# RGB White Balance Calibration

Use this procedure to adjust the color balance of your device for a specific workflow. Calibration lets you set the white point by eye, on the hardware, for the unit that you own. The Twister keeps the white point when you install new firmware.

**What to know before you start:**

- The white point applies to *every* color that the Twister shows, not only white. White is the easiest color to judge against.
- The Twister keeps the white point in the unit, not in your settings file. If you load the `.mfs` file of a different user, you do not get their white point. This is correct, because their LEDs come from a different reel.
- The Twister writes nothing until you save. If you change your mind, press the red encoder. All of your changes are then lost.
- Setup mode switches USB off. Your computer does not detect the Twister until you leave setup mode.

## Entry

Calibration is one item in setup mode. Refer to [Setup Mode](SetupMode.md).

1. Press and hold the **bottom right side button** while you connect the USB cable.
2. Release the button. The bottom row of encoders shows the setup menu.
3. Press the **white encoder** in the bottom row.

All encoders light up white to show that calibration is in operation.

## Controls

Control|Function
--|--
**Right side, top, middle and bottom**|Selects the **red**, **green** or **blue** channel. The RGB LEDs flash in that color to show the channel that you selected.
**Turn any encoder**|Adjusts the channel that you selected. Use the encoder that you look at.
**Press an encoder**|Starts single encoder mode for that LED. Its detent dot comes on blue. Press the encoder again to go back to global adjustment. Press a different encoder to move to that encoder.
**Middle left side button**|Resets all LEDs to the factory calibration. In single encoder mode, it resets only the selected encoder. It does not save.
**Top left side button**|Goes back to the setup menu. Your adjustments stay in the memory of the unit. They are not yet saved.

Calibration has no save button. You save in the setup menu, with the green encoder. One save keeps all of your setup mode changes.

At the start you adjust all of the encoders together. Use single encoder mode if one LED looks different from the others.

## Lights

LEDs|Description
--|--
**Encoder color**|White, as your current settings show it.
**White ring**|The position of that encoder in its range, for the channel that you adjust. Each encoder shows its own position. Thus you can see the difference between all 16 encoders.
**Blue detent dot**|This is the selected encoder.
**Red detent dot**|This encoder is at the end of its range. It no longer follows the others.
**Magenta detent dot**|This is the selected encoder, and it is at the end of its range.

## Recommended adjustments

White appearance|Decrease
--|--
Pink, warm, red|**R**
Yellow|**R**, then **G**
Green|**G**
Cyan, teal|**G**, then **B**
Blue, cold|**B**
Purple, magenta|**R** and **B**

### Procedure

1. Enter calibration. Refer to **Entry** above.
2. Use the white rings as your reference. The 11 indicator LEDs around each encoder are true white LEDs. They are adjacent to the color that you judge.
3. Select a color channel with the right side buttons. The flash shows the channel that you selected.
4. Turn any encoder counterclockwise until the white looks correct.
5. Examine the white again. When you decrease the strongest channel, a smaller color shift usually becomes visible. Do step 3 again. Two passes are usual.
6. When all of the encoders look the same, look for one LED that is different. Press that encoder. Its detent dot comes on blue. Adjust that LED. Press the encoder again to go back to all 16 encoders.
7. Press the top left side button to go back to the setup menu.
8. Press the green encoder to save. A green flash shows that the Twister saved the calibration.

To match a second Twister, put the two units side by side. Use one unit as the reference for the other.

### Decrease, do not increase

Green and blue start at the top of their range. If you increase them, nothing occurs. The ring shows that all 11 LEDs are already on. This is correct, because that position gives you the maximum brightness.

Thus you set the white point when you decrease the channel that is too strong. Do not increase the channel that looks too weak. The range in that direction is sufficient. Single encoder mode also gives you a smaller adjustment, to make one LED agree with its neighbors.

### Brightness

When you decrease a channel, you lose a small quantity of light. You get most of it again.

When you save, the Twister increases all of the calibration until the strongest channel is at the top of its range. Thus you get the maximum light for your white point.

The quantity of light that you get again depends on your adjustment. If you decrease one channel and keep a different channel at the top, you get no light again. The brightness was already at the maximum for that white point. If you decrease all three channels, you get most of the light again. The white point does not change. Only the brightness changes.

## Cancel and revert

To cancel, go back to the setup menu with the top left side button. Then press the **red encoder**. All of your changes are then lost.

To go back to the factory calibration, press the **middle left side button**. This resets all of the encoders. Then save. In single encoder mode, this button resets only the selected encoder. This is useful if you adjusted one encoder too much and the other encoders are correct.
