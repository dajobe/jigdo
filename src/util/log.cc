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

DebugLogger msg("general");

Logger::Logger(const char* unitName)
    : unitNameVal(unitName), unitNameLen(strlen(unitName)),
      enabledVal(false), next(list) {
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
