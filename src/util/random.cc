/* $Id$ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2005  |  richard@
  | \/¯|  Richard Atterer     |  atterer.net
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify it
  under the terms of the GNU General Public License, version 2. See the file
  COPYING for details.

*//** @file

  Pseudo random number generation

*/

#include <config.h>
#include <md5sum.hh>

#include <fstream>
//______________________________________________________________________

void update(MD5Sum& md, uint32 x) {
  md.update(static_cast<byte>(x));
  md.update(static_cast<byte>(x >> 8));
  md.update(static_cast<byte>(x >> 16));
  md.update(static_cast<byte>(x >> 24));
}
//______________________________________________________________________

struct Rand {
  MD5Sum md;

  struct {
    uint32 nr;
    uint32 serial;
    MD5 r;
  } hashData;

  byte* rptr; // points to one of hashData.r's elements
  byte* rend;
  uint32 res; // Bit reservoir
  size_t bitsInRes;
  bool msg;

  Rand(uint32 nr, bool printMessages = false) {
    hashData.nr = nr;
    hashData.serial = 0;
    hashData.r.clear();
    rptr = rend = &hashData.r.sum[0] + 16;
    res = 0;
    bitsInRes = 0;
    msg = printMessages;
  }

  // Create another 128 semi-random bits in md
  void thumbScrew();
  // Return n semi-random bits, n <= 24
  uint32 get(size_t n) {
    while (bitsInRes < n) {
      if (rptr == rend) thumbScrew();
      res |= (*rptr++) << bitsInRes;
      bitsInRes += 8;
    }
    uint32 r = res & ((1 << n) - 1);
    res >>= n;
    bitsInRes -= n;
    return r;
  }
  // Return an integer in the range 0...n-1
  uint32 rnd(size_t n) {
    return static_cast<uint32>(static_cast<uint64>(get(24)) * n / 0x1000000);
  }
};

void Rand::thumbScrew() {
  md.reset();
  update(md, hashData.nr);
  update(md, hashData.serial);
  md.update(&hashData.r.sum[0], 16 * sizeof(byte));
  md.finishForReuse();
  hashData.r = md;
  ++hashData.serial;
  rptr = &hashData.r.sum[0];
  //cout << '<' << hashData.r << '>' << endl;
}
//______________________________________________________________________

int main(int argc, const char* argv[]) {

  if (argc <= 1) {
    cerr << "Syntax: " << argv[0] << " <nr-of-bytes> [<byteVal>]\n"
      "nr-of-bytes can have a trailing `k' for kiloBytes\n"
      "If byteVal is present, not random bytes are written, but bytes with\n"
      "the specified value." << endl;
    return 1;
  }
  //____________________

  // Parse args
  uint32 bytesArg = 0;
  const char* p = argv[1];
  while (*p >= '0' && *p <= '9') {
    bytesArg = bytesArg * 10 + *p - '0';
    ++p;
  }
  if (*p == 'k') {
    bytesArg *= 1024;
    ++p;
  }
  if (*p != '\0') {
    cerr << "Wrong format for argument 1: `" << argv[1] << '\'' << endl;
    return 1;
  }
  //cerr << bytesArg << endl;

  uint32 byteVal = 256;
  if (argc >= 3) {
    byteVal = 0;
    const char* p = argv[2];
    while (*p >= '0' && *p <= '9') {
      byteVal = byteVal * 10 + *p - '0';
      ++p;
    }
  }
  //____________________

  // Init PRNG
  uint32 randomSeed = 0;
  {
    fstream randomFile(".random");
    if (randomFile) randomFile >> randomSeed;
  }
  Rand rand(randomSeed);

  byte buf[1024];
  if (byteVal == 256) {
    // Write random bytes
    while (bytesArg > 0) {
      uint32 n = (bytesArg > 1024 ? 1024 : bytesArg);
      for (uint32 i = 0; i < n; ++i) buf[i] = rand.get(8);
      cout.write((char*)(buf), n);
      bytesArg -= n;
    }
  } else {
    // Write bytes with value of byteVal
    memset(buf, byteVal, 1024);
    while (bytesArg > 0) {
      uint32 n = (bytesArg > 1024 ? 1024 : bytesArg);
      cout.write((char*)(buf), n);
      bytesArg -= n;
    }
  }

  fstream randomFile(".random", fstream::out | fstream::trunc);
  ++randomSeed;
  randomFile << randomSeed;
}
