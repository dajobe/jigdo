/* $Id$ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2003  |  richard@
  | \/¯|  Richard Atterer     |  atterer.net
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

  Test for addServer(), addPart()

  #test-deps job/url-mapping.o util/md5sum.o util/glibc-md5.o net/uri.o

*/

#include <config.h>

#include <debug.hh>
#include <log.hh>
#include <uri.hh>
#include <url-mapping.hh>
//______________________________________________________________________

namespace {

  // Record certain lines of the log
  const char* const UNIT = "url-mapping";
  string logged;
  bool unitEnabled = false; // Was the unit enabled with --debug?
  void loggerPut(const string& unitName, unsigned char unitNameLen,
                 const char* format, int args, const Subst arg[]) {
    if (unitName == UNIT) {
      if (strncmp(format, "add", 6) != 0) {
        logged += Subst::subst(format, args, arg);
        logged += '\n';
      }
      if (!unitEnabled) return;
    }
    Logger::defaultPut(unitName, unitNameLen, format, args, arg);
  }
  void loggerInit() {
    Logger::setOutputFunction(&loggerPut);
    Logger* l = Logger::enumerate();
    while (l != 0 && strcmp(l->name(), UNIT) == 0)
      l = Logger::enumerate(l);
    Assert(l != 0); // Unit not found? Probably wasn't compiled with DEBUG
    unitEnabled = l->enabled();
    if (!unitEnabled) Logger::setEnabled(UNIT);
  }

  // Compare actual/expected log output
  void expect(const char* s) {
    if (logged != s) {
      msg("Error: Expected \"%1\", but got \"%2\"", s, logged);
      Assert(logged == s);
    }
  }

  const string base = "http://baseurl/";

  // Add part
  inline void ap(UrlMap& m, const MD5& md, const char* s) {
    vector<string> v;
    v.push_back(s);
    m.addPart(base, md, v);
  }

  // Add server
  inline void as(UrlMap& m, const char* label, const char* s,
                 Status expectedReturnCode = OK) {
    vector<string> v;
    v.push_back(s);
    Status result = m.addServer(base, label, v);
    Assert(result == expectedReturnCode);
  }

  MD5 md[10];

} // namespace
//______________________________________________________________________

void test1() {
  UrlMap m;
  as(m, "LabelA", "http://myserver.org/");
  as(m, "LabelA", "ftp://mirror.myserver.org/");
  as(m, "LabelB", "LabelC:subdirectory/");
  as(m, "LabelC", "http://some.where.com/jigdo/");
  ap(m, md[0], "X:part0");
  ap(m, md[1], "X:part1");
  ap(m, md[0], "LabelB:some/path/part2");
  as(m, "X", "X=http://localhost:8000/~richard/ironmaiden/");
  ap(m, md[2], "X:part2");
  logged.erase();
  m.dumpJigdoInfo();
  expect("Part AQIDBAUGBwgJCgsMDQ4PEA: X + `part0'\n"
         "Part AQIDBAUGBwgJCgsMDQ4PEA: LabelB + `some/path/part2'\n"
         "Part ERITFBUWFxgZGhscHR4fIA: X + `part1'\n"
         "Part ISIjJCUmJygpKissLS4vMA: X + `part2'\n"
         "Server LabelA: http + `//myserver.org/'\n"
         "Server LabelA: ftp + `//mirror.myserver.org/'\n"
         "Server LabelB: LabelC + `subdirectory/'\n"
         "Server LabelC: http + `//some.where.com/jigdo/'\n"
         "Server X: X=http + `//localhost:8000/~richard/ironmaiden/'\n"
         "Server X=http:  + `X=http:'\n"
         "Server ftp:  + `ftp:'\n"
         "Server http:  + `http:'\n");
}

void test2() {
  UrlMap m;
  as(m, "A", "B");
  as(m, "A", "C:");
  as(m, "A", "D:");
  as(m, "C", "foobar");
  logged.erase();
  m.dumpJigdoInfo();
  expect("Server A: http + `//baseurl/B'\n"
         "Server A: D + `'\n"
         "Server A: C + `'\n"
         "Server C: http + `//baseurl/foobar'\n"
         "Server D:  + `D:'\n"
         "Server http:  + `http:'\n");
}

void test3() {
  // Loops disallowed
  UrlMap m;
  as(m, "asdf", "foo:x");
  as(m, "foo", "asdf:y", FAILED);
  logged.erase();
  m.dumpJigdoInfo();
  expect("Server asdf: foo + `x'\n"
         "Server foo:  + `y'\n"); // Not useful, avoids loop
}

void test4() {
  // Loops disallowed
  UrlMap m;
  as(m, "a", "b:");
  as(m, "b", "c:");
  as(m, "c", "d:");
  as(m, "d", "foo:");
  as(m, "d", "bar:");
  as(m, "d", "a:", FAILED);
  logged.erase();
  m.dumpJigdoInfo();
  expect("Server a: b + `'\n"
         "Server b: c + `'\n"
         "Server bar:  + `bar:'\n"
         "Server c: d + `'\n"
         "Server d: foo + `'\n"
         "Server d:  + `'\n" // Not useful, avoids loop
         "Server d: bar + `'\n"
         "Server foo:  + `foo:'\n");
}

int main(int argc, char* argv[]) {
  if (argc == 2) Logger::scanOptions(argv[1], argv[0]);
  loggerInit();

  int n = 0;
  for (int i = 0; i < 10; ++i)
    for (int j = 0; j < 16; ++j)
      md[i].sum[j] = ++n;

  test1();
  test2();
  test3();
  test4();
  return 0;
}
