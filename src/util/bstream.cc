/* $Id$ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2001-2004  |  richard@
  | \/¯|  Richard Atterer          |  atterer.net
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

  I/O streams for bytes (byte is unsigned char, not regular char)

*/

#include <config.h>

#include <bstream.hh>

#include <stdio.h>

#if !HAVE_WORKING_FSTREAM

bistream bcin(stdin);
bostream bcout(stdout);

bostream& bostream::seekp(off_t off, ios::seekdir dir) {
  if (fail()) return *this;
  int whence;
  if (dir == ios::beg)
    whence = SEEK_SET;
  else if (dir == ios::end)
    whence = SEEK_END;
  else if (dir == ios::cur)
    whence = SEEK_CUR;
  else
    { Assert(false); whence = SEEK_SET; }
  /*int r =*/ fseeko(f, off, whence);
  // Fails: Assert((r == -1) == (ferror(f) != 0));
  return *this;
}

bistream& bistream::seekg(off_t off, ios::seekdir dir) {
  if (fail()) return *this;
  int whence;
  if (dir == ios::beg)
    whence = SEEK_SET;
  else if (dir == ios::end)
    whence = SEEK_END;
  else if (dir == ios::cur)
    whence = SEEK_CUR;
  else
    { Assert(false); whence = SEEK_SET; }
  /*int r =*/ fseeko(f, off, whence);
  // Fails: Assert((r == -1) == (ferror(f) != 0));
  return *this;
}

void bistream::getline(string& l) {
  gcountVal = 0;
  l.clear();
  while (true) {
    int c = fgetc(f);
    if (c >= 0) ++gcountVal;
    if (c == EOF || c == '\n') break;
    l += static_cast<char>(c);
  }
}

void bofstream::open(const char* name, ios::openmode m) {
  Paranoid((m & ios::binary) != 0 && f == 0);
  Paranoid((m & ios::ate) == 0);
  if ((m & ios::trunc) != 0) {
    f = fopen(name, "w+b");
    return;
  }
  // Open existing file for reading and writing
  f = fopen(name, "r+b");
  if (f == NULL && errno == ENOENT)
    f = fopen(name, "w+b"); // ...or create new, empty file
}

bfstream::bfstream(const char* name, ios::openmode m) : biostream() {
  Paranoid((m & ios::binary) != 0);
  Paranoid((m & ios::ate) == 0);
  if ((m & ios::out) == 0)
    f = fopen(name, "rb");
  else if ((m & ios::trunc) != 0)
    f = fopen(name, "w+b");
  else
    f = fopen(name, "r+b");
}

#endif
