EXECBIN  = httpserver
SOURCES  = $(EXECBIN).c queue.c util.c
OBJECTS  = $(SOURCES:.c=.o)
CFLAGS   = -Wall -Wextra -Wpedantic -Wshadow -O2 -pthread
CC       = gcc
INCLUDES = queue.h util.h

all: $(EXECBIN)

$(EXECBIN): $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) -o $@

%.o : %.c
	$(CC) $(CFLAGS) -c $<

format:
	clang-format -i $(SOURCES)

clean:
	-rm -rf $(OBJECTS)

spotless: clean
	-rm -rf $(EXECBIN)



