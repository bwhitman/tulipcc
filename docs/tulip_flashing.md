# Compiling and flashing Tulip CC

## Flash a Tulip without compiling

**Please note, we don't recommend doing this yet.** Tulip is fast moving and it's really better if you figure out how to get the build environment going. It's not that hard. But if you want to try flashing your board without having to build Tulip, here's how:

 * Install [`esptool`](https://docs.espressif.com/projects/esptool/en/latest/esp32/installation.html) if you don't already have it
 * Make sure you have the serial port driver for your board installed. You should see a new /dev/\*USB\* or COM port when attaching it. If you don't, find the drivers for it and install them.
 * Download the 4 bin files in the [latest ESP32-S3 release](https://github.com/bwhitman/tulipcc/releases) and put them all in a folder
 * Connect your board / chip to USB
 * From the folder you have the .bin files in, run `esptool.py write_flash 0x0 bootloader.bin 0x10000 micropython.bin 0x8000 partition-table.bin 0x300000 tulip-lfs.bin`
 * Turn off and on the power to Tulip to start it 

## Compile TulipCC for ESP32-S3

[Tulip CC](../README.md) should build on all platforms, although I've only tested macOS and Linux so far. Please let me know if you have any trouble!

### macOS first time setup

On macOS, install homebrew if you haven't already:

```bash
# install homebrew first, skip this if you already have it...
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
# Then restart your terminal
```

Then install the requirements:

```bash
# install esp-idf's requirements
brew install cmake ninja dfu-util
```

### Linux first time setup

Install the requirements:

```bash
# install esp-idf's requirements
sudo apt install cmake ninja-build dfu-util virtualenv
```

### Build and flash Tulip 

On either macOS or Linux:

```
# install our version of the ESP-IDF that comes with our repository
cd esp-idf
./install.sh esp32s3
source ./export.sh

pip3 install Cython
pip3 install littlefs-python # needed to flash the filesystem
cd ../ports/esp32s3
```

Now connect your Tulip to your computer over USB. If using a breakout board, connect it to the UART connector, not the USB connector. If using our Tulip board, use the USB-C connector. 

```bash
idf.py -D MICROPY_BOARD=TULIP4 flash 
# With a brand new chip or devboard, the first time, you'll want to flash Tulip's filesystem 
# to the flash memory. Run this only once, or each time you modify `tulip_home` if you're developing Tulip itself.
python tulip_fs_create.py
```

You may need to restart Tulip after the flash, bt Tulip should now just turn on whenever you connect USB or power it on. 

To build / debug going forward:

```bash
cd ports/esp32s3
export ../../esp-idf/export.sh # do this once per terminal window
idf.py -D MICROPY_BOARD=TULIP4 flash
idf.py -D MICROPY_BOARD=TULIP4 monitor # shows stderr, use control-] to quit

# If you make changes to the underlying python libraries on micropython, you want to fully clean the build 
idf.py -D MICROPY_BOARD=TULIP4 fullclean
idf.py -D MICROPY_BOARD=TULIP4 flash
```


## Windows build and flash

TODO 

## Questions

Any questions? [Chat with us on our discussions page.](https://github.com/bwhitman/tulipcc/discussions)

