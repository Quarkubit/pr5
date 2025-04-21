CC = gcc
CFLAGS = -Wall -Wextra -I./src
BUILDDIR = .build/
SRCS = src/

SOURCE_NAME = $(notdir $(basename $(wildcard $(SRCS)*.c)))
TARGET = $(BUILDDIR)$(SOURCE_NAME)

OBJS=$(patsubst $(SRCS)%.c, $(BUILDDIR)%.o, $(wildcard $(SRCS)*.c))

$(shell mkdir -p $(BUILDDIR))

all: clean $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

$(BUILDDIR)%.o: $(SRCS)%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILDDIR)*

.PHONY: all clean
