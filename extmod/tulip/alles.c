// Alles multicast synthesizer
// Brian Whitman
// brian@variogr.am
#include "alles.h"

uint8_t board_level;
uint8_t status;


#ifdef ESP_PLATFORM
// mutex that locks writes to the delta queue
SemaphoreHandle_t xQueueSemaphore;

#else
extern int get_first_ip_address(char*);
#endif

uint8_t board_level = ALLES_BOARD_V2;
uint8_t status = RUNNING;

char githash[8];

// AMY synth states
extern struct state global;
extern uint32_t event_counter;
extern uint32_t message_counter;
extern void mcast_send(char*, uint16_t);

uint8_t debug_on = 0;
char raw_file[1] = "";
char * alles_local_ip;


#ifdef ESP_PLATFORM

extern void mcast_listen_task(void *pvParameters);

// Wrap AMY's renderer into 2 FreeRTOS tasks, one per core
void esp_render_task( void * pvParameters) {
    uint8_t which = *((uint8_t *)pvParameters);
    uint8_t start = 0; 
    uint8_t end = OSCS;
    if (AMY_CORES == 2) {
        start = (OSCS/2); 
        end = OSCS;
        if(which == 0) { start = 0; end = (OSCS/2); } 
    }
    fprintf(stderr,"I'm renderer #%d on core #%d and i'm handling oscs %d up until %d\n", which, xPortGetCoreID(), start, end);
    while(1) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        render_task(start, end, which);
        xTaskNotifyGive(alles_fill_buffer_handle);
    }
}


// Make AMY's FABT run forever , as a FreeRTOS task 
void esp_fill_audio_buffer_task() {
#ifdef WM8960
    int16_t lr_block[BLOCK_SIZE*2];
    while(1) {
        int16_t *block = fill_audio_buffer_task();
        for(uint16_t i=0;i<BLOCK_SIZE;i++) {
            lr_block[i*2+0] = block[i];
            lr_block[i*2+1] = block[i];
        }
        size_t written = 0;
        i2s_write((i2s_port_t)CONFIG_I2S_NUM, lr_block, 2*BLOCK_SIZE * BYTES_PER_SAMPLE, &written, portMAX_DELAY); 
        if(written != 2*BLOCK_SIZE*BYTES_PER_SAMPLE) {
            fprintf(stderr,"i2s underrun: %d vs %d\n", written, BLOCK_SIZE*BYTES_PER_SAMPLE);
        }
    }
#else
    while(1) {
        int16_t *block = fill_audio_buffer_task();
        size_t written = 0;
        i2s_write((i2s_port_t)CONFIG_I2S_NUM, block, BLOCK_SIZE * BYTES_PER_SAMPLE, &written, portMAX_DELAY); 
        if(written != BLOCK_SIZE*BYTES_PER_SAMPLE) {
            fprintf(stderr,"i2s underrun: %d vs %d\n", written, BLOCK_SIZE*BYTES_PER_SAMPLE);
        }
    }
#endif


}
#else
void *mcast_listen_task(void *vargp);
#endif


extern char *message_start_pointer;
extern int16_t message_length;


void alles_send_message(char * message, uint16_t len) {
    message_start_pointer = message;
    message_length = len;
    // tell the parse task, time to parse this message into deltas and add to the queue
    parse_task();
}

#ifdef ESP_PLATFORM
// init AMY from the esp. wraps some amy funcs in a task to do multicore rendering on the ESP32 
amy_err_t esp_amy_init() {
    start_amy();
    // We create a mutex for changing the event queue and pointers as two tasks do it at once
    xQueueSemaphore = xSemaphoreCreateMutex();

    // Create rendering threads, one per core so we can deal with dan ellis float math
    static uint8_t zero = 0;
    xTaskCreatePinnedToCore(&esp_render_task, ALLES_RENDER_TASK_NAME, ALLES_RENDER_TASK_STACK_SIZE, &zero, ALLES_RENDER_TASK_PRIORITY, &alles_render_handle, ALLES_RENDER_TASK_COREID);
    // Wait for the render tasks to get going before starting the i2s task
    delay_ms(100);

    // And the fill audio buffer thread, combines, does volume & filters
    xTaskCreatePinnedToCore(&esp_fill_audio_buffer_task, ALLES_FILL_BUFFER_TASK_NAME, ALLES_FILL_BUFFER_TASK_STACK_SIZE, NULL, ALLES_FILL_BUFFER_TASK_PRIORITY, &alles_fill_buffer_handle, ALLES_FILL_BUFFER_TASK_COREID);
    return AMY_OK;
}
#else

extern void *soundio_run(void *vargp);
#include <pthread.h>
amy_err_t unix_amy_init() {
    //sync_init();
    start_amy();
    pthread_t thread_id;
    pthread_create(&thread_id, NULL, soundio_run, NULL);
    return AMY_OK;
}
#endif


#ifdef ESP_PLATFORM



// Setup I2S
amy_err_t setup_i2s(void) {
    //i2s configuration
    i2s_config_t i2s_config = {
         .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
         .sample_rate = SAMPLE_RATE,
         .bits_per_sample = I2S_SAMPLE_TYPE,
#ifdef WM8960
         .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
#else
         .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT, 
#endif
         //.communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_STAND_I2S),
         .communication_format = I2S_COMM_FORMAT_STAND_MSB,
         .intr_alloc_flags = 0, //ESP_INTR_FLAG_LEVEL1, // high interrupt priority
         .dma_buf_count = 2, //I2S_BUFFERS,
         .dma_buf_len = 1024, //BLOCK_SIZE * BYTES_PER_SAMPLE,
         .tx_desc_auto_clear = true,
        };
        
    i2s_pin_config_t pin_config = {
        .bck_io_num = CONFIG_I2S_BCLK, 
        .ws_io_num = CONFIG_I2S_LRCLK,  
        .data_out_num = CONFIG_I2S_DIN, 
        .data_in_num = -1   //Not used
    };
    //SET_PERI_REG_BITS(I2S_TX_TIMING_REG(0), 0x1, 1, I2S_TX_DSYNC_SW_S);

    i2s_driver_install((i2s_port_t)CONFIG_I2S_NUM, &i2s_config, 0, NULL);
    i2s_set_pin((i2s_port_t)CONFIG_I2S_NUM, &pin_config);
    i2s_set_sample_rates((i2s_port_t)CONFIG_I2S_NUM, SAMPLE_RATE);
    return AMY_OK;
}

#endif




#ifdef ESP_PLATFORM
void run_alles() {
    check_init(&esp_event_loop_create_default, "Event");
    check_init(&setup_i2s, "i2s");
    esp_amy_init();
    reset_oscs();
    // Schedule a "turning on" sound
    bleep();

    // Spin this core until the power off button is pressed, parsing events and making sounds
    while(status & RUNNING) {
        delay_ms(10);
    }
}

#else

void * alles_start(void *vargs) {
    alles_local_ip = malloc(256);
    alles_local_ip[0] = 0;
    unix_amy_init();
    reset_oscs();
    // Schedule a "turning on" sound
    bleep();
    // Spin this core until the power off button is pressed, parsing events and making sounds
    while(status & RUNNING) {
        delay_ms(10);
    }
    return 0;
}

#endif

#ifdef ESP_PLATFORM
// Make AMY's parse task run forever, as a FreeRTOS task (with notifications)
void esp_parse_task() {
    while(1) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        parse_task();
        xTaskNotifyGive(alles_receive_handle);
    }
}

#include "esp_wifi.h"
#endif

void alles_init_multicast() {
#ifdef ESP_PLATFORM
    fprintf(stderr, "creating socket\n");
    create_multicast_ipv4_socket();
    fprintf(stderr, "power save off\n");
    esp_wifi_set_ps(WIFI_PS_NONE);
    fprintf(stderr, "creating mcast task\n");
    xTaskCreatePinnedToCore(&mcast_listen_task, ALLES_RECEIVE_TASK_NAME, ALLES_RECEIVE_TASK_STACK_SIZE, NULL, ALLES_RECEIVE_TASK_PRIORITY, &alles_receive_handle, ALLES_RECEIVE_TASK_COREID);
    fprintf(stderr, "creating parse task\n");
    xTaskCreatePinnedToCore(&esp_parse_task, ALLES_PARSE_TASK_NAME, ALLES_PARSE_TASK_STACK_SIZE, NULL, ALLES_PARSE_TASK_PRIORITY, &alles_parse_handle, ALLES_PARSE_TASK_COREID);
#else
    if(alles_local_ip[0]==0) {
        get_first_ip_address(alles_local_ip);
    }
    fprintf(stderr, "creating socket to ip %s\n", alles_local_ip);
    create_multicast_ipv4_socket();
    fprintf(stderr, "creating mcast task\n");
    pthread_t thread_id;
    pthread_create(&thread_id, NULL, mcast_listen_task, NULL);
#endif
}


// sync.c -- keep track of mulitple synths
//extern uint8_t battery_mask;
extern uint8_t ipv4_quartet;
extern char githash[8];
int16_t client_id;
int64_t clocks[255];
int64_t ping_times[255];
uint8_t alive = 1;

extern int64_t computed_delta ; // can be negative no prob, but usually host is larger # than client
extern uint8_t computed_delta_set ; // have we set a delta yet?
extern int64_t last_ping_time;

amy_err_t sync_init() {
    client_id = -1; // for now
    for(uint8_t i=0;i<255;i++) { clocks[i] = 0; ping_times[i] = 0; }
    return AMY_OK;
}



void update_map(uint8_t client, uint8_t ipv4, int64_t time) {
    // I'm called when I get a sync response or a regular ping packet
    // I update a map of booted devices.

    //printf("[%d %d] Got a sync response client %d ipv4 %d time %lld\n",  ipv4_quartet, client_id, client , ipv4, time);
    clocks[ipv4] = time;
    int64_t my_sysclock = get_sysclock();
    ping_times[ipv4] = my_sysclock;

    // Now I basically see what index I would be in the list of booted synths (clocks[i] > 0)
    // And I set my client_id to that index
    uint8_t last_alive = alive;
    uint8_t my_new_client_id = 255;
    alive = 0;
    for(uint8_t i=0;i<255;i++) {
        if(clocks[i] > 0) { 
            if(my_sysclock < (ping_times[i] + (PING_TIME_MS * 2))) { // alive
                //printf("[%d %d] Checking my time %lld against ipv4 %d's of %lld, client_id now %d ping_time[%d] = %lld\n", 
                //    ipv4_quartet, client_id, my_sysclock, i, clocks[i], my_new_client_id, i, ping_times[i]);
                alive++;
            } else {
                //printf("[ipv4 %d client %d] clock %d is dead, ping time was %lld time now is %lld.\n", ipv4_quartet, client_id, i, ping_times[i], my_sysclock);
                clocks[i] = 0;
                ping_times[i] = 0;
            }
            // If this is not me....
            if(i != ipv4_quartet) {
                // predicted time is what we think the alive node should be at by now
                int64_t predicted_time = (my_sysclock - ping_times[i]) + clocks[i];
                if(my_sysclock >= predicted_time) my_new_client_id--;
            } else {
                my_new_client_id--;
            }
        } else {
            // if clocks[] is 0, no need to check
            my_new_client_id--;
        }
    }
    if(client_id != my_new_client_id || last_alive != alive) {
        fprintf(stderr,"[%d] my client_id is now %d. %d alive\n", ipv4_quartet, my_new_client_id, alive);
        client_id = my_new_client_id;
    }
}

void handle_sync(int64_t time, int8_t index) {
    // I am called when I get an s message, which comes along with host time and index
    int64_t sysclock = get_sysclock();
    char message[100];
    // Before I send, i want to update the map locally
    update_map(client_id, ipv4_quartet, sysclock);
    // Send back sync message with my time and received sync index and my client id & battery status (if any)
    sprintf(message, "_s%lldi%dc%dr%dy%dZ", sysclock, index, client_id, ipv4_quartet, 0);
    mcast_send(message, strlen(message));
    // Update computed delta (i could average these out, but I don't think that'll help too much)
    //int64_t old_cd = computed_delta;
    computed_delta = time - sysclock;
    computed_delta_set = 1;
    //if(old_cd != computed_delta) printf("Changed computed_delta from %lld to %lld on sync\n", old_cd, computed_delta);
}

void ping(int64_t sysclock) {
    char message[100];
    //printf("[%d %d] pinging with %lld\n", ipv4_quartet, client_id, sysclock);
    sprintf(message, "_s%lldi-1c%dr%dy%dZ", sysclock, client_id, ipv4_quartet, 0);
    update_map(client_id, ipv4_quartet, sysclock);
    mcast_send(message, strlen(message));
    last_ping_time = sysclock;
}

