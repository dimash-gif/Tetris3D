CXX = g++
CXXFLAGS = -std=c++17 -O2 -Iinclude -Wall -Wextra
LDFLAGS = -lglfw -ldl -lGL -pthread

SRCDIR = src
BINDIR = .
SOURCES = $(SRCDIR)/main.cpp $(SRCDIR)/glad.c
TARGET = $(BINDIR)/tetris3d

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CXX) $(CXXFLAGS) $(SOURCES) -o $(TARGET) $(LDFLAGS)

clean:
	rm -f $(TARGET)

