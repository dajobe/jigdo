/* $Id$ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2003  |  richard@
  | \/¯|  Richard Atterer     |  atterer.net
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

  Logfile / debugging output

  The (Debug)Logger class shouldn't be used directly, only via the macros.
  The following:

    DEBUG_UNIT("unitname")

  ensures that debug("format %1", arg) either prints the debug message (if
  DEBUG is 1), or does nothing at all (if DEBUG is 0). "Nothing at all"
  really means nothing, not even evaluating the argument expressions - at
  least if variable-argument preprocessor macros are supported.

  IMPORTANT: The expressions passed to debug() must not have any side-effects
  if the program is to behave identically with DEBUG=0 and DEBUG=1.

  The following:

    #undef debug
    namespace { Logger debug("unitname"); }

  Is like DEBUG_UNIT, except that the calls to debug() are always compiled
  in, regardless of the setting of DEBUG. Don't forget the #undef debug!

  Use DEBUG_UNIT_LOCAL to define "debug" in the current scope/namespace.

  With DEBUG_TO(FooBar::debug), you can use a non-local debug object (e.g.
  defined in a header somewhere; the macro will define a local "debug"
  reference to FooBar::debug. Making direct calls to FooBar::debug would fail
  if DEBUG=0. Again, there's also a DEBUG_TO_LOCAL, to be used e.g. inside a
  function.

  A "DEBUG_UNIT_LOCAL_DECL;" line should be used inside class definitions to
  introduce a static debug object (or non-static dummy if DEBUG=0). In the
  corresponding .cc file, you will need to add the following:
  #if DEBUG
  Logger MyClass::debug("myclass");
  #endif

*/

#ifndef LOG_HH
#define LOG_HH

#include <config.h>
#include <debug.hh>
#include <nocopy.hh>
#include <string-utf.hh>
//______________________________________________________________________

#if DEBUG
#  define DEBUG_UNIT(_name) namespace { Logger debug(_name); }
#  define DEBUG_UNIT_LOCAL(_name) Logger debug(_name);
#  define DEBUG_UNIT_LOCAL_DECL static Logger debug;
#  define DEBUG_TO(_realobject) namespace { Logger& debug(_realobject); }
#  define DEBUG_TO_LOCAL(_realobject) Logger& debug(_realobject);
#else
#  if HAVE_VARMACRO
#    define DEBUG_UNIT(_name)
#    define DEBUG_UNIT_LOCAL(_name)
#    define DEBUG_UNIT_LOCAL_DECL
#    define DEBUG_TO(_realobject)
#    define DEBUG_TO_LOCAL(_realobject)
#    if defined(__GNUC__) && __GNUC__ < 3
#      define debug(_args...) do { } while (false)
#    else
#      define debug(...) do { } while (false)
#    endif
#  else
#    define DEBUG_UNIT(_name) namespace { IgnoreLogger debug; }
#    define DEBUG_UNIT_LOCAL(_name) IgnoreLogger debug;
#    define DEBUG_UNIT_LOCAL_DECL IgnoreLogger debug;
#    define DEBUG_TO(_realobject) namespace { IgnoreLogger debug; }
#    define DEBUG_TO_LOCAL(_realobject) IgnoreLogger debug;
     /* Var-arg macros not supported, we have to define a dummy class.
        Disadvantage: Arguments of debug() will be evaluated even in
        non-debug builds. */
     struct IgnoreLogger : NoCopy  {
       struct S { template<class X> S(const X&) { } };
       bool enabled() const { return false; }
       operator bool() const { return false; }
       void operator()(const char*) const { }
       void operator()(const char*, S) const { }
       void operator()(const char*, S, S) const { }
       void operator()(const char*, S, S, S) const { }
       void operator()(const char*, S, S, S, S) const { }
       void operator()(const char*, S, S, S, S, S) const { }
       void operator()(const char*, S, S, S, S, S, S) const { }
       void operator()(const char*, S, S, S, S, S, S, S) const { }
       void operator()(const char*, S, S, S, S, S, S, S, S) const { }
       void operator()(const char*, S, S, S, S, S, S, S, S, S) const { }
     };
#  endif
#endif
//______________________________________________________________________

class Logger : NoCopy {
public:
  /** Register a compilation unit. All Loggers MUST *MUST* be static objects!
      unitName must remain valid during the lifetime of all Loggers, best
      make it a string constant. */
  Logger(const char* unitName, bool enabled = false);
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

  /** Scan value of the --debug cmd line option. It is either missing (empty)
      or a comma-separated list of words (we also allow spaces). Each word
      can be preceded by a '!' for negation (i.e. disable debug messages
      rather than enable them). The word is the name of a compilation unit,
      or one of the special values "all" or "help". */
  static void scanOptions(const string& s, const char* binName);

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
extern Logger msg;
//______________________________________________________________________

#endif
