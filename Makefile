CC = gcc

CFLAGS = -Wall -Wextra -std=c11 -O2 -Iinclude

DEBUG_FLAGS = -Wall -Wextra -std=c11 -g -O0 -Iinclude

LDFLAGS = -pthread


TARGET = taskforge_demo


SRC = src/queue.c \
      src/future.c \
      src/taskforge.c \
      tests/test_basic.c


.PHONY: all clean run debug asan tsan


# --------------------------------------------------
# NORMAL BUILD
# --------------------------------------------------

all:
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET) $(LDFLAGS)


# --------------------------------------------------
# DEBUG BUILD
# --------------------------------------------------

debug:
	$(CC) $(DEBUG_FLAGS) $(SRC) -o $(TARGET) $(LDFLAGS)


# --------------------------------------------------
# RUN
# --------------------------------------------------

run: all
	./$(TARGET)


# --------------------------------------------------
# CLEAN
# --------------------------------------------------

clean:
	rm -f $(TARGET)


# --------------------------------------------------
# ADDRESS SANITIZER
# --------------------------------------------------

asan:
	$(CC) \
	-Wall \
	-Wextra \
	-std=c11 \
	-g \
	-fsanitize=address \
	-Iinclude \
	$(SRC) \
	-o $(TARGET) \
	$(LDFLAGS)


# --------------------------------------------------
# THREAD SANITIZER
# --------------------------------------------------

tsan:
	$(CC) \
	-Wall \
	-Wextra \
	-std=c11 \
	-g \
	-fsanitize=thread \
	-Iinclude \
	$(SRC) \
	-o $(TARGET) \
	$(LDFLAGS)
