### Configuration variables

# C compiler
CC = gcc

# compiler flags
CFLAGS = -O2 -Wall -Wpedantic -Werror

# source and build directories
SRCDIR = src
OBJDIR = obj
BINDIR = bin

# executable to produce
TARGET = gopherbroke

# installation directory
INSTALL_PREFIX = /usr/local/


### Internal variables - shouldn't change

# Source files
SRC_FILES = $(wildcard $(SRCDIR)/*.c)

# Objects
OBJ_FILES = $(patsubst $(SRCDIR)/%.c, $(OBJDIR)/%.o, $(SRC_FILES))



### Build targets - shouldn't change

# default target
.PHONY: all
all: $(SRC_FILES) $(BINDIR)/$(TARGET)

$(BINDIR)/$(TARGET): $(OBJ_FILES)
	$(CC) $(OBJ_FILES) -o $@

$(OBJ_FILES): $(OBJDIR)/%.o: $(SRCDIR)/%.c
	$(CC) -c $(CFLAGS) $< -o $@

# install target
.PHONY: install
install:
	cp $(BINDIR)/$(TARGET) $(INSTALL_PREFIX)/bin/$(TARGET)

# uninstall target
.PHONY: uninstall
uninstall:
	rm $(INSTALL_PREFIX)/$(TARGET)

# clean build files
.PHONY: clean
clean:
	rm $(OBJDIR)/*.o $(BINDIR)/$(TARGET)
