/* $Id$ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2001-2002  |  richard@
  | \/¯|  Richard Atterer          |  atterer.net
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

  Access to Gnome/KDE/ini-style configuration files

  #test-deps util/configfile.o

*/

#include <config.h>

#include <iostream>
#include <fstream>
#include <vector>

#include <configfile.hh>
#include <debug.hh>
//______________________________________________________________________

int main(int argc, char* argv[]) {
  if (argc == 1) {
    cerr << "Syntax: " << argv[0] << " <config-file>\n"
            "        " << argv[0] << " <config-file> <section-name>\n"
            "        " << argv[0] << " <config-file> <section-name> "
            "<label-name>" << endl;
    return 1;
  }

  ConfigFile cfg;
  ifstream cfgFile(argv[1]);
  cfgFile >> cfg;
  cfgFile.close();

  if (argc == 2) {
    //________________________________________

    // Go once through file and precede [subject] lines with "#"
    cout << "File `" << argv[1] << "' has " << cfg.size() << " lines:"
         << endl;
    for (ConfigFile::iterator i = cfg.begin(), e = cfg.end(); i != e; ++i) {
      if (i.isSection()) cout << '#'; else cout << '-';
      cout << *i << endl;
    }

    // Output section headers
    cout << "Sections:" << endl;
    for (ConfigFile::iterator i = cfg.firstSection(), e = cfg.end();
         i != e; i.nextSection())
      cout << *i << endl;
    //________________________________________

  } else if (argc == 3) {

    // Output all [section] lines for the section named
    string sectName = argv[2];
    cout << "Section lines for `[" << sectName << "]':" << endl;
    ConfigFile::iterator i = cfg.firstSection(sectName);
    while (i != cfg.end()) {
      cout << *i << endl;
      i.nextSection(sectName);
    }
    //________________________________________

  } else if (argc == 4) {

    // Output all matching label lines
    string sectionName = argv[2];
    string labelName = argv[3];
    cout << "Lines matching section `[" << sectionName << "]', label `"
         << labelName << '\'' << endl;

    size_t off;
    for (ConfigFile::Find f(&cfg, sectionName, labelName, &off);
         !f.finished(); off = f.next()) {
      cout << &*f.section() << ' '; // Address of section line
      string::iterator x = f.label()->begin();
      x += off - 1;
      *x = '|'; // Overwrite '=' with '|'
      cout << *f.label() << endl;
      (*f.label())[off - 1] = '=';
      // Also split value into words
      vector<string> words;
      ConfigFile::split(words, *f.label(), off);
      cout << "         ";
      for (vector<string>::iterator i = words.begin(), e = words.end();
           i != e; ++i)
        cout << " \x1b[7m" << *i << "\x1b[27m";
      cout << endl;
    }
    //________________________________________
  }

  return 0;
}
