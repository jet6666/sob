#
# Generic Makefile for C programs
#

OUTPUT = sob
SOURCES = $(wildcard *.c)
OBJECTS = $(patsubst %.c,%.o,$(SOURCES))
LIBS = -lz
INCLUDES =
CFLAGS = -O2 -g3 -Wall -Wextra -Wno-unused-parameter
LDFLAGS =

.PHONY: all
all: $(OUTPUT)

$(OUTPUT): $(OBJECTS)
	$(CC) -o $(OUTPUT) $(OBJECTS) $(LDFLAGS) $(LIBS)

%.o: %.c
	$(CC) $(INCLUDES) $(CFLAGS) -o $@ -c $<

.PHONY: clean
clean:
	$(RM) -f -r $(OUTPUT) $(OBJECTS)

