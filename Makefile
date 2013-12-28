CC=g++
CFLAGS=-c -Wall -Ofast
LDFLAGS=-lOpenCL
SOURCES=main.cpp Hotspot.cpp Coordinate.cpp
OBJECTS=$(SOURCES:.cpp=.o)
EXECUTABLE=heatmap

all: $(SOURCES) $(EXECUTABLE)
	
$(EXECUTABLE): $(OBJECTS) 
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@

.cpp.o:
	$(CC) $(CFLAGS) $< -o $@
