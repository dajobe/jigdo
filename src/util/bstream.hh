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

//#include <iosfwd> /* Needed for fstream prototypes! */
#include <iostream>
#include <fstream>

#include <stdio.h>

#include <debug.hh>
//____________________

#if HAVE_WORKING_FSTREAM
/* This is commented out: With GCC 3.x (x < 4), libstdc++ is broken for files
   >2GB. We need to re-implement the basic functionality to work around this.
   :-/ */
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
#else

//____________________

// GCC 3.x (x<4) code. We rely on the 64-bit-sized C functions for files
#if !defined _FILE_OFFSET_BITS || _FILE_OFFSET_BITS != 64
#  warning "_FILE_OFFSET_BITS not defined, big file support may not work"
#endif

/* The subtle differences between bad() and fail() are not taken into account
   here - beware! */
class bios {
public:
  operator void*() const { return fail() ? (void*)0 : (void*)(-1); }
  int operator!() const { return fail(); }
  bool fail() const { return f == 0 || ferror(f) != 0; }
  //Incorrect: bool bad() const { return fail(); }
  bool eof() const { return f == 0 || feof(f) != 0; }
protected:
  bios() : f(0) { }
  bios(FILE* stream) : f(stream) { }
  FILE* f;
};

class bostream : virtual public bios {
public:
  inline int put(int c) { return putc(c, f); }
protected:
  bostream() { }
  bostream(FILE* stream) : bios(stream) { }
};

class bistream : virtual public bios {
public:
  inline int get() { return getc(f); }
  inline int peek() { int r = getc(f); ungetc(r, f); return r; }
  inline bistream& seekg(uint64 off, ios::seekdir dir);
protected:
  bistream() { }
  bistream(FILE* stream) : bios(stream) { }
};

class biostream : public bistream, public bostream {
protected:
  biostream(FILE* stream) : bistream(stream), bostream(stream) { }
};
//____________________

class bifstream : public bistream {
public:
  bifstream(FILE* stream) : bistream(stream) { Paranoid(stream == stdin); }
  inline bifstream(const char* name, ios::openmode m);
};

class bofstream : public bostream {
public:
  bofstream(FILE* stream) : bostream(stream) { Paranoid(stream == stdout); }
  inline bofstream(const char* name, ios::openmode m);
};
//____________________

bistream& bistream::seekg(uint64 off, ios::seek_dir dir) {
  if (fail()) return *this;
  int whence;
  if (dir == ios::beg)
    whence = SEEK_SET;
  else if (dir == ios::end)
    whence = SEEK_END;
  else
    { Assert(false); }
  int r = fseek(f, off, whence);
  Assert((r == -1) == (ferror(f) != 0));
}

bifstream::bifstream(const char* name, ios::openmode m) : bistream() {
  Paranoid((m & ios::binary) != 0);
  f = fopen(name, "r");
}

bofstream::bofstream(const char* name, ios::openmode m) : bostream() {
  Paranoid((m & ios::binary) != 0);
  Paranoid((m & ios::ate) == 0);
  if ((m & ios::trunc) != 0)
    f = fopen(name, "w+");
  else
    f = fopen(name, "r+");
}

#endif

#endif /* BSTREAM_HH */
