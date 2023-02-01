# Compiling and flashing Tulip CC

Tulip's firmware is based on Micropython. You'll find our platform independent code in `extmod/tulip` and our platform specific code in `ports/esp32s3` and `ports/macos`. 

Tulip CC on ESP32S3 should build on all platforms, although I've only tested macOS so far. Please let me know if you have any trouble!

## macOS build and flash

To compile your own firmware, start by cloning this repository. 

On macOS, start by installing homebrew
```
# install homebrew first, skip this if you already have it...
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
# Then restart your terminal
```

Do some one-time only setup commmands: 

```
# install esp-idf's requirements
brew install cmake ninja dfu-util

# and install our version of the ESP-IDF that comes with our repository
cd esp-idf
./install.sh esp32s3
source ./export.sh

pip3 install Cython littlefs-python # needed to flash the filesystem
cd ../ports/esp32s3
# Connect your esp32s3 board over USB (to the UART connector on the dev board, not the USB connector)
idf.py -D MICROPY_BOARD=TULIP4 flash
# With a brand new chip or devboard, the first time, you'll want to flash Tulip's filesystem 
# to the flash memory. Run this only once, or each time you modify `tulip_home` if you're developing Tulip itself.
python tulip_fs_create.py
```

Tulip should now just turn on whenever you connect USB or power it on. 

To build / debug going forward:

```
cd ports/esp32s3
export ../../esp-idf/export.sh # do this once per terminal window
idf.py -D MICROPY_BOARD=TULIP4 flash
idf.py -D MICROPY_BOARD=TULIP4 monitor # shows stderr, use control-] to quit

# If you make changes to the underlying python libraries on micropython, you want to fully clean the build 
idf.py -D MICROPY_BOARD=TULIP4 fullclean
idf.py -D MICROPY_BOARD=TULIP4 flash
```


## Linux build and flash

TODO


## Windows build and flash

TODO 

