# === Source and Build Configuration ===
BIN_DIR    = ../bin
SRC_FILES  = ud_pingpong.c

all:
	mpicc -Wall -O0 -o $(BIN_DIR)/ud_pingpong ud_pingpong.c -libverbs

clean:
	rm -f $(BIN_DIR)/*
