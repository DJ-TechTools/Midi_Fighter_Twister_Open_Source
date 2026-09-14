# Setup Mode

Setup mode is a menu on the Twister itself. Use it for the settings that you cannot set from the Midi Fighter Utility.

There are three settings at this time:

- White balance calibration: refer to [White Balance Calibration](WhiteCalibration.md).
- Device number: use a different number for each Twister when you connect more than one unit to the same computer.
- Bank mode: select 4 banks with the sequencer, or 8 banks without the sequencer.

**What to know before you start:**

- Setup mode switches USB off. Your computer does not detect the Twister while the menu is in operation.
- The Twister writes nothing until you save. If you change your mind, press the red encoder.
- One save keeps all of your changes together.

## Entry

Press and hold the **bottom right side button** while you connect the USB cable. Then release the button.

## The menu

The bottom row of encoders is the menu. The top row has the two commands.

Encoder|Color|Function
--|--|--
Bottom row, first|Blue|Turn to select the device number. The ring shows the number.
Bottom row, second|Orange|Turn to select the bank mode. The ring shows which mode.
Bottom row, third|White|Press to start white balance calibration.
Bottom row, fourth|Off|Not in use at this time.
Top row, first|Off, ring lit|Hold for one second to put the unit in bootloader mode.
Top row, third|Red|Press to leave setup mode. Your changes are lost.
Top row, fourth|Green|Press to save your changes and leave setup mode.

## Device number

Use a different device number for each Twister when you connect more than one unit to the same computer. The number changes the name that your computer shows. Unit 1 is `Midi Fighter Twister`. Unit 2 is `Midi Fighter Twister 2`, and so on to unit 8.

1. Enter setup mode.
2. Turn the **blue encoder** in the bottom row. One click of the encoder changes the number by one. The ring shows the device number: one LED for unit 1, two LEDs for unit 2, and so on.
3. Press the **green encoder** to save.

The Twister connects again with the new name. You do not have to disconnect the cable.

Set the device number of one unit at a time. Give each unit a different number.

## Bank mode

The Twister gives you a choice of two bank modes:

- **4 banks with the sequencer.** This is the default. It is what all Twisters do unless you change this setting.
- **8 banks without the sequencer.** This gives you 8 banks of 16 encoders. The sequencer is not available in this mode.

The Twister has memory for one mode only, thus you must select one of the two.

1. Enter setup mode.
2. Turn the **orange encoder** in the bottom row. Turn it to the right for 8 banks. Turn it to the left for 4 banks.
3. Look at the ring to see the selected mode:
   - **4 banks:** LEDs 2, 4, 8 and 10 come on.
   - **8 banks:** the first four LEDs and the last four LEDs come on.
4. Press the **green encoder** to save.

The Twister restarts when you change this setting. This is necessary, because the Twister divides its memory at start up. All of the LEDs flash green, then the Twister restarts and connects to your computer. Wait for your computer to detect it.

**Important: bank 8 and sequencer pattern memory slot 1 use the same memory.**

The last 8 encoders of bank 8 and pattern memory slot 1 are in the same memory in the Twister. Only one of the two modes can keep them. Thus:

- A change to 8 banks erases pattern memory slot 1. The other 15 slots are not affected.
- A change to 4 banks, and then back to 8 banks, returns the last 8 encoders of bank 8 to their defaults.

Banks 5 to 8 get their default settings the first time that you select 8 banks. After that, the Twister keeps your settings for banks 5, 6, 7 and the first 8 encoders of bank 8. You can change to 4 banks and back to 8 banks, and those settings stay as you set them.

Your settings for banks 1 to 4 are never affected by this setting.

The Midi Fighter Utility reads the number of banks from the Twister. Thus the Utility shows 4 or 8 banks to match. Connect the Twister to the Utility again after a change of mode.

## Bootloader mode

Bootloader mode lets the Midi Fighter Utility write new firmware to the Twister.

The **first encoder in the top row** starts it. The RGB stays off. The ring shows LEDs 1, 3, 5, 7, 9 and 11.

You must hold this encoder, because a press is not sufficient. Only the Midi Fighter Utility, a DFU tool, or a disconnect of the cable leaves bootloader mode again. Thus the Twister asks you to show that you mean it.

1. Enter setup mode.
2. Hold the first encoder in the top row. The LEDs in the ring go off one at a time.
3. Keep it held for one second, until all of the LEDs in the ring are off.
4. The Twister disconnects, waits two seconds, then starts the bootloader.
5. Open the Midi Fighter Utility to write the new firmware.

Release the encoder before all of the LEDs go off to stop. The ring comes on again and nothing occurs.

**This command does not save your setup mode changes.** If you changed the device number, the bank mode, or the white balance, press the green encoder first. Then enter setup mode again, and hold this encoder.

To leave bootloader mode without new firmware, use the Midi Fighter Utility, or disconnect the cable and connect it again.

## Set these from the Midi Fighter Utility

The device number and the bank mode are also available as SysEx commands. Thus the Utility can set them without setup mode, and without the user at the unit.

All DJTT SysEx messages have this shape:

```
F0 00 01 79 <command> <data...> F7
```

Command `03` is the system command. The first data byte is the operation.

Operation|Message|Result
--|--|--
Read both settings|`F0 00 01 79 03 05 00 F7`|The unit replies. Refer to the reply below.
Set device number|`F0 00 01 79 03 03 nn F7`|`nn` is 00 to 07, for units 1 to 8.
Set bank mode|`F0 00 01 79 03 04 nn F7`|`nn` is `04` for 4 banks, or `08` for 8 banks.

### Read both settings

The reply has this shape:

```
F0 00 01 79 03 05 01 <number> <numbers> <banks> <maxbanks> F7
```

Byte|Contents
--|--
`01`|Marks a reply. A request has `00` here.
`<number>`|The device number, 00 to 07.
`<numbers>`|How many device numbers the firmware has. This is 08 at this time.
`<banks>`|The bank count, `04` or `08`.
`<maxbanks>`|The largest bank count the firmware has. This is 08 at this time.

The two count bytes let the Utility build its lists from what the unit reports, and not from a value written into the Utility.

### Write a setting

The two write commands write the setting, show a confirmation on the LEDs, then disconnect and reset the unit. Wait for the unit to connect again, then read its configuration.

- The unit ignores a value out of range. It does not reset and gives no reply.
- The unit also ignores the bank mode command if it is in that mode already.
- A change of device number changes the USB product ID. Thus the unit connects as a different device, and the Utility must scan the ports again.
- On macOS the new MIDI port sometimes comes back but does not accept data. This does not occur each time. If the Utility gets no reply after the unit connects again, tell the user to disconnect the cable and connect it again.
- A change of bank mode keeps the same product ID. Thus the unit connects again by itself, and no user action is necessary.
- A change of bank mode has the same effect on EEPROM as a change in setup mode. Refer to [Bank mode](#bank-mode).

## Save and cancel

Press the **green encoder** to save. All of the LEDs flash green. The Twister then leaves setup mode and connects to your computer.

If you changed the bank mode, the Twister restarts after the green flash. Refer to [Bank mode](#bank-mode).

Press the **red encoder** to cancel. The Twister leaves setup mode and keeps your initial settings.
