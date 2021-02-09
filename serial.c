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
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include "errors.h"
#include "serial.h"

static int fd = -1;
static struct termios shadow_options;
static struct termios active_options;
static int shadow_status;
static int active_status;

int open_serial_port(const char *file)
{
    if (fd >= 0)
        return SERIAL_PORT_ALREADY_OPEN;

    if ((fd = open(file, O_RDWR | O_NOCTTY)) < 0)
        return INTERNAL_ERROR;

    if (tcgetattr(fd, &shadow_options) < 0)
        return INTERNAL_ERROR;

    active_options = shadow_options;

    if (ioctl(fd, TIOCMGET, &shadow_options) < 0)
        return INTERNAL_ERROR;

    active_status = shadow_status;

    active_options.c_cflag = B115200 | PARENB | CS8 | CLOCAL | CREAD;
    active_options.c_iflag = IGNBRK | IGNPAR;
    active_options.c_oflag = 0;
    active_options.c_lflag = 0;
    active_options.c_cc[VMIN] = 0;
    active_options.c_cc[VTIME] = 5;

    if (tcflush(fd, TCIFLUSH) < 0)
        return INTERNAL_ERROR;

    if (tcsetattr(fd, TCSANOW, &active_options) < 0)
        return INTERNAL_ERROR;

    return DONE;
}

int close_serial_port(void)
{
    if (ioctl(fd, TIOCMSET, &shadow_status) < 0)
        return INTERNAL_ERROR;

    if (tcsetattr(fd, TCSANOW, &shadow_options) < 0)
        return INTERNAL_ERROR;

    if (close(fd) < 0)
        return INTERNAL_ERROR;

    fd = -1;
    return DONE;
}

int write_serial_port(const void *data, size_t size)
{
    while (size)
    {
        ssize_t count = write(fd, data, size);

        if (count < 0)
        {
            if (errno == EINTR)
                continue;

            return INTERNAL_ERROR;
        }

        data += count;
        size -= count;
    }

    return DONE;
}

int read_serial_port(void *data, size_t size)
{
    while (size)
    {
        ssize_t count = read(fd, data, size);

        if (count < 0)
        {
            if (errno == EINTR)
                continue;

            return INTERNAL_ERROR;
        }

        if (count == 0)
            return NO_DEVICE_REPLY;

        data += count;
        size -= count;
    }

    return DONE;
}

int flush_serial_port(void)
{
    if (tcflush(fd, TCIOFLUSH) < 0)
        return INTERNAL_ERROR;

    return DONE;
}

int configure_serial_port(int timeout)
{
    active_options.c_cc[VTIME] = timeout;

    if (tcsetattr(fd, TCSANOW, &active_options) < 0)
        return INTERNAL_ERROR;

    return DONE;
}

int control_serial_port(int rts, int dtr)
{
    active_status &= ~(TIOCM_RTS | TIOCM_DTR);

    if (rts)
        active_status |= TIOCM_RTS;

    if (dtr)
        active_status |= TIOCM_DTR;

    if (ioctl(fd, TIOCMSET, &active_status) < 0)
        return INTERNAL_ERROR;

    return DONE;
}

int wait_serial_port(int ms)
{
    int result;
    struct timespec time;

    time.tv_sec = ms / 1000;
    time.tv_nsec = 1000000 * (ms % 1000);

    while ((result = nanosleep(&time , &time)))
    {
        if (errno == EINTR)
            continue;

        return INTERNAL_ERROR;
    }

    return DONE;
}
