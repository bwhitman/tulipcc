#include "amy.h"
#include "sine_lutset.h"
#include "impulse_lutset.h"
#include "triangle_lutset.h"

// We only allow a couple of KS oscs as they're RAM hogs 
#define MAX_KS_BUFFER_LEN 802 // 44100/55  -- 55Hz (A1) lowest we can go for KS
#define KS_OSCS 1
float ** ks_buffer; 
uint8_t ks_polyphony_index; 



// For hardware random
#ifdef ESP_PLATFORM
#include <esp_system.h>
#endif

/* Dan Ellis libblosca functions */
const float *choose_from_lutset(float period, lut_entry *lutset, int16_t *plut_size) {
    // Select the best entry from a lutset for a given period. 
    //
    // Args:
    //    period: (float) Target period of waveform in fractional samples.
    //    lutset: Sorted list of LUTs, as generated by create_lutset().
    //
    // Returns:
    //   One of the LUTs from the lutset, best suited to interpolating to generate
    //   a waveform of the desired period.  Its size is returned in *plut_size.
    // Use the earliest (i.e., longest, most harmonics) LUT that works
    // (i.e., will not actually cause aliasing).
    // So start with the highest-bandwidth (and longest) LUTs, but skip them
    // if they result in aliasing.
    const float *lut_table = NULL;
    int lut_size = 0;
    int lut_index = 0;
    while(lutset[lut_index].table_size > 0) {
        lut_table = lutset[lut_index].table;
        lut_size = lutset[lut_index].table_size;
        // What proportion of nyquist does the highest harmonic in this table occupy?
        float lut_bandwidth = 2 * lutset[lut_index].highest_harmonic / (float)lut_size;
        // To complete one cycle of <lut_size> points in <period> steps, each step
        // will need to be this many samples:
        float lut_hop = lut_size / period;
        // If we have a signal with a given bandwidth, but then speed it up by 
        // skipping lut_hop samples per sample, its bandwidth will increase 
        // proportionately.
        float interp_bandwidth = lut_bandwidth * lut_hop;
        if (interp_bandwidth < 0.9) {
            // No aliasing, even with a 10% buffer (i.e., 19.8 kHz).
            break;
        }
        ++lut_index;
    }
    // At this point, we either got to the end of the LUT table, or we found a
    // table we could interpolate without aliasing.
    *plut_size = lut_size;
    return lut_table;
}


// dictionary:
// oldC -- python
// step == scaled_phase
// skip == step (scaled_step)

float render_lut_fm_osc(float * buf, float phase, float step, float incoming_amp, float ending_amp, const float* lut, int16_t lut_size, float * mod, float feedback_level, float * last_two) { 
    int lut_mask = lut_size - 1;
    float past0 = last_two[0];
    float past1 = last_two[1];
    for(uint16_t i=0;i<BLOCK_SIZE;i++) {
        float scaled_phase = lut_size * (phase + mod[i] + feedback_level * ((past1 + past0) / 2.0));
        int base_index = (int)scaled_phase;
        float frac = scaled_phase - base_index;
        float b = lut[base_index & lut_mask];
        float c = lut[(base_index+1) & lut_mask];
        float sample = b + ((c - b) * frac);
        float scaled_amp = incoming_amp + (ending_amp - incoming_amp)*((float)i/(float)BLOCK_SIZE);
        buf[i] += sample * scaled_amp;
        phase += step;
        phase -= (int)phase;
        past1 = past0;
        past0 = sample;
    }
    last_two[0] = past0;
    last_two[1] = past1;
    return phase;// - (int)phase;
}

// TODO -- move this render_LUT to use the "New terminology" that render_lut_fm_osc uses
// pass in unscaled phase, use step instead of skip, etc
float render_lut(float * buf, float step, float skip, float incoming_amp, float ending_amp, const float* lut, int32_t lut_size) { 
    // We assume lut_size == 2^R for some R, so (lut_size - 1) consists of R '1's in binary.
    int lut_mask = lut_size - 1;
    for(uint16_t i=0;i<BLOCK_SIZE;i++) {
        // Floor is very slow on the esp32, so we just cast. Dan told me to add this comment. -- baw
        //uint16_t base_index = (uint16_t)floor(step);
        uint32_t base_index = (uint32_t)step;
        float frac = step - (float)base_index;
        float b = lut[(base_index + 0) & lut_mask];
        float c = lut[(base_index + 1) & lut_mask];
#ifdef LINEAR_INTERP
        // linear interpolation.
        float sample = b + ((c - b) * frac);
#else /* !LINEAR_INTERP => CUBIC_INTERP */
        float a = lut[(base_index - 1) & lut_mask];
        float d = lut[(base_index + 2) & lut_mask];
        // cubic interpolation (TTEM p.46).
        //      float sample = 
        //    - frac * (frac - 1) * (frac - 2) / 6.0 * a
        //    + (frac + 1) * (frac - 1) * (frac - 2) / 2.0 * b
        //    - (frac + 1) * frac * (frac - 2) / 2.0 * c
        //    + (frac + 1) * frac * (frac - 1) / 6.0 * d;
        // Miller's optimization - https://github.com/pure-data/pure-data/blob/master/src/d_array.c#L440
        float cminusb = c - b;
        float sample = b + frac * (cminusb - 0.1666667f * (1.-frac) * ((d - a - 3.0f * cminusb) * frac + (d + 2.0f*a - 3.0f*b)));
#endif /* LINEAR_INTERP */
        float scaled_amp = incoming_amp + (ending_amp - incoming_amp)*((float)i/(float)BLOCK_SIZE);
        buf[i] += sample * scaled_amp;

        step += skip;
        if(step >= lut_size) step -= lut_size;
    }
    return step;
}

float render_am_lut(float * buf, float step, float skip, float incoming_amp, float ending_amp, const float* lut, int16_t lut_size, float *mod, float bandwidth) { 
    int lut_mask = lut_size - 1;
    for(uint16_t i=0;i<BLOCK_SIZE;i++) {
        uint16_t base_index = (uint16_t)step;
        float frac = step - (float)base_index;
        float b = lut[(base_index + 0) & lut_mask];
        float c = lut[(base_index + 1) & lut_mask];
        float sample = b + ((c - b) * frac);
        float mod_sample = mod[i]; // * (1.0 / bandwidth);
        float am = dsps_sqrtf_f32_ansi(1.0-bandwidth) + (mod_sample * dsps_sqrtf_f32_ansi(2.0*bandwidth));
        float scaled_amp = incoming_amp + (ending_amp - incoming_amp)*((float)i/(float)BLOCK_SIZE);
        buf[i] += sample * scaled_amp * am ;
        step += skip;
        if(step >= lut_size) step -= lut_size;
    }
    return step;
}

void lpf_buf(float *buf, float decay, float *state) {
    // Implement first-order low-pass (leaky integrator).
    for (uint16_t i = 0; i < BLOCK_SIZE; ++i) {
        float s = *state;
        buf[i] = decay * s + buf[i];
        *state = buf[i];
    }
}




/* Pulse wave */

void pulse_note_on(uint8_t osc) {
    float period_samples = (float)SAMPLE_RATE / synth[osc].freq;
    synth[osc].lut = choose_from_lutset(period_samples, impulse_lutset, &synth[osc].lut_size);
    synth[osc].step = (float)synth[osc].lut_size * synth[osc].phase;
    // Tune the initial integrator state to compensate for mid-sample alignment of table.
    float skip = synth[osc].lut_size / period_samples;
    float amp = synth[osc].amp * skip * 4.0 / synth[osc].lut_size;
    synth[osc].lpf_state = -0.5 * amp * synth[osc].lut[0];
}

void render_pulse(float * buf, uint8_t osc) {
    // LPF time constant should be ~ 10x osc period, so droop is minimal.
    float period_samples = (float)SAMPLE_RATE / msynth[osc].freq;
    synth[osc].lpf_alpha = 1.0 - 1.0 / (10.0 * period_samples);
    float duty = msynth[osc].duty;
    if (duty < 0.01) duty = 0.01;
    if (duty > 0.99) duty = 0.99;
    float skip = synth[osc].lut_size / period_samples;
    // Scale the impulse proportional to the skip so its integral remains ~constant.
    float amp = msynth[osc].amp * skip * 4.0 / synth[osc].lut_size;
    float pwm_step = synth[osc].step + duty * synth[osc].lut_size;
    if (pwm_step >= synth[osc].lut_size)  pwm_step -= synth[osc].lut_size;
    synth[osc].step = render_lut(buf, synth[osc].step, skip, synth[osc].last_amp, amp, synth[osc].lut, synth[osc].lut_size);
    render_lut(buf, pwm_step, skip, -synth[osc].last_amp, -amp, synth[osc].lut, synth[osc].lut_size);
    lpf_buf(buf, synth[osc].lpf_alpha, &synth[osc].lpf_state);
    synth[osc].last_amp = amp;
}

void pulse_mod_trigger(uint8_t osc) {
    float mod_sr = (float)SAMPLE_RATE / (float)BLOCK_SIZE;
    float period = 1. / (synth[osc].freq/mod_sr);
    synth[osc].step = period * synth[osc].phase;
}

// dpwe sez to use this method for low-freq mod pulse still 
float compute_mod_pulse(uint8_t osc) {
    // do BW pulse gen at SR=44100/64
    float mod_sr = (float)SAMPLE_RATE / (float)BLOCK_SIZE;
    if(msynth[osc].duty < 0.001 || msynth[osc].duty > 0.999) msynth[osc].duty = 0.5;
    float period = 1. / (msynth[osc].freq/(float)mod_sr);
    float period2 = msynth[osc].duty * period; // if duty is 0.5, square wave
    if(synth[osc].step >= period || synth[osc].step == 0)  {
        synth[osc].sample = 1;
        synth[osc].substep = 0; // start the duty cycle counter
        synth[osc].step = 0;
    } 
    if(synth[osc].sample == 1) {
        if(synth[osc].substep++ > period2) {
            synth[osc].sample = -1;
        }
    }
    synth[osc].step++;
    return (synth[osc].sample * msynth[osc].amp); // -1 .. 1
}


/* Saw wave */

void saw_note_on(uint8_t osc) {
    float period_samples = (float)SAMPLE_RATE / synth[osc].freq;
    synth[osc].lut = choose_from_lutset(period_samples, impulse_lutset, &synth[osc].lut_size);
    synth[osc].step = (float)synth[osc].lut_size * synth[osc].phase;
    synth[osc].lpf_state = 0;
    // Tune the initial integrator state to compensate for mid-sample alignment of table.
    float skip = synth[osc].lut_size / period_samples;
    float amp = synth[osc].amp * skip * 4.0  / synth[osc].lut_size;
    synth[osc].lpf_state = -0.5 * amp * synth[osc].lut[0];
    // Calculate the mean of the LUT.
    float lut_sum = 0;
    for (int i = 0; i < synth[osc].lut_size; ++i) {
        lut_sum += synth[osc].lut[i];
    }
    synth[osc].dc_offset = -lut_sum / synth[osc].lut_size;
}

void render_saw(float * buf, uint8_t osc) {
    float period_samples = (float)SAMPLE_RATE / msynth[osc].freq;
    synth[osc].lpf_alpha = 1.0 - 1.0 / (10.0 * period_samples);
    float skip = synth[osc].lut_size / period_samples;
    // Scale the impulse proportional to the skip so its integral remains ~constant.
    float amp = msynth[osc].amp * skip * 4.0 / synth[osc].lut_size;
    synth[osc].step = render_lut(
          buf, synth[osc].step, skip, synth[osc].last_amp, amp, synth[osc].lut, synth[osc].lut_size);
    // Give the impulse train a negative bias so that it integrates to zero mean.
    float offset = amp * synth[osc].dc_offset;
    for (int i = 0; i < BLOCK_SIZE; ++i) {
        buf[i] += offset;
    }
    lpf_buf(buf, synth[osc].lpf_alpha, &synth[osc].lpf_state);
    synth[osc].last_amp = amp;
}



void saw_mod_trigger(uint8_t osc) {
    float mod_sr = (float)SAMPLE_RATE / (float)BLOCK_SIZE;
    float period = 1. / (synth[osc].freq/mod_sr);
    synth[osc].step = period * synth[osc].phase;
}

// TODO -- this should use dpwe code
float compute_mod_saw(uint8_t osc) {
    float mod_sr = (float)SAMPLE_RATE / (float)BLOCK_SIZE;
    float period = 1. / (msynth[osc].freq/mod_sr);
    if(synth[osc].step >= period || synth[osc].step == 0) {
        synth[osc].sample = -1;
        synth[osc].step = 0; // reset the period counter
    } else {
        synth[osc].sample = -1 + (synth[osc].step * (2.0 / period));
    }
    synth[osc].step++;
    return (synth[osc].sample * msynth[osc].amp); 
}


/* triangle wave */

void triangle_note_on(uint8_t osc) {
    float period_samples = (float)SAMPLE_RATE / synth[osc].freq;
    synth[osc].lut = choose_from_lutset(period_samples, triangle_lutset, &synth[osc].lut_size);
    synth[osc].step = (float)synth[osc].lut_size * synth[osc].phase;
}

void render_triangle(float * buf, uint8_t osc) {
    float period_samples = (float)SAMPLE_RATE / msynth[osc].freq;
    float skip = synth[osc].lut_size / period_samples;
    float amp = msynth[osc].amp;
    synth[osc].step = render_lut(buf, synth[osc].step, skip, synth[osc].last_amp, amp, synth[osc].lut, synth[osc].lut_size);
    synth[osc].last_amp = amp;
}


void triangle_mod_trigger(uint8_t osc) {
    float mod_sr = (float)SAMPLE_RATE / (float)BLOCK_SIZE;
    float period = 1. / (synth[osc].freq/mod_sr);
    synth[osc].step = period * synth[osc].phase;
}

// TODO -- this should use dpwe code 
float compute_mod_triangle(uint8_t osc) {
    float mod_sr = (float)SAMPLE_RATE / (float)BLOCK_SIZE;    
    float period = 1. / (msynth[osc].freq/mod_sr);
    if(synth[osc].step >= period || synth[osc].step == 0) {
        synth[osc].sample = -1;
        synth[osc].step = 0; // reset the period counter
    } else {
        if(synth[osc].step < (period/2.0)) {
            synth[osc].sample = -1 + (synth[osc].step * (2 / period * 2));
        } else {
            synth[osc].sample = 1 - ((synth[osc].step-(period/2)) * (2 / period * 2));
        }
    }
    synth[osc].step++;
    return (synth[osc].sample * msynth[osc].amp); // -1 .. 1
    
}

extern int64_t total_samples;



/* FM */
// NB this uses new lingo for step, skip, phase etc
void fm_sine_note_on(uint8_t osc, uint8_t algo_osc) {
    if(synth[osc].ratio >= 0) {
        msynth[osc].freq = (msynth[algo_osc].freq * synth[osc].ratio);
    }
    msynth[osc].freq += synth[osc].detune - 7.0;
    float period_samples = (float)SAMPLE_RATE / msynth[osc].freq;
    synth[osc].lut = choose_from_lutset(period_samples, sine_lutset, &synth[osc].lut_size);
}
void render_fm_sine(float *buf, uint8_t osc, float *mod, float feedback_level, uint8_t algo_osc) {
    if(synth[osc].ratio >= 0) {
        msynth[osc].freq = msynth[algo_osc].freq * synth[osc].ratio;
    }
    msynth[osc].freq += synth[osc].detune - 7.0;    
    float step = msynth[osc].freq / (float)SAMPLE_RATE;
    float amp = msynth[osc].amp;
    synth[osc].phase = render_lut_fm_osc(buf, synth[osc].phase, step, synth[osc].last_amp, amp, 
                 synth[osc].lut, synth[osc].lut_size, mod, feedback_level, synth[osc].last_two);
    synth[osc].last_amp = amp;
}

/* sine */

void sine_note_on(uint8_t osc) {
    // There's really only one sine table, but for symmetry with the other ones...
    //float period_samples = (float)SAMPLE_RATE / synth[osc].freq;
    synth[osc].lut = sine_lutable_0; //choose_from_lutset(period_samples, sine_lutset, &synth[osc].lut_size);
    synth[osc].lut_size = 256;
    synth[osc].step = (float)synth[osc].lut_size * synth[osc].phase;
}

void render_partial(float * buf, uint8_t osc) {
    if(msynth[osc].feedback > 0) {
        float scratch[2][BLOCK_SIZE];
        for(uint16_t i=0;i<BLOCK_SIZE;i++) scratch[0][i] = amy_get_random() *  20.0;
        dsps_biquad_gen_lpf_f32(coeffs[osc], 100.0/SAMPLE_RATE, 0.707);
        #ifdef ESP_PLATFORM
            dsps_biquad_f32_ae32(scratch[0], scratch[1], BLOCK_SIZE, coeffs[osc], delay[osc]);
        #else
            dsps_biquad_f32_ansi(scratch[0], scratch[1], BLOCK_SIZE, coeffs[osc], delay[osc]);
        #endif
        float skip = msynth[osc].freq / (float)SAMPLE_RATE * synth[osc].lut_size;
        float amp = msynth[osc].amp;
        synth[osc].step = render_am_lut(buf, synth[osc].step, skip, synth[osc].last_amp, amp, 
                 synth[osc].lut, synth[osc].lut_size, scratch[1], msynth[osc].feedback);
    } else {
        float skip = msynth[osc].freq / (float)SAMPLE_RATE * synth[osc].lut_size;
        float amp = msynth[osc].amp;
        synth[osc].step = render_lut(buf, synth[osc].step, skip, synth[osc].last_amp, amp, 
                    synth[osc].lut, synth[osc].lut_size);
    }
    synth[osc].last_amp = msynth[osc].amp;
    if(synth[osc].substep==1) {
        // fade in
        //printf("%d fading in partial osc %d from 0 to %f\n", total_samples, osc, msynth[osc].amp);
        synth[osc].substep = 0;
        for(uint16_t i=0;i<BLOCK_SIZE;i++) buf[i] = buf[i] * ((float)i/(float)BLOCK_SIZE);
    }
    if(synth[osc].substep==2) {
        // fade out
        //printf("%d fading out partial osc %d from %f to 0\n", total_samples, osc, msynth[osc].amp);
        synth[osc].substep = 0;
        for(uint16_t i=0;i<BLOCK_SIZE;i++) buf[i] = buf[i] * ((float)(BLOCK_SIZE-i)/(float)BLOCK_SIZE);
        synth[osc].status=OFF; 

    }
    //printf("%d rendering partial osc %d at %f %f\n", total_samples, osc, msynth[osc].amp, msynth[osc].freq);
}

void partial_note_on(uint8_t osc) {
    synth[osc].lut = sine_lutable_0; //choose_from_lutset(period_samples, sine_lutset, &synth[osc].lut_size);
    synth[osc].lut_size = 256;
    if(synth[osc].phase >= 0) {
        synth[osc].step = (float)synth[osc].lut_size * synth[osc].phase;
        synth[osc].substep = 1; // use for block fade
    } // else keep the old step / no fade, it's a continuation

}

void partial_note_off(uint8_t osc) {
    synth[osc].substep = 2;
    synth[osc].note_on_clock = -1;
    synth[osc].note_off_clock = total_samples;   
}

void render_sine(float * buf, uint8_t osc) { 

    float skip = msynth[osc].freq / (float)SAMPLE_RATE * synth[osc].lut_size;
    synth[osc].step = render_lut(buf, synth[osc].step, skip, synth[osc].last_amp, msynth[osc].amp, 
				 synth[osc].lut, synth[osc].lut_size);
    synth[osc].last_amp = msynth[osc].amp;
    //printf("sysclock %d amp %f\n", get_sysclock(), msynth[osc].amp);
}


// TOOD -- not needed anymore
float compute_mod_sine(uint8_t osc) { 
    float mod_sr = (float)SAMPLE_RATE / (float)BLOCK_SIZE;
    int sinlut_size = sine_lutset[0].table_size;
    const float *sinlut = sine_lutset[0].table;
    float skip = msynth[osc].freq / mod_sr * sinlut_size;

    int lut_mask = sinlut_size - 1;
    uint16_t base_index = (uint16_t)(synth[osc].step);
    float frac = synth[osc].step - (float)base_index;
    float b = sinlut[(base_index + 0) & lut_mask];
    float c = sinlut[(base_index + 1) & lut_mask];
    // linear interpolation for the modulation 
    float sample = b + ((c - b) * frac);
    synth[osc].step += skip;
    if(synth[osc].step >= sinlut_size) synth[osc].step -= sinlut_size;
    return (sample * msynth[osc].amp); // -1 .. 1
}


void sine_mod_trigger(uint8_t osc) {
    sine_note_on(osc);
}

// returns a # between -1 and 1
float amy_get_random() {
#ifdef ESP_PLATFORM
    return (((float)esp_random() / UINT32_MAX) * 2.0) - 1.0;
#else
    return (rand() / (float)RAND_MAX * 2.0) - 1.0;
#endif
}

/* noise */

void render_noise(float *buf, uint8_t osc) {
    for(uint16_t i=0;i<BLOCK_SIZE;i++) {
        buf[i] = amy_get_random() * msynth[osc].amp; 
    }
}

float compute_mod_noise(uint8_t osc) {
    return amy_get_random() * msynth[osc].amp;
}

/* karplus-strong */

void render_ks(float * buf, uint8_t osc) {
    if(msynth[osc].freq >= 55) { // lowest note we can play
        uint16_t buflen = (uint16_t)(SAMPLE_RATE / msynth[osc].freq);
        for(uint16_t i=0;i<BLOCK_SIZE;i++) {
            uint16_t index = (uint16_t)(synth[osc].step);
            synth[osc].sample = ks_buffer[ks_polyphony_index][index];
            ks_buffer[ks_polyphony_index][index] = (ks_buffer[ks_polyphony_index][index] + ks_buffer[ks_polyphony_index][(index + 1) % buflen]) * 0.5 * synth[osc].feedback;
            synth[osc].step = (index + 1) % buflen;
            buf[i] = synth[osc].sample * msynth[osc].amp;
        }
    }
}

void ks_note_on(uint8_t osc) {
    if(msynth[osc].freq<=0) msynth[osc].freq = 1;
    uint16_t buflen = (uint16_t)(SAMPLE_RATE / msynth[osc].freq);
    if(buflen > MAX_KS_BUFFER_LEN) buflen = MAX_KS_BUFFER_LEN;
    // init KS buffer with noise up to max
    for(uint16_t i=0;i<buflen;i++) {
        ks_buffer[ks_polyphony_index][i] = amy_get_random();
    }
    ks_polyphony_index++;
    if(ks_polyphony_index == KS_OSCS) ks_polyphony_index = 0;
}

void ks_note_off(uint8_t osc) {
    msynth[osc].amp = 0;
}


void ks_init(void) {
    // 6ms buffer
    ks_polyphony_index = 0;
    ks_buffer = (float**) malloc(sizeof(float*)*KS_OSCS);
    for(int i=0;i<KS_OSCS;i++) ks_buffer[i] = (float*)malloc(sizeof(float)*MAX_KS_BUFFER_LEN); 
}

void ks_deinit(void) {
    for(int i=0;i<KS_OSCS;i++) free(ks_buffer[i]);
    free(ks_buffer);
}





