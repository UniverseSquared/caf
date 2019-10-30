OBJS = \
	src/editor.o \
	src/terminal.o \
	src/main.o
BIN = caf
CFLAGS = -g

.PHONY: all clean

all: $(BIN)

clean:
	rm -f $(BIN) $(OBJS)

$(BIN): $(OBJS)
	$(CC) -o $@ $^ $(CFLAGS)

%.o: %.c
	$(CC) -o $@ -c $< $(CFLAGS)
