CC = gcc -g -D DEBUG -D SINGLE
CFLAGS = -Wall -O2 -g -D DEBUG -D SINGLE

web_srv: clean web_srv.cpp web_srv.h rio.h rio.cpp  static.cpp static.h
	$(CC) $(CFLAGS) -o web_srv rio.cpp web_srv.cpp network.cpp static.cpp -D_GNU_SOURCE 2> sk

dist: clean
	$(CC) $(CFLAGS) -o web_srv  rio.cpp web_srv.cpp network.cpp static.cpp -D_GNU_SOURCE -DPRODUCTION 2> sk
	strip web_srv

epoll: network.cpp network.h web_srv.cpp
	$(CC) $(CFLAGS) -o network network.cpp web_srv.cpp -D_GNU_SOURCE 2> sk

clean:
	rm network web_srv a.out e -f
