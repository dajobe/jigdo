# Project: glibcurl (integration of glib event loop with libcurl)
#  __   _
#  |_) /|  Copyright (C) 2004  |  richard@
#  | \/¯|  Richard Atterer     |  atterer.net
#  ¯ '` ¯

CFLAGS =	-Wall -O2
CPPFLAGS =	-I. \
		$(shell pkg-config glib-2.0 --cflags) \
		$(shell curl-config --cflags)
LIBS =		$(shell pkg-config glib-2.0 --libs) \
		$(shell curl-config --libs)

all:		glibcurl-example

run:		all
		./glibcurl-example http://localhost:8000/~richard/ironmaiden/image

clean:
		rm -f glibcurl-example glibcurl-example.o glibcurl.o

glibcurl-example: glibcurl-example.o glibcurl.o
		$(CC) -o $@ glibcurl-example.o glibcurl.o $(LIBS) $(LDFLAGS)

glibcurl-example.o: glibcurl.o
