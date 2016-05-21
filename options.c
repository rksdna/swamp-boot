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

#include <errno.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include "errors.h"
#include "options.h"

enum state
{
    ENTRY_STATE,
    DASH_STATE,
    SHORT_OPTION_STATE,
    BEFORE_SHORT_ARGUMENT_STATE,
    SHORT_ARGUMENT_STATE,
    DASH_DASH_STATE,
    LONG_OPTION_STATE,
    LONG_ARGUMENT_STATE,
    OPERAND_STATE,
    FORSED_OPERAND_STATE,
    FAIL_STATE
};

struct context
{
    const char *synopsis;
    const struct option *options;
    const struct error *errors;
    const struct option *option;
    const char *s;
    int result;
};

typedef int (* match_t)(const struct option *option, const char *p, int size);

static int as_long_option(const struct option *option, const char *p, int length)
{
    return option->long_name && strlen(option->long_name) == length && !strncmp(p, option->long_name, length);
}

static int as_short_option(const struct option *option, const char *p, int length)
{
    return option->short_name && strlen(option->short_name) == length && !strncmp(p, option->short_name, length);
}

static int has_option(struct context *context, const char *p, match_t match)
{
    const struct option *option = context->options;

    while (option->role != OTHER_OPTION)
    {
        if (match(option, context->s, p - context->s))
            break;

        option++;
    }

    context->option = option;
    return option->role != OTHER_OPTION && option->handler;
}

static int has_other_option(struct context *context)
{
    const struct option *option = context->options;

    while (option->role != OTHER_OPTION)
        option++;

    context->option = option;
    return option->role == OTHER_OPTION && option->handler;
}

static int has_argument(const struct context *context)
{
    return context->option->role == JOINT_OPTION;
}

static enum state fail(struct context *context, int result)
{
    const struct error *error = context->errors;
    const char *usage = "Unexpected error";

    context->result = result;

    while (error->result)
    {
        if (result == error->result)
        {
            usage = result == INTERNAL_ERROR ? strerror(errno) : error->usage;
            break;
        }
        error++;
    }

    fprintf(stdout, TTY_NONE " " TTY_BOLD "FAILED" TTY_NONE " [%s, %d]\n", usage, result);
    return FAIL_STATE;
}

static enum state done(enum state state)
{
    fprintf(stdout, TTY_NONE " done\n");
    return state;
}

static enum state invoke(struct context *context, const char *p, enum state state)
{
    int result = INVALID_OPTION;

    switch (context->option->role)
    {
    case PLAIN_OPTION:
        result = ((plain_handler_t)context->option->handler)();
        break;

    case JOINT_OPTION:
        result = ((joint_handler_t)context->option->handler)(context->s);
        break;

    case USAGE_OPTION:
        result = ((usage_handler_t)context->option->handler)(context->synopsis, context->options, context->errors);
        break;

    case OTHER_OPTION:
        result = ((other_handler_t)context->option->handler)(context->s);
        break;

    default:
        break;
    }

    context->s = 0;
    return result ? fail(context, result) : done(state);
}

static enum state invalid(struct context *context, const char *p)
{
    fprintf(stdout, TTY_NONE "Processing \"%*s\"...", (int)(p - context->s), context->s);
    return fail(context, INVALID_OPTION);
}

static enum state clean(struct context *context, enum state state)
{
    context->s = 0;
    return state;
}

static void collect(struct context *context, const char *p)
{
    if (context->s == 0)
        context->s = p;
}

static enum state process(struct context *context, enum state state, const char *p)
{
    const char dash = '-';
    const char equal = '=';
    const char null = 0;

    collect(context, p);

    switch (state)
    {
    case ENTRY_STATE:
        if (*p == dash)
            return clean(context, DASH_STATE);

        return OPERAND_STATE;

    case DASH_STATE:
        if (*p == dash)
            return clean(context, DASH_DASH_STATE);

        if (isalnum(*p) && has_option(context, p + 1, as_short_option))
            return has_argument(context) ? clean(context, BEFORE_SHORT_ARGUMENT_STATE) : invoke(context, p, SHORT_OPTION_STATE);

        break;

    case SHORT_OPTION_STATE:
        if (*p == null)
            return clean(context, ENTRY_STATE);

        if (isalnum(*p) && has_option(context, p + 1, as_short_option))
            return has_argument(context) ? invalid(context, p) : invoke(context, p, SHORT_OPTION_STATE);

        break;

    case BEFORE_SHORT_ARGUMENT_STATE:
        if (*p == null)
            return clean(context, SHORT_ARGUMENT_STATE);

        return SHORT_ARGUMENT_STATE;

    case SHORT_ARGUMENT_STATE:
        if (*p == null)
            return invoke(context, p, ENTRY_STATE);

        return SHORT_ARGUMENT_STATE;

    case DASH_DASH_STATE:
        if (*p == null)
            return clean(context, FORSED_OPERAND_STATE);

        if (isalnum(*p))
            return LONG_OPTION_STATE;

        break;

    case LONG_OPTION_STATE:
        if (*p == null && has_option(context, p, as_long_option))
            return has_argument(context) ? clean(context, LONG_ARGUMENT_STATE) : invoke(context, p, ENTRY_STATE);

        if (*p == equal && has_option(context, p, as_long_option))
            return has_argument(context) ? clean(context, LONG_ARGUMENT_STATE) : invalid(context, p);

        if (isalnum(*p) || ispunct(*p))
            return LONG_OPTION_STATE;

        break;

    case LONG_ARGUMENT_STATE:
        if (*p == null)
            return invoke(context, p, ENTRY_STATE);

        return LONG_ARGUMENT_STATE;

    case OPERAND_STATE:
    case FORSED_OPERAND_STATE:
        if (*p == null)
            return has_other_option(context) ? invoke(context, p, state == OPERAND_STATE ? ENTRY_STATE : state) : invalid(context, p);

        return state;

    default:
        break;
    }

    return invalid(context, p);
}

int invoke_options(const char *synopsis, const struct option options[], const struct error errors[], int argc, char *argv[])
{
    struct context context = {synopsis, options, errors, 0, 0, 0};
    enum state state = ENTRY_STATE;
    const char *p = (argc--, *++argv);

    while (argc)
    {
        if ((state = process(&context, state, p)) == FAIL_STATE)
            break;

        p = *p ? p + 1 : (argc--, *++argv);
    }

    if ((state == LONG_ARGUMENT_STATE) || (state == SHORT_ARGUMENT_STATE))
        process(&context, state, "");

    return context.result;
}

static void usage(FILE *file, const char *p, int width)
{
    const char *s = p;
    int column = 0;

    while (*p)
    {
        if (isspace(*p))
        {
            if (column >= width)
            {
                fprintf(file, "\t%.*s\n", column, s);
                column = 0;
            }

            if (column == 0)
                s = p + 1;

            if (column)
                column++;
        }
        else
        {
            column++;
        }

        p++;
    }
    fprintf(file, "\t%.*s\n\n", column, s);
}

int usage_options(const char *synopsis, const struct option options[], const struct error errors[])
{
    const struct option *option = options;
    const struct error *error = errors;

    fprintf(stdout, TTY_NONE "Synopsis:\n");
    fprintf(stdout, TTY_NONE "\t%s\n\n", synopsis);
    fprintf(stdout, TTY_NONE "Options:\n");

    while (option->role != OTHER_OPTION)
    {
        if (option->short_name && option->long_name)
        {
            fprintf(stdout, TTY_BOLD "-%s, --%s", option->short_name, option->long_name);
        }
        else
        {
            if (option->short_name)
                fprintf(stdout, TTY_BOLD "-%s", option->short_name);

            if (option->long_name)
                fprintf(stdout, TTY_BOLD "--%s", option->long_name);
        }

        fprintf(stdout, option->role == JOINT_OPTION ? TTY_NONE " " TTY_UNLN "ARG\n" TTY_NONE : TTY_NONE "\n" TTY_NONE);
        usage(stdout, option->usage, 40);
        option++;
    }

    fprintf(stdout, TTY_NONE "Return results:\n");

    while (error->result)
    {
        fprintf(stdout, TTY_UNLN "%d" TTY_NONE "\t%s\n", error->result, error->usage);
        error++;
    }

    fprintf(stdout, TTY_UNLN "%d" TTY_NONE "\t%s\n", error->result, error->usage);
    fprintf(stdout, TTY_NONE "\nPrintitng help...");
    return 0;
}
