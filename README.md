# Keyboard backlight service
This repo provides an application and a systemd service which controls the 
backlight of a keyboard. It's only tested for thinkpads but may work on 
other devices as well

## Installation
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

## Configuration
````
tp_kbd_backlight help
    -i ignore an input device
       This device does not re enable keyboard backlight.
       Separate multiple device by space.
       Default: use all mice and keyboard.
    -t configure timeout after which the backlight will be turned off
       Defaults to 30s 
    -m configure mouse mode (0..2)
       0 use all mice (default)
       1 use all internal mice only
       2 ignore mice
    -b set keyboard backlight device path
       defaults to /sys/class/leds/tpacpi::kbd_backlight
````


