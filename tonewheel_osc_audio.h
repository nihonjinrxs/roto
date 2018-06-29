#ifndef TONEWHEEL_OSC_H
#define TONEWHEEL_OSC_H

#include <Audio.h>
#include "tonewheel_osc.h"

class TonewheelOsc : public AudioStream {
  public:
    TonewheelOsc() : AudioStream(0, NULL) {
    }

    void init() {
        osc = tonewheel_osc_new();
    }
    void update() {
        audio_block_t *block;
        block = allocate();
        if (!block) {
            return;
        }
        tonewheel_osc_fill(osc, block->data, AUDIO_BLOCK_SAMPLES);
        transmit(block, 0);
        release(block);
    }

    void setVolume(uint8_t tonewheel, uint8_t volume) {
        tonewheel_osc_set_volume(osc, tonewheel, volume);
    }

  private:
    tonewheel_osc *osc;
};

#endif