/* $Id$ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2001-2002  |  richard@
  | \/¯|  Richard Atterer          |  atterer.net
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

*/

#include <stdarg.h>
#include <stdio.h>

#include <debug.hh>
#include <string.hh>
//______________________________________________________________________

namespace {
  const int BUF_LEN = 40; // Enough room for 128-bit integers. :-)
  char buf[BUF_LEN];
  const char* const PAD = "                                        ";
  const char* const PAD_END = PAD + 40;
}

string& append(string& s, double x) {
  snprintf(buf, BUF_LEN, "%.1f", x);
  buf[BUF_LEN - 1] = '\0';
  return s += buf;
}
string& append(string& s, int x) {
  snprintf(buf, BUF_LEN, "%d", x);
  buf[BUF_LEN - 1] = '\0';
  return s += buf;
}
string& append(string& s, unsigned x) {
  snprintf(buf, BUF_LEN, "%u", x);
  buf[BUF_LEN - 1] = '\0';
  return s += buf;
}
string& append(string& s, unsigned x, int width) {
  Assert(*PAD_END == '\0' && width < PAD_END - PAD);
  int written = snprintf(buf, BUF_LEN, "%u", x);
  if (written < width) s += PAD_END - width + written;
  buf[BUF_LEN - 1] = '\0';
  return s += buf;
}
string& append(string& s, long x) {
  snprintf(buf, BUF_LEN, "%ld", x);
  buf[BUF_LEN - 1] = '\0';
  return s += buf;
}
string& append(string& s, unsigned long x) {
  snprintf(buf, BUF_LEN, "%lu", x);
  buf[BUF_LEN - 1] = '\0';
  return s += buf;
}
string& append(string& s, unsigned long x, int width) {
  Assert(*PAD_END == '\0' && width < PAD_END - PAD);
  int written = snprintf(buf, BUF_LEN, "%lu", x);
  if (written < width) s += PAD_END - width + written;
  buf[BUF_LEN - 1] = '\0';
  return s += buf;
}
#if HAVE_UNSIGNED_LONG_LONG
string& append(string& s, unsigned long long x) {
  snprintf(buf, BUF_LEN, "%llu", x);
  buf[BUF_LEN - 1] = '\0';
  return s += buf;
}
string& append(string& s, unsigned long long x, int width) {
  Assert(*PAD_END == '\0' && width < PAD_END - PAD);
  int written = snprintf(buf, BUF_LEN, "%llu", x);
  if (written < width) s += PAD_END - width + written;
  buf[BUF_LEN - 1] = '\0';
  return s += buf;
}
#endif
//______________________________________________________________________

string Subst::subst(const char* format, int args, const Subst arg[]) {
  // Create output
  string result;
  const char* i = format - 1;
  char max = '1' + args;
  while (*++i != '\0') {
    if (*i != '%') { result += *i; continue; }
    ++i;
    if (*i == '\0') break;
    if (*i == '%') { result += '%'; continue; }
    if (*i >= '1' && *i < max) {
      int n = *i - '1';
      switch (arg[n].type) {
        case CHAR:     result += arg[n].val.charVal; break;
        case CHAR_P:   result += arg[n].val.charPtr; break;
        case STRING_P: result += *arg[n].val.stringPtr; break;
        default:
          switch (arg[n].type) {
            case INT:
              snprintf(buf, BUF_LEN, "%d", arg[n].val.intVal); break;
            case UNSIGNED:
              snprintf(buf, BUF_LEN, "%u", arg[n].val.unsignedVal); break;
            case LONG:
              snprintf(buf, BUF_LEN, "%ld", arg[n].val.longVal); break;
            case ULONG:
              snprintf(buf, BUF_LEN, "%lu", arg[n].val.ulongVal); break;
#           if HAVE_UNSIGNED_LONG_LONG
            case ULONGLONG:
              snprintf(buf, BUF_LEN, "%llu", arg[n].val.ulonglongVal); break;
#           endif
            default: buf[0] = '\0'; break;
          }
          buf[BUF_LEN - 1] = '\0';
          result += buf;
      }
      continue;
    }
    result += '%';
    result += *i;
  }

  return result;
}
