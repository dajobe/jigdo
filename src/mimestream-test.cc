/* $Id$ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2000-2002  |  richard@
  | \/¯|  Richard Atterer          |  atterer.net
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

  Convert binary data to/from ASCII using Base64 encoding

*/

#include <config.h>

#include <iostream>

#include <mimestream.cc>
//______________________________________________________________________

int main() {
  Base64String m;
  char str[] = "Worble, woffle, ßßß";

  for (char* s = str; *s != 0; ++s) m << *s;
  m.flush();
  cout << m.result() << endl;

  m.result().erase();
  for (char* s = str; *s != 0; ++s) m << (*s);
  m << flush; cout << m.result() << endl;

  m.result().erase();
  m.write(str, sizeof(str) - 1); m << flush << flush;
  cout << m.result() << endl;
  m.result().erase();
  m.write((unsigned char*)str, sizeof(str) - 1); m.flush();
  cout << m.result() << endl;
  m.result().erase();
  m.write((signed char*)str, sizeof(str) - 1); m.flush();
  cout << m.result() << endl;
  m.result().erase();
  m.write((void*)str, sizeof(str) - 1); m.flush();
  cout << m.result() << endl;

  m.result().erase();
  m << str << flush;
  cout << m.result() << endl;
  m.result().erase();
  m << (unsigned char*)str; m.flush();
  cout << m.result() << endl;
  m.result().erase();
  m << (signed char*)str; m.flush();
  cout << m.result() << endl;
  m.result().erase();
  m << (void*)str; m.flush();
  cout << m.result() << endl;

  cout << "V29yYmxlLCB3b2ZmbGUsIN/f3w <=" << endl;
  //____________________

  m.result().erase();
  uint32 i = 0xf154ba1f;
  m.put(i).flush();
  cout << m.result() << endl;

  m.result().erase();
  m << i;
  m.flush();
  cout << m.result() << endl;

  m.result().erase();
  m << char(0x1f) << (unsigned char)(0xba) << 0xfff54 << '\xf1';
  m.flush();
  cout << m.result() << endl;

  cout << "H7pU8Q <=" << endl;
  return 0;
}
