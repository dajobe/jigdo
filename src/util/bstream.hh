/* $Id$ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2001-2004  |  richard@
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
typedef istream bistream;
typedef ostream bostream;
typedef iostream biostream;

typedef ifstream bifstream;
typedef ofstream bofstream;
typedef fstream bfstream;

#else

//____________________

/* With GCC 3.x (x < 4), libstdc++ is broken for files >2GB. We need to
   re-implement the basic functionality to work around this. :-/ */
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
  bool good() const { return f != 0 && feof(f) == 0 && ferror(f) == 0; }
  void close() { fclose(f); f = 0; }
  ~bios() { fclose(f); }
protected:
  bios() : f(0) { }
  bios(FILE* stream) : f(stream) { }
  FILE* f;
};

class bostream : virtual public bios {
public:
  inline int put(int c) { return putc(c, f); }
  inline bostream& seekp(uint64 off, ios::seekdir dir = ios::beg);
  inline bostream& seekp(int off, ios::seekdir dir);
  inline bostream& write(const char* p, streamsize n);
protected:
  bostream() { }
  bostream(FILE* stream) : bios(stream) { }
};

class bistream : virtual public bios {
public:
  inline int get();
  inline int peek();
  inline bistream& seekg(uint64 off, ios::seekdir dir = ios::beg);
  inline bistream& seekg(int off, ios::seekdir dir);
  inline uint64 tellg() const;
  inline void getline(string& l);
  inline bistream& read(char* p, streamsize n);
  inline uint64 gcount() { uint64 r = gcountVal; gcountVal = 0; return r; }
  inline void sync() const { } // NOP
protected:
  bistream() : gcountVal(0) { }
  bistream(FILE* stream) : bios(stream) { }
private:
  uint64 gcountVal;
};

class biostream : virtual public bistream, virtual public bostream {
protected:
  biostream() : bistream(), bostream() { }
  biostream(FILE* stream) : bistream(stream), bostream(stream) { }
};
//____________________

class bifstream : public bistream {
public:
  bifstream(FILE* stream) : bistream(stream) { Paranoid(stream == stdin); }
  inline bifstream(const char* name, ios::openmode m = ios::in);
  inline void open(const char* name, ios::openmode m = ios::in);
};

class bofstream : public bostream {
public:
  bofstream(FILE* stream) : bostream(stream) { Paranoid(stream == stdout); }
  inline bofstream(const char* name, ios::openmode m = ios::out);
  inline void open(const char* name, ios::openmode m = ios::out);
};

class bfstream : public biostream {
public:
  inline bfstream(const char* path, ios::openmode);
};
//____________________

int bistream::get() {
  int r = getc(f);
  gcountVal = (r >= 0 ? 1 : 0);
  return r;
}

int bistream::peek() {
  int r = getc(f);
  gcountVal = (r >= 0 ? 1 : 0);
  ungetc(r, f);
  return r;
}

bistream& bistream::seekg(uint64 off, ios::seekdir dir) {
  if (fail()) return *this;
  int whence;
  if (dir == ios::beg)
    whence = SEEK_SET;
  else if (dir == ios::end)
    whence = SEEK_END;
  else
    { Assert(false); whence = SEEK_SET; }
  int r = fseek(f, off, whence);
  Assert((r == -1) == (ferror(f) != 0));
  return *this;
}

bistream& bistream::seekg(int off, ios::seekdir dir) {
  Paranoid(off < 0 && dir == ios::end);
  if (fail()) return *this;
  int r = fseek(f, off, SEEK_END);
  Assert((r == -1) == (ferror(f) != 0));
  return *this;
}

bostream& bostream::seekp(uint64 off, ios::seekdir dir) {
  if (fail()) return *this;
  int whence;
  if (dir == ios::beg)
    whence = SEEK_SET;
  else if (dir == ios::end)
    whence = SEEK_END;
  else
    { Assert(false); whence = SEEK_SET; }
  int r = fseek(f, off, whence);
  Assert((r == -1) == (ferror(f) != 0));
  return *this;
}

bostream& bostream::seekp(int off, ios::seekdir dir) {
  Paranoid(off < 0 && dir == ios::end);
  if (fail()) return *this;
  int r = fseek(f, off, SEEK_END);
  Assert((r == -1) == (ferror(f) != 0));
  return *this;
}

uint64 bistream::tellg() const {
  return ftell(f);
}

void bistream::getline(string& l) {
  gcountVal = 0;
  while (true) {
    int c = fgetc(f);
    if (c >= 0) ++gcountVal;
    if (c == EOF || c == '\n') return;
    l += static_cast<char>(c);
  }
}

bostream& bostream::write(const char* p, streamsize n) {
  fwrite(p, 1, n, f);
  return *this;
}

bistream& bistream::read(char* p, streamsize n) {
  gcountVal = fread(p, 1, n, f);
  return *this;
}

inline void getline(bistream& bi, string& l) {
  bi.getline(l);
}

bifstream::bifstream(const char* name, ios::openmode m) : bistream() {
  open(name, m);
}

void bifstream::open(const char* name, ios::openmode m) {
  Paranoid((m & ios::binary) != 0 && f == 0);
  f = fopen(name, "r");
}

bofstream::bofstream(const char* name, ios::openmode m) : bostream() {
  open(name, m);
}

void bofstream::open(const char* name, ios::openmode m) {
  Paranoid((m & ios::binary) != 0 && f == 0);
  Paranoid((m & ios::ate) == 0);
  if ((m & ios::trunc) != 0)
    f = fopen(name, "w+");
  else
    f = fopen(name, "r+");
}

bfstream::bfstream(const char* name, ios::openmode m) : biostream() {
  Paranoid((m & ios::binary) != 0);
  Paranoid((m & ios::ate) == 0);
  if ((m & ios::out) == 0)
    f = fopen(name, "r");
  else if ((m & ios::trunc) != 0)
    f = fopen(name, "w+");
  else
    f = fopen(name, "r+");
}

#endif /* HAVE_WORKING_FSTREAM */
//______________________________________________________________________

// Avoid lots of ugly reinterpret_casts in the code itself
inline bistream& readBytes(bistream& s, byte* buf, streamsize count) {
  return s.read(reinterpret_cast<char*>(buf), count);
}

inline biostream& readBytes(biostream& s, byte* buf, streamsize count) {
  s.read(reinterpret_cast<char*>(buf), count);
  return s;
}

inline bostream& writeBytes(bostream& s, const byte* buf, streamsize count) {
  return s.write(reinterpret_cast<const char*>(buf), count);
}

inline biostream& writeBytes(biostream& s, const byte* buf,
                             streamsize count) {
  s.write(reinterpret_cast<const char*>(buf), count);
  return s;
}


#endif /* BSTREAM_HH */
