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

#include <config.h>

#include <compat.hh>
#include <stdio.h>
#include <stdlib.h>
#include <set>

#if WINDOWS
#  include <windows.h>
#  include <winbase.h>
#endif
//______________________________________________________________________

#if HAVE_TRUNCATE
// No additional code required
//______________________________________________________________________

#elif !HAVE_TRUNCATE && HAVE_FTRUNCATE
// Truncate using POSIX ftruncate()
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#ifndef O_LARGEFILE
#  define O_LARGEFILE 0 // Linux: Allow 64-bit file sizes on 32-bit arches
#endif
int compat_truncate(const char* path, uint64 length) {
  int fd = open(path, O_RDWR | O_LARGEFILE);
  if (fd == -1) return -1;
  if (ftruncate(fd, length) != 0) { close(fd); return -1; }
  return close(fd);
}
//______________________________________________________________________

#elif !HAVE_TRUNCATE && WINDOWS
// Truncate using native Windows API
int compat_truncate(const char* path, uint64 length) {
  // TODO error handling: GetLastError(), FormatMessage()
  HANDLE handle = CreateFile(path, GENERIC_READ | GENERIC_WRITE, 0, 0,
                             OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
  if (handle == INVALID_HANDLE_VALUE) return -1;
  LONG lengthHi = length >> 32;
  if (SetFilePointer(handle, length, &lengthHi, FILE_BEGIN) == 0xffffffffU
      || SetEndOfFile(handle) == 0U) {
    CloseHandle(handle);
    return -1;
  }
  if (CloseHandle(handle) == 0) return -1;
  return 0;
}
//______________________________________________________________________

#else
#  error "No implementation for truncating files!"
#endif
//====================================================================

#if WINDOWS
// Delete destination before renaming
int compat_rename(const char* src, const char* dst) {
  remove(dst); // Ignore errors
  return rename(src, dst);
}
#endif
//====================================================================

#if !WINDOWS && !HAVE_SETENV
namespace {
  struct CmpTilEq {
    /* Like less<>, but only compares up to the first '=' in the string. All
       *strings must* contain a '='. */
    bool operator()(const string& x, const string& y) const {
      string::const_iterator xx = x.begin(), yy = y.begin();
      char a, b;
      do {
        a = *xx; ++xx;
        b = *yy; ++yy;
        if (a == '=') return b != '=';
      } while (a == b);
      if (b == '=') return false;
      return a < b;
    }
  };
}

/* This is probably a candidate for the least efficient setenv-via-putenv
   implementation ever. */
bool compat_setenv(const char* name, const char* value) {
  typedef set<string, CmpTilEq> VarSet;
  static VarSet vars;
  string var = name;
  var += '=';
  var += value;
  VarSet::iterator old = vars.find(var);
  if (old != vars.end()) vars.erase(old);
  pair<VarSet::iterator,bool> ins = vars.insert(var);
  return putenv(const_cast<char*>(ins.first->c_str())) == 0 ?
    SUCCESS : FAILURE;
}
#endif
//====================================================================

#if !WINDOWS && HAVE_IOCTL_WINSZ && HAVE_FILENO
#include <sys/ioctl.h>
#include <errno.h>
int ttyWidth() {
  struct winsize w;
  if (ioctl(fileno(stderr), TIOCGWINSZ, &w) == -1
      || w.ws_col == 0)
    return 0;
  return w.ws_col;
}
#endif
