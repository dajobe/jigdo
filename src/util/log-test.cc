/* $Id$ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2003  |  richard@
  | \/¯|  Richard Atterer     |  atterer.net
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

  Logfile / debugging output

  #test-deps

*/

#include <config.h>

#include <iostream>
#include <sstream>

#include <log.hh>
//______________________________________________________________________

Logger info("Log-test");
Logger debugg("Flame-fest");

namespace {

  ostringstream out;

  void put(const string& unitName, unsigned char unitNameLen,
           const char* format, int args, const Subst arg[]) {
    out << unitName << ':';
    if (unitNameLen < 15) out << "               " + unitNameLen;
    out << Subst::subst(format, args, arg) << endl;
  }

}

int main() {
  // Don't do this; it interferes and makes the test fail:
  //if (argc == 2) Logger::scanOptions(argv[1], argv[0]);

  Logger::setOutputFunction(&put);

  Logger::setEnabled("Log-test");
  string c = " (correct)";
  info("The answer: %1%2", 42, c);
  debugg("yo");
  Logger::setEnabled("Flame-fest");
  debugg("boo");

  Assert(out.str() ==
    "Log-test:       The answer: 42 (correct)\n"
    "Flame-fest:     boo\n");

  return 0;
}

