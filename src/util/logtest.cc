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

#include <config.h>
#include <log.hh>
//______________________________________________________________________

Logger info("Log-test");
DebugLogger debug("Flame-fest");

int main() {
  Logger::setEnabled("Log-test");

  string c = " (correct)";
  info("The answer: %1%2", 42, c);
  debug("yo");
  Logger::setEnabled("Flame-fest");
  debug("boo");
  return 0;
}

