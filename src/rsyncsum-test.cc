/* $Id$ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2000-2002  |  richard@
  | \/¯|  Richard Atterer          |  atterer.net
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

  A 32 or 64 bit rolling checksum

  Command line argument: Name of file to use for test. Will not output
  anything if test is OK.

*/

#include <stdio.h>
#include <stdlib.h>

#include <iostream>
#include <fstream>

#include <rsyncsum.cc>
//______________________________________________________________________

#ifdef CREATE_CONSTANTS

int main(int, char* argv[]) {
  uint32 data[256];
  FILE* f = fopen(argv[1], "r");
  fread(data, sizeof(uint32), 256, f);
  for (int row = 0; row < 64; ++row) {
    uint32* r = data + 4*row;
    printf("  0x%08x, 0x%08x, 0x%08x, 0x%08x,\n", r[0], r[1], r[2], r[3]);
  }
}

//======================================================================

#else

namespace {
  int errs;
  char estr[17] = "                ";
}

inline uint32 get_checksum1(byte *buf1,int len) {
  RsyncSum s(buf1, len);
  return s.get();
}

inline void error(int i, bool assertion) {
  if (assertion) {
    if (estr[i] == ' ')
      estr[i] = '.';
  } else {
    ++errs;
    estr[i] = '*';
  }
}

void printBlockSums(size_t blockSize, const char* fileName) {
  ifstream file(fileName, ios::binary);
  byte buf[blockSize];
  byte* bufEnd = buf + blockSize;

  while (file) {
    // read another block
    byte* cur = buf;
    while (cur < bufEnd && file) {
      file.read(cur, bufEnd - cur); // Fill buffer
      cur += file.gcount();
    }
    RsyncSum64 sum(buf, cur - buf);
    cout << ' ' << sum << endl;
  }
  exit(0);
}

int main(int argc, char* argv[]) {
  if (argc == 3) {
    // 2 cmdline args, blocksize and filename. Print RsyncSums of all blocks
    printBlockSums(atoi(argv[1]), argv[2]);
  } else if (argc != 2) {
    cerr << "Try " << argv[0] << " [blocksize] filename" << endl;
    exit(1);
  }
  // 1 cmdline arg => play around a little with the data
  const int CHUNK = 8192;
  FILE* f = fopen(argv[1], "r");
  byte* mem = (byte*)malloc(CHUNK);
  size_t size = CHUNK; /* of *mem */
  size_t read = 0;
  size_t totalread = 0; /* bytes in file */

  while ((read = fread(mem + totalread, 1, CHUNK, f)) != 0 && !feof(f)) {
    totalread += read;
    size = totalread + CHUNK;
    /*printf("%d  %d  %d\n", read, totalread, size);*/
    mem = (byte*)realloc(mem, size);
  }
  totalread += read;
  //________________________________________

  errs = 0;

  {
    RsyncSum rs;
    rs.addBack(mem, totalread);
    error(0, get_checksum1(mem, totalread) == rs.get());

    if (totalread > 256) {
      RsyncSum roll(mem + 32, 64);
      RsyncSum noRoll(roll);
      rs.reset().addBack(mem, 128);
      for (int i = 0; i < 32; ++i) {
        RsyncSum x = rs;
        // add stuff to end
        x.addBack(mem + 128, i);
        error(1, get_checksum1(mem, 128 + i) == x.get());
        // roll by removing one byte at front, adding one at end
        roll.removeFront(mem[32+i], 64).addBack(mem[32+64+i]);
      }
      error(2, get_checksum1(mem+64, 64) == roll.get());
      // roll by 32 bytes in one go
      noRoll.removeFront(mem+32, 32, 64).addBack(mem+32+64, 32);
      error(3, roll == noRoll);
    }
  }
  //____________________
  {
    RsyncSum64 rs, y;

    if (totalread > 256) {
      RsyncSum64 roll(mem + 32, 64);
      RsyncSum64 noRoll(roll);
      rs.reset().addBack(mem, 128);
      for (int i = 0; i < 32; ++i) {
        RsyncSum64 x = rs;
        // add stuff to end
        x.addBack(mem + 128, i);
        error(4, y.reset().addBack(mem, 128 + i) == x);
        // roll by removing one byte at front, adding one at end
        roll.removeFront(mem[32+i], 64).addBack(mem[32+64+i]);
      }
      error(2, y.reset().addBack(mem+64, 64) == roll);
      // roll by 32 bytes in one go
      noRoll.removeFront(mem+32, 32, 64).addBack(mem+32+64, 32);
      error(3, roll == noRoll);
    }
  }

  if (errs != 0)
    printf("%s %s\n", estr, argv[1]);
  //________________________________________

  return 0;
}

#endif
