/* $Id$ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2003  |  richard@
  | \/¯|  Richard Atterer     |  atterer.net
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

  Logfile / debugging output

*/

#include <iostream>
#include <string>

#include <string.h>

#include <log.hh>
//______________________________________________________________________

/* Static head of linked list of Logger objects in different compilation
   units. Traversed e.g. to enable/disable specific units' messages with the
   --debug command line switch. */
Logger* Logger::list = 0;

Logger msg("general");

Logger::Logger(const char* unitName, bool enabled)
    : unitNameVal(unitName), unitNameLen(strlen(unitName)),
      enabledVal(enabled), next(list) {
  list = this;
}

bool Logger::setEnabled(const char* unitName, bool enable) {
  Logger* l = list;
  if (unitName == 0) {
    // Change all loggers
    while (l != 0) {
      l->enabledVal = enable;
      l = l->next;
    }
    return true;
  }

  // Only change one Logger
  while (l != 0) {
    if (strcmp(unitName, l->unitNameVal) == 0) {
      l->enabledVal = enable;
      return true;
    }
    l = l->next;
  }
  return false; // Not found
}

void Logger::put(const char* format, int args, const Subst arg[]) const {
  cerr << unitNameVal << ':';
  if (unitNameLen < 15) cerr << "               " + unitNameLen;
  cerr << Subst::subst(format, args, arg) << endl;
}
//______________________________________________________________________

/* The value of the --debug cmd line option is either missing (empty) or a
   comma-separated list of words (we also allow spaces for the fun of it).
   Each word can be preceded by a '~' for negation (i.e. disable debug
   messages rather than enable them). The word is the name of a compilation
   unit, or one of the special values "all" or "help". */
void Logger::scanOptions(const string& s, const char* binName) {
  unsigned i = 0;
  string word;
  bool enable;
  unsigned len = s.length();
  while (i < len) {
    word.erase();
    enable = true;
    while ((s[i] == '~' || s[i] == ' ') && i < len) {
      if (s[i] == '~') enable = !enable;
      ++i;
    }
    while (s[i] != ' ' && s[i] != ',' && i < len) {
      word += s[i]; ++i;
    }
    while ((s[i] == ',' || s[i] == ' ') && i < len) ++i;
    if (word == "all") { // Argument "all" - all units
      Logger::setEnabled(0, enable);
    } else if (word != "help") { // Other word - the name of a unit
      // Do not fail if unit not found - some units are only there with DEBUG
      if (!Logger::setEnabled(word.c_str(), enable)) {
        cerr << subst(_("%1: Unit `%2' not found while scanning --debug "
                        "argument"), binName, word) << endl;
        throw Cleanup(3);
      }
    } else { // Argument "help" - print list of units
      Logger* l = Logger::enumerate();
      cerr << _(
      "By default, debug output is disabled except for `assert'. Argument\n"
      "to --debug is a comma-separated list of unit names, or `all' for\n"
      "all units. Just `--debug' is equivalent to`--debug=all'. Output for\n"
      "the listed units is enabled, precede a name with `~' to disable it.\n"
      "Registered units:");
      while (l != 0) {
        cerr << ' ' << l->name();
        l = Logger::enumerate(l);
      }
      cerr << endl;
      throw Cleanup(3);
    }
  }
}
