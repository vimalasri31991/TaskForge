
CC = gcc
CFLAGS = -Wall -Wextra -Wpedantic -std=c11 -g
CPPFLAGS = -Iinclude
LDFLAGS = -pthread

SRC = src/taskforge.c src/queue.c src/future.c

MODULE1_TARGET = module1_demo
MODULE3_TARGET = module3_demo
MODULE4_TARGET = module4_demo

.PHONY: all clean run module1 module3 module4 debug asan tsan

all: module1 module3 module4

$(MODULE1_TARGET): tests/test_basic.c $(SRC)
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ $^ $(LDFLAGS)

$(MODULE3_TARGET): tests/test_cancellation.c $(SRC)
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ $^ $(LDFLAGS)

$(MODULE4_TARGET): tests/test_backpressure.c $(SRC)
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ $^ $(LDFLAGS)

module1: $(MODULE1_TARGET)

module3: $(MODULE3_TARGET)

module4: $(MODULE4_TARGET)

run: all
	./$(MODULE1_TARGET)
	./$(MODULE3_TARGET)
	./$(MODULE4_TARGET)

debug: CFLAGS += -O0
debug: clean all

asan: CFLAGS += -fsanitize=address -fno-omit-frame-pointer
asan: LDFLAGS += -fsanitize=address
asan: clean all

tsan: CFLAGS += -fsanitize=thread
tsan: LDFLAGS += -fsanitize=thread
tsan: clean all

clean:
	rm -f $(MODULE1_TARGET) $(MODULE3_TARGET) $(MODULE4_TARGET)
