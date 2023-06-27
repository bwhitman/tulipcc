

#include <stdio.h>
#include "polyfills.h"

#include "py/runtime.h"
#include "py/mperrno.h"
#include "py/mphal.h"
#include "display.h"
#include "bresenham.h"
#include "extmod/vfs.h"
#include "py/stream.h"
#include "alles.h"
#include "midi.h"
#include "ui.h"
#include "genhdr/mpversion.h"

#ifdef ESP_PLATFORM
#include "tasks.h"
#endif


// Graphics

// tulip.display_clock(18)
// clock = tulip.display_clock()
extern float unix_display_set_clock();
extern void display_start();
extern void display_stop();
extern void save_tfb();
extern void restore_tfb();
extern uint8_t tfb_active;

STATIC mp_obj_t tulip_display_clock(size_t n_args, const mp_obj_t *args) {
    if(n_args==1) {
        uint16_t mhz = mp_obj_get_int(args[0]);
        display_set_clock(mhz);
        return mp_const_none;
    }
    return mp_obj_new_int(PIXEL_CLOCK_MHZ);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(tulip_display_clock_obj, 0, 1, tulip_display_clock);

STATIC mp_obj_t tulip_display_restart(size_t n_args, const mp_obj_t *args) {
    display_set_clock(PIXEL_CLOCK_MHZ);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(tulip_display_restart_obj, 0, 0, tulip_display_restart);

STATIC mp_obj_t tulip_display_stop(size_t n_args, const mp_obj_t *args) {
    display_stop();
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(tulip_display_stop_obj, 0, 0, tulip_display_stop);


STATIC mp_obj_t tulip_display_start(size_t n_args, const mp_obj_t *args) {
    display_start();
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(tulip_display_start_obj, 0, 0, tulip_display_start);


STATIC mp_obj_t tulip_tfb_stop(size_t n_args, const mp_obj_t *args) {
    tfb_active = 0;
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(tulip_tfb_stop_obj, 0, 0, tulip_tfb_stop);


STATIC mp_obj_t tulip_tfb_start(size_t n_args, const mp_obj_t *args) {
    tfb_active = 1;
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(tulip_tfb_start_obj, 0, 0, tulip_tfb_start);

STATIC mp_obj_t tulip_tfb_log_start(size_t n_args, const mp_obj_t *args) {
    tfb_log = 1;
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(tulip_tfb_log_start_obj, 0, 0, tulip_tfb_log_start);


STATIC mp_obj_t tulip_tfb_log_stop(size_t n_args, const mp_obj_t *args) {
    tfb_log = 0;
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(tulip_tfb_log_stop_obj, 0, 0, tulip_tfb_log_stop);



STATIC mp_obj_t tulip_gpu_log_start(size_t n_args, const mp_obj_t *args) {
    gpu_log = 1;
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(tulip_gpu_log_start_obj, 0, 0, tulip_gpu_log_start);


STATIC mp_obj_t tulip_gpu_log_stop(size_t n_args, const mp_obj_t *args) {
    gpu_log = 0;
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(tulip_gpu_log_stop_obj, 0, 0, tulip_gpu_log_stop);



STATIC mp_obj_t tulip_tfb_save(size_t n_args, const mp_obj_t *args) {
    save_tfb();
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(tulip_tfb_save_obj, 0, 0, tulip_tfb_save);

STATIC mp_obj_t tulip_tfb_restore(size_t n_args, const mp_obj_t *args) {
    restore_tfb();
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(tulip_tfb_restore_obj, 0, 0, tulip_tfb_restore);

// fps = tulip.fps()
STATIC mp_obj_t tulip_fps(size_t n_args, const mp_obj_t *args) {
    return mp_obj_new_float_from_f(reported_fps);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(tulip_fps_obj, 0, 0, tulip_fps);

// usage = tulip.gpu()
STATIC mp_obj_t tulip_gpu(size_t n_args, const mp_obj_t *args) {
    return mp_obj_new_float_from_f(reported_gpu_usage);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(tulip_gpu_obj, 0, 0, tulip_gpu);


//tulip.bg_swap()
STATIC mp_obj_t tulip_bg_swap(size_t n_args, const mp_obj_t *args) {
    display_swap();
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(tulip_bg_swap_obj, 0, 0, tulip_bg_swap);



STATIC mp_obj_t tulip_ticks_ms(size_t n_args, const mp_obj_t *args) {
    return mp_obj_new_int(get_ticks_ms());
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(tulip_ticks_ms_obj, 0, 0, tulip_ticks_ms);

// tulip.bg_pixel(x,y, pal_idx)
// (r,g,b) = tulip.bg_pixel(x,y)
STATIC mp_obj_t tulip_bg_pixel(size_t n_args, const mp_obj_t *args) {
    uint16_t x = mp_obj_get_int(args[0]);
    uint16_t y = mp_obj_get_int(args[1]);
    if(n_args == 3) { // set
        // Set the pixel
        uint8_t pal_idx = mp_obj_get_int(args[2]);
        display_set_bg_pixel_pal(x,y,pal_idx);
        return mp_const_none; 
    } else { // get the pixel
        return mp_obj_new_int( display_get_bg_pixel_pal(x,y)); 
    }
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(tulip_bg_pixel_obj, 2, 3, tulip_bg_pixel);



// This version of bg_clear is 2.5x as fast as the old one, as it's just copying within SPIRAM.
// tulip.bg_clear(pal_idx)
// tulip.bg_clear() # uses default
STATIC mp_obj_t tulip_bg_clear(size_t n_args, const mp_obj_t *args) {
    uint8_t pal_idx = bg_pal_color;
    if(n_args == 1) {
        pal_idx = mp_obj_get_int(args[0]);
    }
    // Set a single pixel
    display_set_bg_pixel_pal(0,0,pal_idx);
    // Copy that pixel
    for (uint16_t j = 0; j < V_RES+OFFSCREEN_Y_PX; j++) {
        for (uint16_t i = 0; i < H_RES+OFFSCREEN_X_PX; i++) {
            (bg)[(((j*(H_RES+OFFSCREEN_X_PX) + i)*BYTES_PER_PIXEL) + 0)] = (bg)[0];
        }
    }    
    return mp_const_none; 
}


STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(tulip_bg_clear_obj, 0, 1, tulip_bg_clear);


// tulip.bg_bitmap(x, y, w, h, bitmap)  --> sets or gets bitmap to fb ram
STATIC mp_obj_t tulip_bg_bitmap(size_t n_args, const mp_obj_t *args) {
    uint16_t x = mp_obj_get_int(args[0]);
    uint16_t y = mp_obj_get_int(args[1]);
    uint16_t w = mp_obj_get_int(args[2]);
    uint16_t h = mp_obj_get_int(args[3]);
    if(n_args == 5) {
        // Set the rect with bitmap pixels
        mp_buffer_info_t bufinfo;
        if (mp_obj_get_type(args[4]) == &mp_type_bytes) {
            mp_get_buffer(args[4], &bufinfo, MP_BUFFER_READ);
            display_set_bg_bitmap_raw(x, y, w, h, (uint8_t*)bufinfo.buf);
        }
        return mp_const_none; 
    } else {
        // return a bitmap
        uint8_t bitmap[w*h*BYTES_PER_PIXEL];  
        display_get_bg_bitmap_raw(x,y,w,h, bitmap);
        return mp_obj_new_bytes(bitmap, w*h*BYTES_PER_PIXEL);
    }
}

STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(tulip_bg_bitmap_obj, 4, 5, tulip_bg_bitmap);


// tulip.bg_blit(x, y, w, h, x1, y1)  --> copies bitmap ram
STATIC mp_obj_t tulip_bg_blit(size_t n_args, const mp_obj_t *args) {
    uint16_t x = mp_obj_get_int(args[0]);
    uint16_t y = mp_obj_get_int(args[1]);
    uint16_t w = mp_obj_get_int(args[2]);
    uint16_t h = mp_obj_get_int(args[3]);
    uint16_t x1 = mp_obj_get_int(args[4]);
    uint16_t y1 = mp_obj_get_int(args[5]);
    if(n_args > 6) {
        display_bg_bitmap_blit_alpha(x,y,w,h,x1,y1);
    } else {
        display_bg_bitmap_blit(x,y,w,h,x1,y1);
    }
    return mp_const_none;
}

STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(tulip_bg_blit_obj, 6, 7, tulip_bg_blit);



// tulip.bg_png(bytes, x,y)
// tulip.bg_png(filename, x,y)
STATIC mp_obj_t tulip_bg_png(size_t n_args, const mp_obj_t *args) {
    unsigned error;
    unsigned char* image;
    unsigned width, height;
    uint16_t x = mp_obj_get_int(args[1]);
    uint16_t y = mp_obj_get_int(args[2]);

    mp_buffer_info_t bufinfo;
    uint8_t file = 0;
    if (mp_obj_get_type(args[0]) == &mp_type_bytes) {
        mp_get_buffer(args[0], &bufinfo, MP_BUFFER_READ);
    } else {
        uint32_t fs = file_size(mp_obj_str_get_str(args[0]));
        bufinfo.buf = malloc_caps(fs, MALLOC_CAP_SPIRAM);
        bufinfo.len = fs;
        read_file(mp_obj_str_get_str(args[0]), (uint8_t *)bufinfo.buf, -1, 1);
        file = 1;
    }
    error = lodepng_decode_memory(&image, &width, &height, (uint8_t*)bufinfo.buf, bufinfo.len, LCT_RGBA, 8);
    if(error) printf("error %u: %s\n", error, lodepng_error_text(error));
    display_set_bg_bitmap_rgba(x,y,width,height,image);
    free_caps(image);
    if(file) {
        free_caps(bufinfo.buf);
    }
    return mp_const_none;
}

STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(tulip_bg_png_obj, 3, 3, tulip_bg_png);

//tulip.bg_scroll(line, x_offset, y_offset, x_speed, y_speed)
//tulip.bg_scroll() # resets
STATIC mp_obj_t tulip_bg_scroll(size_t n_args, const mp_obj_t *args) {
    if(n_args<5) {
        // Reset
        for(uint16_t line_no=0;line_no<V_RES;line_no++) {
            x_offsets[line_no] = 0;
            y_offsets[line_no] = line_no;
            x_speeds[line_no] = 0;
            y_speeds[line_no] = 0;
        }
    } else {
        uint16_t line_no = mp_obj_get_int(args[0]);
        uint16_t x_offset = mp_obj_get_int(args[1]);
        uint16_t y_offset = mp_obj_get_int(args[2]);
        int16_t x_speed = mp_obj_get_int(args[3]);
        int16_t y_speed = mp_obj_get_int(args[4]);
        x_offsets[line_no] = x_offset;
        y_offsets[line_no] = y_offset;
        x_speeds[line_no] = x_speed;
        y_speeds[line_no] = y_speed;
    }
    return mp_const_none;
}

STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(tulip_bg_scroll_obj, 0, 5, tulip_bg_scroll);

STATIC mp_obj_t tulip_bg_scroll_x_speed(size_t n_args, const mp_obj_t *args) {
    uint16_t line_no = mp_obj_get_int(args[0]);
    int16_t x_speed = mp_obj_get_int(args[1]);
    x_speeds[line_no] = x_speed;
    return mp_const_none;
}

STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(tulip_bg_scroll_x_speed_obj, 2, 2, tulip_bg_scroll_x_speed);


STATIC mp_obj_t tulip_bg_scroll_y_speed(size_t n_args, const mp_obj_t *args) {
    uint16_t line_no = mp_obj_get_int(args[0]);
    int16_t y_speed = mp_obj_get_int(args[1]);
    y_speeds[line_no] = y_speed;
    return mp_const_none;
}

STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(tulip_bg_scroll_y_speed_obj, 2, 2, tulip_bg_scroll_y_speed);


STATIC mp_obj_t tulip_bg_scroll_x_offset(size_t n_args, const mp_obj_t *args) {
    uint16_t line_no = mp_obj_get_int(args[0]);
    int16_t x_offset = mp_obj_get_int(args[1]);
    x_offsets[line_no] = x_offset;
    return mp_const_none;
}

STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(tulip_bg_scroll_x_offset_obj, 2, 2, tulip_bg_scroll_x_offset);


STATIC mp_obj_t tulip_bg_scroll_y_offset(size_t n_args, const mp_obj_t *args) {
    uint16_t line_no = mp_obj_get_int(args[0]);
    int16_t y_offset = mp_obj_get_int(args[1]);
    y_offsets[line_no] = y_offset;
    return mp_const_none;
}

STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(tulip_bg_scroll_y_offset_obj, 2, 2, tulip_bg_scroll_y_offset);



// tulip.tfb_str(x,y, str, [format], [fg_color], [bg_color])
// (str, format, fg, bg) = tulip.tfb_str(x,y)
STATIC mp_obj_t tulip_tfb_str(size_t n_args, const mp_obj_t *args) {
    uint16_t x = mp_obj_get_int(args[0]);
    uint16_t y = mp_obj_get_int(args[1]);
    uint8_t set = 0;
    if(n_args > 2) set = 1;
    if(set) {
        const char * str = mp_obj_str_get_str(args[2]);
        for(uint16_t i=0;i<strlen(str);i++) {
            TFB[y*TFB_COLS+x+i] = str[i];
        }
        if(n_args > 3) {
            if(mp_obj_get_int(args[3])>=0) {
                for(uint16_t i=0;i<strlen(str);i++) {
                    TFBf[y*TFB_COLS+x+i] = mp_obj_get_int(args[3]);
                }
            }
        }
        if(n_args > 4 ) {
            if(mp_obj_get_int(args[4])>=0) {
                for(uint16_t i=0;i<strlen(str);i++) {
                    TFBfg[y*TFB_COLS+x+i] = mp_obj_get_int(args[4]);
                }
            }
        }
        if(n_args > 5 ) {
            if(mp_obj_get_int(args[5])>=0) {
                for(uint16_t i=0;i<strlen(str);i++) {
                    TFBbg[y*TFB_COLS+x+i] = mp_obj_get_int(args[5]);
                }
            }
        }
        return mp_const_none; 
    } else {
        mp_obj_t tuple[4];
        tuple[0] = mp_obj_new_str((const char*)(TFB + (y*TFB_COLS+x)), 1);
        tuple[1] = mp_obj_new_int(TFBf[y*TFB_COLS+x]);
        tuple[2] = mp_obj_new_int(TFBfg[y*TFB_COLS+x]);
        tuple[3] = mp_obj_new_int(TFBbg[y*TFB_COLS+x]);
        return mp_obj_new_tuple(4,tuple);
    }
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(tulip_tfb_str_obj, 2, 6, tulip_tfb_str);



mp_obj_t frame_callback = NULL; 
mp_obj_t frame_arg = NULL; 
mp_obj_t midi_callback = NULL; 
mp_obj_t ui_callback = NULL; 
mp_obj_t touch_callback = NULL; 
mp_obj_t bg_touch_callback = NULL; 


void tulip_frame_isr() {
    if(frame_callback != NULL) {
        // Schedule the python callback given to run asap
        mp_sched_schedule(frame_callback, frame_arg);
#ifdef ESP_PLATFORM
        //mp_hal_wake_main_task_from_isr();
#endif
    }
}

void tulip_midi_isr() {
    if(midi_callback != NULL)
        mp_sched_schedule(midi_callback, mp_const_none); 
}

void tulip_ui_isr(uint8_t ui_id) {
    if(ui_callback != NULL)
        mp_sched_schedule(ui_callback, mp_obj_new_int(ui_id)); 
}

void tulip_touch_isr(uint8_t up) {
    if(touch_callback != NULL) {
        mp_sched_schedule(touch_callback, mp_obj_new_int(up)); 
    }
}

void tulip_bg_touch_isr(uint8_t id) {
    if(bg_touch_callback != NULL) {
        mp_sched_schedule(bg_touch_callback, mp_obj_new_int(id)); 
    }
}



// tulip.frame_callback(cb, arg)
// tulip.frame_callback() -- stops 
STATIC mp_obj_t tulip_frame_callback(size_t n_args, const mp_obj_t *args) {
    if(n_args == 0) {
        frame_callback = NULL;
    } else {
        frame_callback = args[0];
    }
    if(n_args > 1) {
        frame_arg = args[1];
    }
    return mp_const_none;
}

STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(tulip_frame_callback_obj, 0, 2, tulip_frame_callback);


// tulip.midi_callback(cb)
// tulip.midi_callback() -- stops 
STATIC mp_obj_t tulip_midi_callback(size_t n_args, const mp_obj_t *args) {
    if(n_args == 0) {
        midi_callback = NULL;
    } else {
        midi_callback = args[0];
    }
    return mp_const_none;
}

STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(tulip_midi_callback_obj, 0, 2, tulip_midi_callback);



// tulip.ui_callback(cb)
// tulip.ui_callback() -- stops 
STATIC mp_obj_t tulip_ui_callback(size_t n_args, const mp_obj_t *args) {
    if(n_args == 0) {
        ui_callback = NULL;
    } else {
        ui_callback = args[0];
    }
    return mp_const_none;
}

STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(tulip_ui_callback_obj, 0, 1, tulip_ui_callback);


// tulip.ui_callback(cb)
// tulip.ui_callback() -- stops 
STATIC mp_obj_t tulip_touch_callback(size_t n_args, const mp_obj_t *args) {
    if(n_args == 0) {
        touch_callback = NULL;
    } else {
        touch_callback = args[0];
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(tulip_touch_callback_obj, 0, 1, tulip_touch_callback);

// tulip.bg_touch_callback(cb)
// tulip.bg_touch_callback() -- stops 
STATIC mp_obj_t tulip_bg_touch_callback(size_t n_args, const mp_obj_t *args) {
    if(n_args == 0) {
        bg_touch_callback = NULL;
    } else {
        bg_touch_callback = args[0];
    }
    return mp_const_none;
}

STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(tulip_bg_touch_callback_obj, 0, 1, tulip_bg_touch_callback);


STATIC mp_obj_t tulip_midi_in(size_t n_args, const mp_obj_t *args) {
    if(last_midi_len > 0) {
        return mp_obj_new_bytes(last_midi, last_midi_len);
    } 
    return mp_const_none;
}

STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(tulip_midi_in_obj, 0, 0, tulip_midi_in);


STATIC mp_obj_t tulip_midi_out(size_t n_args, const mp_obj_t *args) {
    if(mp_obj_get_type(args[0]) == &mp_type_bytes) {
        mp_buffer_info_t bufinfo;
        mp_get_buffer(args[0], &bufinfo, MP_BUFFER_READ);
        midi_out((uint8_t*)bufinfo.buf, bufinfo.len);
    } else {
        mp_obj_t *items;
        size_t len;
        mp_obj_get_array(args[0], &len, &items);
        uint8_t *b = malloc_caps(len, MALLOC_CAP_INTERNAL);
        for(uint16_t i=0;i<(uint16_t)len;i++) {
            b[i] = mp_obj_get_int(items[i]);
        }
        midi_out(b, len);
        free_caps(b);
    }
    return mp_const_none;
}

STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(tulip_midi_out_obj, 1, 1, tulip_midi_out);


//(w,h,bytes) = sprite_png(pngdata, mem_pos) 
//(w,h,bytes) = sprite_png("filename.png", mem_pos)
STATIC mp_obj_t tulip_sprite_png(size_t n_args, const mp_obj_t *args) {
    unsigned error;
    unsigned char* image;
    unsigned width, height;
    uint16_t mem_pos = mp_obj_get_int(args[1]);
    mp_buffer_info_t bufinfo;
    uint8_t file = 0;
    if (mp_obj_get_type(args[0]) == &mp_type_bytes) {
        mp_get_buffer(args[0], &bufinfo, MP_BUFFER_READ);
    } else {
        uint32_t fs = file_size(mp_obj_str_get_str(args[0]));
        bufinfo.buf = malloc_caps(fs, MALLOC_CAP_SPIRAM);
        bufinfo.len = fs;
        read_file(mp_obj_str_get_str(args[0]), (uint8_t *)bufinfo.buf, -1, 1);
        file = 1;
    }
    error = lodepng_decode_memory(&image, &width, &height, (uint8_t*)bufinfo.buf, bufinfo.len, LCT_RGBA, 8);
    if(error) printf("error %u: %s\n", error, lodepng_error_text(error));
    display_load_sprite_rgba(mem_pos, width*height*BYTES_PER_PIXEL, image);
    free_caps(image);
    if(file) free_caps(bufinfo.buf);
    mp_obj_t tuple[3];
    tuple[0] = mp_obj_new_int(width);
    tuple[1] = mp_obj_new_int(height);
    tuple[2] = mp_obj_new_int(width*height*BYTES_PER_PIXEL);
    return mp_obj_new_tuple(3, tuple);
}

STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(tulip_sprite_png_obj, 2, 2, tulip_sprite_png);


//bytes = sprite_bitmap(bitmap, mem_pos) 
//buffer_of_bytes = sprite_bitmap(mem_pos, length)
STATIC mp_obj_t tulip_sprite_bitmap(size_t n_args, const mp_obj_t *args) {
    if (mp_obj_get_type(args[0]) == &mp_type_bytes) {
        uint16_t mem_pos = mp_obj_get_int(args[1]);
        mp_buffer_info_t bufinfo;
        mp_get_buffer(args[0], &bufinfo, MP_BUFFER_READ);
        display_load_sprite_raw(mem_pos, bufinfo.len, bufinfo.buf);
        return mp_obj_new_int(bufinfo.len);
    } 
    uint16_t mem_pos = mp_obj_get_int(args[0]);
    uint16_t length = mp_obj_get_int(args[1]);
    return mp_obj_new_bytes(sprite_ram+mem_pos, length);
}

STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(tulip_sprite_bitmap_obj, 2, 2, tulip_sprite_bitmap);

//sprite_register(34, mem_pos, w,h) # 34 = sprite number, can be up to ...
STATIC mp_obj_t tulip_sprite_register(size_t n_args, const mp_obj_t *args) {
    uint16_t spriteno = mp_obj_get_int(args[0]);
    uint32_t mem_pos = mp_obj_get_int(args[1]);
    uint16_t width = mp_obj_get_int(args[2]);
    uint16_t height = mp_obj_get_int(args[3]);
    if(spriteno < SPRITES) {
        sprite_mem[spriteno] = mem_pos;
        sprite_w_px[spriteno] = width;
        sprite_h_px[spriteno] = height;
    } else {
        fprintf(stderr, "register bad spriteno %d\n", spriteno);
    }
    return mp_const_none;
}

STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(tulip_sprite_register_obj, 4, 4, tulip_sprite_register);


//sprite_move(34, x,y)
STATIC mp_obj_t tulip_sprite_move(size_t n_args, const mp_obj_t *args) {
    uint16_t spriteno = mp_obj_get_int(args[0]);
    uint16_t x = mp_obj_get_int(args[1]);
    uint16_t y = mp_obj_get_int(args[2]);
    if(spriteno < SPRITES) {
        if(check_dim_xy(x,y)) {
            sprite_x_px[spriteno] = x;
            sprite_y_px[spriteno] = y;
        } else {
            fprintf(stderr, "bad sprite xy %d %d\n", x,y);
        }
    } else {
        fprintf(stderr, "move bad spriteno %d\n", spriteno);
    }
    return mp_const_none;
}

STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(tulip_sprite_move_obj, 3, 3, tulip_sprite_move);

STATIC mp_obj_t tulip_sprite_on(size_t n_args, const mp_obj_t *args) {
    uint16_t spriteno = mp_obj_get_int(args[0]);
    if(spriteno < SPRITES) sprite_vis[spriteno] = SPRITE_IS_SPRITE;
    return mp_const_none;
}

STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(tulip_sprite_on_obj, 1, 1, tulip_sprite_on);


STATIC mp_obj_t tulip_sprite_off(size_t n_args, const mp_obj_t *args) {
    uint16_t spriteno = mp_obj_get_int(args[0]);
    if(spriteno < SPRITES) sprite_vis[spriteno] = 0;
    return mp_const_none;
}

STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(tulip_sprite_off_obj, 1, 1, tulip_sprite_off);


STATIC mp_obj_t tulip_sprite_clear(size_t n_args, const mp_obj_t *args) {
    display_reset_sprites();
    return mp_const_none;
}

STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(tulip_sprite_clear_obj, 0, 0, tulip_sprite_clear);


STATIC mp_obj_t tulip_collisions(size_t n_args, const mp_obj_t *args) {
    mp_obj_t list = mp_obj_new_list(0, NULL);
    // iterate through all fields
    for(uint8_t a=0;a<32;a++) {
        for(uint8_t b=a+1;b<32;b++) {
            if(collide_mask_get(a,b)) {
                mp_obj_t tuple[2];
                tuple[0] = mp_obj_new_int(a);
                tuple[1] = mp_obj_new_int(b);
                mp_obj_list_append(list, mp_obj_new_tuple(2, tuple));
            }
        }
    }
    // clear collision
    // TODO -- divine 62 a better way 
    for(uint8_t i=0;i<62;i++) collision_bitfield[i] = 0;
    return list;
}

STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(tulip_collisions_obj, 0, 0, tulip_collisions);



extern void editor(const char * filename);
STATIC mp_obj_t tulip_edit(size_t n_args, const mp_obj_t *args) {
    if(n_args) {
        editor(mp_obj_str_get_str(args[0]));
    } else {
        editor(NULL);
    }
    return mp_const_none;
}

STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(tulip_edit_obj, 0, 1, tulip_edit);

extern void unix_display_timings(uint16_t, uint16_t, uint16_t, uint16_t, uint16_t, uint16_t);
STATIC mp_obj_t tulip_gpu_reset(size_t n_args, const mp_obj_t *args) {
    display_reset_bg();
    display_reset_sprites();
    display_reset_tfb();
    return mp_const_none;
}

STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(tulip_gpu_reset_obj, 0, 0, tulip_gpu_reset);



STATIC mp_obj_t tulip_int_screenshot(size_t n_args, const mp_obj_t *args) {
    char fn[50];
    strcpy(fn, mp_obj_str_get_str(args[0]));
    display_screenshot(fn);
    return mp_const_none;

}

STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(tulip_int_screenshot_obj, 1, 1, tulip_int_screenshot);


extern void mcast_send(char*, uint16_t len);

STATIC mp_obj_t tulip_alles_send(size_t n_args, const mp_obj_t *args) {
    if(n_args > 1) {
        if(mp_obj_get_int(args[1])) { // mesh
            mcast_send( (char*)mp_obj_str_get_str(args[0]), strlen(mp_obj_str_get_str(args[0])));
            return mp_const_none;
        }
    }
    alles_send_message((char*)mp_obj_str_get_str(args[0]), strlen(mp_obj_str_get_str(args[0])));
    return mp_const_none;
}

STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(tulip_alles_send_obj, 1, 2, tulip_alles_send);

extern char * alles_local_ip;
STATIC mp_obj_t tulip_multicast_start(size_t n_args, const mp_obj_t *args) {
    if(n_args>0) {
        strcpy(alles_local_ip, mp_obj_str_get_str(args[0]));
    }
    alles_init_multicast();
    return mp_const_none;

}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(tulip_multicast_start_obj, 0, 1, tulip_multicast_start);


STATIC mp_obj_t tulip_brightness(size_t n_args, const mp_obj_t *args) {
    if(n_args > 0) {
        display_brightness(mp_obj_get_int(args[0]));
        return mp_const_none;
    } else {
        return mp_obj_new_int(brightness);
    }

}

STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(tulip_brightness_obj, 0, 1, tulip_brightness);

extern uint16_t check_joy();
STATIC mp_obj_t tulip_joy(size_t n_args, const mp_obj_t *args) {
    return mp_obj_new_int(check_joy());
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(tulip_joy_obj, 0, 0, tulip_joy);


extern uint8_t last_scan[8];
STATIC mp_obj_t tulip_keys(size_t n_args, const mp_obj_t *args) {
    mp_obj_t tuple[7];
    tuple[0] = mp_obj_new_int(last_scan[0]);
    for(uint8_t i=0;i<6;i++) tuple[i+1] = mp_obj_new_int(last_scan[i+2]);
    return mp_obj_new_tuple(7, tuple);

}

STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(tulip_keys_obj, 0, 0, tulip_keys);


extern int16_t last_touch_x[3];
extern int16_t last_touch_y[3];
STATIC mp_obj_t tulip_touch(size_t n_args, const mp_obj_t *args) {
    mp_obj_t tuple[6];
    for(uint8_t i=0;i<3;i++) {
        tuple[i*2] = mp_obj_new_int(last_touch_x[i]);
        tuple[i*2+1] = mp_obj_new_int(last_touch_y[i]);
    }
    return mp_obj_new_tuple(6, tuple);

}

STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(tulip_touch_obj, 0, 0, tulip_touch);

#ifdef ESP_PLATFORM
#include "touchscreen.h"
#else
int16_t touch_x_delta = 0;
int16_t touch_y_delta = 0;
#endif
STATIC mp_obj_t tulip_touch_delta(size_t n_args, const mp_obj_t *args) {
    if(n_args > 0) {
        touch_x_delta = mp_obj_get_int(args[0]);
        touch_y_delta = mp_obj_get_int(args[1]);
    } else {
        mp_obj_t tuple[2];
        tuple[0] = mp_obj_new_int(touch_x_delta);
        tuple[1] = mp_obj_new_int(touch_y_delta);
        return mp_obj_new_tuple(2, tuple);
    }
    return mp_const_none;

}

STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(tulip_touch_delta_obj, 0, 2, tulip_touch_delta);


STATIC mp_obj_t tulip_key_wait(size_t n_args, const mp_obj_t *args) {
    int16_t ch = mp_hal_stdin_rx_chr();
    return mp_obj_new_int(ch);
}

STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(tulip_key_wait_obj, 0, 0, tulip_key_wait);


STATIC mp_obj_t tulip_key(size_t n_args, const mp_obj_t *args) {
    return mp_obj_new_int(check_rx_char());
}

STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(tulip_key_obj, 0, 0, tulip_key);

extern uint8_t keyboard_send_keys_to_micropython;
STATIC mp_obj_t tulip_key_scan(size_t n_args, const mp_obj_t *args) {
    keyboard_send_keys_to_micropython = !(mp_obj_get_int(args[0]));
    return mp_const_none;
}

STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(tulip_key_scan_obj, 1, 1, tulip_key_scan);


extern float compute_cpu_usage(uint8_t debug);
STATIC mp_obj_t tulip_cpu(size_t n_args, const mp_obj_t *args) {
    // for now just printf to uart
    float idle;
    if(n_args > 0) {
        idle = compute_cpu_usage(1);        
    } else {
        idle = compute_cpu_usage(0);
    }
    return mp_obj_new_float_from_f(idle);
}

STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(tulip_cpu_obj, 0, 1, tulip_cpu);

#ifndef ESP_PLATFORM
extern char* get_tulip_home_path();
STATIC mp_obj_t tulip_app_path(size_t n_args, const mp_obj_t *args) {
    char * path = get_tulip_home_path();
    return mp_obj_new_str(path, strlen(path));
}

STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(tulip_app_path_obj, 0, 0, tulip_app_path);
#endif


//void bg_touch_register(uint8_t ui_id, uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
//void bg_touch_deregister(uint8_t ui_id) {

STATIC mp_obj_t tulip_bg_touch_up(size_t n_args, const mp_obj_t *args) {
    uint8_t id = mp_obj_get_int(args[0]);
    return mp_obj_new_int(bg_touch_up(id));
}

STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(tulip_bg_touch_up_obj, 1, 1, tulip_bg_touch_up);


STATIC mp_obj_t tulip_bg_touch_register(size_t n_args, const mp_obj_t *args) {
    uint8_t id = mp_obj_get_int(args[0]);
    uint16_t x = mp_obj_get_int(args[1]);
    uint16_t y = mp_obj_get_int(args[2]);
    uint16_t w = mp_obj_get_int(args[3]);
    uint16_t h = mp_obj_get_int(args[4]);
    bg_touch_register(id,x,y,w,h);
    return mp_const_none;
}

STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(tulip_bg_touch_register_obj, 5, 5, tulip_bg_touch_register);

STATIC mp_obj_t tulip_bg_touch_del(size_t n_args, const mp_obj_t *args) {
    uint8_t id = mp_obj_get_int(args[0]);
    bg_touch_deregister(id);
    return mp_const_none;
}

STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(tulip_bg_touch_del_obj, 1, 1, tulip_bg_touch_del);


STATIC mp_obj_t tulip_bg_bezier(size_t n_args, const mp_obj_t *args) {
    uint16_t x0 = mp_obj_get_int(args[0]);
    uint16_t y0 = mp_obj_get_int(args[1]);
    uint16_t x1 = mp_obj_get_int(args[2]);
    uint16_t y1 = mp_obj_get_int(args[3]);
    uint16_t x2 = mp_obj_get_int(args[4]);
    uint16_t y2 = mp_obj_get_int(args[5]);
    uint16_t pal_idx = mp_obj_get_int(args[6]);
    plotQuadBezier(x0,y0,x1,y1,x2,y2,pal_idx);
    return mp_const_none;
}

STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(tulip_bg_bezier_obj, 7, 7, tulip_bg_bezier);



STATIC mp_obj_t tulip_bg_line(size_t n_args, const mp_obj_t *args) {
    int16_t x0 = mp_obj_get_int(args[0]);
    int16_t y0 = mp_obj_get_int(args[1]);
    int16_t x1 = mp_obj_get_int(args[2]);
    int16_t y1 = mp_obj_get_int(args[3]);
    uint8_t pal_idx = mp_obj_get_int(args[4]);
    drawLine(x0,y0,x1,y1,pal_idx);
    return mp_const_none;
}

STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(tulip_bg_line_obj, 5, 5, tulip_bg_line);

STATIC mp_obj_t tulip_bg_roundrect(size_t n_args, const mp_obj_t *args) {
    uint16_t x = mp_obj_get_int(args[0]);
    uint16_t y = mp_obj_get_int(args[1]);
    uint16_t w = mp_obj_get_int(args[2]);
    uint16_t h = mp_obj_get_int(args[3]);
    uint16_t r = mp_obj_get_int(args[4]);
    uint16_t pal_idx = mp_obj_get_int(args[5]);
    if(n_args > 6) {
        if(mp_obj_get_int(args[6])>0) {
            fillRoundRect(x,y,w,h,r,pal_idx);
            return mp_const_none;
        }
    }
    drawRoundRect(x,y,w,h,r,pal_idx);
    return mp_const_none;
}

STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(tulip_bg_roundrect_obj, 6, 7, tulip_bg_roundrect);


STATIC mp_obj_t tulip_bg_rect(size_t n_args, const mp_obj_t *args) {
    uint16_t x = mp_obj_get_int(args[0]);
    uint16_t y = mp_obj_get_int(args[1]);
    uint16_t w = mp_obj_get_int(args[2]);
    uint16_t h = mp_obj_get_int(args[3]);
    uint16_t pal_idx = mp_obj_get_int(args[4]);
    if(n_args > 5) {
        if(mp_obj_get_int(args[5])>0) {
            fillRect(x,y,w,h,pal_idx);
            return mp_const_none;
        } 
    }
    drawRect(x,y,w,h,pal_idx);
    return mp_const_none;
}

STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(tulip_bg_rect_obj, 5, 6, tulip_bg_rect);

STATIC mp_obj_t tulip_bg_circle(size_t n_args, const mp_obj_t *args) {
    uint16_t x = mp_obj_get_int(args[0]);
    uint16_t y = mp_obj_get_int(args[1]);
    uint16_t r = mp_obj_get_int(args[2]);
    uint16_t pal_idx = mp_obj_get_int(args[3]);
    if(n_args > 4) {
        if(mp_obj_get_int(args[4])>0) {
            fillCircle(x,y,r,pal_idx);
            return mp_const_none;
        } 
    }
    drawCircle(x,y,r,pal_idx);
    return mp_const_none;
}

STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(tulip_bg_circle_obj, 4, 5, tulip_bg_circle);


STATIC mp_obj_t tulip_bg_triangle(size_t n_args, const mp_obj_t *args) {
    uint16_t x0 = mp_obj_get_int(args[0]);
    uint16_t y0 = mp_obj_get_int(args[1]);
    uint16_t x1 = mp_obj_get_int(args[2]);
    uint16_t y1 = mp_obj_get_int(args[3]);
    uint16_t x2 = mp_obj_get_int(args[4]);
    uint16_t y2 = mp_obj_get_int(args[5]);
    uint16_t pal_idx = mp_obj_get_int(args[6]);
    if(n_args > 7) {
        if(mp_obj_get_int(args[7])>0) {
            fillTriangle(x0, y0, x1, y1, x2, y2, pal_idx);
            return mp_const_none;
        }
    }
    drawTriangle(x0, y0, x1, y1, x2, y2, pal_idx);
    return mp_const_none;
}

STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(tulip_bg_triangle_obj, 7, 8, tulip_bg_triangle);

STATIC mp_obj_t tulip_bg_fill(size_t n_args, const mp_obj_t *args) {
    uint16_t x0 = mp_obj_get_int(args[0]);
    uint16_t y0 = mp_obj_get_int(args[1]);
    uint16_t pal_idx = mp_obj_get_int(args[2]);
    fill(x0,y0,pal_idx);
    return mp_const_none;
}

STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(tulip_bg_fill_obj, 3, 3, tulip_bg_fill);


STATIC mp_obj_t tulip_bg_char(size_t n_args, const mp_obj_t *args) {
    uint16_t c = mp_obj_get_int(args[0]);
    uint16_t x = mp_obj_get_int(args[1]);
    uint16_t y = mp_obj_get_int(args[2]);
    uint16_t pal_idx = mp_obj_get_int(args[3]);
    uint16_t font_no = mp_obj_get_int(args[4]);
    return mp_obj_new_int(draw_new_char(c, x, y, pal_idx, font_no));
}

STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(tulip_bg_char_obj, 5, 5, tulip_bg_char);

STATIC mp_obj_t tulip_bg_str(size_t n_args, const mp_obj_t *args) {
    const char *str =  mp_obj_str_get_str(args[0]);
    uint16_t x = mp_obj_get_int(args[1]);
    uint16_t y = mp_obj_get_int(args[2]);
    uint16_t pal_idx = mp_obj_get_int(args[3]);
    uint16_t font_no = mp_obj_get_int(args[4]);
    if(n_args>5) {
        uint16_t w = mp_obj_get_int(args[5]);
        uint16_t h = mp_obj_get_int(args[6]);        
        return mp_obj_new_int(draw_new_str(str, x, y, pal_idx, font_no, w, h, 1));
    }
    return mp_obj_new_int(draw_new_str(str, x, y, pal_idx, font_no, 0, 0, 0));
}

STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(tulip_bg_str_obj, 5, 7, tulip_bg_str);



STATIC mp_obj_t tulip_timing(size_t n_args, const mp_obj_t *args) {
    if(n_args == 0) {
        mp_obj_t tuple[12];
        tuple[0] = mp_obj_new_int(H_RES);
        tuple[1] = mp_obj_new_int(V_RES);
        tuple[2] = mp_obj_new_int(OFFSCREEN_X_PX);
        tuple[3] = mp_obj_new_int(OFFSCREEN_Y_PX);
        tuple[4] = mp_obj_new_int(H_RES_D);
        tuple[5] = mp_obj_new_int(V_RES_D);
#ifdef ESP_PLATFORM
        tuple[6] = mp_obj_new_int(HSYNC_BACK_PORCH);
        tuple[7] = mp_obj_new_int(HSYNC_FRONT_PORCH);
        tuple[8] = mp_obj_new_int(HSYNC_PULSE_WIDTH);
        tuple[9] = mp_obj_new_int(VSYNC_BACK_PORCH);
        tuple[10] = mp_obj_new_int(VSYNC_FRONT_PORCH);
        tuple[11] = mp_obj_new_int(VSYNC_PULSE_WIDTH);
#else
        tuple[6] = mp_obj_new_int(-1);
        tuple[7] = mp_obj_new_int(-1);
        tuple[8] = mp_obj_new_int(-1);
        tuple[9] = mp_obj_new_int(-1);
        tuple[10] = mp_obj_new_int(-1);
        tuple[11] = mp_obj_new_int(-1);
#endif
        return mp_obj_new_tuple(12, tuple);
    }

#ifdef ESP_PLATFORM
        esp32s3_display_timings(mp_obj_get_int(args[0]), mp_obj_get_int(args[1]), mp_obj_get_int(args[2]), mp_obj_get_int(args[3]), mp_obj_get_int(args[4]), \
                    mp_obj_get_int(args[5]), mp_obj_get_int(args[6]), mp_obj_get_int(args[7]), mp_obj_get_int(args[8]), mp_obj_get_int(args[9]), mp_obj_get_int(args[10]), mp_obj_get_int(args[11]));
#else
        unix_display_timings(mp_obj_get_int(args[0]), mp_obj_get_int(args[1]), mp_obj_get_int(args[2]), mp_obj_get_int(args[3]), mp_obj_get_int(args[4]), mp_obj_get_int(args[5]));
#endif
    return mp_const_none;
}

STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(tulip_timing_obj, 0, 12, tulip_timing);




STATIC mp_obj_t tulip_window(size_t n_args, const mp_obj_t *args) {
    if(n_args == 0) {
        mp_obj_t tuple[2];
        tuple[0] = mp_obj_new_int(H_RES_D);
        tuple[1] = mp_obj_new_int(V_RES_D);
        return mp_obj_new_tuple(2, tuple);
    }

#ifdef ESP_PLATFORM
        esp32s3_display_timings(H_RES, V_RES, OFFSCREEN_X_PX, OFFSCREEN_Y_PX, mp_obj_get_int(args[0]), mp_obj_get_int(args[1]), \
            HSYNC_BACK_PORCH, HSYNC_FRONT_PORCH, HSYNC_PULSE_WIDTH, VSYNC_BACK_PORCH, VSYNC_FRONT_PORCH, VSYNC_PULSE_WIDTH);

#else
        unix_display_timings(H_RES, V_RES, OFFSCREEN_X_PX, OFFSCREEN_Y_PX, mp_obj_get_int(args[0]), mp_obj_get_int(args[1]));
#endif
    return mp_const_none;
}

STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(tulip_window_obj, 0, 2, tulip_window);


//tulip.button(id, "text", x,y,w,h,r,fg_color,btn_color,filled)
STATIC mp_obj_t tulip_ui_button(size_t n_args, const mp_obj_t *args) {
    uint8_t ui_id = mp_obj_get_int(args[0]);
    ui_button_new(ui_id, 
            mp_obj_str_get_str(args[1]), // text
            mp_obj_get_int(args[2]), // x
            mp_obj_get_int(args[3]), // y
            mp_obj_get_int(args[4]), // w
            mp_obj_get_int(args[5]), // h
            mp_obj_get_int(args[6]), // fg_color
            mp_obj_get_int(args[7]), // btn_color
            mp_obj_get_int(args[8]), // filled 
            mp_obj_get_int(args[9]) // font 

            );
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(tulip_ui_button_obj, 10, 10, tulip_ui_button);


// style == 0 - filled, 1 - x, 2 - circle
//tulip.checkbox(id,val,x,y,w,mark_color,box_color, style)
// val = tulip.checkbox(id)
// tulip.checkbox(id, val)
STATIC mp_obj_t tulip_ui_checkbox(size_t n_args, const mp_obj_t *args) {
    uint8_t ui_id = mp_obj_get_int(args[0]);
    if(n_args == 1) { 
        uint8_t c = ui_check_get_val(ui_id);
        return mp_obj_new_int(c);
    } else if(n_args==2) {
        uint8_t v = mp_obj_get_int(args[1]);
        ui_check_set_val(ui_id, v);
    } else {
        ui_check_new(ui_id, 
            mp_obj_get_int(args[1]), // val
            mp_obj_get_int(args[2]), // x
            mp_obj_get_int(args[3]), // y
            mp_obj_get_int(args[4]), // w
            mp_obj_get_int(args[5]), // mark_color
            mp_obj_get_int(args[6]), // box_color
            mp_obj_get_int(args[7]) // style
            );
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(tulip_ui_checkbox_obj, 0, 8, tulip_ui_checkbox);



//tulip.text(id, "text", x,y,w,h,r,text_color,box_color)
STATIC mp_obj_t tulip_ui_text(size_t n_args, const mp_obj_t *args) {
    uint8_t ui_id = mp_obj_get_int(args[0]);
    if(n_args < 8) { 
        char * t = ui_text_get_val(ui_id);
        return mp_obj_new_str(t, strlen(t));
    } else {
        ui_text_new(ui_id, 
                mp_obj_str_get_str(args[1]), // text
                mp_obj_get_int(args[2]), // x
                mp_obj_get_int(args[3]), // y
                mp_obj_get_int(args[4]), // w
                mp_obj_get_int(args[5]), // h
                mp_obj_get_int(args[6]), // text_color
                mp_obj_get_int(args[7]), // box_color
                mp_obj_get_int(args[8]) // font_no
                );
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(tulip_ui_text_obj, 1, 9, tulip_ui_text);


STATIC mp_obj_t tulip_ui_slider(size_t n_args, const mp_obj_t *args) {
    uint8_t ui_id = mp_obj_get_int(args[0]);
    if(n_args == 1) { 
        return mp_obj_new_float(ui_slider_get_val(ui_id));
    } else if(n_args == 2) {
        ui_slider_set_val(ui_id,mp_obj_get_float(args[1]) );
    } else {
        ui_slider_new(ui_id, 
            mp_obj_get_float(args[1]), // val        
            mp_obj_get_int(args[2]), // x
            mp_obj_get_int(args[3]), // y
            mp_obj_get_int(args[4]), // w
            mp_obj_get_int(args[5]), // h
            mp_obj_get_int(args[6]), // bar_color
            mp_obj_get_int(args[7]) // handle_color
            );
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(tulip_ui_slider_obj, 1, 8, tulip_ui_slider);


STATIC mp_obj_t tulip_ui_del(size_t n_args, const mp_obj_t *args) {
    uint8_t ui_id = mp_obj_get_int(args[0]);
    ui_element_del(ui_id);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(tulip_ui_del_obj, 1, 1, tulip_ui_del);



STATIC mp_obj_t tulip_ui_active(size_t n_args, const mp_obj_t *args) {
    uint8_t ui_id = mp_obj_get_int(args[0]);
    uint8_t active = mp_obj_get_int(args[1]);
    ui_element_active(ui_id, active);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(tulip_ui_active_obj, 2, 2, tulip_ui_active);


STATIC mp_obj_t tulip_build_strings(size_t n_args, const mp_obj_t *args) {
    mp_obj_t tuple[3];
    tuple[0] = mp_obj_new_str(MICROPY_GIT_TAG, strlen(MICROPY_GIT_TAG));
    tuple[1] = mp_obj_new_str(MICROPY_GIT_HASH, strlen(MICROPY_GIT_HASH));
    tuple[2] = mp_obj_new_str(MICROPY_BUILD_DATE, strlen(MICROPY_BUILD_DATE));
    return mp_obj_new_tuple(3, tuple);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(tulip_build_strings_obj, 0,0, tulip_build_strings);


STATIC const mp_rom_map_elem_t tulip_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR__tulip) },
    { MP_ROM_QSTR(MP_QSTR_display_clock), MP_ROM_PTR(&tulip_display_clock_obj) },
    { MP_ROM_QSTR(MP_QSTR_display_restart), MP_ROM_PTR(&tulip_display_restart_obj) },
    { MP_ROM_QSTR(MP_QSTR_display_stop), MP_ROM_PTR(&tulip_display_stop_obj) },
    { MP_ROM_QSTR(MP_QSTR_display_start), MP_ROM_PTR(&tulip_display_start_obj) },
    { MP_ROM_QSTR(MP_QSTR_bg_swap), MP_ROM_PTR(&tulip_bg_swap_obj) },
    { MP_ROM_QSTR(MP_QSTR_tfb_stop), MP_ROM_PTR(&tulip_tfb_stop_obj) },
    { MP_ROM_QSTR(MP_QSTR_tfb_log_start), MP_ROM_PTR(&tulip_tfb_log_start_obj) },
    { MP_ROM_QSTR(MP_QSTR_tfb_log_stop), MP_ROM_PTR(&tulip_tfb_log_stop_obj) },
    { MP_ROM_QSTR(MP_QSTR_tfb_start), MP_ROM_PTR(&tulip_tfb_start_obj) },
    { MP_ROM_QSTR(MP_QSTR_tfb_save), MP_ROM_PTR(&tulip_tfb_save_obj) },
    { MP_ROM_QSTR(MP_QSTR_tfb_restore), MP_ROM_PTR(&tulip_tfb_restore_obj) },
    { MP_ROM_QSTR(MP_QSTR_fps), MP_ROM_PTR(&tulip_fps_obj) },
    { MP_ROM_QSTR(MP_QSTR_gpu), MP_ROM_PTR(&tulip_gpu_obj) },
    { MP_ROM_QSTR(MP_QSTR_ticks_ms), MP_ROM_PTR(&tulip_ticks_ms_obj) },
    { MP_ROM_QSTR(MP_QSTR_bg_pixel), MP_ROM_PTR(&tulip_bg_pixel_obj) },
    { MP_ROM_QSTR(MP_QSTR_bg_png), MP_ROM_PTR(&tulip_bg_png_obj) },
    { MP_ROM_QSTR(MP_QSTR_bg_clear), MP_ROM_PTR(&tulip_bg_clear_obj) },
    { MP_ROM_QSTR(MP_QSTR_bg_scroll), MP_ROM_PTR(&tulip_bg_scroll_obj) },
    { MP_ROM_QSTR(MP_QSTR_bg_scroll_x_speed), MP_ROM_PTR(&tulip_bg_scroll_x_speed_obj) },
    { MP_ROM_QSTR(MP_QSTR_bg_scroll_y_speed), MP_ROM_PTR(&tulip_bg_scroll_y_speed_obj) },
    { MP_ROM_QSTR(MP_QSTR_bg_scroll_x_offset), MP_ROM_PTR(&tulip_bg_scroll_x_offset_obj) },
    { MP_ROM_QSTR(MP_QSTR_bg_scroll_y_offset), MP_ROM_PTR(&tulip_bg_scroll_y_offset_obj) },
    { MP_ROM_QSTR(MP_QSTR_tfb_str), MP_ROM_PTR(&tulip_tfb_str_obj) },
    { MP_ROM_QSTR(MP_QSTR_frame_callback), MP_ROM_PTR(&tulip_frame_callback_obj) },
    { MP_ROM_QSTR(MP_QSTR_midi_callback), MP_ROM_PTR(&tulip_midi_callback_obj) },
    { MP_ROM_QSTR(MP_QSTR_ui_callback), MP_ROM_PTR(&tulip_ui_callback_obj) },
    { MP_ROM_QSTR(MP_QSTR_touch_callback), MP_ROM_PTR(&tulip_touch_callback_obj) },
    { MP_ROM_QSTR(MP_QSTR_bg_touch_callback), MP_ROM_PTR(&tulip_bg_touch_callback_obj) },
    { MP_ROM_QSTR(MP_QSTR_midi_in), MP_ROM_PTR(&tulip_midi_in_obj) },
    { MP_ROM_QSTR(MP_QSTR_midi_out), MP_ROM_PTR(&tulip_midi_out_obj) },
    { MP_ROM_QSTR(MP_QSTR_bg_bitmap), MP_ROM_PTR(&tulip_bg_bitmap_obj) },
    { MP_ROM_QSTR(MP_QSTR_bg_blit), MP_ROM_PTR(&tulip_bg_blit_obj) },
    { MP_ROM_QSTR(MP_QSTR_sprite_png), MP_ROM_PTR(&tulip_sprite_png_obj) },
    { MP_ROM_QSTR(MP_QSTR_sprite_bitmap), MP_ROM_PTR(&tulip_sprite_bitmap_obj) },
    { MP_ROM_QSTR(MP_QSTR_sprite_register), MP_ROM_PTR(&tulip_sprite_register_obj) },
    { MP_ROM_QSTR(MP_QSTR_sprite_move), MP_ROM_PTR(&tulip_sprite_move_obj) },
    { MP_ROM_QSTR(MP_QSTR_sprite_on), MP_ROM_PTR(&tulip_sprite_on_obj) },
    { MP_ROM_QSTR(MP_QSTR_sprite_off), MP_ROM_PTR(&tulip_sprite_off_obj) },
    { MP_ROM_QSTR(MP_QSTR_sprite_clear), MP_ROM_PTR(&tulip_sprite_clear_obj) },
    { MP_ROM_QSTR(MP_QSTR_collisions), MP_ROM_PTR(&tulip_collisions_obj) },
    { MP_ROM_QSTR(MP_QSTR_edit), MP_ROM_PTR(&tulip_edit_obj) },
    { MP_ROM_QSTR(MP_QSTR_int_screenshot), MP_ROM_PTR(&tulip_int_screenshot_obj) },
    { MP_ROM_QSTR(MP_QSTR_multicast_start), MP_ROM_PTR(&tulip_multicast_start_obj) },
    { MP_ROM_QSTR(MP_QSTR_alles_send), MP_ROM_PTR(&tulip_alles_send_obj) },
    { MP_ROM_QSTR(MP_QSTR_brightness), MP_ROM_PTR(&tulip_brightness_obj) },
    { MP_ROM_QSTR(MP_QSTR_keys), MP_ROM_PTR(&tulip_keys_obj) },
    { MP_ROM_QSTR(MP_QSTR_touch), MP_ROM_PTR(&tulip_touch_obj) },
    { MP_ROM_QSTR(MP_QSTR_touch_delta), MP_ROM_PTR(&tulip_touch_delta_obj) },
    { MP_ROM_QSTR(MP_QSTR_key_wait), MP_ROM_PTR(&tulip_key_wait_obj) },
    { MP_ROM_QSTR(MP_QSTR_key), MP_ROM_PTR(&tulip_key_obj) },
    { MP_ROM_QSTR(MP_QSTR_key_scan), MP_ROM_PTR(&tulip_key_scan_obj) },
    { MP_ROM_QSTR(MP_QSTR_cpu), MP_ROM_PTR(&tulip_cpu_obj) },
    { MP_ROM_QSTR(MP_QSTR_gpu_reset), MP_ROM_PTR(&tulip_gpu_reset_obj) },
    { MP_ROM_QSTR(MP_QSTR_bg_touch_register), MP_ROM_PTR(&tulip_bg_touch_register_obj) },
    { MP_ROM_QSTR(MP_QSTR_bg_touch_up), MP_ROM_PTR(&tulip_bg_touch_up_obj) },
    { MP_ROM_QSTR(MP_QSTR_bg_touch_del), MP_ROM_PTR(&tulip_bg_touch_del_obj) },
    { MP_ROM_QSTR(MP_QSTR_bg_circle), MP_ROM_PTR(&tulip_bg_circle_obj) },
    { MP_ROM_QSTR(MP_QSTR_bg_bezier), MP_ROM_PTR(&tulip_bg_bezier_obj) },
    { MP_ROM_QSTR(MP_QSTR_bg_line), MP_ROM_PTR(&tulip_bg_line_obj) },
    { MP_ROM_QSTR(MP_QSTR_bg_roundrect), MP_ROM_PTR(&tulip_bg_roundrect_obj) },
    { MP_ROM_QSTR(MP_QSTR_bg_triangle), MP_ROM_PTR(&tulip_bg_triangle_obj) },
    { MP_ROM_QSTR(MP_QSTR_bg_fill), MP_ROM_PTR(&tulip_bg_fill_obj) },
    { MP_ROM_QSTR(MP_QSTR_bg_rect), MP_ROM_PTR(&tulip_bg_rect_obj) },
    { MP_ROM_QSTR(MP_QSTR_bg_char), MP_ROM_PTR(&tulip_bg_char_obj) },
    { MP_ROM_QSTR(MP_QSTR_bg_str), MP_ROM_PTR(&tulip_bg_str_obj) },
    { MP_ROM_QSTR(MP_QSTR_timing), MP_ROM_PTR(&tulip_timing_obj) },
    { MP_ROM_QSTR(MP_QSTR_window), MP_ROM_PTR(&tulip_window_obj) },
    { MP_ROM_QSTR(MP_QSTR_gpu_log_start), MP_ROM_PTR(&tulip_gpu_log_start_obj) },
    { MP_ROM_QSTR(MP_QSTR_gpu_log_stop), MP_ROM_PTR(&tulip_gpu_log_stop_obj) },
    { MP_ROM_QSTR(MP_QSTR_ui_button), MP_ROM_PTR(&tulip_ui_button_obj) },
    { MP_ROM_QSTR(MP_QSTR_ui_text), MP_ROM_PTR(&tulip_ui_text_obj) },
    { MP_ROM_QSTR(MP_QSTR_ui_slider), MP_ROM_PTR(&tulip_ui_slider_obj) },
    { MP_ROM_QSTR(MP_QSTR_ui_checkbox), MP_ROM_PTR(&tulip_ui_checkbox_obj) },
    { MP_ROM_QSTR(MP_QSTR_ui_del), MP_ROM_PTR(&tulip_ui_del_obj) },
    { MP_ROM_QSTR(MP_QSTR_ui_active), MP_ROM_PTR(&tulip_ui_active_obj) },
    { MP_ROM_QSTR(MP_QSTR_joy), MP_ROM_PTR(&tulip_joy_obj) },
    { MP_ROM_QSTR(MP_QSTR_build_strings), MP_ROM_PTR(&tulip_build_strings_obj) },


// Special platform specific things
#ifndef ESP_PLATFORM
    { MP_ROM_QSTR(MP_QSTR_app_path), MP_ROM_PTR(&tulip_app_path_obj) },
#endif

};

STATIC MP_DEFINE_CONST_DICT(tulip_module_globals, tulip_module_globals_table);

const mp_obj_module_t tulip_module = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&tulip_module_globals,
};


MP_REGISTER_MODULE(MP_QSTR__tulip, tulip_module); // MICROPY_PY_TULIP);
