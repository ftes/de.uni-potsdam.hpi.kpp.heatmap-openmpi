CC=mpic++
CFLAGS=-c -Wall -Ofast -std=c++11
LDFLAGS=
SOURCES=main.cpp Hotspot.cpp Coordinate.cpp timing.cpp
OBJECTS=$(SOURCES:.cpp=.o)
EXECUTABLE=heatmap

all: $(SOURCES) $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(OBJECTS) -o $@ $(LDFLAGS)

.cpp.o:
	$(CC) $(CFLAGS) $< -o $@
