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

#include <stdio.h>
#include <memory.h>
#include "errors.h"
#include "buffer.h"

#define INTEL_DATA 0x00
#define INTEL_END_OF_FILE 0x01
#define INTEL_EXTENDED_ADDRESS 0x04
#define INTEL_START_ADDRESS 0x05

struct load_context
{
    uint32_t startup;
    uint32_t min;
    uint32_t max;
    uint32_t origin;
    size_t size;
    uint8_t *data;
    uint16_t shadow;
};

struct save_context
{
    uint32_t origin;
    size_t size;
    const uint8_t *data;
    uint16_t shadow;
};

static uint8_t *ihex32_data(struct load_context *context, uint32_t address)
{
    if (address >= context->origin && address <= context->origin + context->size - 1)
        return context->data + address - context->origin;

    return 0;
}

static int read_ihex32_chunk(struct load_context *context, FILE *stream)
{
    uint8_t size, scratch, checksum;
    uint16_t offset;

    if (fscanf(stream, ":%02hhX%04hX%02hhX", &size, &offset, &scratch) != 3)
        return INTERNAL_ERROR;

    checksum = size + scratch + offset + (offset >> 8);

    switch (scratch)
    {
    case INTEL_DATA:
        while (size--)
        {
            uint32_t address = (context->shadow << 16) + offset++;
            uint8_t *data = ihex32_data(context, address);

            if (!data)
                return INVALID_FILE_CONTENT;

            if (address > context->max)
                context->max = address;

            if (address < context->min)
                context->min = address;

            if (fscanf(stream, "%02hhX", &scratch) != 1)
                return INTERNAL_ERROR;

            checksum += scratch;
            *data = scratch;
        }
        break;

    case INTEL_END_OF_FILE:
        break;

    case INTEL_EXTENDED_ADDRESS:
        if (fscanf(stream, "%04hX", &context->shadow) != 1)
            return INTERNAL_ERROR;

        checksum += context->shadow + (context->shadow >> 8);
        break;

    case INTEL_START_ADDRESS:
        if (fscanf(stream, "%08X", &context->startup) != 1)
            return INTERNAL_ERROR;

        checksum += context->startup + (context->startup >> 8) + (context->startup >> 16) + (context->startup >> 24);
        break;

    default:
        return INVALID_FILE_CONTENT;
    }

    if (fscanf(stream, "%02hhX\n", &scratch) != 1)
        return INTERNAL_ERROR;

    if ((uint8_t)(checksum + scratch))
        return INVALID_FILE_CHECKSUM;

    return DONE;
}

int load_file_buffer(struct buffer *buffer, const char *file)
{
    struct load_context context =
    {
        0, 0xFFFFFFFF, 0x00000000, buffer->origin, buffer->size, (uint8_t *)buffer->data, 0
    };

    FILE *stream = fopen(file, "rt");
    if (!stream)
        return INTERNAL_ERROR;

    while (!feof(stream))
    {
        int result;
        if ((result = read_ihex32_chunk(&context, stream)))
            return result;
    }

    if (fclose(stream))
        return INTERNAL_ERROR;

    buffer->startup = context.startup;

    if (context.min > context.max)
    {
        buffer->size = 0;
    }
    else
    {
        buffer->size = context.max - context.min + 1;
        buffer->data = buffer->data + context.min - buffer->origin;
        buffer->origin = context.min;
    }

    return DONE;
}

static int write_ihex32_data(struct save_context *context, FILE *stream, uint8_t size)
{
    uint8_t checksum;

    if (fprintf(stream, ":%02X%04X00", size, (uint16_t)context->origin) != 9)
        return INTERNAL_ERROR;

    checksum = size + context->origin + (context->origin >> 8);

    while (size--)
    {
        if (fprintf(stream, "%02X", *context->data) != 2)
            return INTERNAL_ERROR;

        checksum += *context->data++;
        context->origin++;
        context->size--;
    }

    if (fprintf(stream, "%02X\n", (uint8_t)-checksum) != 3)
        return INTERNAL_ERROR;

    return DONE;
}

static int write_ihex32_address(struct save_context *context, FILE *stream)
{
    uint8_t checksum;

    if (context->origin >> 16 == context->shadow)
        return DONE;

    context->shadow = context->origin >> 16;
    checksum = 0x06 + context->shadow + (context->shadow >> 8);

    if (fprintf(stream, ":02000004%04X%02X\n", context->shadow, (uint8_t)(-checksum)) != 16)
        return INTERNAL_ERROR;

    return DONE;
}

static size_t ihex32_size(struct save_context *context)
{
    uint32_t end = context->origin + (context->size < 16 ? context->size : 16);

    if (context->origin >> 16 != end >> 16)
        end = end & 0xFFFF0000;

    return end - context->origin;
}

int save_file_buffer(struct buffer *buffer, const char *file)
{
    struct save_context context =
    {
        buffer->origin, buffer->size, (const uint8_t *)buffer->data, 0
    };

    FILE *stream = fopen(file, "wt");
    if (!stream)
        return INTERNAL_ERROR;

    while (context.size)
    {
        int result;
        int count = ihex32_size(&context);

        if ((result = write_ihex32_address(&context, stream)))
            return result;

        if ((result = write_ihex32_data(&context, stream, count)))
            return result;
    }

    if (fprintf(stream, ":00000001FF\n") != 12)
        return INTERNAL_ERROR;

    if (fclose(stream))
        return INTERNAL_ERROR;

    return DONE;
}

void clear_buffer(struct buffer *buffer, uint8_t value)
{
    memset(buffer->data, value, buffer->size);
}
