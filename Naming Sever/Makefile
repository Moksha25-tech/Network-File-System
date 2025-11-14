GLOBAL_DEPS_SRC = ..
GLOBAL_DEPS = Externals.c 
CC = /usr/bin/gcc
CFLAGS = -fdiagnostics-color=always -g
SRC_DIR = .
OBJ_DIR = obj
TARGET = NS
PORTS_TO_FREE = 8080 8081
LOG_FILE = NSlog.log

SRC_FILES = $(wildcard $(SRC_DIR)/*.c)
OBJ_FILES = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRC_FILES))

.PHONY: NS clean free_ports

all: $(TARGET) free_ports
$(TARGET): $(OBJ_FILES)
	$(CC) $(CFLAGS) $(GLOBAL_DEPS_SRC)/$(GLOBAL_DEPS) $^ -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

free_ports:
	@for port in $(PORTS_TO_FREE); do \
        echo "Releasing port $$port"; \
        lsof -ti :$$port | xargs -r kill; \
    done

clean:
	@echo "Cleaning up..."
	rm -rf $(OBJ_DIR) $(TARGET) 
	@if [ -n "$(log)" ] && [ "$(log)" = "true" ]; then \
		echo "Removing log file..."; \
    	rm -rf $(LOG_FILE); \
	fi

log:
	cat $(LOG_FILE) | less

compile: clean all
	@echo "Compile complete"
	sleep 1
	clear
	@echo "Running the program..."
	./$(TARGET) 
	@echo "Program has finished running"
	@echo "To view the log file, run 'make logs'"