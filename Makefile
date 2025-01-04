CASHIER_EXEC = cashier
CLIENT_EXEC = client
FIREMAN_EXEC = fireman
MANAGER_EXEC = manager

SRCS = cashier.c client.c fireman.c manager.c helper.c
HEADERS = helper.h

CC = gcc
CFLAGS = -Wall -Wextra -pedantic -std=c99 -g
LDFLAGS = -pthread

all: $(CASHIER_EXEC) $(CLIENT_EXEC) $(FIREMAN_EXEC) $(MANAGER_EXEC)

$(CASHIER_EXEC): cashier.c helper.c $(HEADERS)
	$(CC) $(CFLAGS) -o $@ cashier.c helper.c $(LDFLAGS)

$(CLIENT_EXEC): client.c helper.c $(HEADERS)
	$(CC) $(CFLAGS) -o $@ client.c helper.c $(LDFLAGS)

$(FIREMAN_EXEC): fireman.c helper.c $(HEADERS)
	$(CC) $(CFLAGS) -o $@ fireman.c helper.c $(LDFLAGS)

$(MANAGER_EXEC): manager.c helper.c $(HEADERS)
	$(CC) $(CFLAGS) -o $@ manager.c helper.c $(LDFLAGS)

clean:
	rm -f $(CASHIER_EXEC) $(CLIENT_EXEC) $(FIREMAN_EXEC) $(MANAGER_EXEC) *.o

distclean: clean
	rm -f *~ core

.PHONY: all clean distclean

