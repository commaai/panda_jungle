Welcome to the jungle
======
Firmware for the internal Panda Jungle gen2 testing board.
Forked from panda firmware v1.5.3.

## Updating the firmware on your jungle
Updating the firmware is easy! In the `board` folder, run:
``` bash
make
```

If you somehow bricked your jungle, you'll need a [comma key](https://comma.ai/shop/products/comma-key) to put the microcontroller in DFU mode. When powered on while holding the key button to put it in DFU mode, running `make recover` in `board/` should unbrick it.

## udev rules
To make the jungle usable without root permissions, you might need to setup udev rules for it.
On ubuntu, this should do the trick:
``` bash
sudo tee /etc/udev/rules.d/12-panda_jungle.rules <<EOF
SUBSYSTEM=="usb", ATTRS{idVendor}=="bbaa", ATTRS{idProduct}=="ddcf", MODE="0666"
SUBSYSTEM=="usb", ATTRS{idVendor}=="bbaa", ATTRS{idProduct}=="ddef", MODE="0666"
EOF
sudo udevadm control --reload-rules && sudo udevadm trigger
```


