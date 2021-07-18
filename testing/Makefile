EXECBIN  = queue_test
SOURCES  = $(EXECBIN).c ../queue.c
CFLAGS   = -Wall -Wextra -Wpedantic -Wshadow -O2 -pthread
CC       = gcc

all: $(EXECBIN)

$(EXECBIN): $(SOURCES)
	$(CC) $(CFLAGS) $(SOURCES) -o $@

grind: $(EXECBIN)
	valgrind --leak-check=full --show-reachable=yes ./$(EXECBIN)

clean:
	-rm -rf $(EXECBIN) $(EXECBIN).o queue.o



