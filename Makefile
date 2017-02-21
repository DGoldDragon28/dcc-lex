CC = gcc # will eventually be dcc
CFLAGS = -std=c99 -Wall -W -pedantic -O2 -LC:/MinGW/msys/1.0/lib
EXEC = dcc-lex
OBJS = main.o
INCL =

default: $(EXEC)

$(EXEC): $(OBJS)
	$(CC) $(CFLAGS) -o $(EXEC) $(OBJS)

%.o: %.c $(INCL)
	$(CC) $(CFLAGS) -c -o $@ $<

debug: $(OBJS)
	$(CC) $(CFLAGS) -g -o $(EXEC) $(OBJS)

clean:
	rm -f $(EXEC) $(OBJS)

all: clean $(EXEC)

.PHONY: default clean all debug

