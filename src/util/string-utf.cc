/* $Id$ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2003  |  richard@
  | \/¯|  Richard Atterer     |  atterer.net
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

  Helper functions for dealing with UTF-8 strings

*/

#include <config.h>
#include <glib.h>
#include <stdarg.h>
#include <stdio.h>

#include <debug.hh>
#include <string-utf.hh>
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

namespace {

// flag bits
static const int F = 1 << 0; // fast substitution
static const int L = 1 << 1; // locale-format string arg, not UTF-8
static const int E = 1 << 2; // escape: turn < into &lt; etc
//____________________

// Convert input to UTF-8. Returned string must be freed.
inline gchar* localeToUTF8(const char* input) {
  gsize written;
  GError* error = NULL;
  int len = strlen(input);
  gchar* s = g_locale_to_utf8(input, len, NULL, &written, &error);
  if (error == NULL) return s;
  if (s != NULL) g_free(s);

  /* Maybe this is just me, but for me, glib always thinks that my charset is
     "ANSI_X3.4-1968" - ?! Fall back to ISO-8859-15 if glib failed above.
     Users can override this by setting the CHARSET variable. */
  g_clear_error(&error);
  s = g_convert(input, len, "UTF-8", "ISO-8859-1", NULL, &written, &error);
  if (error == NULL || s != NULL) return s;
  g_clear_error(&error);
  return g_strdup("[UTF8ConvFail]");
}
//____________________

/* Append s to result according to flags */
void strSubst(string& result, const char* s, int flags) {
  const char* e;

  switch (flags) {

    case F:
      Paranoid(false); // Already handled in doSubst() below
      break;

    case   L:
    case F|L:
      // Convert locale-format to UTF
      s = localeToUTF8(s);
      result += s;
      g_free((gpointer)s);
      break;

    case   L|E:
    case F|L|E:
      // Convert locale-format to UTF and turn < into &lt;
      s = localeToUTF8(s);
      e = s;
      while (*s != '\0') {
        if (*s == '&') result += "&amp;";
        else if (*s == '<') result += "&lt;";
        else if (*s == '>') result += "&gt;";
        else result += *s;
        ++s;
      }
      g_free((gpointer)e);
      break;

    case F|E:
      while (*s != '\0') {
        if (*s == '&') result += "&amp;";
        else if (*s == '<') result += "&lt;";
        else if (*s == '>') result += "&gt;";
        else result += *s;
        ++s;
      }
      break;

    case 0:
      // Verify UTF-8, only append up to first invalid character
      g_utf8_validate(s, -1, &e);
      result.append(s, e - s);
      break;

    case E:
      // Verify UTF-8 data, turn < into &lt;
      g_utf8_validate(s, -1, &e);
      while (s < e) {
        if (*s == '&') result += "&amp;";
        else if (*s == '<') result += "&lt;";
        else if (*s == '>') result += "&gt;";
        else result += *s;
        ++s;
      }
      break;

    default:
      Paranoid(false);
  }
}

} // namespace
//____________________

/* Look at arg[n] and append it to result according to flags */
inline void Subst::doSubst(string& result, const Subst arg[], int n,
                           int flags) {
  switch (arg[n].type) {
    case INT:
      snprintf(buf, BUF_LEN, "%d", arg[n].val.intVal);
      buf[BUF_LEN - 1] = '\0'; result += buf; break;
    case UNSIGNED:
      snprintf(buf, BUF_LEN, "%u", arg[n].val.unsignedVal);
      buf[BUF_LEN - 1] = '\0'; result += buf; break;
    case LONG:
      snprintf(buf, BUF_LEN, "%ld", arg[n].val.longVal);
      buf[BUF_LEN - 1] = '\0'; result += buf; break;
    case ULONG:
      snprintf(buf, BUF_LEN, "%lu", arg[n].val.ulongVal);
      buf[BUF_LEN - 1] = '\0'; result += buf; break;
#   if HAVE_UNSIGNED_LONG_LONG
    case ULONGLONG:
      snprintf(buf, BUF_LEN, "%llu", arg[n].val.ulonglongVal);
      buf[BUF_LEN - 1] = '\0'; result += buf; break;
#   endif
    case DOUBLE:
      snprintf(buf, BUF_LEN, "%f", arg[n].val.doubleVal);
      buf[BUF_LEN - 1] = '\0'; result += buf; break;
    case CHAR:
      result += arg[n].val.charVal;
      break;
    case CHAR_P:
      if (flags == F)
        result += arg[n].val.charPtr;
      else
        strSubst(result, arg[n].val.charPtr, flags);
      break;
    case STRING_P:
      if (flags == F)
        result += *arg[n].val.stringPtr;
      else
        strSubst(result, arg[n].val.stringPtr->c_str(), flags);
      break;
    case POINTER:
      snprintf(buf, BUF_LEN, "%p", arg[n].val.pointerVal);
      buf[BUF_LEN - 1] = '\0'; result += buf; break;
  }
}
//____________________

string Subst::subst(const char* format, int args, const Subst arg[]) {
  string result;
  gunichar x;
  const char* i = format;
  unsigned max = '1' + args;

  while (true) {
    // Search through string until either '%' or '\0' found
    const char* j = i;
    while (true) {
      x = g_utf8_get_char(j);
      if (x == 0 || x == '%') break;
      j = g_utf8_next_char(j);
    }
    // x == '%' or x == 0, normal string between [i;j)
    result.append(i, j - i);
    if (x == 0) return result;

    // '%' escape detected
    int flags = 0;
    while (true) {
      j = g_utf8_next_char(j);
      x = g_utf8_get_char(j);
      // Handle special flags between % and digit
      if (x == 0) return result;
      else if (x == '%') { result += '%'; break; }
      else if (x == 'F') flags |= F;
      else if (x == 'L') flags |= L;
      else if (x == 'E') flags |= E;
      // Ignore other characters, loop until digit found
      else if (x >= '1' && x <= '9') {
        if (x < max) doSubst(result, arg, x - '1', flags); // Arg subst
        break;
      }
    }
    // Now j points to digit in "%1" or second '%' in "%%"

    i = ++j; // i and j point after digit/%
  }
}
