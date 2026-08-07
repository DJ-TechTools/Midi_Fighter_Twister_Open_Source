#ifndef NATIVE_MODE_H_
#define NATIVE_MODE_H_

    #include <asf.h>

    bool native_mode_is_active(void);
    //toggles native mode on/off (e.g. from a hardware button combo)
    void native_mode_toggle(void);

    void native_mode_handle_sysex_command(uint8_t length, uint8_t* buffer);
    //returns true if the MIDI event was consumed by native mode
    bool native_mode_consume_midi_event(uint8_t type, uint8_t channel, uint8_t number, uint8_t value);

    //returns true if the encoder display was updated for native mode
    bool native_mode_update_encoder_display_single(uint8_t idx);

    //returns true if encoder rotary input was processed by native mode
    bool native_mode_process_encoder_input_rotary(uint8_t idx, int16_t delta);
    //returns true if encoder switch input was processed by native mode
    bool native_mode_process_encoder_input_switch_pressed(uint8_t idx, bool pressed);
    //returns true if side switch input was processed by native mode
    bool native_mode_process_side_switch_pressed(uint8_t idx, bool pressed);

#endif /* NATIVE_MODE_H_ */
