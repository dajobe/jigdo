/* $Id$ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2001-2002  |  richard@
  | \/¯|  Richard Atterer          |  atterer.net
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

  subst("Format %1, %2", arg1, arg2) creates strings with the arguments
  filled in, and does so in a safer way than sprintf() and friends.

*/

#ifndef STRING_HH
#define STRING_HH

#include <config.h>

#include <string>
//______________________________________________________________________

/** Convert a number x to characters and append to a string. */
string& append(string& s, double x);
string& append(string& s, int x);
string& append(string& s, unsigned x);
string& append(string& s, long x);
string& append(string& s, unsigned long x);
#if HAVE_UNSIGNED_LONG_LONG
string& append(string& s, unsigned long long x);
#endif
/** Convert a number x to characters and append to a string, padding
    at front with space characters. width must be <40*/
string& append(string& s, unsigned x, int width);
string& append(string& s, unsigned long x, int width);
#if HAVE_UNSIGNED_LONG_LONG
string& append(string& s, unsigned long long x, int width);
#endif
//______________________________________________________________________

class Subst {
public:
  Subst(int x)           { type = INT;      val.intVal = x; }
  Subst(unsigned x)      { type = UNSIGNED; val.unsignedVal = x; }
  Subst(long x)          { type = LONG;     val.longVal = x; }
  Subst(unsigned long x) { type = ULONG;    val.ulongVal = x; }
# if HAVE_UNSIGNED_LONG_LONG
    Subst(unsigned long long x) { type = ULONGLONG; val.ulonglongVal = x; }
# endif
  Subst(double x)        { type = DOUBLE;   val.doubleVal = x; }
  Subst(char x)          { type = CHAR;     val.charVal = x; }
  Subst(const char* x)   { type = CHAR_P;   val.charPtr = x; }
  Subst(const string& x) { type = STRING_P; val.stringPtr = &x; }
  Subst(const string* x) { type = STRING_P; val.stringPtr = x; }
  Subst(const void* x)   { type = POINTER;  val.pointerVal = x; }
  static string subst(const char* format, int args, const Subst arg[]);

private:

# ifdef STRING_UTF_HH
  static inline void doSubst(string& result, const Subst arg[], int n,
                             int flags);
# endif
  enum {
    INT, UNSIGNED, LONG, ULONG, ULONGLONG, DOUBLE, CHAR, CHAR_P, STRING_P,
    POINTER
  } type;
  union {
    int intVal;
    unsigned unsignedVal;
    long longVal;
    unsigned long ulongVal;
#   if HAVE_UNSIGNED_LONG_LONG
      unsigned long long ulonglongVal;
#   endif
    double doubleVal;
    char charVal;
    const char* charPtr;
    const string* stringPtr;
    const void* pointerVal;
  } val;
};
//______________________________________________________________________

/* Example:
   cout << subst("file `%1' not found: %2",
                 string("foo"), strerror(errno)); */

inline string subst(const char* format, Subst a) {
  return Subst::subst(format, 1, &a);
}
inline string subst(const char* format, Subst a, Subst b) {
  Subst arg[] = { a, b };
  return Subst::subst(format, 2, arg);
}
inline string subst(const char* format, Subst a, Subst b, Subst c) {
  Subst arg[] = { a, b, c };
  return Subst::subst(format, 3, arg);
}
inline string subst(const char* format, Subst a, Subst b, Subst c, Subst d) {
  Subst arg[] = { a, b, c, d };
  return Subst::subst(format, 4, arg);
}
inline string subst(const char* format, Subst a, Subst b, Subst c, Subst d,
                    Subst e) {
  Subst arg[] = { a, b, c, d, e };
  return Subst::subst(format, 5, arg);
}
inline string subst(const char* format, Subst a, Subst b, Subst c, Subst d,
                    Subst e, Subst f) {
  Subst arg[] = { a, b, c, d, e, f };
  return Subst::subst(format, 6, arg);
}
inline string subst(const char* format, Subst a, Subst b, Subst c, Subst d,
                    Subst e, Subst f, Subst g) {
  Subst arg[] = { a, b, c, d, e, f, g };
  return Subst::subst(format, 7, arg);
}
inline string subst(const char* format, Subst a, Subst b, Subst c, Subst d,
                    Subst e, Subst f, Subst g, Subst h) {
  Subst arg[] = { a, b, c, d, e, f, g, h };
  return Subst::subst(format, 8, arg);
}
inline string subst(const char* format, Subst a, Subst b, Subst c, Subst d,
                    Subst e, Subst f, Subst g, Subst h, Subst i) {
  Subst arg[] = { a, b, c, d, e, f, g, h, i };
  return Subst::subst(format, 9, arg);
}

#endif
