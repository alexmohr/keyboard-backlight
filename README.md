# Keyboard backlight service
[![Build Status](https://travis-ci.com/alexmohr/keyboard-backlight.svg?branch=master)](https://travis-ci.com/alexmohr/keyboard-backlight)

This repo provides an application and a systemd service which controls the 
backlight of a keyboard. It's only tested for thinkpads but may work on 
other devices as well

## Installation

### Arch Linux
The package is in the AUR and called  ``tp-kb-backlight-git``

### Install from source
To build the binary run
````
mkdir build
cd build
cmake ../
make
```` 

Use ``make install`` to install the application and ``make service`` 
to install the service and enable the systemd service. 

Make sure to run ``make install`` BEFORE ``make service`` 

### Binary (not recommended)
Download the latest package from github. Extract it an run the following commands.

````
sudo cp keyboard_backlight /usr/bin
sudo cp keyboard_backlight.service /etc/systemd/sytem
sudo systemctl enable --now keyboard_backlight.service
```` 


# Uninstall
For automated removal run the command below in the cmake folder
````
make uninstall
````

To manually delete the files 
````
sudo systemctl disable --now keyboard_backlight.service
sudo rm /etc/systemd/system/keyboard_backlight.service
sudo rm /usr/bin/keyboard_backlight
````




## Configuration
````
keyboard_backlight 1.2.1 
    -h show this help
    -i ignore an input device
       This device does not re enable keyboard backlight.
       Separate multiple device by space.
       Default: use all mice and keyboard.
    -t configure timeout in seconds after which the backlight will be turned off
       Defaults to 30s 
    -m configure mouse mode (0..2)
       0 use all mice (default)
       1 use all internal mice only
       2 ignore mice
    -b set keyboard backlight device path
       defaults to /sys/class/leds/tpacpi::kbd_backlight
    -f stay in foreground and do not start daemon
    -s Set a brightness value from 0..2 and exit
````


