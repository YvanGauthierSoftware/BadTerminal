CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -O2
CPPFLAGS ?= -Iinclude
LDFLAGS ?=
LDLIBS ?=

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
LDLIBS += -lutil
endif

TARGET := badterm
SOURCES := src/main.c src/pty.c src/terminal.c src/event_loop.c
OBJECTS := $(SOURCES:.c=.o)
TEST_TARGET := test_core
TEST_SOURCES := tests/test_core.c src/pty.c src/event_loop.c
TEST_OBJECTS := $(TEST_SOURCES:.c=.o)

.PHONY: all clean run test

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) $(LDLIBS) -o $@

src/%.o: src/%.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

-include $(OBJECTS:.o=.d)

run: $(TARGET)
	./$(TARGET)

$(TEST_TARGET): $(TEST_OBJECTS)
	$(CC) $(LDFLAGS) $(TEST_OBJECTS) $(LDLIBS) -o $@

test: $(TEST_TARGET) $(TARGET)
	./$(TEST_TARGET)
	./tests/test_cli.sh ./$(TARGET)

clean:
	rm -f $(TARGET) $(OBJECTS) $(OBJECTS:.o=.d) $(TEST_TARGET) $(TEST_OBJECTS) $(TEST_OBJECTS:.o=.d)