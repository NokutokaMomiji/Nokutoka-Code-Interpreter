CC = gcc
CFLAGS = -g -Wall -std=c99
SRC_DIR := src
OBJ_DIR := obj
BIN_DIR := bin

EXE := $(BIN_DIR)/momiji

SRC := $(wildcard $(SRC_DIR)/*.c)
OBJ := $(SRC:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

MKDIR_P = mkdir -p
CP = cp

ifeq ($(OS),Windows_NT)
	RM = rm -r -force
	COPY = xcopy /t /e
else
	RM = rm -rf
	COPY = cp -r
endif

.PHONY: all debug clean

all: $(EXE)

debug: CFLAGS += -DDEBUG_TRACE_EXECUTION -DDEBUG_PRINT_CODE
debug: $(EXE)

$(EXE): $(OBJ) | $(BIN_DIR)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@ -g

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -I include -c $< -o $@ -g

$(BIN_DIR) $(OBJ_DIR):
	mkdir $@

clean:
	rm -rf $(OBJ_DIR)

-include $(OBJ:.o=.d)
