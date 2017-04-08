# Swamp-boot

flash memory programming for the STM32 microcontrollers. Uses signals RTS and DTR to reset the microcontroller and select the boot mode. Provides forwarding controller console output to the standard output for interfacing with IDE.

Write `cdc.hex` the file through `/dev/ttyUSB0` example call:

```
swamp-boot -c /dev/ttyUSB0 -e -w cdc.hex -t -d

Swamp-boot, version 0.9
Connect "/dev/ttyUSB0"...V3.1...PID0445... done
Erasing... done
Writing from "cdc.hex"... done
hello
connected
baudrate: 9600, data 8, parity 0, stop 0
Tracing... done
Disconnecting... done
```

Supported options:

```
Synopsis:
	swamp-boot [OPTIONS] 

Options:
--rts ARG
	Select RTS mode: reset - for device RESET,
	nreset - for inverted device RESET, boot
	- for device BOOT0 (default), nboot - for
	inverted device BOOT0, set - stay at high
	level, clear - stay at low level

--dtr ARG
	Select DTR mode: reset - for device RESET
	(default), nreset - for inverted device RESET,
	boot - for device BOOT0, nboot - for inverted
	device BOOT0, set - stay at high level, clear
	- stay at low level

-c, --connect ARG
	Open serial port and connect to device bootloader

-u, --unprotect
	Erase and read-out unprotect device memory

-r, --read ARG
	Read data from device memory to file

-e, --erase
	Erase device memory

-w, --write ARG
	Write data from file to device memory

-p, --protect
	Read-out protect device memory

--trace-time ARG
	Set trace intercharacter interval in seconds
	(5 default)

--trace-size ARG
	Set maximum trace log size (4096 default)

-t, --trace
	Restart device in user mode, with redirecting
	device output to stdout

-d, --disconnect
	Disconnect device and close serial port

-h, --help
	Print this help

Return values:
9	Invalid checksum of file
8	Invalid device memory location or invalid record in file
7	Unsupported device
6	Invalid reply from device bootloader
5	No reply from device bootloader
4	Serial port already open
3	Internal error
2	Invalid actual parameter
1	Invalid option
0	No errors, all done
```
