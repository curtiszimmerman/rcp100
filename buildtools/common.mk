CXX = g++
CXXFLAGS +=  -c -g -fstack-protector-all -Wall -Werror=format-security -fPIC -pie -fPIE -D_FORTIFY_SOURCE=2 -O2
CC=gcc
CFLAGS += -c -g -fstack-protector-all -Wall -Werror=format-security -fPIC -pie -fPIE -D_FORTIFY_SOURCE=2 -O2
LDFLAGS= -rdynamic -lrt -lpthread -z relro -z now
AR_OPT=rcs
INCLUDE += -I$(MAINLINE)/src/include

STATIC_LIBRARIES += $(MAINLINE)/src/librcp/librcp.a

all: $(EXECUTABLE) $(LIBRARY)

#-------------------------------------------------------
# .c and .cpp processing
#-------------------------------------------------------
CSOURCES+=$(wildcard *.[c])
CPPSOURCES+=$(wildcard *.cpp)
OBJECTS = $(CSOURCES:.c=.o) $(CPPSOURCES:.cpp=.o)

$(EXECUTABLE):$(OBJECTS) $(MAINLINE)/src/librcp/librcp.a
	$(CXX) $(OBJECTS) $(STATIC_LIBRARIES) $(LDFLAGS) -o $@; chmod 755 $(EXECUTABLE); cp $(EXECUTABLE) $(MAINLINE)/distro/bin/.

$(LIBRARY): $(OBJECTS)
	$(AR) $(AR_OPT) $(LIBRARY) $(OBJECTS)
	cp $(LIBRARY) $(MAINLINE)/distro/usr/lib/.

.cpp.o:
	$(CXX) $(CXXFLAGS) $(INCLUDE) $< -o $@

.c.o:
	$(CC) $(CFLAGS) $(INCLUDE) $< -o $@


clean:; rm -f $(OBJECTS) $(EXECUTABLE) $(LIBRARY) $(EXTRACLEAN) .dep

.dep:; $(CXX) $(INCLUDE) -M  $(wildcard *.[c]) $(wildcard *.cpp) > .dep

ifndef NODEP
include .dep
endif
