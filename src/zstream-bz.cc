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
#include <zstream-bz.hh>
//______________________________________________________________________

DEBUG_UNIT("zstream-bz")

namespace {

  // Turn zlib error codes/messages into C++ exceptions
  void throwZerrorBz(int status) {
    Assert(status != BZ_OK);
    switch (status) {
    case BZ_OK:
      break;
    case BZ_MEM_ERROR:
      throw bad_alloc();
      break;
    case -1: case -2: case -4: case -5:
    case -6: case -7: case -8: case -9:
      throw Zerror(status, ZibstreamBz::bzerrorstrings[-status]);
    default:
      string m = "libbz error ";
      m += status;
      throw Zerror(status, m);
    }
  }

} // namespace
//______________________________________________________________________

const char* ZibstreamBz::bzerrorstrings[] = {
  "OK"
  ,"SEQUENCE_ERROR"
  ,"PARAM_ERROR"
  ,"MEM_ERROR"
  ,"DATA_ERROR"
  ,"DATA_ERROR_MAGIC"
  ,"IO_ERROR"
  ,"UNEXPECTED_EOF"
  ,"OUTBUFF_FULL"
  ,"CONFIG_ERROR"
  ,""
  ,""
  ,""
};
//______________________________________________________________________

void ZobstreamBz::open(bostream& s, unsigned chunkLimit, int level,
                       unsigned todoBufSz) {
  // Unlike zlib, libbz2 does not support level==0
  if (level == 0) level = 1;
  compressLevel = level;

  z.next_in = z.next_out = 0;
  z.avail_out = (zipBuf == 0 ? 0 : ZIPDATA_SIZE);
  z.total_in_lo32 = z.total_in_hi32 = 0;
  debug("ZobstreamBz::open deflateInit2");
  int status = BZ2_bzCompressInit(&z, level, 0/*verbosity*/,
                                  0/*default workFactor*/);
  if (status != BZ_OK) throwZerrorBz(status);

  // Declare stream as open
  debug("opening");
  Zobstream::open(s, chunkLimit, todoBufSz);
  debug("opened");
}
//______________________________________________________________________

unsigned ZobstreamBz::partId() {
  return 0x41544144u; // "DATA"
}

void ZobstreamBz::deflateEnd() {
  int status = BZ2_bzCompressEnd(&z);
  if (status != BZ_OK) throwZerrorBz(status);
}

void ZobstreamBz::deflateReset() {
  int status = BZ2_bzCompressEnd(&z);
  if (status != BZ_OK) throwZerrorBz(status);

  z.next_in = z.next_out = 0;
  z.avail_out = (zipBuf == 0 ? 0 : ZIPDATA_SIZE);
  z.total_in_lo32 = z.total_in_hi32 = 0;
  debug("ZobstreamBz::open deflateInit2");
  status = BZ2_bzCompressInit(&z, compressLevel, 0/*verbosity*/,
                              0/*default workFactor*/);
  if (status != BZ_OK) throwZerrorBz(status);
}
//______________________________________________________________________

void ZobstreamBz::zip2(byte* start, unsigned len, bool finish) {
  debug("zip2 %1 bytes at %2", len, start);
  int flush = (finish ? BZ_FINISH : BZ_RUN);
  Assert(is_open());

#warning "TODO: Feed exactly x*100000 bytes before finishing"
  // If big enough, finish and write out this chunk
  if (z.total_out_lo32 > chunkLim()) flush = BZ_FINISH;

  // true <=> must call BZ2_bzCompress() at least once
  bool callZlibOnce = (flush != BZ_RUN);

  z.next_in = reinterpret_cast<char*>(start);
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
      z.next_out = reinterpret_cast<char*>(zd->data);
      z.avail_out = ZIPDATA_SIZE;
      //cerr << "Zob: new ZipData @ " << &zd->data << endl;
    }

    debug("deflate ni=%1 ai=%2 no=%3 ao=%4",
          z.next_in, z.avail_in, z.next_out, z.avail_out);
    //memset(z.next_in, 0, z.avail_in);
    //memset(z.next_out, 0, z.avail_out);
    int status = BZ2_bzCompress(&z, flush); // Call libbz2
    debug("deflated ni=%1 ai=%2 no=%3 ao=%4 status=%5",
          z.next_in, z.avail_in, z.next_out, z.avail_out, status);
    //cerr << "zip(" << (void*)start << ", " << len << ", " << flush
    //     << ") returned " << status << endl;
    if (status == BZ_STREAM_END) {
      writeZipped();
      flush = BZ_RUN;
    }

    if (status == BZ_STREAM_END) continue;
    if (status != BZ_OK) throwZerrorBz(status);
  }
}
