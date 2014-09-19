EXECUTABLE = cmdparser
all: $(EXECUTABLE)

CXX = g++
CC=gcc
CFLAGS += -c -g
LDFLAGS= 

#-------------------------------------------------------
# .c and .cpp processing
#-------------------------------------------------------
CSOURCES+=$(wildcard *.[c])
OBJECTS = $(CSOURCES:.c=.o)

$(EXECUTABLE):$(OBJECTS)
	$(CXX) $(LDFLAGS) $(OBJECTS) -o $@

.c.o:
	$(CC) $(CFLAGS) $(INCLUDE) $< -o $@


clean:; rm -f $(OBJECTS) $(EXECUTABLE) .dep

.dep:; $(CXX) -M  $(wildcard *.[c]) > .dep

ifndef NODEP
include .dep
endif
