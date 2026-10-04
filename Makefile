CC = gcc

CFLAGS = -Wall -Wextra -std=c11 -O2 -Iinclude

LDFLAGS = -pthread

TARGET = taskforge_demo

SRC = src/queue.c \
      src/future.c \
      src/taskforge.c \
      tests/test_basic.c

all:
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET) $(LDFLAGS)

debug:
	$(CC) -Wall -Wextra -std=c11 -g -O0 \
	-Iinclude $(SRC) -o $(TARGET) $(LDFLAGS)

clean:
	rm -f $(TARGET)

run: all
	./$(TARGET)

asan:
	$(CC) -Wall -Wextra -std=c11 -g \
	-fsanitize=address \
	-Iinclude $(SRC) -o $(TARGET) $(LDFLAGS)

tsan:
	$(CC) -Wall -Wextra -std=c11 -g \
	-fsanitize=thread \
	-Iinclude $(SRC) -o $(TARGET) $(LDFLAGS)
