#Compiler
CXX = clang++
CXXFLAGS = -Wall -std=c++17

#Executable name
TARGET = bin/main

#Source Files
SRCS = src/main.cpp

#Object Files
OBJS = $(SRCS:.cpp=.o)

#Rule to build the executable
$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS)

#Rule to build object files
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@
#Clean Rule
clean:
	rm -f $(TARGET) $(OBJS)
