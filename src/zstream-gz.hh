/* $Id$ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2001-2002  |  richard@
  | \/¯|  Richard Atterer          |  atterer.net
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

*/

#ifndef ZSTREAM_GZ_HH
#define ZSTREAM_GZ_HH

#include <config.h>

#include <zlib.h>

#include <zstream.hh>
//______________________________________________________________________

struct ZerrorGz : public Zerror {
  ZerrorGz(int s, const string& m) : Zerror(s, m) { }
  int status;
};
//______________________________________________________________________

class ZobstreamGz : public Zobstream {
public:
  inline ZobstreamGz(bostream& s, size_t chunkLimit,
                     int level = Z_DEFAULT_COMPRESSION, int windowBits = 15,
                     int memLevel = 8, size_t todoBufSz = 256U,
                     MD5Sum* md = 0);

  /** @param s Output stream
      @param chunkLimit Size limit for output data, will buffer this much
      @param level 0 to 9
      @param windowBits zlib param
      @param memLevel zlib param
      @param todoBufSz Size of mini buffer, which holds data sent to
      the stream with single put() calls or << statements */
  void open(bostream& s, size_t chunkLimit, int level =Z_DEFAULT_COMPRESSION,
            int windowBits = 15, int memLevel = 8, size_t todoBufSz = 256U);

protected:
  virtual void deflateEnd();
  virtual void deflateReset();
  virtual unsigned totalOut() const { return z.total_out; }
  virtual unsigned totalIn() const { return z.total_in; }
  virtual unsigned availOut() const { return z.avail_out; }
  virtual unsigned availIn() const { return z.avail_in; }
  virtual byte* nextOut() const { return z.next_out; }
  virtual byte* nextIn() const { return z.next_in; }
  virtual void setTotalOut(unsigned n) { z.total_out = n; }
  virtual void setTotalIn(unsigned n) { z.total_in = n; }
  virtual void setAvailOut(unsigned n) { z.avail_out = n; }
  virtual void setAvailIn(unsigned n) { z.avail_in = n; }
  virtual void setNextOut(byte* n) { z.next_out = n; }
  virtual void setNextIn(byte* n) { z.next_in = n; }
  virtual void zip2(byte* start, unsigned len, bool finish = false);

private:
  // Throw a Zerror exception, or bad_alloc() for status==Z_MEM_ERROR
//   inline void throwZerror(int status, const char* zmsg);

  z_stream z;
};
//______________________________________________________________________

ZobstreamGz::ZobstreamGz(bostream& s, size_t chunkLimit, int level,
                         int windowBits, int memLevel, size_t todoBufSz,
                         MD5Sum* md) : Zobstream(md) {
  z.zalloc = (alloc_func)0;
  z.zfree = (free_func)0;
  z.opaque = 0;
  open(s, chunkLimit, level, windowBits, memLevel, todoBufSz);
}

#endif
