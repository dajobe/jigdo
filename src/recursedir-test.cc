/* $Id$ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2001-2002  |  richard@
  | \/¯|  Richard Atterer          |  atterer.net
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

  Create recursive directory listing, avoiding symlink loops

*/

#include <config.h>

#include <iostream>

#define DEBUG 1
#include <debug.hh>
//______________________________________________________________________

#include <recursedir.cc>
#include <debug.cc>
//______________________________________________________________________

int main(int argc, const char* argv[]) {
  RecurseDir r;
  for (int i = 1; i < argc; ++i) {
    const char* arg = argv[i];
    if (arg[0] == '-') {
      cerr << "Reading filenames from `" << (arg + 1) << '\'' << endl;
      r.addFilesFrom(arg + 1);
    } else {
      r.addFile(arg);
    }
  }
  cerr << "Begin" << endl;
  string obj;
  while (true) {
    try {
      bool status = r.getName(obj);
      if (status) break;
      cout << obj << endl;
    }
    catch (RecurseError e) {
      cerr << "Caught RecurseError: " << e.message << endl;
    }
  }
  cerr << "Finished" << endl;
  return 0;
}
