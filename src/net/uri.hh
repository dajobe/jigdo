/* $Id$ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2001-2003  |  richard@
  | \/¯|  Richard Atterer          |  atterer.net
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

*/

#ifndef URI_HH
#define URI_HH

#include <config.h>

#include <string>

/** Create a new URI from an absolute base URI and a relative URI. (rel can
    also be absolute, in this case, the result in dest equals rel.) */
void uriJoin(string* dest, const string& base, const string& rel);

//______________________________________________________________________

inline unsigned findLabelColon(const string& s) {
  string::const_iterator i = s.begin(), e = s.end();
  while (i != e) {
    if (*i == '/' || static_cast<unsigned char>(*i) <= ' ') return 0;
    if (*i == ':') return i - s.begin();
    ++i;
  }
  return 0;
}

#endif
