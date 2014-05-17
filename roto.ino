/* Copyright (c) 2011 Peter Teichman */

#include <Arduino.h>
#include <MIDI.h>

#undef ROTO_TEST_TONES

#include "tonewheels.h"

#define BUFFER_LEN 128

int16_t buffer1[BUFFER_LEN];
int16_t buffer2[BUFFER_LEN];
int16_t *cur = 0;
int16_t cur_sample;

volatile boolean gen_buffer1;
volatile boolean gen_buffer2;

uint8_t num_keys_down=0;

void handleControlChange(byte channel, byte controller, byte value) {
    /* map MIDI CC# 110-118 to drawbars */
    uint8_t drawbar = controller - 109;

    /* invert value and convert it to drawbar settings 0..8. Invert to
     * match organ behavior, where pulling a drawbar closer raises its
     * volume. This might not be appropriate on all keyboards. */
    value = map(127-value, 0, 127, 0, 8);

    tonewheels_set_drawbar(drawbar, value);
}

void handleNoteOn(byte channel, byte note, byte velocity) {
    uint8_t key = midiNoteToKey(note);

    tonewheels_key_down(key);
    num_keys_down++;
    bitWrite(PORTD, 7, 1);
}

void handleNoteOff(byte channel, byte note, byte velocity) {
    uint8_t key = midiNoteToKey(note);

    tonewheels_key_up(key);
    num_keys_down--;
    if (num_keys_down == 0) {
        bitWrite(PORTD, 7, 0);
        /* Clear all tonewheels when the last key is released. This
         * is a workaround until live drawbar changes are supported. */
        tonewheels_init();
    }
}

uint8_t midiNoteToKey(unsigned int note) {
    return note - 11;
}

void setup() {
    tonewheels_init();
    MIDI.begin(0);
    MIDI.setHandleControlChange(handleControlChange);

    /* MidiVox DATA LED */
    pinMode(7, OUTPUT);

    cli();

    /* Enable interrupt on timer2 == 127, with clk/8 prescaler. At 16MHz,
     this gives a timer interrupt at 15625Hz. */
    TIMSK2 = (1 << OCIE2A);
    OCR2A = 127;

    /* clear/reset timer on match */
    TCCR2A = 1<<WGM21 | 0<<WGM20; /* CTC mode, reset on match */
    TCCR2B = 0<<CS22 | 1<<CS21 | 0<<CS20; /* clk, /8 prescaler */

    SPCR = 0x50;
    SPSR = 0x01;
    DDRB |= 0x2E;
    PORTB |= (1<<1);

    tonewheels_sample_v(&buffer1[0], BUFFER_LEN);
    tonewheels_sample_v(&buffer2[0], BUFFER_LEN);
    cur = &buffer1[0];

    /* set default drawbar volumes to 888000000 */
    handleControlChange(0, 110, 0);
    handleControlChange(0, 111, 0);
    handleControlChange(0, 112, 0);

    sei();
}

ISR(TIMER2_COMPA_vect) {
    /* OCR2A has been cleared, per TCCR2A above */
    OCR2A = 127;

    cur_sample = *cur >> 4;

    /* convert from signed 11-bit to unsigned 12-bit for the DAC */
    uint16_t sample = cur_sample + 0x800;

    PORTB &= ~(1<<1);

    /* buffered, 1x gain, active mode */
    SPDR = highByte(sample) | 0x70;
    while (!(SPSR & (1<<SPIF)));

    SPDR = lowByte(sample);
    while (!(SPSR & (1<<SPIF)));

    PORTB |= (1<<1);

    /* move to the next sample */
    *cur++;

    if (cur == &buffer1[BUFFER_LEN]) {
        if (gen_buffer2) {
            /* if we're still regenerating buffer2, replay buffer1 */
            cur = buffer1;
        } else {
            /* play buffer2, regenerate buffer1 */
            gen_buffer1 = true;
            cur = buffer2;
        }
    } else if (cur == &buffer2[BUFFER_LEN]) {
        if (gen_buffer1) {
            cur = buffer2;
        } else {
            gen_buffer2 = true;
            cur = buffer1;
        }
    }
}

#ifdef ROTO_TEST_TONES
uint16_t lastChange = 0;
boolean playing = false;
#endif

void loop() {
    if (gen_buffer1) {
        tonewheels_sample_v(&buffer1[0], BUFFER_LEN);
        gen_buffer1 = false;
    } else if (gen_buffer2) {
        tonewheels_sample_v(&buffer2[0], BUFFER_LEN);
        gen_buffer2 = false;
    }

    MIDI.read();

#ifdef ROTO_TEST_TONES
    uint16_t now = millis();
    if (now - lastChange > 1000) {
        lastChange = now;
        playing = !playing;

        if (playing) {
            handleNoteOn(1, 48, 1);
            handleNoteOn(1, 52, 1);
            handleNoteOn(1, 55, 1);
            handleNoteOn(1, 58, 1);
        } else {
            handleNoteOff(1, 48, 1);
            handleNoteOff(1, 52, 1);
            handleNoteOff(1, 55, 1);
            handleNoteOff(1, 58, 1);
        }
    }
#endif
}
