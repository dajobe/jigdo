/* $Id$ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2001-2002  |  richard@
  | \/¯|  Richard Atterer          |  atterer.net
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

  I/O streams for bytes (byte is unsigned char, not regular char)

  This was first solved with typedefs like "typedef
  basic_istream<byte> bistream;". That turns out to be difficult,
  though, since you need to supply your own implementation for
  char_traits and basic_fstream. The current typedefs aren't very
  useful except to indicate in the source: "This is /intended/ to be
  used for binary data, not text".

*/

#ifndef BSTREAM_HH
#define BSTREAM_HH

#include <config.h>

#include <iosfwd> /* Needed for fstream prototypes! */
#include <iostream>
//____________________

typedef istream bistream;
typedef ostream bostream;
typedef iostream biostream;

typedef ifstream bifstream;
typedef ofstream bofstream;
typedef fstream bfstream;

// Avoid lots of ugly reinterpret_casts in the code itself
inline istream& readBytes(istream& s, byte* buf, streamsize count) {
  return s.read(reinterpret_cast<char*>(buf), count);
}

inline iostream& readBytes(iostream& s, byte* buf, streamsize count) {
  s.read(reinterpret_cast<char*>(buf), count);
  return s;
}

inline ostream& writeBytes(ostream& s, const byte* buf, streamsize count) {
  return s.write(reinterpret_cast<const char*>(buf), count);
}

inline iostream& writeBytes(iostream& s, const byte* buf, streamsize count) {
  s.write(reinterpret_cast<const char*>(buf), count);
  return s;
}

// template<class somestream>
// inline somestream& readBytes(somestream& s, byte* buf, streamsize count) {
//   return s.read(reinterpret_cast<char*>(buf), count);
// }

// template<class somestream>
// inline somestream& writeBytes(somestream& s, byte* buf, streamsize count) {
//   return s.write(reinterpret_cast<char*>(buf), count);
// }

#endif /* BSTREAM_HH */
