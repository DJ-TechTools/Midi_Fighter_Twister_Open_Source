/*
             LUFA Library
     Copyright (C) Dean Camera, 2013.

  dean [at] fourwalledcubicle [dot] com
           www.lufa-lib.org
*/

/*
  Copyright 2013  Dean Camera (dean [at] fourwalledcubicle [dot] com)

  Permission to use, copy, modify, distribute, and sell this
  software and its documentation for any purpose is hereby granted
  without fee, provided that the above copyright notice appear in
  all copies and that both that the copyright notice and this
  permission notice and warranty disclaimer appear in supporting
  documentation, and that the name of the author not be used in
  advertising or publicity pertaining to distribution of the
  software without specific, written prior permission.

  The author disclaims all warranties with regard to this
  software, including all implied warranties of merchantability
  and fitness.  In no event shall the author be liable for any
  special, indirect or consequential damages or any damages
  whatsoever resulting from loss of use, data or profits, whether
  in an action of contract, negligence or other tortious action,
  arising out of or in connection with the use or performance of
  this software.
*/

/** \file
 *
 *  USB Device Descriptors, for library use whe--n in USB device mode. Descriptors are special
 *  computer-readable structures which the host requests upon device enumeration, to determine
 *  the device's capabilities and functions.
 */

#include "Descriptors.h"
#include "constants.h"

#if USE_ALTERNATE_ID != 0
#warning ALTERNATE USB ID is in USE!!!
#endif

/** Device descriptor structure. This descriptor, located in FLASH memory, describes the overall
 *  device characteristics, including the supported USB version, control endpoint size and the
 *  number of device configurations. The descriptor is read out by the USB host when the enumeration
 *  process begins.
 */

// Runtime selected USB identity. identity_init() reads it from EEPROM before
// the unit services the host, thus the first enumeration uses it.
uint8_t g_device_identity = 0;

/** Device descriptors, one per identity, identical but for the product ID.
 *  Whole descriptors in flash rather than one patched in RAM, because
 *  USE_FLASH_DESCRIPTORS is on and LUFA streams these straight out of flash.
 */
const USB_Descriptor_Device_t PROGMEM DeviceDescriptors[NUM_DEVICE_IDS] =
{
	{
		.Header                 = {.Size = sizeof(USB_Descriptor_Device_t), .Type = DTYPE_Device},

		.USBSpecification       = VERSION_BCD(1,1,0),
		.Class                  = USB_CSCP_NoDeviceClass,
		.SubClass               = USB_CSCP_NoDeviceSubclass,
		.Protocol               = USB_CSCP_NoDeviceProtocol,

		.Endpoint0Size          = FIXED_CONTROL_ENDPOINT_SIZE,

		.VendorID               = 0x2580,
		.ProductID              = 0x0007,
		.ReleaseNumber          = VERSION_BCD(0,0,1),

		.ManufacturerStrIndex   = STRING_ID_Manufacturer,
		.ProductStrIndex        = STRING_ID_Product,
		.SerialNumStrIndex      = STRING_ID_Serial,

		.NumberOfConfigurations = FIXED_NUM_CONFIGURATIONS
	},
	{
		.Header                 = {.Size = sizeof(USB_Descriptor_Device_t), .Type = DTYPE_Device},

		.USBSpecification       = VERSION_BCD(1,1,0),
		.Class                  = USB_CSCP_NoDeviceClass,
		.SubClass               = USB_CSCP_NoDeviceSubclass,
		.Protocol               = USB_CSCP_NoDeviceProtocol,

		.Endpoint0Size          = FIXED_CONTROL_ENDPOINT_SIZE,

		.VendorID               = 0x2580,
		.ProductID              = 0x1007,
		.ReleaseNumber          = VERSION_BCD(0,0,1),

		.ManufacturerStrIndex   = STRING_ID_Manufacturer,
		.ProductStrIndex        = STRING_ID_Product,
		.SerialNumStrIndex      = STRING_ID_Serial,

		.NumberOfConfigurations = FIXED_NUM_CONFIGURATIONS
	},
	{
		.Header                 = {.Size = sizeof(USB_Descriptor_Device_t), .Type = DTYPE_Device},

		.USBSpecification       = VERSION_BCD(1,1,0),
		.Class                  = USB_CSCP_NoDeviceClass,
		.SubClass               = USB_CSCP_NoDeviceSubclass,
		.Protocol               = USB_CSCP_NoDeviceProtocol,

		.Endpoint0Size          = FIXED_CONTROL_ENDPOINT_SIZE,

		.VendorID               = 0x2580,
		.ProductID              = 0x2007,
		.ReleaseNumber          = VERSION_BCD(0,0,1),

		.ManufacturerStrIndex   = STRING_ID_Manufacturer,
		.ProductStrIndex        = STRING_ID_Product,
		.SerialNumStrIndex      = STRING_ID_Serial,

		.NumberOfConfigurations = FIXED_NUM_CONFIGURATIONS
	},
	{
		.Header                 = {.Size = sizeof(USB_Descriptor_Device_t), .Type = DTYPE_Device},

		.USBSpecification       = VERSION_BCD(1,1,0),
		.Class                  = USB_CSCP_NoDeviceClass,
		.SubClass               = USB_CSCP_NoDeviceSubclass,
		.Protocol               = USB_CSCP_NoDeviceProtocol,

		.Endpoint0Size          = FIXED_CONTROL_ENDPOINT_SIZE,

		.VendorID               = 0x2580,
		.ProductID              = 0x3007,
		.ReleaseNumber          = VERSION_BCD(0,0,1),

		.ManufacturerStrIndex   = STRING_ID_Manufacturer,
		.ProductStrIndex        = STRING_ID_Product,
		.SerialNumStrIndex      = STRING_ID_Serial,

		.NumberOfConfigurations = FIXED_NUM_CONFIGURATIONS
	},
	{
		.Header                 = {.Size = sizeof(USB_Descriptor_Device_t), .Type = DTYPE_Device},

		.USBSpecification       = VERSION_BCD(1,1,0),
		.Class                  = USB_CSCP_NoDeviceClass,
		.SubClass               = USB_CSCP_NoDeviceSubclass,
		.Protocol               = USB_CSCP_NoDeviceProtocol,

		.Endpoint0Size          = FIXED_CONTROL_ENDPOINT_SIZE,

		.VendorID               = 0x2580,
		.ProductID              = 0x4007,
		.ReleaseNumber          = VERSION_BCD(0,0,1),

		.ManufacturerStrIndex   = STRING_ID_Manufacturer,
		.ProductStrIndex        = STRING_ID_Product,
		.SerialNumStrIndex      = STRING_ID_Serial,

		.NumberOfConfigurations = FIXED_NUM_CONFIGURATIONS
	},
	{
		.Header                 = {.Size = sizeof(USB_Descriptor_Device_t), .Type = DTYPE_Device},

		.USBSpecification       = VERSION_BCD(1,1,0),
		.Class                  = USB_CSCP_NoDeviceClass,
		.SubClass               = USB_CSCP_NoDeviceSubclass,
		.Protocol               = USB_CSCP_NoDeviceProtocol,

		.Endpoint0Size          = FIXED_CONTROL_ENDPOINT_SIZE,

		.VendorID               = 0x2580,
		.ProductID              = 0x5007,
		.ReleaseNumber          = VERSION_BCD(0,0,1),

		.ManufacturerStrIndex   = STRING_ID_Manufacturer,
		.ProductStrIndex        = STRING_ID_Product,
		.SerialNumStrIndex      = STRING_ID_Serial,

		.NumberOfConfigurations = FIXED_NUM_CONFIGURATIONS
	},
	{
		.Header                 = {.Size = sizeof(USB_Descriptor_Device_t), .Type = DTYPE_Device},

		.USBSpecification       = VERSION_BCD(1,1,0),
		.Class                  = USB_CSCP_NoDeviceClass,
		.SubClass               = USB_CSCP_NoDeviceSubclass,
		.Protocol               = USB_CSCP_NoDeviceProtocol,

		.Endpoint0Size          = FIXED_CONTROL_ENDPOINT_SIZE,

		.VendorID               = 0x2580,
		.ProductID              = 0x6007,
		.ReleaseNumber          = VERSION_BCD(0,0,1),

		.ManufacturerStrIndex   = STRING_ID_Manufacturer,
		.ProductStrIndex        = STRING_ID_Product,
		.SerialNumStrIndex      = STRING_ID_Serial,

		.NumberOfConfigurations = FIXED_NUM_CONFIGURATIONS
	},
	{
		.Header                 = {.Size = sizeof(USB_Descriptor_Device_t), .Type = DTYPE_Device},

		.USBSpecification       = VERSION_BCD(1,1,0),
		.Class                  = USB_CSCP_NoDeviceClass,
		.SubClass               = USB_CSCP_NoDeviceSubclass,
		.Protocol               = USB_CSCP_NoDeviceProtocol,

		.Endpoint0Size          = FIXED_CONTROL_ENDPOINT_SIZE,

		.VendorID               = 0x2580,
		.ProductID              = 0x7007,
		.ReleaseNumber          = VERSION_BCD(0,0,1),

		.ManufacturerStrIndex   = STRING_ID_Manufacturer,
		.ProductStrIndex        = STRING_ID_Product,
		.SerialNumStrIndex      = STRING_ID_Serial,

		.NumberOfConfigurations = FIXED_NUM_CONFIGURATIONS
	},
};

/** Configuration descriptor structure. This descriptor, located in FLASH memory, describes the usage
 *  of the device in one of its supported configurations, including information about any device interfaces
 *  and endpoints. The descriptor is read out by the USB host during the enumeration process when selecting
 *  a configuration so that the host may correctly communicate with the USB device.
 */
const USB_Descriptor_Configuration_t PROGMEM ConfigurationDescriptor =
{
	.Config =
		{
			.Header                   = {.Size = sizeof(USB_Descriptor_Configuration_Header_t), .Type = DTYPE_Configuration},

			.TotalConfigurationSize   = sizeof(USB_Descriptor_Configuration_t),
			.TotalInterfaces          = 2,

			.ConfigurationNumber      = 1,
			.ConfigurationStrIndex    = NO_DESCRIPTOR,

			.ConfigAttributes         = (USB_CONFIG_ATTR_RESERVED),

			.MaxPowerConsumption      = USB_CONFIG_POWER_MA(480)
		},

	.Audio_ControlInterface =
		{
			.Header                   = {.Size = sizeof(USB_Descriptor_Interface_t), .Type = DTYPE_Interface},

			.InterfaceNumber          = 0,
			.AlternateSetting         = 0,

			.TotalEndpoints           = 0,

			.Class                    = AUDIO_CSCP_AudioClass,
			.SubClass                 = AUDIO_CSCP_ControlSubclass,
			.Protocol                 = AUDIO_CSCP_ControlProtocol,

			.InterfaceStrIndex        = NO_DESCRIPTOR
		},

	.Audio_ControlInterface_SPC =
		{
			.Header                   = {.Size = sizeof(USB_Audio_Descriptor_Interface_AC_t), .Type = DTYPE_CSInterface},
			.Subtype                  = AUDIO_DSUBTYPE_CSInterface_Header,

			.ACSpecification          = VERSION_BCD(1,0,0),
			.TotalLength              = sizeof(USB_Audio_Descriptor_Interface_AC_t),

			.InCollection             = 1,
			.InterfaceNumber          = 1,
		},

	.Audio_StreamInterface =
		{
			.Header                   = {.Size = sizeof(USB_Descriptor_Interface_t), .Type = DTYPE_Interface},

			.InterfaceNumber          = 1,
			.AlternateSetting         = 0,

			.TotalEndpoints           = 2,

			.Class                    = AUDIO_CSCP_AudioClass,
			.SubClass                 = AUDIO_CSCP_MIDIStreamingSubclass,
			.Protocol                 = AUDIO_CSCP_StreamingProtocol,

			.InterfaceStrIndex        = NO_DESCRIPTOR
		},

	.Audio_StreamInterface_SPC =
		{
			.Header                   = {.Size = sizeof(USB_MIDI_Descriptor_AudioInterface_AS_t), .Type = DTYPE_CSInterface},
			.Subtype                  = AUDIO_DSUBTYPE_CSInterface_General,

			.AudioSpecification       = VERSION_BCD(1,0,0),

			.TotalLength              = (sizeof(USB_Descriptor_Configuration_t) -
			                             offsetof(USB_Descriptor_Configuration_t, Audio_StreamInterface_SPC))
		},

	.MIDI_In_Jack_Emb =
		{
			.Header                   = {.Size = sizeof(USB_MIDI_Descriptor_InputJack_t), .Type = DTYPE_CSInterface},
			.Subtype                  = AUDIO_DSUBTYPE_CSInterface_InputTerminal,

			.JackType                 = MIDI_JACKTYPE_Embedded,
			.JackID                   = 0x01,

			.JackStrIndex             = NO_DESCRIPTOR
		},

	.MIDI_In_Jack_Ext =
		{
			.Header                   = {.Size = sizeof(USB_MIDI_Descriptor_InputJack_t), .Type = DTYPE_CSInterface},
			.Subtype                  = AUDIO_DSUBTYPE_CSInterface_InputTerminal,

			.JackType                 = MIDI_JACKTYPE_External,
			.JackID                   = 0x02,

			.JackStrIndex             = NO_DESCRIPTOR
		},

	.MIDI_Out_Jack_Emb =
		{
			.Header                   = {.Size = sizeof(USB_MIDI_Descriptor_OutputJack_t), .Type = DTYPE_CSInterface},
			.Subtype                  = AUDIO_DSUBTYPE_CSInterface_OutputTerminal,

			.JackType                 = MIDI_JACKTYPE_Embedded,
			.JackID                   = 0x03,

			.NumberOfPins             = 1,
			.SourceJackID             = {0x02},
			.SourcePinID              = {0x01},

			.JackStrIndex             = NO_DESCRIPTOR
		},

	.MIDI_Out_Jack_Ext =
		{
			.Header                   = {.Size = sizeof(USB_MIDI_Descriptor_OutputJack_t), .Type = DTYPE_CSInterface},
			.Subtype                  = AUDIO_DSUBTYPE_CSInterface_OutputTerminal,

			.JackType                 = MIDI_JACKTYPE_External,
			.JackID                   = 0x04,

			.NumberOfPins             = 1,
			.SourceJackID             = {0x01},
			.SourcePinID              = {0x01},

			.JackStrIndex             = NO_DESCRIPTOR
		},

	.MIDI_In_Jack_Endpoint =
		{
			.Endpoint =
				{
					.Header              = {.Size = sizeof(USB_Audio_Descriptor_StreamEndpoint_Std_t), .Type = DTYPE_Endpoint},

					.EndpointAddress     = MIDI_STREAM_OUT_EPADDR,
					.Attributes          = (EP_TYPE_BULK | ENDPOINT_ATTR_NO_SYNC | ENDPOINT_USAGE_DATA),
					.EndpointSize        = MIDI_STREAM_EPSIZE,
					.PollingIntervalMS   = 0x00
				},

			.Refresh                  = 0,
			.SyncEndpointNumber       = 0
		},

	.MIDI_In_Jack_Endpoint_SPC =
		{
			.Header                   = {.Size = sizeof(USB_MIDI_Descriptor_Jack_Endpoint_t), .Type = DTYPE_CSEndpoint},
			.Subtype                  = AUDIO_DSUBTYPE_CSEndpoint_General,

			.TotalEmbeddedJacks       = 0x01,
			.AssociatedJackID         = {0x01}
		},

	.MIDI_Out_Jack_Endpoint =
		{
			.Endpoint =
				{
					.Header              = {.Size = sizeof(USB_Audio_Descriptor_StreamEndpoint_Std_t), .Type = DTYPE_Endpoint},

					.EndpointAddress     = MIDI_STREAM_IN_EPADDR,
					.Attributes          = (EP_TYPE_BULK | ENDPOINT_ATTR_NO_SYNC | ENDPOINT_USAGE_DATA),
					.EndpointSize        = MIDI_STREAM_EPSIZE,
					.PollingIntervalMS   = 0x00
				},

			.Refresh                  = 0,
			.SyncEndpointNumber       = 0
		},

	.MIDI_Out_Jack_Endpoint_SPC =
		{
			.Header                   = {.Size = sizeof(USB_MIDI_Descriptor_Jack_Endpoint_t), .Type = DTYPE_CSEndpoint},
			.Subtype                  = AUDIO_DSUBTYPE_CSEndpoint_General,

			.TotalEmbeddedJacks       = 0x01,
			.AssociatedJackID         = {0x03}
		}
};

/** Language descriptor structure. This descriptor, located in FLASH memory, is returned when the host requests
 *  the string descriptor with index 0 (the first index). It is actually an array of 16-bit integers, which indicate
 *  via the language ID table available at USB.org what languages the device supports for its string descriptors.
 */
const USB_Descriptor_String_t PROGMEM LanguageString =
{
	.Header                 = {.Size = USB_STRING_LEN(1), .Type = DTYPE_String},

	.UnicodeString          = {LANGUAGE_ID_ENG}
};

/** Manufacturer descriptor string. This is a Unicode string containing the manufacturer's details in human readable
 *  form, and is read out upon request by the host when the appropriate string ID is requested, listed in the Device
 *  Descriptor.
 */
const USB_Descriptor_String_t PROGMEM ManufacturerString =
{
	.Header                 = {.Size = USB_STRING_LEN(12), .Type = DTYPE_String},

	.UnicodeString          = L"DJ TechTools"
};

/** Product descriptor string. This is a Unicode string containing the product's details in human readable form,
 *  and is read out upon request by the host when the appropriate string ID is requested, listed in the Device
 *  Descriptor.
 */
/** Product descriptor strings, one for each identity. USB_Descriptor_String_t
 *  ends in a flexible array member, and C does not permit an array of it. Thus
 *  this type copies it with a fixed length string. Header.Size holds the true
 *  length, and the host reads only that length. Slot 0 keeps the original name
 *  with no number.
 */
typedef struct
{
	USB_Descriptor_Header_t Header;
	wchar_t                 UnicodeString[23];
} ATTR_PACKED ProductString_t;

const ProductString_t PROGMEM ProductStrings[NUM_DEVICE_IDS] =
{
	{
		.Header                 = {.Size = USB_STRING_LEN(20), .Type = DTYPE_String},
		.UnicodeString          = L"Midi Fighter Twister"
	},
	{
		.Header                 = {.Size = USB_STRING_LEN(22), .Type = DTYPE_String},
		.UnicodeString          = L"Midi Fighter Twister 2"
	},
	{
		.Header                 = {.Size = USB_STRING_LEN(22), .Type = DTYPE_String},
		.UnicodeString          = L"Midi Fighter Twister 3"
	},
	{
		.Header                 = {.Size = USB_STRING_LEN(22), .Type = DTYPE_String},
		.UnicodeString          = L"Midi Fighter Twister 4"
	},
	{
		.Header                 = {.Size = USB_STRING_LEN(22), .Type = DTYPE_String},
		.UnicodeString          = L"Midi Fighter Twister 5"
	},
	{
		.Header                 = {.Size = USB_STRING_LEN(22), .Type = DTYPE_String},
		.UnicodeString          = L"Midi Fighter Twister 6"
	},
	{
		.Header                 = {.Size = USB_STRING_LEN(22), .Type = DTYPE_String},
		.UnicodeString          = L"Midi Fighter Twister 7"
	},
	{
		.Header                 = {.Size = USB_STRING_LEN(22), .Type = DTYPE_String},
		.UnicodeString          = L"Midi Fighter Twister 8"
	},
};

/** Device Serial Numbers - We have four to allow users to user multiple MF3Ds at once
 */ 
SerialString_t SerialString;

//const USB_Descriptor_String_t PROGMEM SerialString =
//{
	//.Header                 = {.Size = USB_STRING_LEN(8), 
							   //.Type = DTYPE_String},
	//.UnicodeString          = L"6666666A"
//};


//const USB_Descriptor_String_t PROGMEM ProductString_Alternate = // !Summer2016Update: USB String Modification
//{
//
	////.Header                 = {.Size = USB_STRING_LEN(20), .Type = DTYPE_String},
	////.UnicodeString          = L"Midi Fighter TWISTER"
	//.Header                 = {.Size = USB_STRING_LEN(22), .Type = DTYPE_String},
	//.UnicodeString          = L"Midi Fighter Twister 2"
//};


/** This function is called by the library when in device mode, and must be overridden (see library "USB Descriptors"
 *  documentation) by the application code so that the address and size of a requested descriptor can be given
 *  to the USB library. When the device receives a Get Descriptor request on the control endpoint, this function
 *  is called so that the descriptor details can be passed back and the appropriate descriptor sent back to the
 *  USB host.
 */
uint16_t CALLBACK_USB_GetDescriptor(const uint16_t wValue,
									const uint8_t wIndex,
									const void** const DescriptorAddress
									#if (defined(ARCH_HAS_MULTI_ADDRESS_SPACE) || defined(__DOXYGEN__)) && \
									!(defined(USE_FLASH_DESCRIPTORS) || defined(USE_EEPROM_DESCRIPTORS) || defined(USE_RAM_DESCRIPTORS))
									, uint8_t* const DescriptorMemorySpace
									#endif
									)
{
	const uint8_t  DescriptorType   = (wValue >> 8);
	const uint8_t  DescriptorNumber = (wValue & 0xFF);

	const void* Address = NULL;
	uint16_t    Size    = NO_DESCRIPTOR;

	switch (DescriptorType)
	{
case DTYPE_Device:
Address = &DeviceDescriptors[g_device_identity];
Size    = sizeof(USB_Descriptor_Device_t);
break;

		case DTYPE_Configuration:
			Address = &ConfigurationDescriptor;
			Size    = sizeof(USB_Descriptor_Configuration_t);
			break;
		case DTYPE_String:
			switch (DescriptorNumber)
			{
				case STRING_ID_Language:
					Address = &LanguageString;
					Size    = pgm_read_byte(&LanguageString.Header.Size);
					break;
				case STRING_ID_Manufacturer:
					Address = &ManufacturerString;
					Size    = pgm_read_byte(&ManufacturerString.Header.Size);
					break;
				case STRING_ID_Product:
					Address = &ProductStrings[g_device_identity];
					Size    = pgm_read_byte(&ProductStrings[g_device_identity].Header.Size);
				break;
				//case STRING_ID_Serial:  // !Summer2016Update: could add serial number to descriptors (not necessary, used additional Product IDs)
					//Address = &SerialString;
					//Size = SerialString.Header.Size;
					////Size    = pgm_read_byte(&SerialString.Header.Size);
					//break;
				case STRING_ID_Serial:
				Address = &SerialString;
				Size = SerialString.Header.Size;
				//*DescriptorMemorySpace = MEMSPACE_RAM;
				break;
				
			}

			break;
	}

	*DescriptorAddress = Address;
	return Size;
}

