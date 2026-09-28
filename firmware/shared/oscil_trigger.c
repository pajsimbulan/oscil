#include "oscil_trigger.h"
#include <stddef.h>

long trig_find(const uint16_t *buf, size_t from, size_t to, const trig_cfg_t *cfg, float *frac) {
    if (from == 0) from = 1; //need a previous sample
    int armed  = 0;
    int lo = (int)cfg->level - cfg->hyst; //arm threshold, rising
    int hi = (int)cfg->level + cfg->hyst; // arm threshold, falling

    for(size_t i = from; i < to; i++) {
        int s = buf[i];
        if(cfg->edge == TRIG_RISING) {
            if(s < lo) armed = 1;
            else if (armed && s >= cfg->level) goto hit;
        } else {
            if( s > hi) armed = 1;
            else if (armed && s <= cfg->level) goto hit;
        }
        continue;
hit:
        if(frac) {
            float a = buf[i-1];
            float b = buf[i];
            float f = (b != a) ? ( (float)cfg->level -a) / (b-a) : 0.0f;
            *frac = f < 0.0f? 0.0f : (f > 1.0f ? 1.0f: f);
        }
        return (long) i;
    }
    return -1;

}
