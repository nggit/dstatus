dstatus
=======
A minimalist status monitor/bar for dwm Window Manager written in C. Inspired from existing [status monitor](https://dwm.suckless.org/status_monitor/).
Can display info such as CPU usage, CPU temperature, memory usage, and network throughput (download and upload speed). Seamlessly detects network interface changes. For example, if you use multiple network interfaces such as WLAN, Ethernet, USB, etc.

Build Dependencies
----------------
libX11-dev (deb-based package name) or libX11-devel (rpm-based package name).

Compiling & Installing
----------------------
    $ make
    # make install
Before compiling you might want to customize the [config.mk](config.mk) to fit your system.

Running
-------
    $ dstatus &
Or simply add `dstatus &` to your startup script as usual.

Screenshot
----------
![dstatus](screenshot.png)
