/* $Id$ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2004  |  richard@
  | \/¯|  Richard Atterer     |  atterer.net
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

*/

#include <config.h>

#include <uri.hh>
#include <compat.hh>
#include <debug.hh>
#include <log.hh>
//______________________________________________________________________

//DEBUG_UNIT("uri")

/* RFC 1738 sez:
   <scheme>://<user>:<password>@<host>:<port>/<url-path>
   Some or all of the parts "<user>:<password>@", ":<password>",
   ":<port>", and "/<url-path>" may be excluded. */

/** Create a new URI from an absolute base URI and a relative URI. (rel can
    also be absolute, in this case, the result in dest equals rel.) */
void uriJoin(string* dest, const string& base, const string& rel) {
  if (isRealUrl(rel)) {
    *dest = rel;
    return;
  }

  /* Parse base URL. We can assume that that URL is in a valid format, so the
     url-path starts after the 3rd '/', if that is present */
  string::size_type hostSlash = base.find('/');
  if (hostSlash != string::npos) {
    hostSlash = base.find('/', hostSlash + 1);
    if (hostSlash != string::npos)
      hostSlash = base.find('/', hostSlash + 1);
  }
  *dest = base;
  if (hostSlash == string::npos) {
    hostSlash = dest->length();
    *dest += '/';
  }
  // hostSlash points to the '/' after the hostname

  // Remove leafname from dest
  string::size_type n = dest->find_last_of('/');
  dest->erase(n + 1);

  string::size_type r = 0; // Offset from which onward to append rel to dest
  if (rel.length() > 0 && rel[0] == '/') {
    // rel is server-absolute
    dest->erase(hostSlash + 1);
    ++r;
  }

  // Scan through rel, applying path components to dest
  while (r < rel.length()) {
    //debug("url='%1', rel='%2'", *dest, rel.c_str() + r);
    if (compat_compare(rel, r, 2, "./", 2) == 0) {
      r += 2;
    } else if (compat_compare(rel, r, 3, "../", 3) == 0) {
      //debug("a");
      r += 3;
      string::size_type n = dest->length() - 1;
      Assert((*dest)[n] == '/');
      n = dest->rfind('/', n - 1);
      if (n != string::npos && n >= hostSlash)
        dest->erase(n + 1);
    } else {
      string::size_type n = rel.find('/', r);
      if (n != string::npos) ++n; else n = rel.length();
      //debug("%1 %2 %3", rel, r, n);
      dest->append(rel, r, n - r);
      r = n;
    }
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
  unsigned l = findLabelColon(s);
  return l > 0 && s.length() >= l + 3 && s[l + 1] == '/' && s[l + 2] == '/';
}
