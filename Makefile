#
# Swamp-boot - flash memory programming for the STM32 microcontrollers
# Copyright (c) 2016 rksdna, fasked
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
#

# Project files

TARGET = swamp-boot
DESTDIR = /usr
BIN = $(TARGET)
SRC = $(wildcard *.c)
OBJ = $(SRC:.c=.o)
DEP = $(SRC:.c=.d)

# Tools and flags

CC = gcc
CP = cp
RM = rm -f

CFLAGS = -Wall -MD
LFLAGS =

-include $(DEP)

# Targets

.PHONY: all clean install

all: $(BIN)

$(BIN): $(OBJ)
	@echo "Linking $@..."
	@$(CC) $(LFLAGS) -o $@ $^

%.o: %.c
	@ echo "Compiling $@..."
	$(CC) -c $(CFLAGS) -o $@ $<

install: $(BIN)
	@echo "Installing $^..."
	$(CP) $< $(DESTDIR)/bin

clean:
	@echo "Cleaning..."
	$(RM) $(OBJ) $(DEP) $(BIN)

