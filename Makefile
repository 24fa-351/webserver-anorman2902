# Compiler and flags
CC = clang
CFLAGS = -Wall -g -lpthread

# Output executable
OUTPUT = webserver

# Source files
SRCS = webserver.c

# Detect if we're running in PowerShell or WSL
ifeq ($(shell uname -o 2>/dev/null), Msys)
    MAKECMD = wsl make
    RUNCMD = wsl ./$(OUTPUT)
else
    MAKECMD = $(MAKE)
    RUNCMD = ./$(OUTPUT)
endif

# Default target
all:
	$(MAKECMD) $(OUTPUT)

# Build the executable
$(OUTPUT): $(SRCS)
	$(CC) $(CFLAGS) -o $(OUTPUT) $(SRCS)

# Clean up build artifacts
clean:
	rm -f $(OUTPUT)

# Run the webserver
run: $(OUTPUT)
	$(RUNCMD) -p 8080
