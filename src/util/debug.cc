/* $Id$ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 1999-2002 Richard Atterer
  | \/¯|  <atterer@informatik.tu-muenchen.de>
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

*/

#include <iostream>
#if defined DEBUG && defined HAVE_UNISTD_H
#  include <unistd.h> /* for sleep() */
#endif

#include <config.h>
#include <debug.hh>
#include <log.hh>
//______________________________________________________________________

#undef debug
namespace { Logger debug("assert", true); }
//______________________________________________________________________

int Debug::assertFail(const char* assertion, const char* file,
                      unsigned int line) {
  debug("%1:%2: `%3' failed", file, line, assertion);
  return 0;
}
//______________________________________________________________________

#if DEBUG && UNIX

#include <unistd.h>
#include <stdlib.h>

/* In order for memprof to be used, the process needs to sleep after
   exiting from main(). ~DebugSingleton() must be called after all
   other singleton dtors. This is ensured (non-portably) through link
   order. */
struct DebugSingleton {
  ~DebugSingleton() {
    if (getenv("_MEMPROF_SOCKET") != 0) sleep(60*60);
  }
};

namespace {
  DebugSingleton ds;
}

#endif
