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

#define DEBUG 1
#include <debug.hh>
#include <log.hh>
#include <uri.hh>
//______________________________________________________________________

#include <makeimagedl-graph.cc>

MakeImageDl::MakeImageDl(IO* ioPtr, const string& jigdoUri,
                         const string& destination)
    : io(ioPtr), stateVal(DOWNLOADING_JIGDO),
      jigdoUrl(jigdoUri), childrenVal(), dest(destination),
      tmpDirVal(), mi(),
      imageNameVal(), imageInfoVal(), imageShortInfoVal(), templateUrlVal(),
      templateMd5Val(0) {
}

Job::MakeImageDl::~MakeImageDl() {
}
//______________________________________________________________________

namespace {

  // Record certain lines of the log
  const char* const UNIT = "makeimagedl-graph";
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
  inline void ap(MakeImageDl& dl, const MD5& md, const char* s) {
    vector<string> v;
    v.push_back(s);
    dl.addPart(base, md, v);
  }

  // Add server
  inline void as(MakeImageDl& dl, const char* label, const char* s,
                 Status expectedReturnCode = OK) {
    vector<string> v;
    v.push_back(s);
    Status result = dl.addServer(base, label, v);
    Assert(result == expectedReturnCode);
  }

  MD5 md[10];

} // namespace
//______________________________________________________________________

using namespace Job;

void test1() {
  string dummy;
  MakeImageDl dl(0, dummy, dummy);
  as(dl, "LabelA", "http://myserver.org/");
  as(dl, "LabelA", "ftp://mirror.myserver.org/");
  as(dl, "LabelB", "LabelC:subdirectory/");
  as(dl, "LabelC", "http://some.where.com/jigdo/");
  ap(dl, md[0], "X:part0");
  ap(dl, md[1], "X:part1");
  ap(dl, md[0], "LabelB:some/path/part2");
  as(dl, "X", "X=http://localhost:8000/~richard/ironmaiden/");
  ap(dl, md[2], "X:part2");
  logged.erase();
  dl.dumpJigdoInfo();
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
  string dummy;
  MakeImageDl dl(0, dummy, dummy);
  as(dl, "A", "B");
  as(dl, "A", "C:");
  as(dl, "A", "D:");
  as(dl, "C", "foobar");
  logged.erase();
  dl.dumpJigdoInfo();
  expect("Server A: http + `//baseurl/B'\n"
         "Server A: D + `'\n"
         "Server A: C + `'\n"
         "Server C: http + `//baseurl/foobar'\n"
         "Server D:  + `D:'\n"
         "Server http:  + `http:'\n");
}

void test3() {
  // Loops disallowed
  string dummy;
  MakeImageDl dl(0, dummy, dummy);
  as(dl, "asdf", "foo:x");
  as(dl, "foo", "asdf:y", FAILED);
  logged.erase();
  dl.dumpJigdoInfo();
  expect("Server asdf: foo + `x'\n"
         "Server foo:  + `y'\n"); // Not useful, avoids loop
}

void test4() {
  // Loops disallowed
  string dummy;
  MakeImageDl dl(0, dummy, dummy);
  as(dl, "a", "b:");
  as(dl, "b", "c:");
  as(dl, "c", "d:");
  as(dl, "d", "foo:");
  as(dl, "d", "bar:");
  as(dl, "d", "a:", FAILED);
  logged.erase();
  dl.dumpJigdoInfo();
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
