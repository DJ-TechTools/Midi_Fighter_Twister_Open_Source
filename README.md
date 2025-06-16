# Midi Fighter Twister - Open Source Firmware

## Compilation
### Prerequisites

- installed CMake
- installed AVR Toolchain and libm
   - on Linux: `sudo apt-get install gcc-avr avr-libc`
   - on Windows: can be downloaded from [here](https://www.microchip.com/en-us/tools-resources/develop/microchip-studio/gcc-compilers)
      - maybe [this](https://gnutoolchains.com/avr/) might work as well?
      - extract and set the environment variable `AVR_ROOT` to the full `bin` directory path

### How to build

```
cmake -S . -B build
cmake --build build
```

## Installation
You need the Midi Fighter Utility which can be downloaded [here](https://store.djtechtools.com/pages/midi-fighter-utility)
1. Connect the Midi Fighter Twister to your computer directly (DO NOT USE A USB HUB!)
1. Launch the Midifighter Utility software which should automatically detect the connected device. (If not, make sure that no other software is currently using the Midifighter Twister!)
1. *Tools > Midifighter > Load Custom Firmware > For a Twister*
1. Navigate to the *Midi_Fighter_Twister.hex* file and open it
1. Choose "Yes" to proceed
1. Wait until the firmware update process is completed
