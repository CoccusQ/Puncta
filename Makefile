CC     = gcc
CFLAGS = -Wall -Wextra -Wunused-macros -O3
TARGET = puncta.exe
SRC    = puncta.c

all: $(TARGET)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

.PHONY: clean

clean:
	rm -f $(TARGET)
