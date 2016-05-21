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

#ifndef ERRORS_H
#define ERRORS_H

enum
{
    DONE = 0,
    INVALID_OPTION,
    INVALID_OPTIONS_ARGUMENT,
    INTERNAL_ERROR,
    SERIAL_PORT_ALREADY_OPEN,
    NO_DEVICE_REPLY,
    INVALID_DEVICE_REPLY,
    UNSUPPORTED_DEVICE,
    INVALID_FILE_CONTENT,
    INVALID_FILE_CHECKSUM
};

#endif
