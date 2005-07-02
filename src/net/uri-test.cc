/* $Id$ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2004  |  richard@
  | \/¯|  Richard Atterer     |  atterer.net
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

  #test-deps glibcurl/glibcurl.o net/uri.o compat.o
  #test-ldflags $(CURLLIBS) $(LDFLAGS_WINSOCK)

*/

#define DEBUG 1
#include <config.h>

#include <log.hh>
#include <uri.hh>

namespace {

  void testUriJoin(const char* base, const char* rel, const char* expected) {
    string s = "anything", b = base, r = rel;
    uriJoin(&s, b, r);
    msg("base=%1, rel=%2, result=%3, expected=%4", b, r, s, expected);
    Assert(s == expected);
  }

}

int main(int argc, char* argv[]) {
  if (argc == 2) Logger::scanOptions(argv[1], argv[0]);

  testUriJoin("foo", "bar", "foo/bar");
  testUriJoin("http://base/", "rel", "http://base/rel");
  testUriJoin("http://base/boo.html", "rel", "http://base/rel");
  testUriJoin("http://base/", "http://wah/", "http://wah/");
  testUriJoin("http://base/", "telnet://", "telnet://");

  /* Should ideally eliminate ../ if possible. */
  testUriJoin("http://base/", "../../x", "http://base/x");
  testUriJoin("http://base/a/b/", "../x/./y", "http://base/a/x/y");

  testUriJoin("http://cdimage.debian.org/pub/cdimage-testing/cd/jigdo-area/"
              "i386/sarge-i386-1.jigdo", "/debian-cd/debian-servers.jigdo",
              "http://cdimage.debian.org/debian-cd/debian-servers.jigdo");
  testUriJoin("http://cdimage.debian.org",
              "/debian-cd/debian-servers.jigdo",
              "http://cdimage.debian.org/debian-cd/debian-servers.jigdo");

  testUriJoin("http://host/leaf", "foo", "http://host/foo");
  testUriJoin("http://host/leaf", "../../foo", "http://host/foo");
  testUriJoin("http://host/leaf", "../../foo/", "http://host/foo/");
  testUriJoin("http://host/a/b/c", "/bar", "http://host/bar");
  testUriJoin("http://host/a/b/c", "/", "http://host/");

  msg("Exit");
  return 0;
}
