#ifdef ESP_PLATFORM
#include "esp_task.h"
#ifndef TASKS_H
#define TASKS_H
#include "alles.h"

// All of the ESP tasks that we create in one place

#define DISPLAY_TASK_PRIORITY (ESP_TASK_PRIO_MIN + 5)
#define USB_TASK_PRIORITY (ESP_TASK_PRIO_MIN + 1)
#define TOUCHSCREEN_TASK_PRIORITY (ESP_TASK_PRIO_MIN + 1)
#define TULIP_MP_TASK_PRIORITY (ESP_TASK_PRIO_MIN + 1)
#define MIDI_TASK_PRIORITY (ESP_TASK_PRIO_MIN + 2)
#define ALLES_TASK_PRIORITY (ESP_TASK_PRIO_MIN + 2)
#define ALLES_PARSE_TASK_PRIORITY (ESP_TASK_PRIO_MIN +2)
#define ALLES_RECEIVE_TASK_PRIORITY (ESP_TASK_PRIO_MIN + 3)
#define ALLES_RENDER_TASK_PRIORITY (ESP_TASK_PRIO_MIN + 1)
#define ALLES_FILL_BUFFER_TASK_PRIORITY (ESP_TASK_PRIO_MIN + 4)

#define DISPLAY_TASK_COREID (0)
#define USB_TASK_COREID (1)
#define TOUCHSCREEN_TASK_COREID  (0)
#define TULIP_MP_TASK_COREID (1)
#define MIDI_TASK_COREID (0)
#define ALLES_TASK_COREID (1)
#define ALLES_PARSE_TASK_COREID (0)
#define ALLES_RECEIVE_TASK_COREID (1)
#define ALLES_RENDER_TASK_COREID (1)
#define ALLES_FILL_BUFFER_TASK_COREID (1)

#define DISPLAY_TASK_STACK_SIZE    (4 * 1024) 
#define USB_TASK_STACK_SIZE    (2 * 1024) 
#define TOUCHSCREEN_TASK_STACK_SIZE (4 * 1024)
#define TULIP_MP_TASK_STACK_SIZE      (16 * 1024)
#define MIDI_TASK_STACK_SIZE (4 * 1024)
#define ALLES_TASK_STACK_SIZE    (2 * 1024) 
#define ALLES_PARSE_TASK_STACK_SIZE (4 * 1024)
#define ALLES_RECEIVE_TASK_STACK_SIZE (2 * 1024)
#define ALLES_RENDER_TASK_STACK_SIZE (8 * 1024)
#define ALLES_FILL_BUFFER_TASK_STACK_SIZE (4 * 1024)

#define DISPLAY_TASK_NAME "display_task"
#define USB_TASK_NAME  "usb_task"
#define TOUCHSCREEN_TASK_NAME "touchscreen_task"
#define TULIP_MP_TASK_NAME "tulip_mp_task"
#define MIDI_TASK_NAME "midi_task"
#define ALLES_TASK_NAME  "alles_task"
#define ALLES_PARSE_TASK_NAME "alles_parse_task"
#define ALLES_RECEIVE_TASK_NAME "alles_receive_task"
#define ALLES_RENDER_TASK_NAME "alles_render_task"
#define ALLES_FILL_BUFFER_TASK_NAME "alles_fill_buffer_task"

#define MAX_TASKS 22 // includes system tasks

TaskHandle_t display_handle;
TaskHandle_t usb_handle;
TaskHandle_t touchscreen_handle;
TaskHandle_t tulip_mp_handle;
TaskHandle_t midi_handle;
TaskHandle_t alles_handle;
TaskHandle_t alles_parse_handle;
TaskHandle_t alles_receive_handle;
TaskHandle_t alles_render_handle;
TaskHandle_t alles_fill_buffer_handle;
TaskHandle_t idle_0_handle;
TaskHandle_t idle_1_handle;

// For CPU usage
unsigned long last_task_counters[MAX_TASKS];
#endif
#endif
