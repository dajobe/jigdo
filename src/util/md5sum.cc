/* $Id$ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2000-2004  |  richard@
  | \/¯|  Richard Atterer          |  atterer.net
  ¯ '` ¯
  "Ported" to C++ by RA. Uses glibc code for the actual algorithm.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

  Quite secure 128-bit checksum

*/

#include <config.h>

#include <iostream>
#include <vector>

#include <glibc-md5.hh>
#include <md5sum.hh>
#include <md5sum.ih>
//______________________________________________________________________

void MD5Sum::ProgressReporter::error(const string& message) {
  cerr << message << endl;
}
void MD5Sum::ProgressReporter::info(const string& message) {
  cerr << message << endl;
}
void MD5Sum::ProgressReporter::readingMD5(uint64, uint64) { }

MD5Sum::ProgressReporter MD5Sum::noReport;
//______________________________________________________________________

MD5Sum::MD5Sum(const MD5Sum& md) {
  if (md.p == 0) {
    p = 0;
    for (int i = 0; i < 16; ++i) sum[i] = md.sum[i];
  } else {
    p = new md5_ctx();
    *p = *md.p;
  }
}
//________________________________________

// NB must work with self-assign
MD5Sum& MD5Sum::operator=(const MD5Sum& md) {
# if DEBUG
  finished = md.finished;
# endif
  if (md.p == 0) {
    delete p;
    p = 0;
    for (int i = 0; i < 16; ++i) sum[i] = md.sum[i];
  } else {
    if (p == 0) p = new md5_ctx();
    *p = *md.p;
  }
  return *this;
}
//______________________________________________________________________

string MD5::toString() const {
  Base64String m;
  m.write(sum, 16).flush();
  return m.result();
}
//______________________________________________________________________

bool MD5::operator_less2(const MD5& x) const {
  if (sum[1] < x.sum[1]) return true;
  if (sum[1] > x.sum[1]) return false;
  if (sum[2] < x.sum[2]) return true;
  if (sum[2] > x.sum[2]) return false;
  if (sum[3] < x.sum[3]) return true;
  if (sum[3] > x.sum[3]) return false;
  if (sum[4] < x.sum[4]) return true;
  if (sum[4] > x.sum[4]) return false;
  if (sum[5] < x.sum[5]) return true;
  if (sum[5] > x.sum[5]) return false;
  if (sum[6] < x.sum[6]) return true;
  if (sum[6] > x.sum[6]) return false;
  if (sum[7] < x.sum[7]) return true;
  if (sum[7] > x.sum[7]) return false;
  if (sum[8] < x.sum[8]) return true;
  if (sum[8] > x.sum[8]) return false;
  if (sum[9] < x.sum[9]) return true;
  if (sum[9] > x.sum[9]) return false;
  if (sum[10] < x.sum[10]) return true;
  if (sum[10] > x.sum[10]) return false;
  if (sum[11] < x.sum[11]) return true;
  if (sum[11] > x.sum[11]) return false;
  if (sum[12] < x.sum[12]) return true;
  if (sum[12] > x.sum[12]) return false;
  if (sum[13] < x.sum[13]) return true;
  if (sum[13] > x.sum[13]) return false;
  if (sum[14] < x.sum[14]) return true;
  if (sum[14] > x.sum[14]) return false;
  if (sum[15] < x.sum[15]) return true;
  return false;
}
//______________________________________________________________________

uint64 MD5Sum::updateFromStream(bistream& s, uint64 size, size_t bufSize,
                                ProgressReporter& pr) {
  uint64 nextReport = REPORT_INTERVAL; // When next to call reporter
  uint64 toRead = size;
  uint64 bytesRead = 0;
  vector<byte> buffer;
  buffer.resize(bufSize);
  byte* buf = &buffer[0];
  // Read from stream and update *this
  while (s && !s.eof() && toRead > 0) {
    size_t n = (toRead < bufSize ? toRead : bufSize);
    readBytes(s, buf, n);
    n = s.gcount();
    update(buf, n);
    bytesRead += n;
    toRead -= n;
    if (bytesRead >= nextReport) {
      pr.readingMD5(bytesRead, size);
      nextReport += REPORT_INTERVAL;
    }
  }
  return bytesRead;
}
