/* $Id$ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2001-2003  |  richard@
  | \/¯|  Richard Atterer          |  atterer.net
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

*/

#include <config.h>

#include <uri.hh>
#include <compat.hh>
//______________________________________________________________________

/** Create a new URI from an absolute base URI and a relative URI. (rel can
    also be absolute, in this case, the result in dest equals rel.) */
void uriJoin(string* dest, const string& base, const string& rel) {
//   string::const_iterator i = rel.begin(), e = rel.end();
//   while (i != e && isalpha(*i)) ++i;
//   if (i != e && *i == ':') {
  if (findLabelColon(rel) != 0) {
    *dest = rel; // Absolute: rel starts with alphabetic chars followed by :
  } else {
    string::size_type n = base.find_last_of('/');
    if (n == string::npos)
      *dest = base;
    else
      dest->assign(base, 0, n + 1);
    *dest += rel;
  }
}
//______________________________________________________________________

unsigned findLabelColon(const string& s) {
  string::const_iterator i = s.begin(), e = s.end();
  while (i != e) {
    if (*i == '/' || static_cast<unsigned char>(*i) <= ' ') return 0;
    if (*i == ':') return i - s.begin();
    ++i;
  }
  return 0;
}
//______________________________________________________________________

bool isRealUrl(const string& s) {
  return compat_compare(s, 0, 6, "http:/", 6) == 0
    || compat_compare(s, 0, 6, "https:/", 7) == 0
    || compat_compare(s, 0, 5, "ftp:/", 5) == 0
    || compat_compare(s, 0, 5, "ftps:/", 6) == 0;
}
