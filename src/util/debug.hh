/* $Id$ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 1999-2003  |  richard@
  | \/¯|  Richard Atterer          |  atterer.net
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

*//** @file

  Debugging aids, including Error objects and assertions.

  The Error class is just a convenient base class for all kinds of
  errors which result in a text error message for printing to the
  user.

  'Paranoid()' is only compiled in if DEBUG is defined.
  'Assert()' is always compiled in.

  General rule of thumb: 'Assert' is used to check things possibly
  passed from other modules, 'Paranoid' for consistency checks where
  only the current module changes the state. ('Module' = e.g. source
  file and accompanying header, but may be >1 source file if
  appropriate.)

*/

#ifndef DEBUG_HH
#define DEBUG_HH

#include <config.h>

#include <algorithm>
#include <string>
//______________________________________________________________________

// class OptDebug {
// public:
//   static bool optDebug() { return d; }
//   static void setOptDebug(bool dd) { d = dd; }
// private:
//   OptDebug(); // Do not instantiate
//   static bool d;
// };

// inline bool optDebug() { return OptDebug::optDebug(); }
//______________________________________________________________________

/* For C (not C++), GCC 2.95 gives an error if a function param name is not
   given. GCC 3.4 gives a warning if it is given, but unused. Define param
   name to nothing for 3.4 */
#if defined __GNUC__ && __GNUC__ < 3
#  define UNUSED_PARAM(_p) _p
#else
#  define UNUSED_PARAM(_p)
#endif


/** To avoid warnings if a function argument is only used by Paranoid(), i.e.
    only if DEBUG is defined */
#if DEBUG
#  define DEBUG_ONLY_PARAM(_p) _p
#else
#  define DEBUG_ONLY_PARAM(_p)
#endif
//______________________________________________________________________

namespace Debug {

  /** True if to call abort() after failed assertion. Default is true. */
  extern bool abortAfterFailedAssertion;

  /** Print an "Assertion failed" message */
  extern int assertFail(const char* assertion, const char* file,
                        unsigned int line);

//   extern int assertMessage(const char* assertion, const char* file,
//                            unsigned int line);

}
#if DEBUG
#  define Paranoid(_expr) \
   ((void) ((_expr) ? 0 : \
            Debug::assertFail(#_expr, __FILE__, __LINE__)))
#else
#  define Paranoid(expr)
#endif

#define Assert(_expr) \
   ((void) ((_expr) ? 0 : \
            Debug::assertFail(#_expr, __FILE__, __LINE__)))
#define Message(_str) Debug::assertFail((_str), __FILE__, __LINE__)
//______________________________________________________________________

/** The messages are always in UTF-8! */
struct Error {
  explicit Error(const string& m) : message(m) { }
  explicit Error(const char* m) : message(m) { }
# if DEBUG
  explicit Error(string& m, bool flag) {
    Assert(flag == true); swap(message, m); }
# else
  explicit Error(string& m, bool) { swap(message, m); }
# endif
  string message;
};

/** Thrown to indicate: Don't report anything, but unwind stack and
    return specified value from main() */
struct Cleanup {
  explicit Cleanup(int r) : returnValue(r) { }
  int returnValue;
};
//______________________________________________________________________

#endif
