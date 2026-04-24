# Compiler
CXX = g++
CXXFLAGS = -std=c++11 -Wall -Wextra -O2

# Target executable
TARGET = raycast

# Source files
SRCS = rayTracer.cpp

# Object files
OBJS = $(SRCS:.cpp=.o)

# Default rule
all: $(TARGET)

# Link
$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS)

# Compile
%.o: %.cpp raycast_lib.h
	$(CXX) $(CXXFLAGS) -c $<

# Clean
clean:
	rm -f $(OBJS) $(TARGET)