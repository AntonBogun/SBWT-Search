# Makefile

# Variables
CXX = g++ 
CXXFLAGS = -Wall -Wextra -std=c++20 -O3 -g

# Name of the output executable
OUTPUT = diff_tool

all: $(OUTPUT)

$(OUTPUT): main.cpp
	$(CXX) $(CXXFLAGS) main.cpp -o $(OUTPUT)

clean:
	rm -f $(OUTPUT)
