
SRC_PATH := src/
SRC_PATH += src/ofs/
SRC_PATH += src/utils/


MAIN_SRC := $(foreach path, $(SRC_PATH), $(wildcard $(path)*.c) )

SRC = $(MAIN_SRC)

OUTPUT_PATH = ./
OUTPUT_NAME = ofs.fs
OUTPUT = $(OUTPUT_PATH)$(OUTPUT_NAME)

LIBS_NAME := fuse3

CFLAGS = $(foreach path, $(SRC_PATH), -I $(path) ) `pkg-config --libs fuse3` -o $(OUTPUT)

debug:
	@$(CC) $(SRC) -w $(CFLAGS) -g

compile:
	@$(CC) $(SRC) -w $(CFLAGS)

#run: compile
#	@./$(OUTPUT)