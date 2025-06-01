#Compiler
CXX = g++
CXXFLAGS = -Wall -std=c++17

BINDIR = bin
SRCDIR = src

#Executable names 
PROXY = $(BINDIR)/myproxy
TARGET = $(PROXY) 

#Source Files
SRCS = $(SRCDIR)/myproxy.cpp

#Default rule
all: $(TARGET)

#Rule to build myclient
$(PROXY): src/myproxy.o
	$(CXX) $(CXXFLAGS) -o $@ $^

#Rule to build object files
$(SRCDIR)/%.o: $(SRCDIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

#Clean Rule
clean:
	rm -f $(PROXY) $(SRCDIR)/*.o
