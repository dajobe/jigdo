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

#ifndef LOG_HH
#define LOG_HH

#include <config.h>
#include <debug.hh>
#include <nocopy.hh>
#include <string-utf.hh>
//______________________________________________________________________

#if DEBUG
#  define DebugLogger Logger
#else
struct DebugLogger : NoCopy  {
  DebugLogger(const char*) { }
  bool enabled() const { return false; }
  operator bool() const { return false; }
  void operator()(const char*) const { }
  void operator()(const char*, Subst) const { }
  void operator()(const char*, Subst, Subst) const { }
  void operator()(const char*, Subst, Subst, Subst) { }
  void operator()(const char*, Subst, Subst, Subst, Subst) { }
  void operator()(const char*, Subst, Subst, Subst, Subst, Subst) { }
  void operator()(const char*, Subst, Subst, Subst, Subst, Subst, Subst) { }
  void operator()(const char*, Subst, Subst, Subst, Subst, Subst, Subst,
                  Subst) { }
  void operator()(const char*, Subst, Subst, Subst, Subst, Subst, Subst,
                  Subst, Subst) { }
  void operator()(const char*, Subst, Subst, Subst, Subst, Subst, Subst,
                  Subst, Subst, Subst) { }
};
#endif
//______________________________________________________________________

class Logger : NoCopy {
public:
  /** Register a compilation unit. All Loggers MUST *MUST* be static objects!
      unitName must remain valid during the lifetime of all Loggers, best
      make it a string constant. */
  Logger(const char* unitName);
  bool enabled() const { return enabledVal; }
  operator bool() const { return enabledVal; }
  const char* name() const { return unitNameVal; }

  /** Enable/disable messages for specific units. By default, messages are
      disabled.
      @param unitName Name, or null for all units
      @return true if successful (i.e. unit exists) */
  static bool setEnabled(const char* unitName, bool enable = true);

  /** Walk through list of registered Logger objects. Call without arg to
      start, then call with returned value til 0 is returned:
      Logger* l = Logger::enumerate();
      while (l != 0) {
        cerr << ' ' << l->name();
        l = Logger::enumerate(l);
      }  */
  static inline Logger* enumerate(Logger* l = 0) {
    return (l ? l->next : list); }

  void operator()(const char* format) const {
    if (enabled()) put(format, 0, 0);
  }
  void operator()(const char* format, Subst a) const {
    if (enabled()) put(format, 1, &a);
  }
  void operator()(const char* format, Subst a, Subst b) const {
    if (!enabled()) return;
    Subst arg[] = { a, b };
    put(format, 2, arg);
  }
  void operator()(const char* format, Subst a, Subst b, Subst c) const {
    if (!enabled()) return;
    Subst arg[] = { a, b, c };
    put(format, 3, arg);
  }
  void operator()(const char* format, Subst a, Subst b, Subst c, Subst d)
      const {
    if (!enabled()) return;
    Subst arg[] = { a, b, c, d };
    put(format, 4, arg);
  }
  void operator()(const char* format, Subst a, Subst b, Subst c, Subst d,
                  Subst e) const {
    if (!enabled()) return;
    Subst arg[] = { a, b, c, d, e };
    put(format, 5, arg);
  }
  void operator()(const char* format, Subst a, Subst b, Subst c, Subst d,
                  Subst e, Subst f) const {
    if (!enabled()) return;
    Subst arg[] = { a, b, c, d, e, f };
    put(format, 6, arg);
  }
  void operator()(const char* format, Subst a, Subst b, Subst c, Subst d,
                  Subst e, Subst f, Subst g) const {
    if (!enabled()) return;
    Subst arg[] = { a, b, c, d, e, f, g };
    put(format, 7, arg);
  }
  void operator()(const char* format, Subst a, Subst b, Subst c, Subst d,
                  Subst e, Subst f, Subst g, Subst h) const {
    if (!enabled()) return;
    Subst arg[] = { a, b, c, d, e, f, g, h };
    put(format, 8, arg);
  }
  void operator()(const char* format, Subst a, Subst b, Subst c, Subst d,
                  Subst e, Subst f, Subst g, Subst h, Subst i) const {
    if (!enabled()) return;
    Subst arg[] = { a, b, c, d, e, f, g, h, i };
    put(format, 9, arg);
  }

private:
  void put(const char* format, int args, const Subst arg[]) const;

  static Logger* list; // Linked list of registered units
  static string buf; // Temporary buffer for output string, printed at endl

  const char* unitNameVal;
  unsigned char unitNameLen;
  bool enabledVal; // only print messages if true
  Logger* next; // Next in linked list
};
//______________________________________________________________________

/** The default debugging logger uses the unit name "general". If possible,
    you should define a static, per-compilation-unit logger with a more
    descriptive name and use that instead. */
extern DebugLogger msg;

//______________________________________________________________________

#endif
