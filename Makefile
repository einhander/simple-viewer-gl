CC=g++
CFLAGS=-O2 -c -Wall -I/usr/include
LDFLAGS=-s -lGL -lGLU -lglut -lImlib2

SOURCES= \
	src/imageborder.cpp \
	src/checkerboard.cpp \
	src/notavailable.cpp \
	src/progress.cpp \
	src/quad.cpp \
	src/quadimage.cpp \
	src/fileslist.cpp \
	src/imageloader.cpp \
	src/infobar.cpp \
	src/main.cpp \
	src/window.cpp

OBJECTS=$(SOURCES:.cpp=.o)
EXECUTABLE=sviewgl

all: $(SOURCES) $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@

.cpp.o:
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm $(OBJECTS) $(EXECUTABLE)
