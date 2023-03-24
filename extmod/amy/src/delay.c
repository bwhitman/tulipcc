#include "amy.h"
#include "delay.h"

int is_power_of_two(int val) {
    while(val) {
        if (val == 1) return true;
        if (val & 1) return false;
        val >>= 1;
    }
    return false;  // zero is not a power of 2.
}


delay_line_t *new_delay_line(int len, float initial_delay) {
    // Check that len is a power of 2.
    if (!is_power_of_two(len)) { fprintf(stderr, "delay line len must be power of 2, not %d\n", len); abort(); }
    delay_line_t *delay_line = (delay_line_t*)malloc_caps(sizeof(delay_line_t), MALLOC_CAP_INTERNAL); 
    delay_line->samples = (float*)malloc_caps(len * sizeof(float), MALLOC_CAP_INTERNAL);
    delay_line->len = len;
    delay_line->next_in = 0;
    delay_line->next_out = len - initial_delay;
    for (int i = 0; i < len; ++i) {
        delay_line->samples[i] = 0;
    }
    return delay_line;
}


void delay_line_in(float *in, int n_samples, delay_line_t *delay_line) {
    // Store new samples in circular buffer.
    int index = delay_line->next_in;
    int index_mask = delay_line->len - 1; // will be all 1s because len is guaranteed 2**n.
    while(n_samples-- > 0) {
        delay_line->samples[index++] = *in++;
        index &= index_mask;
    }
    delay_line->next_in = index;
}

void delay_line_out(float *out, int n_samples, float* inc_delta, float delta_scale, delay_line_t *delay_line) {
    // Read a block of samples out from the delay line.
    // "step" is a real-valued read-from sample index; "inc" is a real-valued step, so the resampling
    // can be non-constant delay.  Function returns the final value of step (to re-use in the next
    // block).
    int delay_len = delay_line->len;
    int index_mask = delay_len - 1; // will be all 1s because len is guaranteed 2**n.
    float *delay = delay_line->samples;
    float step = delay_line->next_out;
    while(n_samples-- > 0) {
        // Interpolated sample copied from oscillators.c:render_lut
        uint32_t base_index = (uint32_t)step;
        float frac = step - (float)base_index;
        float b = delay[(base_index + 0) & index_mask];
        float c = delay[(base_index + 1) & index_mask];
#ifdef LINEAR_INTERP
        // linear interpolation.
        float sample = b + ((c - b) * frac);
#else /* !LINEAR_INTERP => CUBIC_INTERP */
        float a = delay[(base_index - 1) & index_mask];
        float d = delay[(base_index + 2) & index_mask];
        // Miller's optimization - https://github.com/pure-data/pure-data/blob/master/src/d_array.c#L440
        float cminusb = c - b;
        float sample = b + frac * (cminusb - 0.1666667f * (1.0f-frac) * ((d - a - 3.0f * cminusb) * frac + (d + 2.0f*a - 3.0f*b)));
#endif /* LINEAR_INTERP */
        *out++ = sample;
        step += 1.0 + delta_scale * *inc_delta++;
        if(step >= delay_len) step -= delay_len;
    }
    delay_line->next_out = step;
}


void apply_variable_delay(float *block, delay_line_t *delay_line, float *delay_mod, float delay_scale) {
    delay_line_in(block, BLOCK_SIZE, delay_line);
    delay_line_out(block, BLOCK_SIZE, delay_mod, delay_scale, delay_line);
}
