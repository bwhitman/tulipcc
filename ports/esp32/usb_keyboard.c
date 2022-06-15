// usb_keyboard.c

#include "esp_rom_gpio.h"
#include "driver/gpio.h"
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "usb/usb_host.h"
#include "usb_hid_keys.h"
#include "tulip_helpers.h"

const size_t USB_HID_DESC_SIZE = 9;

typedef union {
    struct {
        uint8_t bLength;                    /**< Size of the descriptor in bytes */
        uint8_t bDescriptorType;            /**< Constant name specifying type of HID descriptor. */
        uint16_t bcdHID;                    /**< USB HID Specification Release Number in Binary-Coded Decimal (i.e., 2.10 is 210H) */
        uint8_t bCountryCode;               /**< Numeric expression identifying country code of the localized hardware. */
        uint8_t bNumDescriptor;             /**< Numeric expression specifying the number of class descriptors. */
        uint8_t bHIDDescriptorType;         /**< Constant name identifying type of class descriptor. See Section 7.1.2: Set_Descriptor Request for a table of class descriptor constants. */
        uint16_t wHIDDescriptorLength;      /**< Numeric expression that is the total size of the Report descriptor. */
        uint8_t bHIDDescriptorTypeOpt;      /**< Optional constant name identifying type of class descriptor. See Section 7.1.2: Set_Descriptor Request for a table of class descriptor constants. */
        uint16_t wHIDDescriptorLengthOpt;   /**< Optional numeric expression that is the total size of the Report descriptor. */
    } USB_DESC_ATTR;
    uint8_t val[9];
} usb_hid_desc_t;



const TickType_t HOST_EVENT_TIMEOUT = 1;
const TickType_t CLIENT_EVENT_TIMEOUT = 1;

usb_host_client_handle_t Client_Handle;
usb_device_handle_t Device_Handle;
typedef void (*usb_host_enum_cb_t)(const usb_config_desc_t *config_desc);
static usb_host_enum_cb_t _USB_host_enumerate;
bool isKeyboard = false;
bool isKeyboardReady = false;
uint8_t KeyboardInterval;
bool isKeyboardPolling = false;
int64_t KeyboardTimer=0;

const size_t KEYBOARD_IN_BUFFER_SIZE = 8;
usb_transfer_t *KeyboardIn = NULL;


void _client_event_callback(const usb_host_client_event_msg_t *event_msg, void *arg)
{
  esp_err_t err;
  switch (event_msg->event)
  {
    /**< A new device has been enumerated and added to the USB Host Library */
    case USB_HOST_CLIENT_EVENT_NEW_DEV:
      printf("New device address: %d", event_msg->new_dev.address);
      err = usb_host_device_open(Client_Handle, event_msg->new_dev.address, &Device_Handle);
      if (err != ESP_OK) printf("usb_host_device_open: %x", err);

      usb_device_info_t dev_info;
      err = usb_host_device_info(Device_Handle, &dev_info);
      if (err != ESP_OK) printf("usb_host_device_info: %x", err);
      printf("speed: %d dev_addr %d vMaxPacketSize0 %d bConfigurationValue %d",
          dev_info.speed, dev_info.dev_addr, dev_info.bMaxPacketSize0,
          dev_info.bConfigurationValue);

      const usb_device_desc_t *dev_desc;
      err = usb_host_get_device_descriptor(Device_Handle, &dev_desc);
      if (err != ESP_OK) printf("usb_host_get_device_desc: %x", err);

      const usb_config_desc_t *config_desc;
      err = usb_host_get_active_config_descriptor(Device_Handle, &config_desc);
      if (err != ESP_OK) printf("usb_host_get_config_desc: %x", err);
      (*_USB_host_enumerate)(config_desc);
      break;
    /**< A device opened by the client is now gone */
    case USB_HOST_CLIENT_EVENT_DEV_GONE:
      printf("Device Gone handle: %x", (uint32_t)event_msg->dev_gone.dev_hdl);
      break;
    default:
      printf("Unknown value %d", event_msg->event);
      break;
  }
}

// Reference: esp-idf/examples/peripherals/usb/host/usb_host_lib/main/usb_host_lib_main.c

void usbh_setup(usb_host_enum_cb_t enumeration_cb)
{
  const usb_host_config_t config = {
    .intr_flags = ESP_INTR_FLAG_LEVEL1,
  };
  esp_err_t err = usb_host_install(&config);
  printf("usb_host_install: %x", err);

  const usb_host_client_config_t client_config = {
    .is_synchronous = false,
    .max_num_event_msg = 5,
    .async = {
        .client_event_callback = _client_event_callback,
        .callback_arg = Client_Handle
    }
  };
  err = usb_host_client_register(&client_config, &Client_Handle);
  printf("usb_host_client_register: %x", err);

  _USB_host_enumerate = enumeration_cb;
}

void usbh_task(void)
{
  uint32_t event_flags;

  esp_err_t err = usb_host_lib_handle_events(HOST_EVENT_TIMEOUT, &event_flags);
  if (err == ESP_OK) {
    if (event_flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) {
      printf("No more clients");
    }
    if (event_flags & USB_HOST_LIB_EVENT_FLAGS_ALL_FREE) {
      printf("No more devices");
    }
  }
  else {
    if (err != ESP_ERR_TIMEOUT) {
      printf("usb_host_lib_handle_events: %x flags: %x", err, event_flags);
    }
  }

  err = usb_host_client_handle_events(Client_Handle, CLIENT_EVENT_TIMEOUT);
  if ((err != ESP_OK) && (err != ESP_ERR_TIMEOUT)) {
    printf("usb_host_client_handle_events: %x", err);
  }
}

uint16_t scan_ascii(uint8_t code, uint8_t modifier) {
    uint8_t shift = (modifier & KEY_MOD_LSHIFT || modifier & KEY_MOD_RSHIFT);
    uint8_t ctrl  = (modifier & KEY_MOD_LCTRL || modifier & KEY_MOD_RCTRL);
    //uint8_t alt   = (modifier & KEY_MOD_LALT || modifier & KEY_MOD_RALT);
    //uint8_t meta  = (modifier & KEY_MOD_LMETA || modifier & KEY_MOD_RMETA);

    if(code >= 0x04 && code <= 0x1d) {
        // control-chars
        if(ctrl) return (code-0x04)+ 1;
        // capital letters
        if(shift) return (code-0x04)+'A';
        return code-0x04 + 'a';
    }
    if(code >= 0x1e && code <= 0x27) {
        if(shift) {
            switch(code) {
                case 0x1e: return '!'; 
                case 0x1f: return '@'; 
                case 0x20: return '#'; 
                case 0x21: return '$'; 
                case 0x22: return '%'; 
                case 0x23: return '^'; 
                case 0x24: return '&'; 
                case 0x25: return '*'; 
                case 0x26: return '('; 
                case 0x27: return ')'; 
            }
        } else {
            if(code == 0x27) return '0';
            return (code-0x1e) + '1';
        }
    }
    switch(code) {
        case KEY_ENTER: return 13;  
        case KEY_ESC: return 27; 
        case KEY_BACKSPACE: return 8; 
        case KEY_TAB: return 9; 
        case KEY_PAGEUP: return 25;
        case KEY_PAGEDOWN: return 22;

        case KEY_SPACE: return ' '; 
        case KEY_MINUS: if(shift) return '_'; else return '-';
        case KEY_EQUAL: if(shift) return '+'; else return '=';
        case KEY_LEFTBRACE: if(shift) return '{'; else return '[';
        case KEY_RIGHTBRACE: if(shift) return '}'; else return ']';
        case KEY_BACKSLASH: if(shift) return '|'; else return '\\';
        case KEY_HASHTILDE: if(shift) return '~'; else return '#';
        case KEY_SEMICOLON: if(shift) return ':'; else return ';';
        case KEY_APOSTROPHE: if(shift) return '\"'; else return '\'';
        case KEY_GRAVE: if(shift) return '~'; else return '`';
        case KEY_COMMA: if(shift) return '<'; else return ',';
        case KEY_DOT: if(shift) return '>'; else return '.';
        case KEY_SLASH: if(shift) return '?'; else return '/';

        // We return extended codes for these, and get converted to ANSI for the repl down the line
        case KEY_UP: return 259; 
        case KEY_DOWN: return 258; 
        case KEY_LEFT: return 260; 
        case KEY_RIGHT: return 261;
        case KEY_DELETE: return 262; 

    }
    return 0;
}

// TODO, a key_held function that returns 7 uints

// Keep track of which keys / scan codes are being held
uint8_t last_scan[8] = {0,0,0,0,0,0,0,0};

void decode_report(uint8_t *p) {
    // First byte, modifier mask
    uint8_t modifier = p[0];
    // Second byte, reserved
    // next 6 bytes, scan codes (for rollover)
    for(uint8_t i=2;i<8;i++) {
		if(p[i]!=0) {
			uint8_t skip = 0;
			for(uint8_t j=2;j<8;j++) {
				if(last_scan[j] == p[i]) skip = 1;
			}
			if(!skip) { // only process new keys
		        uint16_t c = scan_ascii(p[i], modifier);
		        // Check if c is already being held
		        if(c) {
		        	// Deal with the extended codes that need ANSI escape chars 
		        	if(c>257 && c<263) { 
		        		tx_char(27);
		        		tx_char('[');
		        		if(c==258) tx_char('B');
		        		if(c==259) tx_char('A');
		        		if(c==260) tx_char('D');
		        		if(c==261) tx_char('C');
		        		if(c==262) { tx_char('3'); tx_char(126); }
		        	} else {
		        		tx_char(c);
		        	}
				}
			}
			
		} 
    }
    for(uint8_t i=0;i<8;i++) last_scan[i] = p[i];
}

void keyboard_transfer_cb(usb_transfer_t *transfer)
{
  if (Device_Handle == transfer->device_handle) {
    isKeyboardPolling = false;
    if (transfer->status == 0) {
      if (transfer->actual_num_bytes == 8) {
        uint8_t *const p = transfer->data_buffer;
        decode_report(p);
      }
      else {
        printf("Keyboard boot hid transfer too short or long");
      }
    }
    else {
      printf("transfer->status %d", transfer->status);
    }
  }
}

void check_interface_desc_boot_keyboard(const void *p)
{
  const usb_intf_desc_t *intf = (const usb_intf_desc_t *)p;

  if ((intf->bInterfaceClass == USB_CLASS_HID) &&
      (intf->bInterfaceSubClass == 1) &&
      (intf->bInterfaceProtocol == 1)) {
    isKeyboard = true;
    printf("Claiming a boot keyboard!");
    esp_err_t err = usb_host_interface_claim(Client_Handle, Device_Handle,
        intf->bInterfaceNumber, intf->bAlternateSetting);
    if (err != ESP_OK) printf("usb_host_interface_claim failed: %x", err);
  }
}

void prepare_endpoint(const void *p)
{
  const usb_ep_desc_t *endpoint = (const usb_ep_desc_t *)p;
  esp_err_t err;

  // must be interrupt for HID
  if ((endpoint->bmAttributes & USB_BM_ATTRIBUTES_XFERTYPE_MASK) != USB_BM_ATTRIBUTES_XFER_INT) {
    printf("Not interrupt endpoint: 0x%02x", endpoint->bmAttributes);
    return;
  }
  if (endpoint->bEndpointAddress & USB_B_ENDPOINT_ADDRESS_EP_DIR_MASK) {
    err = usb_host_transfer_alloc(KEYBOARD_IN_BUFFER_SIZE, 0, &KeyboardIn);
    if (err != ESP_OK) {
      KeyboardIn = NULL;
      printf("usb_host_transfer_alloc In fail: %x", err);
      return;
    }
    KeyboardIn->device_handle = Device_Handle;
    KeyboardIn->bEndpointAddress = endpoint->bEndpointAddress;
    KeyboardIn->callback = keyboard_transfer_cb;
    KeyboardIn->context = NULL;
    isKeyboardReady = true;
    KeyboardInterval = endpoint->bInterval;
    printf("USB boot keyboard ready");
  }
  else {
    printf("Ignoring interrupt Out endpoint");
  }
}

void show_config_desc_full(const usb_config_desc_t *config_desc)
{
  // Full decode of config desc.
  const uint8_t *p = &config_desc->val[0];
  static uint8_t USB_Class = 0;
  uint8_t bLength;
  for (int i = 0; i < config_desc->wTotalLength; i+=bLength, p+=bLength) {
    bLength = *p;
    if ((i + bLength) <= config_desc->wTotalLength) {
      const uint8_t bDescriptorType = *(p + 1);
      switch (bDescriptorType) {
        case USB_B_DESCRIPTOR_TYPE_DEVICE:
          printf("USB Device Descriptor should not appear in config");
          break;
        case USB_B_DESCRIPTOR_TYPE_CONFIGURATION:
          break;
        case USB_B_DESCRIPTOR_TYPE_STRING:
          printf("USB string desc TBD");
          break;
        case USB_B_DESCRIPTOR_TYPE_INTERFACE:
          check_interface_desc_boot_keyboard(p);
          break;
        case USB_B_DESCRIPTOR_TYPE_ENDPOINT:
          if (isKeyboard && KeyboardIn == NULL) prepare_endpoint(p);
          break;
        case USB_B_DESCRIPTOR_TYPE_DEVICE_QUALIFIER:
          printf("USB device qual desc TBD");
          break;
        case USB_B_DESCRIPTOR_TYPE_OTHER_SPEED_CONFIGURATION:
          printf("USB Other Speed TBD");
          break;
        case USB_B_DESCRIPTOR_TYPE_INTERFACE_POWER:
          printf("USB Interface Power TBD");
          break;
        case 0x21:
          // HID 
          break;
        default:
          printf("Unknown USB Descriptor Type: 0x%x", bDescriptorType);
          break;
      }
    }
    else {
      printf("USB Descriptor invalid");
      return;
    }
  }
}

void usb_keyboard_start()
{
  usbh_setup(show_config_desc_full);
  while(1) {
      usbh_task();

      KeyboardTimer = esp_timer_get_time() / 1000;
      if (isKeyboardReady && !isKeyboardPolling && (KeyboardTimer > KeyboardInterval)) {
        KeyboardIn->num_bytes = 8;
        esp_err_t err = usb_host_transfer_submit(KeyboardIn);
        if (err != ESP_OK) {
          printf("usb_host_transfer_submit In fail: %x", err);
        }
        isKeyboardPolling = true;
        KeyboardTimer = 0;
      }
    }
}