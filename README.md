# tsc2uinput
User mode driver for Raspberry Pi's Tontek/MZTX-PI-EXT  320x240 LCD with TSC2003 resistive touchscreen controller

This short (kind of) experimental driver reads XY pos and touch state
though the I2C bus (on the connector P1 of Raspberries) and sends events
to /dev/uinput.  It is to be used with the Tontek / MZTX-PI-EXT LCD Touchscreen.

/etc/rc.local launches 
/usr/local/bin/mztx06a & # touchscreen Tontec
which sends screen buffer diffs through the SPI bus (which is slow and computer intensive)

Relevant documents :
* http://elinux.org/MZTX-PI-EXT
* http://www.ti.com/lit/ds/symlink/tsc2003.pdf (datasheet)
* https://github.com/derekhe/wavesahre-7inch-touchscreen-driver (/dev/uinput example)

usage : 'sudo ./tsc2uinput

Notes :
* The calibration is hardwired... (and there is debug output)
* Touchpad emulation with relative events should probably be more appropriate

Setup in /boot/config.txt (mztx06a is in 'landscape' mode)

framebuffer_width=320
framebuffer_height=240

----------------
There now exist affordable HDMI LCD screens.
