/* $Id$ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2003  |  richard@
  | \/¯|  Richard Atterer     |  atterer.net
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

  Test for addServer(), addPart()

  #test-deps job/url-mapping.o util/configfile.o util/md5sum.o util/glibc-md5.o net/uri.o

*/

#include <config.h>

#include <configfile.hh>
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
    logged.erase();
  }

  const string base = "http://baseurl/";

  // Add part
  void ap(UrlMap& m, const MD5& md, const char* s) {
    vector<string> value;
    ConfigFile::split(value, s, 0);
//     vector<string> v;
//     v.push_back(s);
    const char* result = m.addPart(base, md, value);
    if (result != 0) msg("addPart: %1", result);
    Assert(result == 0);
  }

  // Add server
  void as(UrlMap& m, const char* label, const char* s,
          bool expectFailure = false) {
    vector<string> value;
    ConfigFile::split(value, s, 0);
//     vector<string> v;
//     v.push_back(s);
    const char* result = m.addServer(base, label, value);
    if (result != 0) msg("addServer: %1", result);
    Assert(expectFailure == (result != 0));
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
  as(m, "foo", "asdf:y", true);
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
  as(m, "d", "a:", true);
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
//______________________________________________________________________

void expectEnum(PartUrlMapping* m, const char* expected) {
  vector<UrlMapping*> best;
  string result;
  while (true) {
    string s = m->enumerate(&best);
    if (s.empty()) break;
    if (!result.empty()) result += ' ';
    result += s;
  }
  if (result != expected) {
    msg("Error: Expected \"%1\", but got \"%2\"", expected, result);
    Assert(result == expected);
  }
  msg("OK, got \"%1\"", result);
}

/* NB: In the following tests, all mappings must have different
   weights. Otherwise, different rounding errors in different implementations
   will lead to a different output order, and make the tests fail. */

void score1() { // Single leaf object
  UrlMap m;
  vector<string> v;
  v.push_back("fooboo/bar/baz");
  m.addPart("", md[0], v);
  m.dumpJigdoInfo();
  expectEnum(m[md[0]], "fooboo/bar/baz");
}

void score2() { // Simple chain, 2 objects long
  UrlMap m;
  ap(m, md[0], "fooboo/bar/baz");
  m.dumpJigdoInfo();
  expectEnum(m[md[0]], "http://baseurl/fooboo/bar/baz");
}

void score3() { // Diamond-shaped graph
  UrlMap m;
  as(m, "Label", "Server:x/ --try-first=", true);
  ap(m, md[0], "Label:some/path");
  as(m, "Label", "Server:y/ --try-first");
  as(m, "Server", "ftp://server.org:80/");
  m.dumpJigdoInfo();
  expectEnum(m[md[0]],
             "ftp://server.org:80/y/some/path "
             "ftp://server.org:80/x/some/path");
}

void score4() { // Full mesh, many leaf possibilities
  UrlMap m;
  ap(m, md[1], "A:a --try-first");
  ap(m, md[1], "A:b");
  ap(m, md[1], "A:c --try-last");
  ap(m, md[1], "A:d --try-first=3.0");
  as(m, "A", "S:l --try-first=-13");
  as(m, "A", "S:m --try-last=.222");
  as(m, "A", "S:n --try-first=3.4");
  as(m, "A", "S:o --try-first --try-first=6.5");
  as(m, "S", "p://x/");
  m.dumpJigdoInfo();
  expectEnum(m[md[1]], "p://x/od p://x/oa p://x/ob p://x/oc p://x/nd p://x/na "
             "p://x/nb p://x/md p://x/nc p://x/ma p://x/mb p://x/mc p://x/ld "
             "p://x/la p://x/lb p://x/lc");
}

void score5() { // Full mesh, one leaf possibility
  UrlMap m;
  ap(m, md[1], "A:.");
  as(m, "A", "S:l --try-first=.1");
  as(m, "A", "S:m --try-first=.21");
  as(m, "A", "S:n --try-first=.32");
  as(m, "A", "S:o --try-last=.222");
  as(m, "S", "T:a --try-first=.4");
  as(m, "S", "T:b --try-first=.5");
  as(m, "S", "T:c --try-first=.6");
  as(m, "S", "T:d --try-first=3.4");
  as(m, "T", "http://x/");
  m.dumpJigdoInfo();
  expectEnum(m[md[1]], "http://x/dn. http://x/dm. http://x/dl. http://x/do. "
             "http://x/cn. http://x/bn. http://x/cm. http://x/an. "
             "http://x/bm. http://x/cl. http://x/am. http://x/bl. "
             "http://x/al. http://x/co. http://x/bo. http://x/ao.");
}
//______________________________________________________________________

int main(int argc, char* argv[]) {
  if (argc == 2) Logger::scanOptions(argv[1], argv[0]);

  int n = 0;
  for (int i = 0; i < 10; ++i)
    for (int j = 0; j < 16; ++j)
      md[i].sum[j] = ++n;

  msg("Score tests");
  UrlMapping::setNoRandomInitialWeight();
  score1();
  score2();
  score3();
  score4();
  score5();

  msg("Graph build tests");
  loggerInit();
  test1();
  test2();
  test3();
  test4();
  return 0;
}
