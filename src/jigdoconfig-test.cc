/* $Id$ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2001-2002  |  richard@
  | \/¯|  Richard Atterer          |  atterer.net
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

  Representation for config data in a .jigdo file - based on ConfigFile

  #test-deps jigdoconfig.o util/configfile.o

*/

#include <iostream>

#include <jigdoconfig.hh>
#include <debug.hh>
//______________________________________________________________________

struct PR : public JigdoConfig::ProgressReporter {
  void error(const string& message, const size_t lineNr) {
    if (lineNr > 0) cerr << lineNr << ": ";
    cerr << message << endl;
  }
  void info(const string& message, const size_t lineNr) {
    if (lineNr > 0) cerr << lineNr << ": ";
    cerr << message << endl;
  }
} myPR;

int main(int argc, char* argv[]) {
  if (argc == 1) {
    cerr << "Syntax: " << argv[0] << " <config-file>" << endl;
    return 1;
  }
  JigdoConfig jc(argv[1], myPR);
}
