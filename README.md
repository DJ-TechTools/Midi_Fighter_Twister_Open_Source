# Midi Fighter Twister - Open Source Firmware

## Compilation
The MF Twister Project makes use of the Atmel ASF extensions & LUFA Module.

To compile, the following symbols must be defined

```
DEBUG
F_USB=48000000UL
F_CPU=32000000UL
USE_LUFA_CONFIG_HEADER
ARCH=ARCH_XMEGA
BOARD=USER_BOARD
```

The compiler must also link libm

The project also requires a user board configuration header file. If this is missing add the following to conf_board.h

```
#define CONF_BOARD_ENABLE_USARTD0
```

## Installation
You need the Midi Fighter Utility which can be downloaded [here](https://store.djtechtools.com/pages/midi-fighter-utility)
1. Connect the Midi Fighter Twister to your computer directly (DO NOT USE A USB HUB!)
1. Launch the Midifighter Utility software which should automatically detect the connected device. (If not, make sure that no other software is currently using the Midifighter Twister!)
1. *Tools > Midifighter > Load Custom Firmware > For a Twister*
1. Navigate to the *Midi_Fighter_Twister.hex* file and open it
1. Choose "Yes" to proceed
1. Wait until the firmware update process is completed
