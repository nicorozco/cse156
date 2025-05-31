#Compiler
CXX = g++
CXXFLAGS = -Wall -std=c++17

BINDIR = bin
SRCDIR = src

#Executable names 
CLIENT = $(BINDIR)/myclient
SERVER = $(BINDIR)/myserver
TARGET = $(CLIENT) $(SERVER)


#Source Files
SRCS = $(SRCDIR)/myclient.cpp $(SRCDIR)/myserver.cpp

#Default rule
all: $(TARGET)

#Rule to build myclient
$(CLIENT): src/myclient.o
	$(CXX) $(CXXFLAGS) -o $@ $^

#Rule to build myserver
$(SERVER): src/myserver.o
	$(CXX) $(CXXFLAGS) -o $@ $^

#Rule to build object files
$(SRCDIR)/%.o: $(SRCDIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

#Clean Rule
clean:
	rm -f $(CLIENT) $(SERVER) $(SRCDIR)/*.o
