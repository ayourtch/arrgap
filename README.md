This is a small convenience utility for a more convenient working in
a secured environment where your VPN blocks all of the connections
to local network, but nonetheless you would like to retain the terminal
shell access to the devices on it.

For this, one can configure a raspberry pi zero w to act as a USB gadget
with the login shell on the serial port - thus, for your main laptop it will
appear as a serial port dongle connected to "some" linux computer.

Since the serial communication is uninhibited, you can get the simultaneous
control over the device on the local network (from raspberry), as well
as in the secured environment (directly over VPN).

However, there is a catch: the serial interface assumes a fixed window size.
This makes it supremely inconvenient to use it in a window - the moment
you resize the window away from the default 80x24, all your output goes
very corrupt.

Quite obviously - because SIGWINCH, which is sent to your local terminal
when you resize the window, is not sent over the serial interface.
This small program is there to take care of that: first, on your main/secured
host launch "AIRGAP_MASTER=y ./arrgap". This will give you a shell.

In this shell, connect to the serial port, raspberry pi, etc. On the raspberry
pi, start ./arrgap - this will give you another shell. If you now start a
full screen terminal app (like vi or tmux), and resize your terminal,
you will see that the target application has resized as well!

This is achieved by emitting from the host machine an escape sequence each
time it gets a SIGWINCH, and the target machine intercepts this escape sequence
and sends the appropriate ioctl() locally, setting the terminal window size.
The local apps notice that and adapt accordingly.



