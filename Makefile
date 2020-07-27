SOURCES = editor.cpp text.cpp terminal.cpp command.cpp bindings.cpp mode.cpp emacs.cpp history.cpp
OBJECTS = $(SOURCES:%.cpp=%.o)
INSTALL_HEADERS = editor.hpp text.hpp terminal.hpp command.hpp bindings.hpp mode.hpp emacs.hpp history.hpp forward-decls.hpp
TEST_SOURCES = test.cpp
LIB = libredline.a

CXX = $(GXX)
CXX ?= g++
LDFLAGS += -lcurses
CXXFLAGS += -O2 -Wall -Wextra

PREFIX ?= .

all : $(LIB)
clean :
	rm -f redline $(OBJECTS) $(LIB)
redline :
	ln -s . redline
$(LIB) : redline $(OBJECTS)
	rm -f $@
	$(AR) rusc $@ $(OBJECTS)
test : $(LIB) $(TEST_SOURCES)
install : $(LIB) $(INSTALL_HEADERS)
	mkdir -p $(PREFIX)/lib $(PREFIX)/include/redline
	cp -f $(LIB) $(PREFIX)/lib
	cp -f $(INSTALL_HEADERS) $(PREFIX)/include/redline

.PHONY : all install
