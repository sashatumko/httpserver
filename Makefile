TARGET=httpserver
SOURCES=$(TARGET).cpp queue.cpp
CXXFLAGS=-std=gnu++11 -g -Wall -Wextra -Wpedantic -Wshadow -pthread -O2
OBJECTS=$(SOURCES:.cpp=.o)
DEPS=$(SOURCES:.cpp=.d)
INCLUDES = queue.h

CXX=clang++

all: $(TARGET)

clean:
	-rm $(DEPS) $(OBJECTS)

spotless: clean
	-rm $(TARGET)

format:
	clang-format -i $(SOURCES) $(INCLUDES)

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJECTS)

-include $(DEPS)

.PHONY: all clean format spotless

memcheck :
	valgrind --leak-check=full ./rpcserver localhost:8912