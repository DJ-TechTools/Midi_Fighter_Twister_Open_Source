# Midi Fighter Twister - Open Source Firmware
The MF Twister Project makes use of the Atmel ASF extensions & LUFA Module.

To compile following symbols must be defined

DEBUG
F_USB=48000000UL
F_CPU=32000000UL
USE_LUFA_CONFIG_HEADER
ARCH=ARCH_XMEGA
BOARD=USER_BOARD

The compiler must also link libm

The project also requires a user board configuration header file, if this is missing add the following to conf_board.h

#define CONF_BOARD_ENABLE_USARTD0
