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
      string m = subst("libbz error %1", status);
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

/* libbz2 subdivides input data into chunks of 100000 to 900000 bytes,
   depending on compression level. For best compression results, BZIP input
   data chunks should be exactly as large as the block size. The bzip2
   sources use n*100000-19 as the actual block size in one place, we'll use
   n*100000-50 just to be safe. NB: chunkLim() is the OUTPUT size limit for
   gzip, but the INPUT size limit for bzip2. */
void ZobstreamBz::open(bostream& s, int level, unsigned todoBufSz) {

  // Unlike zlib, libbz2 does not support level==0
  if (level == 0) level = 1;
  compressLevel = level;

  z.next_in = 0;
  z.next_out = reinterpret_cast<char*>(zipBuf->data);
  z.avail_out = (zipBuf == 0 ? 0 : ZIPDATA_SIZE);
  z.total_in_lo32 = z.total_in_hi32 = 0;
  debug("BZ2_bzCompressInit");
  int verbosity = 0;
# if DEBUG
  if (debug.enabled()) verbosity = 2;
# endif
  int status = BZ2_bzCompressInit(&z, level, verbosity,
                                  0/*default workFactor*/);
  if (status == BZ_OK)
    memReleased = false;
  else
    throwZerrorBz(status);

  // Declare stream as open
  debug("opening");
  Zobstream::open(s, 100000 * level - 50, todoBufSz);
  debug("opened, chunkLim=%1", chunkLim());
}
//______________________________________________________________________

unsigned ZobstreamBz::partId() {
  return 0x41544144u; // "DATA"
}

void ZobstreamBz::deflateEnd() {
  int status = BZ2_bzCompressEnd(&z);
  memReleased = true;
  if (status != BZ_OK) throwZerrorBz(status);
}

void ZobstreamBz::deflateReset() {
  int status = BZ2_bzCompressEnd(&z);
  memReleased = true;
  if (status != BZ_OK) throwZerrorBz(status);

  z.next_in = 0;
  z.next_out = reinterpret_cast<char*>(zipBuf->data);
  z.avail_out = (zipBuf == 0 ? 0 : ZIPDATA_SIZE);
  z.total_in_lo32 = z.total_in_hi32 = 0;
  debug("BZ2_bzCompressInit deflateReset");
  int verbosity = 0;
# if DEBUG
  if (debug.enabled()) verbosity = 2;
# endif
  status = BZ2_bzCompressInit(&z, compressLevel, verbosity,
                              0/*default workFactor*/);
  if (status == BZ_OK)
    memReleased = false;
  else
    throwZerrorBz(status);
}
//______________________________________________________________________

void ZobstreamBz::zip2(byte* start, unsigned len, bool finish) {
  debug("zip2 %1 bytes at %2", len, start);
  int flush = (finish ? BZ_FINISH : BZ_RUN);
  Assert(is_open());

  if (z.total_in_lo32 <= chunkLim() && chunkLim() <= z.total_in_lo32 + len)
    flush = BZ_FINISH;

  // true <=> must call BZ2_bzCompress() at least once
  bool callLibBzOnce = (flush != BZ_RUN);

  z.next_in = reinterpret_cast<char*>(start);
  z.avail_in = len;
  while (z.avail_in != 0 || z.avail_out == 0 || callLibBzOnce) {
    callLibBzOnce = false;

    // If big enough, finish and write out this chunk
    int availInDifference = 0;
    if (z.total_in_lo32 <= chunkLim()
        && chunkLim() <= z.total_in_lo32 + z.avail_in) {
      // Adjust avail_in to hit the bzip2 block size exactly
      availInDifference = chunkLim() - z.total_in_lo32 - z.avail_in; // <0
      flush = BZ_FINISH;
    }

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
          (void*)(z.next_in), z.avail_in, (void*)(z.next_out), z.avail_out);
    //memset(z.next_in, 0, z.avail_in);
    //memset(z.next_out, 0, z.avail_out);
    z.avail_in += availInDifference;
    debug("deflate ai=%1 availInDifference=%2 ti=%3",
          z.avail_in, availInDifference, z.total_in_lo32);
    int status = BZ2_bzCompress(&z, flush); // Call libbz2
    z.avail_in -= availInDifference;
    debug("deflated ni=%1 ai=%2 no=%3 ao=%4 status=%5", (void*)(z.next_in),
          z.avail_in, (void*)(z.next_out), z.avail_out, status);
    if (status == BZ_STREAM_END) {
      unsigned ai = z.avail_in;
      char* ni = z.next_in;
      writeZipped(0x50495a42u); // BZIP
      flush = BZ_RUN;
      z.avail_in = ai;
      z.next_in = ni;
    }

    if (status == BZ_STREAM_END) continue;
    if (status != BZ_RUN_OK && status != BZ_FINISH_OK) throwZerrorBz(status);
  }
}
