/* $Id$ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2000-2002  |  richard@
  | \/¯|  Richard Atterer          |  atterer.net
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

  Convert binary data to/from ASCII using Base64 encoding

  #test-deps

*/

#include <config.h>

#include <iostream>

#include <debug.hh>
#include <log.hh>
#include <mimestream.hh>
//______________________________________________________________________

int main(int argc, char* argv[]) {
  if (argc == 2) Logger::scanOptions(argv[1], argv[0]);

  const char* correct = "V29yYmxlLCB3b2ZmbGUsIN_f3w";
  Base64String m;
  char str[] = "Worble, woffle, ßßß";

  for (char* s = str; *s != 0; ++s) m << *s;
  m.flush();
  Assert(m.result() == correct);

  m.result().erase();
  for (char* s = str; *s != 0; ++s) m << (*s);
  m << flush; Assert(m.result() == correct);

  m.result().erase();
  m.write(str, sizeof(str) - 1); m << flush << flush;
  Assert(m.result() == correct);
  m.result().erase();
  m.write((unsigned char*)str, sizeof(str) - 1); m.flush();
  Assert(m.result() == correct);
  m.result().erase();
  m.write((signed char*)str, sizeof(str) - 1); m.flush();
  Assert(m.result() == correct);
  m.result().erase();
  m.write((void*)str, sizeof(str) - 1); m.flush();
  Assert(m.result() == correct);

  m.result().erase();
  m << str << flush;
  Assert(m.result() == correct);
  m.result().erase();
  m << (unsigned char*)str; m.flush();
  Assert(m.result() == correct);
  m.result().erase();
  m << (signed char*)str; m.flush();
  Assert(m.result() == correct);
  m.result().erase();
  m << (void*)str; m.flush();
  Assert(m.result() == correct);
  //____________________

  correct = "H7pU8Q";

  m.result().erase();
  uint32 i = 0xf154ba1f;
  m.put(i).flush();
  Assert(m.result() == correct);

  m.result().erase();
  m << i;
  m.flush();
  Assert(m.result() == correct);

  m.result().erase();
  m << char(0x1f) << (unsigned char)(0xba) << 0xfff54 << '\xf1';
  m.flush();
  Assert(m.result() == correct);
  //____________________

  /* Try many of the 16.7 million ways of a "3 byte" <=> "4 ASCII chars"
     mapping */
  byte x[3*256];
  for (unsigned k = 0; k < 256; ++k) x[k * 3 + 2] = byte(k);
  Base64String toAscii;
  Base64StringI toBin;
  unsigned j = 0;
  for (unsigned i = 0; i < 256; i += 57) {
    for (unsigned k = 0; k < 256; ++k) x[k * 3 + 0] = i;
    j = j & 0xffU;
    for (; j < 256; j += 43) {
      for (unsigned k = 0; k < 256; ++k) x[k * 3 + 1] = j;
      // Binary -> ASCII
      toAscii.write(x, 3*256).flush();
      Assert(toAscii.result().length() == 4*256);
      // ASCII -> binary
      toBin.put(toAscii.result().data(), 4*256);
      vector<byte>& r = toBin.result();
      Assert(r.size() == 3*256);
      for (unsigned k = 0; k < 256; ++k) {
        if (r[k*3] != x[k*3] || r[k*3+1] != x[k*3+1] || r[k*3+2] != x[k*3+2]) {
          cout << endl;
          cout << "x=["<<int(x[k*3])<<','<<int(x[k*3+1])<<','<<int(x[k*3+2])<<"] "
                  "r=["<<int(r[k*3])<<','<<int(r[k*3+1])<<','<<int(r[k*3+2])<<"] "
               << toAscii.result() << endl;
          return 1;
        }
      }
      toBin.result().clear();
      toAscii.result().erase();
      toBin.reset();
    }
    //cout << ' ' << 255-i << "   \r" << flush;
  }

  return 0;
}
