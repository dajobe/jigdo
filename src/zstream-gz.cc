/* $Id$ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2004-2005  |  richard@
  | \/¯|  Richard Atterer          |  atterer.net
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

  Zlib compression layer which integrates with C++ streams

*/

#include <config.h>

#include <errno.h>
#include <string.h>
#include <algorithm>
#include <fstream>
#include <new>

#include <log.hh>
#include <md5sum.hh>
#include <serialize.hh>
#include <string.hh>
#include <zstream-gz.hh>
//______________________________________________________________________

DEBUG_UNIT("zstream-gz")

namespace {

  // Turn zlib error codes/messages into C++ exceptions
  void throwZerrorGz(int status, const char* zmsg) {
    string m;
    if (zmsg != 0) m += zmsg;
    Assert(status != Z_OK);
    switch (status) {
    case Z_OK:
      break;
    case Z_ERRNO:
      if (!m.empty() && errno != 0) m += " - ";
      if (errno != 0) m += strerror(errno);
      throw Zerror(Z_ERRNO, m);
      break;
    case Z_MEM_ERROR:
      throw bad_alloc();
      break;
      // NB: fallthrough:
    case Z_STREAM_ERROR:
      if (m.empty()) m = "zlib Z_STREAM_ERROR";
    case Z_DATA_ERROR:
      if (m.empty()) m = "zlib Z_DATA_ERROR";
    case Z_BUF_ERROR:
      if (m.empty()) m = "zlib Z_BUF_ERROR";
    case Z_VERSION_ERROR:
      if (m.empty()) m = "zlib Z_VERSION_ERROR";
    default:
      throw Zerror(status, m);
    }
  }

}
//________________________________________

// void Zobstream::throwZerror(int status, const char* zmsg) {
//   ::throwZerror(status, zmsg);
// }
// void Zibstream::throwZerror(int status, const char* zmsg) {
//   ::throwZerror(status, zmsg);
// }
//______________________________________________________________________

void ZobstreamGz::open(bostream& s, unsigned chunkLimit, int level,
                       int windowBits, int memLevel, unsigned todoBufSz) {
  z.next_in = 0;
  z.next_out = zipBuf->data;
  z.avail_out = (zipBuf == 0 ? 0 : ZIPDATA_SIZE);
  z.total_in = 0;
  debug("deflateInit2");
  int status = deflateInit2(&z, level, Z_DEFLATED, windowBits, memLevel,
                            Z_DEFAULT_STRATEGY);
  if (status == Z_OK)
    memReleased = false;
  else
    throwZerrorGz(status, z.msg);

  // Declare stream as open
  debug("opening");
  Zobstream::open(s, chunkLimit, todoBufSz);
  debug("opened");
}
//______________________________________________________________________

unsigned ZobstreamGz::partId() {
  return 0x41544144u; // "DATA"
}

void ZobstreamGz::deflateEnd() {
  int status = ::deflateEnd(&z);
  memReleased = true;
  if (status != Z_OK) throwZerrorGz(status, z.msg);
}

void ZobstreamGz::deflateReset() {
  int status = ::deflateReset(&z);
  if (status != Z_OK) throwZerrorGz(status, z.msg);
}
//______________________________________________________________________

void ZobstreamGz::zip2(byte* start, unsigned len, bool finish) {
  debug("zip2 %1 bytes at %2", len, start);
  int flush = (finish ? Z_FINISH : Z_NO_FLUSH);
  Assert(is_open());

  // If big enough, finish and write out this chunk
  if (z.total_out > chunkLim()) flush = Z_FINISH;

  // true <=> must call deflate() at least once
  bool callZlibOnce = (flush != Z_NO_FLUSH);

  z.next_in = start;
  z.avail_in = len;
  while (z.avail_in != 0 || z.avail_out == 0 || callZlibOnce) {
    callZlibOnce = false;

    if (z.avail_out == 0) {
      // Get another output buffer object
      ZipData* zd;
      if (zipBufLast == 0 || zipBufLast->next == 0) {
        zd = new ZipData();
        if (zipBuf == 0) zipBuf = zd;
        if (zipBufLast != 0) zipBufLast->next = zd;
      } else {
        zd = zipBufLast->next;
      }
      zipBufLast = zd;
      z.next_out = zd->data;
      z.avail_out = ZIPDATA_SIZE;
      //cerr << "Zob: new ZipData @ " << &zd->data << endl;
    }

    debug("deflate ni=%1 ai=%2 no=%3 ao=%4",
          z.next_in, z.avail_in, z.next_out, z.avail_out);
    //memset(z.next_in, 0, z.avail_in);
    //memset(z.next_out, 0, z.avail_out);
    int status = deflate(&z, flush); // Call zlib
    debug("deflated ni=%1 ai=%2 no=%3 ao=%4 status=%5",
          z.next_in, z.avail_in, z.next_out, z.avail_out, status);
    //cerr << "zip(" << (void*)start << ", " << len << ", " << flush
    //     << ") returned " << status << endl;
    if (status == Z_STREAM_END
//      || (status == Z_OK && z.total_out > chunkLim)
        || (flush == Z_FULL_FLUSH && z.total_in != 0)) {
      writeZipped(0x41544144u); // DATA
      flush = Z_NO_FLUSH;
    }

    if (status == Z_STREAM_END) continue;
    if (status != Z_OK) throwZerrorGz(status, z.msg);
  }
}
