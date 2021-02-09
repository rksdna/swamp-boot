/*
 * Swamp-boot - flash memory programming for the STM32 microcontrollers
 * Copyright (c) 2016 rksdna, fasked
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <time.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <memory.h>
#include "buffer.h"
#include "errors.h"
#include "serial.h"
#include "options.h"

#ifndef VERSION
#define VERSION 0
#endif

struct device
{
    uint16_t pid;
    size_t size;
    const char *name;
};

static const struct device devices[] =
{
    {0x0440, 0x00040000, "F05xxx/030x8"},
    {0x0444, 0x00040000, "F03xx4/03xx6"},
    {0x0442, 0x00040000, "F030xC/09xxx"},
    {0x0445, 0x00040000, "F04xxx/070x6"},
    {0x0448, 0x00040000, "F070xB/071xx/072xx"},
    {0x0412, 0x00008000, "F10xxx low-density"},
    {0x0410, 0x00020000, "F10xxx medium-density"},
    {0x0414, 0x00080000, "F10xxx high-density"},
    {0x0420, 0x00020000, "F10xxx medium-density value line"},
    {0x0428, 0x00080000, "F10xxx high-density value line"},
    {0x0418, 0x00040000, "F105xx/107xx"},
    {0x0430, 0x00100000, "F10xxx extra-density"},
    {0x0423, 0x00040000, "F401xB/401xC"},
};

static const char *modes[] =
{
    "reset",
    "nreset",
    "boot",
    "nboot",
    "set",
    "clear"
};

static int rts_mode = 2;
static int dtr_mode = 0;
static int trace_size = 4096;
static int trace_time = 5;
static const struct device *selected_device = devices;
static uint8_t device_version;
static uint8_t device_erase_command;
static uint8_t device_buffer[512];
static uint8_t device_memory[1024*1024];

static int reset_device(int boot)
{
    int result;
    const int state[2][6] =
    {
        {1, 0, boot, !boot, 1, 0},
        {0, 1, boot, !boot, 1, 0}
    };

    if ((result = control_serial_port(state[0][rts_mode], state[0][dtr_mode])))
        return result;

    if ((result = wait_serial_port(1)))
        return result;

    if ((result = control_serial_port(state[1][rts_mode], state[1][dtr_mode])))
        return result;

    return DONE;
}

static int try_to_handshake_device(void)
{
    int result;

    device_buffer[0] = 0x7F;

    if ((result = wait_serial_port(5)))
        return result;

    if ((result = flush_serial_port()))
        return result;

    if ((result = write_serial_port(&device_buffer, 1)))
        return result;

    if ((result = read_serial_port(&device_buffer, 1)))
        return result;

    return device_buffer[0] == 0x79 ? DONE : INVALID_DEVICE_REPLY;
}

static int handshake_device(void)
{
    int result;
    int count = 5;

    if ((result = configure_serial_port(1)))
        return result;

    while (count-- && (result = try_to_handshake_device()))
        continue;

    if ((result = configure_serial_port(50)))
        return result;

    return result;
}

static uint8_t device_checksum(uint8_t *data, size_t size)
{
    uint8_t checksum = 0x00;

    if (size == 1)
        return ~*data;

    while (size--)
        checksum ^= *data++;

    return checksum;
}

static int device_request(size_t size)
{
    int result;

    device_buffer[size] = device_checksum(device_buffer, size);

    if ((result = write_serial_port(device_buffer, size + 1)))
        return result;

    if ((result = read_serial_port(device_buffer, 1)))
        return result;

    return device_buffer[0] == 0x79 ? DONE : INVALID_DEVICE_REPLY;
}

static int device_response(size_t size)
{
    int result;

    if ((result = read_serial_port(device_buffer, size + 1)))
        return result;

    return device_buffer[size] == 0x79 ? DONE : INVALID_DEVICE_REPLY;
}

static int select_device(uint16_t pid)
{
    int count = sizeof(devices) / sizeof(struct device);

    fprintf(stdout, TTY_NONE "PID%04X...", pid);
    selected_device = devices;

    while (count--)
    {
        if (selected_device->pid == pid)
            return DONE;

        selected_device++;
    }

    return UNSUPPORTED_DEVICE;
}

static int select_mode(const char *mode, int *index)
{
    int count = sizeof(modes) / sizeof(const char *);

    while (count--)
    {
        if (!strcmp(mode, modes[count]))
        {
            *index = count;
            return DONE;
        }
    }

    return INVALID_OPTIONS_ARGUMENT;
}

static int select_rts_mode(const char *mode)
{
    fprintf(stdout, TTY_NONE "Selecting RTS mode \"%s\"...", mode);
    return select_mode(mode, &rts_mode);
}

static int select_dtr_mode(const char *mode)
{
    fprintf(stdout, TTY_NONE "Selecting DTR mode \"%s\"...", mode);
    return select_mode(mode, &dtr_mode);
}

static int connect_device(const char *file)
{
    int result;

    fprintf(stdout, TTY_NONE "Connect \"%s\"...", file);

    if ((result = open_serial_port(file)))
        return result;

    if ((result = reset_device(1)))
        return result;

    if ((result = handshake_device()))
        return result;

    device_buffer[0] = 0x00;
    if ((result = device_request(1)))
        return result;

    if ((result = device_response(13)))
        return result;

    device_version = device_buffer[1];
    device_erase_command = device_buffer[8];
    fprintf(stdout, TTY_NONE "V%1X.%1X...", device_version >> 4, device_version & 0x0F);

    device_buffer[0] = 0x02;
    if ((result = device_request(1)))
        return result;

    if ((result = device_response(3)))
        return result;

    if ((result = select_device(device_buffer[1] << 8 | device_buffer[2])))
        return result;

    return DONE;
}

static int unprotect_device(void)
{
    int result;

    fprintf(stdout, TTY_NONE "Readout unprotecting...");

    device_buffer[0] = 0x92;
    if ((result = device_request(1)))
        return result;

    if ((result = device_response(0)))
        return result;

    if ((result = handshake_device()))
        return result;

    return DONE;
}

static int read_device_memory(const struct buffer *buffer)
{
    uint32_t address = buffer->origin;
    uint8_t *data = buffer->data;
    size_t size = buffer->size;

    while (size)
    {
        int result;
        size_t count = size < 256 ? size : 256;

        device_buffer[0] = 0x11;
        if ((result = device_request(1)))
            return result;

        device_buffer[0] = address >> 24;
        device_buffer[1] = address >> 16;
        device_buffer[2] = address >> 8;
        device_buffer[3] = address;
        if ((result = device_request(4)))
            return result;

        device_buffer[0] = count - 1;
        if ((result = device_request(1)))
            return result;

        if ((result = read_serial_port(data, count)))
            return result;

        size -= count;
        data += count;
        address += count;
    }

    return DONE;
}

static int read_device(const char *file)
{
    int result;
    struct buffer buffer =
    {
        0, 0x08000000, selected_device->size, device_memory
    };

    fprintf(stdout, TTY_NONE "Reading to \"%s\"...", file);

    if ((result = read_device_memory(&buffer)))
        return result;

    if ((result = save_file_buffer(&buffer, file)))
        return result;

    return DONE;
}

static int erase_device(void)
{
    int result;

    fprintf(stdout, TTY_NONE "Erasing...");

    device_buffer[0] = device_erase_command;
    if ((result = device_request(1)))
        return result;

    device_buffer[0] = 0xFF;
    device_buffer[1] = 0xFF;
    if ((result = device_request(device_erase_command == 0x44 ? 2 : 1)))
        return result;

    return DONE;
}

static int adjust_device(const char *mode)
{
    int result;
    const uint8_t voltage = atoi(mode);

    fprintf(stdout, TTY_NONE "Adjust voltage \"%d\"...", voltage);

    device_buffer[0] = 0x31;
    if ((result = device_request(1)))
        return result;

    device_buffer[0] = 0xFF;
    device_buffer[1] = 0xFF;
    device_buffer[2] = 0x00;
    device_buffer[3] = 0x00;
    if ((result = device_request(4)))
        return result;

    device_buffer[0] = 0;
    device_buffer[1] = voltage;

    if ((result = device_request(2)))
        return result;

    return DONE;
}

static int write_device_memory(const struct buffer *buffer)
{
    uint32_t address = buffer->origin;
    uint8_t *data = buffer->data;
    size_t size = buffer->size;

    while (size)
    {
        int result;
        size_t count = size < 256 ? size : 256;

        device_buffer[0] = 0x31;
        if ((result = device_request(1)))
            return result;

        device_buffer[0] = address >> 24;
        device_buffer[1] = address >> 16;
        device_buffer[2] = address >> 8;
        device_buffer[3] = address;
        if ((result = device_request(4)))
            return result;

        device_buffer[0] = count - 1;
        memcpy(device_buffer + 1, data, count);
        if ((result = device_request(1 + count)))
            return result;

        size -= count;
        data += count;
        address += count;
    }

    return DONE;
}

static int write_device(const char *file)
{
    int result;
    struct buffer buffer =
    {
        0, 0x08000000, selected_device->size, device_memory
    };

    fprintf(stdout, TTY_NONE "Writing from \"%s\"...", file);

    if ((result = load_file_buffer(&buffer, file)))
        return result;

    if ((result = write_device_memory(&buffer)))
        return result;

    return result;
}

static int protect_device(void)
{
    int result;

    fprintf(stdout, TTY_NONE "Readout protecting...");

    device_buffer[0] = 0x82;
    if ((result = device_request(1)))
        return result;

    if ((result = device_response(0)))
        return result;

    if ((result = handshake_device()))
        return result;

    return DONE;
}

static int set_trace_time(const char *time)
{
    fprintf(stdout, TTY_NONE "Set trace time \"%s\"...", time);
    return sscanf(time, "%d", &trace_time) == 1 && trace_time >= 1 && trace_time <= 60 ? DONE : INVALID_OPTIONS_ARGUMENT;
}

static int set_trace_size(const char *size)
{
    fprintf(stdout, TTY_NONE "Set trace size \"%s\"...", size);
    return sscanf(size, "%d", &trace_size) == 1 && trace_size >= 1 ? DONE : INVALID_OPTIONS_ARGUMENT;
}

static int trace_device_console(void)
{
    int result;
    int count = 0;
    time_t base = time(0);

    if ((result = reset_device(0)))
        return result;

    while (count < trace_size)
    {
        if (time(0) - base > trace_time)
            break;

        if ((result = read_serial_port(device_buffer, 1)))
        {
            if (result == NO_DEVICE_REPLY)
                continue;

            return result;
        }

        fprintf(stdout, isprint(device_buffer[0]) || isspace(device_buffer[0]) ? TTY_NONE "%c" : TTY_NONE "[%02X]", device_buffer[0]);
        fflush(stdout);
        count++;
        base = time(0);
    }

    return DONE;
}

static int trace_device(void)
{
    int result = trace_device_console();

    fprintf(stdout, TTY_NONE "Tracing...");
    return result;
}

static int disconnect_device(void)
{
    int result;

    fprintf(stdout, TTY_NONE "Disconnecting...");

    if ((result = close_serial_port()))
        return result;

    return DONE;
}

int main(int argc, char* argv[])
{
    static const struct option options[] =
    {
        {JOINT_OPTION, 0, "rts", "Select RTS mode: reset - for device RESET, nreset - for inverted device RESET, boot - for device BOOT0 (default), nboot - for inverted device BOOT0, set - stay at high level, clear - stay at low level", select_rts_mode},
        {JOINT_OPTION, 0, "dtr", "Select DTR mode: reset - for device RESET (default), nreset - for inverted device RESET, boot - for device BOOT0, nboot - for inverted device BOOT0, set - stay at high level, clear - stay at low level", select_dtr_mode},
        {JOINT_OPTION, "c", "connect", "Open serial port and connect to device bootloader", connect_device},
        {PLAIN_OPTION, "u", "unprotect", "Erase and read-out unprotect device memory", unprotect_device},
        {JOINT_OPTION, "r", "read", "Read data from device memory to file", read_device},
        {PLAIN_OPTION, "e", "erase", "Erase device memory", erase_device},
        {JOINT_OPTION, "a", "adjust", "Adjust device voltage: 0 - [1.8 V, 2.1 V], 1 - [2.1 V, 2.4 V], 2 - [2.4 V, 2.7 V], 3 - [2.7 V, 3.6 V], 4 - [2.7 V, 3.6 V] with Vpp", adjust_device},
        {JOINT_OPTION, "w", "write", "Write data from file to device memory", write_device},
        {PLAIN_OPTION, "p", "protect", "Read-out protect device memory", protect_device},
        {JOINT_OPTION, 0, "trace-time", "Set trace intercharacter interval in seconds (5 default)", set_trace_time},
        {JOINT_OPTION, 0, "trace-size", "Set maximum trace log size (4096 default)", set_trace_size},
        {PLAIN_OPTION, "t", "trace", "Restart device in user mode, with redirecting device output to stdout", trace_device},
        {PLAIN_OPTION, "d", "disconnect", "Disconnect device and close serial port", disconnect_device},
        {USAGE_OPTION, "h", "help", "Print this help", usage_options},
        {OTHER_OPTION}
    };

    static const struct error errors[] =
    {
        {INVALID_FILE_CHECKSUM, "Invalid checksum of file"},
        {INVALID_FILE_CONTENT, "Invalid device memory location or invalid record in file"},
        {UNSUPPORTED_DEVICE, "Unsupported device"},
        {INVALID_DEVICE_REPLY, "Invalid reply from device bootloader"},
        {NO_DEVICE_REPLY, "No reply from device bootloader"},
        {SERIAL_PORT_ALREADY_OPEN, "Serial port already open"},
        {INTERNAL_ERROR, "Internal error"},
        {INVALID_OPTIONS_ARGUMENT, "Invalid actual parameter"},
        {INVALID_OPTION, "Invalid option"},
        {DONE, "No errors, all done"},
    };

    static char stdout_buffer[256];
    setvbuf(stdout, stdout_buffer, _IOLBF, sizeof(stdout_buffer));
    fprintf(stdout, TTY_NONE "Swamp-boot, version 0.%d\n", VERSION);

    return invoke_options(TTY_BOLD "swamp-boot" TTY_NONE " [" TTY_UNLN "OPTIONS" TTY_NONE "] ", options, errors, argc, argv);
}
