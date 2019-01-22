# Copyright (C) 2018 Paul Kocialkowski <paul.kocialkowski@bootlin.com>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.


# Tools

CC = gcc

# Project

NAME = v4l2-request-test

# Directories

BUILD = build
OUTPUT = .

# Sources

SOURCES = v4l2-request-test.c v4l2.c drm.c presets.c
OBJECTS = $(SOURCES:.c=.o)
DEPS = $(SOURCES:.c=.d)

# Compiler

CFLAGS += -Wunused-variable -Iinclude
CFLAGS += $(shell pkg-config --cflags libdrm)
LDFLAGS = $(shell pkg-config --libs libdrm)

# Produced files

BUILD_OBJECTS = $(addprefix $(BUILD)/,$(OBJECTS))
BUILD_DEPS = $(addprefix $(BUILD)/,$(DEPS))
BUILD_BINARY = $(BUILD)/$(NAME)
BUILD_DIRS = $(sort $(dir $(BUILD_BINARY) $(BUILD_OBJECTS)))

OUTPUT_BINARY = $(OUTPUT)/$(NAME)
OUTPUT_DIRS = $(sort $(dir $(OUTPUT_BINARY)))

all: $(OUTPUT_BINARY)

$(BUILD_DIRS):
	@mkdir -p $@

$(BUILD_OBJECTS): $(BUILD)/%.o: %.c | $(BUILD_DIRS)
	@echo " CC     $<"
	@$(CC) $(CFLAGS) -MMD -MF $(BUILD)/$*.d -c $< -o $@

$(BUILD_BINARY): $(BUILD_OBJECTS)
	@echo " LINK   $@"
	@$(CC) $(CFLAGS) -o $@ $(BUILD_OBJECTS) $(LDFLAGS)

$(OUTPUT_DIRS):
	@mkdir -p $@

$(OUTPUT_BINARY): $(BUILD_BINARY) | $(OUTPUT_DIRS)
	@echo " BINARY $@"
	@cp $< $@

.PHONY: clean
clean:
	@echo " CLEAN"
	@rm -rf $(foreach object,$(basename $(BUILD_OBJECTS)),$(object)*) $(basename $(BUILD_BINARY))*
	@rm -rf $(OUTPUT_BINARY)

.PHONY: distclean
distclean: clean
	@echo " DISTCLEAN"
	@rm -rf $(BUILD)

-include $(BUILD_DEPS)
