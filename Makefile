# Compiler
CC = clang

# Libraries
LIBS =

# Directories
PROJECT_DIR = $(HOME)/entity-component-system
SRC_DIR = src
BUILD_DIR = $(PROJECT_DIR)/build
OBJ_DIR = $(BUILD_DIR)/obj
BIN = $(BUILD_DIR)/ecs

# Flags
WARN_FLAGS = -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wstrict-prototypes
SANITIZE ?= address,undefined
SANITIZE_FLAGS = $(if $(1),-fsanitize=$(1) -fno-omit-frame-pointer)
FLAGS = $(WARN_FLAGS) -std=c17 -I$(SRC_DIR) -g -O1 $(call SANITIZE_FLAGS,$(SANITIZE))

# Sources
SRCS = $(shell find $(SRC_DIR) -type f -name '*.c')
OBJS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRCS))

.PHONY: all run build clean

all: run

run: build
	@"$(BIN)"

build: $(BIN)

clean:
	@rm -rf $(BUILD_DIR)

$(BIN): $(OBJS)
	@mkdir -p $(@D)
	$(CC) $(FLAGS) $^ $(LIBS) -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(@D)
	$(CC) $(FLAGS) -MMD -MP -c $< -o $@

-include $(OBJS:.o=.d)