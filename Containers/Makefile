CC=g++
CXX=g++

EXESRC=sockets.cpp container.cpp
EXEOBJ=$(EXESRC:.cpp=)

CFLAGS = -Wall -Wvla -std=c++11
CXXFLAGS = -Wall -Wvla -std=c++11

TAR=tar
TARFLAGS=-cvf
TARNAME=ex5.tar
TARSRCS=$(EXESRC) Makefile README

all: $(EXEOBJ)

$(sockets) $(container): % : %.cpp
	$(CXX) $(CXXFLAGS) $@ -o $^

clean:
	$(RM) $(TARGETS) $(OSMLIB) $(OBJ) $(EXEOBJ) *~ *core

depend:
	makedepend -- $(CFLAGS) -- $(SRC) $(LIBSRC)

tar:
	$(TAR) $(TARFLAGS) $(TARNAME) $(TARSRCS)