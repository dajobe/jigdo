/* $Id$ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2001-2003  |  richard@
  | \/¯|  Richard Atterer          |  atterer.net
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

  Cross-platform compatibility

*/

#ifndef COMPAT_HH
#define COMPAT_HH

#include <config.h>

#include <string>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#if WINDOWS
#  include <windows.h>
#endif
//______________________________________________________________________

// No operator<< for uint64, so define our own
#if !HAVE_OUTUINT64
#include <iostream>
inline ostream& operator<<(ostream& s, const uint64 x) {
  s << static_cast<unsigned long>(x / 1000000000)
    << static_cast<unsigned long>(x % 1000000000);
  return s;
}
#endif
//______________________________________________________________________

/* Truncate a file to a given length. Behaviour undefined if given
   length is bigger than current file size */
#if HAVE_TRUNCATE
inline int compat_truncate(const char* path, uint64 length) {
  return truncate(path, length);
}
#else
int compat_truncate(const char* path, uint64 length);
#endif
//______________________________________________________________________

/* Rename a file. Mingw does provide rename(), but gives an error if
   the destination name already exists. This one doesn't. */
#if WINDOWS
int compat_rename(const char* src, const char* dst);
#else
inline int compat_rename(const char* src, const char* dst) {
  return rename(src, dst);
}
#endif
//______________________________________________________________________

/* Create a directory */
#if WINDOWS
inline int compat_mkdir(const char* newDir) {
  return mkdir(newDir);
}
#else
inline int compat_mkdir(const char* newDir) {
  return mkdir(newDir, 0777);
}
#endif
//______________________________________________________________________

// Set/overwrite environment variable, return SUCCESS or FAILURE
#if WINDOWS
inline bool compat_setenv(const char* name, const char* value) {
  return (SetEnvironmentVariable(name, value) != 0) ? SUCCESS : FAILURE;
}
#elif HAVE_SETENV /* Linux, BSD */
inline bool compat_setenv(const char* name, const char* value) {
  return (setenv(name, value, 1) == 0) ? SUCCESS : FAILURE;
}
#else /* Solaris and other Unices without setenv() */
bool compat_setenv(const char* name, const char* value);
#endif
//______________________________________________________________________

/* Width in characters of the tty (for progress display), or 0 if not
   a tty or functions not present on system. */
#if WINDOWS
inline int ttyWidth() { return 80; }
#elif !HAVE_IOCTL_WINSZ || !HAVE_FILENO
inline int ttyWidth() { return 0; }
#else
extern int ttyWidth();
#endif
//______________________________________________________________________

/** (For "file:" URI handling) If directory separator on this system
    is not '/', exchange the actual separator for '/' and vice versa
    in the supplied string. Do the same for any non-'.' file extension
    separator. We trust in the optimizer to remove unnecessary
    code. */
inline void compat_swapFileUriChars(string& s) {
  // Need this "if" because gcc cannot optimize away loops
  if (DIRSEP != '/' || EXTSEP != '.') {
    for (string::iterator i = s.begin(), e = s.end(); i != e; ++i) {
      if (DIRSEP != '/' && *i == DIRSEP) *i = '/';
      else if (DIRSEP != '/' && *i == '/') *i = DIRSEP;
      else if (EXTSEP != '.' && *i == EXTSEP) *i = '.';
      else if (EXTSEP != '.' && *i == '.') *i = EXTSEP;
    }
  }
}
//______________________________________________________________________

#if HAVE_STRINGCMP
inline int compat_compare(const string& s1, string::size_type pos1,
    string::size_type n1, const string& s2, string::size_type pos2 = 0,
    string::size_type n2 = string::npos) {
  return s1.compare(pos1, n1, s2, pos2, n2);
}
#else
inline int compat_compare(const string& s1, string::size_type pos1,
    string::size_type n1, const string& s2, string::size_type pos2 = 0,
                          string::size_type n2 = string::npos) {
  string::size_type r1 = s1.length() - pos1;
  if (r1 > n1) r1 = n1;
  string::size_type r2 = s2.length() - pos2;
  if (r2 > n2) r2 = n2;
  string::size_type r = r2;
  int rdiff = r1 - r2;
  if (rdiff < 0) r = r1;
  string::const_iterator i1 = s1.begin() + pos1;
  string::const_iterator i2 = s2.begin() + pos2;
  while (r > 0) {
    if (*i1 < *i2) return -1;
    if (*i1 > *i2) return 1;
    ++i1; ++i2;
    --r;
  }
  return rdiff;
}
#endif
//______________________________________________________________________

#endif
