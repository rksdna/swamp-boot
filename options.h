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

#ifndef OPTIONS_H
#define OPTIONS_H

#ifdef NO_TTY
#define TTY_BOLD
#define TTY_UNLN
#define TTY_NONE
#else
#define TTY_BOLD "\e[1m"
#define TTY_UNLN "\e[4m"
#define TTY_NONE "\e[0m"
#endif

enum role
{
    PLAIN_OPTION,
    JOINT_OPTION,
    USAGE_OPTION,
    OTHER_OPTION
};

struct option
{
    enum role role;
    const char *short_name;
    const char *long_name;
    const char *usage;
    const void *handler;
};

struct error
{
    int result;
    const char *usage;
};

typedef int (* plain_handler_t)(void);
typedef int (* joint_handler_t)(const char *argument);
typedef int (* usage_handler_t)(const char *synopsis, const struct option options[], const struct error errors[]);
typedef int (* other_handler_t)(const char *operand);

int invoke_options(const char *synopsis, const struct option options[], const struct error errors[], int argc, char *argv[]);
int usage_options(const char *synopsis, const struct option options[], const struct error errors[]);

#endif
