// usb_keyboard.c
#include "tdeck_keyboard.h"
#include <stdio.h>
#include "driver/i2c.h"
#include "esp_log.h"
#include "string.h"
#include "display.h"
#include "soc/io_mux_reg.h"
#include "ui.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "usb_keyboard.h"
#include <ctype.h>

#define TIMEOUT_MS    10

int state = 0;
QueueHandle_t interruptQueue;
struct KeyMapping {
    // The following characters should be available to map to an alternative in
    // all the structs below with the exception of altMappings. The numbers 0-9
    // are unavailable within altMappings due to the alt keycodes.
    // Default: q w e r t y u i o p a s d f g h j k l z x c v b n m $ <space>
    // Shift: Q W E R T Y U I O P A S D F G H J K L Z X C V B N M
    // Symbol: # 1 2 3 ( ) _ - + @ * 4 5 6 / : ; \ " 7 8 9 ? ! , . 0
    char original;
    int alternative;
};

static void IRAM_ATTR gpio_interrupt_handler(void *args)
{
    int pinNumber = (int)args;
    xQueueSendFromISR(interruptQueue, &pinNumber, NULL);
}

// Send trackpall spins as arrow keys.
// The push current sends "enter"
void watch_trackball(void *params)
{
    uint8_t raw_data[40];
    for(uint8_t i=0;i<40;i++) raw_data[i] = 0;
    int pinNumber = 0;
    while (true)
    {
        if (xQueueReceive(interruptQueue, &pinNumber, portMAX_DELAY))
        {
            if(pinNumber == TDECK_TRACKBALL_UP) send_key_to_micropython(TDECK_UP_KEY);
            if(pinNumber == TDECK_TRACKBALL_DOWN) send_key_to_micropython(TDECK_DOWN_KEY);
            if(pinNumber == TDECK_TRACKBALL_LEFT) send_key_to_micropython(TDECK_LEFT_KEY);
            if(pinNumber == TDECK_TRACKBALL_RIGHT) send_key_to_micropython(TDECK_RIGHT_KEY);
            if(pinNumber == TDECK_TRACKBALL_CLICK) send_key_to_micropython(CARRIAGE_RETURN);
            //fprintf(stderr,"GPIO %d was pressed. The state is %d\n", pinNumber, gpio_get_level(pinNumber));
        }
    }
}

int get_alternative_char(struct KeyMapping mappings[], int size, char original) {
    for (int i = 0; i < size; ++i) {
        if (mappings[i].original == original) {
            return mappings[i].alternative;
        }
    }
    // Return the original character if no alternative is found
    return original;
}

void run_tdeck_keyboard() {

    bool alt_char_mode = false;
    bool ctrl_toggle = false;
    bool alt_toggle = false;
    uint8_t rx_data[5];
    uint8_t char_to_send[1];
    struct KeyMapping charMappings[] = {
        // Default keys
        {'q', '~'},
        {'w', '&'},
        {'e', '|'},
        {'r', '%'},
        {'t', '{'},
        {'y', '}'},
        {'u', '^'},
        {'i', '<'},
        {'o', '>'},
        {'p', '='},
        {'a', 172},     // Prints nothing but should be a ¬ sign
        {'g', '\\'},    // Backslash needs escaping
        {'k', '`'},
        {'$', 163},     // Prints nothing but should be a £ sign
        {' ', '\t'},    // Sends tab
        // Requires symbol to be pressed after alt+c
        {'(', '['},
        {')', ']'},
    };
    struct KeyMapping ctrlMappings[] = {
        // Default keys
        {'q', DEVICE_CONTROL_1},
        {'w', END_OF_TRANSMISSION_BLOCK},
        {'e', ENQUIRY_CHARACTER},
        {'r', DEVICE_CONTROL_2},
        {'t', DEVICE_CONTROL_4},
        {'y', END_OF_MEDIUM},
        {'u', NEGATIVE_ACKNOWLEDGE_CHARACTER},
        {'i', HORIZONTAL_TAB},
        {'o', SHIFT_IN},
        {'p', DATA_LINK_ESCAPE},
        {'a', START_OF_HEADING},
        {'s', DEVICE_CONTROL_3},
        {'d', END_OF_TRANSMISSION_CHARACTER},
        {'f', ACKNOWLEDGE_CHARACTER},
        {'g', BELL_CHARACTER},
        {'h', BACKSPACE_CHARACTER},
        {'j', LINEFEED_CHARACTER},
        {'k', VERTICAL_TAB},
        {'l', FORMFEED},
        {'z', SUBSTITUTE_CHARACTER},
        {'x', CANCEL_CHARACTER},
        {'c', END_OF_TEXT_CHARACTER},
        {'v', SYNCHRONOUS_IDLE},
        {'b', START_OF_TEXT},
        {'n', SHIFT_OUT},
        {'m', CARRIAGE_RETURN},
        {'$', DELETE_CHARACTER},
        {' ', ESCAPE_CHARACTER},
    };
    struct KeyMapping altMappings[] = {
        // Default keys
        {'e', TDECK_UP_KEY},
        {'x', TDECK_DOWN_KEY},
        {'s', TDECK_LEFT_KEY},
        {'f', TDECK_RIGHT_KEY},
    };
    int charMappingsSize = sizeof(charMappings) / sizeof(charMappings[0]);
    int ctrlMappingsSize = sizeof(ctrlMappings) / sizeof(ctrlMappings[0]);
    int altMappingsSize = sizeof(altMappings) / sizeof(altMappings[0]);
    int alt_number_list[5];
    int alt_number_index = 0;
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = TDECK_I2C_SDA,
        .scl_io_num = TDECK_I2C_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 400000,
    };
    i2c_param_config(I2C_NUM_0, &conf);

    ESP_ERROR_CHECK(i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0));

    gpio_set_direction(TDECK_TRACKBALL_UP, GPIO_MODE_INPUT);
    gpio_pulldown_dis(TDECK_TRACKBALL_UP);
    gpio_pullup_en(TDECK_TRACKBALL_UP);
    gpio_set_intr_type(TDECK_TRACKBALL_UP, GPIO_INTR_NEGEDGE);

    gpio_set_direction(TDECK_TRACKBALL_DOWN, GPIO_MODE_INPUT);
    gpio_pulldown_dis(TDECK_TRACKBALL_DOWN);
    gpio_pullup_en(TDECK_TRACKBALL_DOWN);
    gpio_set_intr_type(TDECK_TRACKBALL_DOWN, GPIO_INTR_NEGEDGE);

    gpio_set_direction(TDECK_TRACKBALL_LEFT, GPIO_MODE_INPUT);
    gpio_pulldown_dis(TDECK_TRACKBALL_LEFT);
    gpio_pullup_en(TDECK_TRACKBALL_LEFT);
    gpio_set_intr_type(TDECK_TRACKBALL_LEFT, GPIO_INTR_NEGEDGE);

    gpio_set_direction(TDECK_TRACKBALL_RIGHT, GPIO_MODE_INPUT);
    gpio_pulldown_dis(TDECK_TRACKBALL_RIGHT);
    gpio_pullup_en(TDECK_TRACKBALL_RIGHT);
    gpio_set_intr_type(TDECK_TRACKBALL_RIGHT, GPIO_INTR_NEGEDGE);

    gpio_set_direction(TDECK_TRACKBALL_CLICK, GPIO_MODE_INPUT);
    gpio_pulldown_dis(TDECK_TRACKBALL_CLICK);
    gpio_pullup_en(TDECK_TRACKBALL_CLICK);
    gpio_set_intr_type(TDECK_TRACKBALL_CLICK, GPIO_INTR_NEGEDGE);

    interruptQueue = xQueueCreate(10, sizeof(int));
    xTaskCreate(watch_trackball, "watch_trackball", 4096, NULL, 1, NULL);

    gpio_isr_handler_add(TDECK_TRACKBALL_UP, gpio_interrupt_handler, (void *)TDECK_TRACKBALL_UP);
    gpio_isr_handler_add(TDECK_TRACKBALL_DOWN, gpio_interrupt_handler, (void *)TDECK_TRACKBALL_DOWN);
    gpio_isr_handler_add(TDECK_TRACKBALL_LEFT, gpio_interrupt_handler, (void *)TDECK_TRACKBALL_LEFT);
    gpio_isr_handler_add(TDECK_TRACKBALL_RIGHT, gpio_interrupt_handler, (void *)TDECK_TRACKBALL_RIGHT);
    gpio_isr_handler_add(TDECK_TRACKBALL_CLICK, gpio_interrupt_handler, (void *)TDECK_TRACKBALL_CLICK);

    while (1) {
        i2c_master_read_from_device(I2C_NUM_0, LILYGO_KB_SLAVE_ADDRESS, rx_data, 1, pdMS_TO_TICKS(TIMEOUT_MS));
        if (rx_data[0] > 0) {
            // Toggle ctrl key (shift+microphone or shift-0)
            if (rx_data[0] == SHIFT_MICROPHONE) {
                ctrl_toggle = !ctrl_toggle;
            // Toggle alt key (shift+$ or shift-speaker)
            } else if (rx_data[0] == END_OF_TRANSMISSION_CHARACTER) {
                alt_toggle = !alt_toggle;
            // Set alternate character mode (alt+c), this is the key with `alt` printed on it that isnt actually an alt key
            } else if (rx_data[0] == FORMFEED) {
                alt_char_mode = !alt_char_mode;
            } else {
                // Send alternate characters if alternate character set is enabled, otherwise send as is
                if (alt_char_mode) {
                    char_to_send[0] = get_alternative_char(charMappings, charMappingsSize, rx_data[0]);
                    alt_char_mode = false;
                } else {
                    char_to_send[0] = rx_data[0];
                }
                // Send as is, combining with ctrl or alt if toggled
                if (ctrl_toggle) {
                    send_key_to_micropython(get_alternative_char(ctrlMappings, ctrlMappingsSize, char_to_send[0]));
                    ctrl_toggle = false;  // Reset toggle after sending
                } else if (alt_toggle) {
                    if (isdigit(rx_data[0])) {
                        // Store numbers in the list for alt key
                        alt_number_list[alt_number_index++] = rx_data[0] - '0';
                    } else if (rx_data[0] == CARRIAGE_RETURN) {
                        // Send the combined number when carriage return is encountered
                        int combined_number = 0;
                        for (int i = 0; i < alt_number_index; ++i) {
                            combined_number = combined_number * 10 + alt_number_list[i];
                        }
                        send_key_to_micropython(combined_number);
                        alt_toggle = false;   // Reset toggle after sending
                        alt_number_index = 0; // Reset the index
                    } else {
                        send_key_to_micropython(get_alternative_char(altMappings, altMappingsSize, char_to_send[0]));
                        alt_toggle = false;   // Reset toggle after sending
                        alt_number_index = 0; // Reset the index
                    }
                } else {
                    send_key_to_micropython(char_to_send[0]);
                }
            }
        }
    }
}