CC = gcc
CFLAGS = -Wall -std=c99
BINDIR = bin

# detectar OS
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S), Linux)			# linux
    LIBS = -pthread
else ifeq ($(UNAME_S), Darwin)   	# mac
    LIBS = -pthread
else                              	# windows
    LIBS = -pthread -lws2_32
endif

$(shell mkdir -p $(BINDIR))

TARGETS = server client

all: $(TARGETS)

%: %.c
	$(CC) $(CFLAGS) -o $(BINDIR)/$@ $^ $(LIBS)

server: server.h
client: # sin header

.PHONY: clean
clean:
	rm -f $(addprefix $(BINDIR)/, $(TARGETS))