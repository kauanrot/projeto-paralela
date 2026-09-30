CC      = gcc
CFLAGS  = -O2 -Wall -Wextra -pthread
SRC     = src
BIN     = bin
TARGETS = $(BIN)/log_analyzer_seq $(BIN)/log_analyzer_par $(BIN)/log_analyzer_par_optimized

all: $(TARGETS)

$(BIN)/%: $(SRC)/%.c | $(BIN)
	$(CC) $(CFLAGS) -o $@ $<

$(BIN):
	mkdir -p $(BIN)

clean:
	rm -rf $(BIN)

.PHONY: all clean
