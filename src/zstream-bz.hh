/* $Id$ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2004-2005  |  richard@
  | \/¯|  Richard Atterer          |  atterer.net
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

*/

#ifndef ZSTREAM_BZ_HH
#define ZSTREAM_BZ_HH

#include <config.h>

#include <bzlib.h>

#include <log.hh>
#include <zstream.hh>
//______________________________________________________________________

struct ZerrorBz : public Zerror {
  ZerrorBz(int s, const string& m) : Zerror(s, m) { }
  int status;
};
//______________________________________________________________________

class ZobstreamBz : public Zobstream {
public:
  inline ZobstreamBz(bostream& s, int level /*= 6*/,
                     unsigned todoBufSz /*= 256U*/, MD5Sum* md /*= 0*/);
  ~ZobstreamBz() { Assert(memReleased); }

  /** @param s Output stream
      @param level 1 to 9 (0 is allowed but interpreted as 1)
      @param todoBufSz Size of mini buffer, which holds data sent to
      the stream with single put() calls or << statements */
  void open(bostream& s, int level /*= 6*/, unsigned todoBufSz/* = 256U*/);

protected:
  virtual unsigned partId();
  virtual void deflateEnd();
  virtual void deflateReset();
  virtual unsigned totalOut() const { return z.total_out_lo32; }
  virtual unsigned totalIn() const { return z.total_in_lo32; }
  virtual unsigned availOut() const { return z.avail_out; }
  virtual unsigned availIn() const { return z.avail_in; }
  virtual byte* nextOut() const { return reinterpret_cast<byte*>(z.next_out); }
  virtual byte* nextIn() const { return reinterpret_cast<byte*>(z.next_in); }
  virtual void setTotalOut(unsigned n) {
    z.total_out_lo32 = n; z.total_out_hi32 = 0; }
  virtual void setTotalIn(unsigned n) {
    z.total_in_lo32 = n; z.total_in_hi32 = 0; }
  virtual void setAvailOut(unsigned n) { z.avail_out = n; }
  virtual void setAvailIn(unsigned n) { z.avail_in = n; }
  virtual void setNextOut(byte* n) {
    z.next_out = reinterpret_cast<char*>(n); }
  virtual void setNextIn(byte* n) {
    z.next_in = reinterpret_cast<char*>(n); }
  virtual void zip2(byte* start, unsigned len, bool finish = false);

private:
  bz_stream z;
  int compressLevel;
  bool memReleased;
};
//______________________________________________________________________

class ZibstreamBz : public Zibstream::Impl {
public:

  class ZibstreamBzError : public Zerror {
  public:
    ZibstreamBzError(int s, const string& m) : Zerror(s, m) { }
  };

  ZibstreamBz() : status(0), memReleased(true) { }
  ~ZibstreamBz() { Assert(memReleased); }

  virtual unsigned totalOut() const { return z.total_out_lo32; }
  virtual unsigned totalIn() const { return z.total_in_lo32; }
  virtual unsigned availOut() const { return z.avail_out; }
  virtual unsigned availIn() const { return z.avail_in; }
  virtual byte* nextOut() const { return reinterpret_cast<byte*>(z.next_out); }
  virtual byte* nextIn() const { return reinterpret_cast<byte*>(z.next_in); }
  virtual void setTotalOut(unsigned n) {
    z.total_out_lo32 = n; z.total_out_hi32 = 0; }
  virtual void setTotalIn(unsigned n) {
    z.total_in_lo32 = n; z.total_in_hi32 = 0; }
  virtual void setAvailIn(unsigned n) { z.avail_in = n; }
  virtual void setNextIn(byte* n) {
    z.next_in = reinterpret_cast<char*>(n); }

  virtual void init() {
    z.bzalloc = 0;
    z.bzfree = 0;
    z.opaque = 0;
    status = BZ2_bzDecompressInit(&z, 0/*silent*/, 0/*fast*/);
    if (ok()) memReleased = false;
  }
  virtual void end() { status = BZ2_bzDecompressEnd(&z); memReleased = true; }
  virtual void reset() {
    end();
    if (status == BZ_OK) init();
  }

  virtual void inflate(byte** nextOut, unsigned* availOut) {
    z.next_out = reinterpret_cast<char*>(*nextOut); z.avail_out = *availOut;
    status = BZ2_bzDecompress(&z);
    *nextOut = reinterpret_cast<byte*>(z.next_out); *availOut = z.avail_out;
  }
  virtual bool streamEnd() const { return status == BZ_STREAM_END; }
  virtual bool ok() const { return status == BZ_OK; }

  static const char* bzerrorstrings[];
  virtual void throwError() const {
    int s = status;
    if (s > 0) s = 0;
    throw ZibstreamBzError(status, bzerrorstrings[s*-1]);
  }
private:
  int status;
  bz_stream z;
  bool memReleased;
};
//======================================================================

ZobstreamBz::ZobstreamBz(bostream& s, int level, unsigned todoBufSz,
                         MD5Sum* md)
    : Zobstream(md), memReleased(true) {
  z.bzalloc = 0;
  z.bzfree = 0;
  z.opaque = 0;
  open(s, level, todoBufSz);
}

#endif
