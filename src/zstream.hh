/* $Id$ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2001-2004  |  richard@
  | \/¯|  Richard Atterer          |  atterer.net
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

  Zlib compression layer which integrates with C++ streams. When
  deflating, chops up data into DATA chunks of approximately
  zippedBufSz (see ctor below and ../doc/TechDetails.txt).

  Maybe this should use streambuf, but 1) I don't really understand
  streambuf, and 2) the C++ library that comes with GCC 2.95 probably
  doesn't fully support it (not sure tho).

*/

#ifndef ZSTREAM_HH
#define ZSTREAM_HH

#include <config.h>

#include <iostream>

#include <bstream.hh>
#include <config.h>
#include <debug.hh>
#include <md5sum.fh>
#include <zstream.fh>
//______________________________________________________________________

/** Error messages returned by the zlib library. Note that bad_alloc()
    is thrown if zlib returns Z_MEM_ERROR. Base class for ZerrorGz. */
struct Zerror : Error {
  Zerror(int s, const string& m) : Error(m), status(s) { }
  int status;
};

/** Output stream which compresses the data sent to it before writing
    it to its final destination. The b is for buffered - this class
    will buffer *all* generated output in memory until the bytes
    written to the underlying stream (i.e. the compressed data) exceed
    chunkLimit - once this happens, the zlib data is flushed and the
    resultant compressed chunk written out with a header (cf
    ../doc/TechDetails.txt for format). This is jigdo-specific.

    Additional features mainly useful for jigdo: If an MD5Sum object
    is passed to Zobstream(), any data written to the output stream is
    also passed to the object. */
class Zobstream {
public:

  inline explicit Zobstream(MD5Sum* md = 0);
  /** Calls close(), which might throw a Zerror exception! Call
      close() before destroying the object to avoid this. */
  virtual ~Zobstream() { close(); delete zipBuf; Assert(todoBuf == 0); }
//   inline Zobstream(bostream& s, size_t chunkLimit,
//                    int level = Z_DEFAULT_COMPRESSION, int windowBits = 15,
//                    int memLevel = 8, size_t todoBufSz = 256U,
//                    MD5Sum* md = 0);
  bool is_open() const { return stream != 0; }
  /** @param s Output stream
      @param chunkLimit Size limit for output data, will buffer this much
      @param level 0 to 9
      @param windowBits zlib param
      @param memLevel zlib param
      @param todoBufSz Size of mini buffer, which holds data sent to
      the stream with single put() calls or << statements */
//   void open(bostream& s, size_t chunkLimit, int level =Z_DEFAULT_COMPRESSION,
//             int windowBits = 15, int memLevel = 8, size_t todoBufSz = 256U);
  /// Forces any remaining data to be compressed and written out
  void close();

  /// Get reference to underlying ostream
  bostream& getStream() { return *stream; }

  /// Output 1 character
  inline Zobstream& put(unsigned char x);
  inline Zobstream& put(signed char x);
  /// Output the low 8 bits of an integer
  inline Zobstream& put(int x);
  inline Zobstream& put(char x);
  /// Output 32 bit integer in little-endian order
  Zobstream& put(uint32 x);
  /// Output n characters
//   inline Zobstream& write(const char* x, size_t n);
//   inline Zobstream& write(const signed char* x, size_t n);
//   Zobstream& write(const unsigned char* x, size_t n);
//   inline Zobstream& write(const void* x, size_t n);
  inline Zobstream& write(const byte* x, unsigned n);

protected:
  static const unsigned ZIPDATA_SIZE = 64*1024;

  // Child classes must call this when their open() is called
  inline void open(bostream& s, unsigned chunkLimit, unsigned todoBufSz);
  unsigned chunkLim() const { return chunkLimVal; }
  // Write data in zipBuf
  void writeZipped();

  virtual void deflateEnd() = 0; // May throw Zerror
  virtual void deflateReset() = 0; // May throw Zerror

  virtual unsigned totalOut() const = 0;
  virtual unsigned totalIn() const = 0;
  virtual unsigned availOut() const = 0;
  virtual unsigned availIn() const = 0;
  virtual byte* nextOut() const = 0;
  virtual byte* nextIn() const = 0;
  virtual void setTotalOut(unsigned n) = 0;
  virtual void setTotalIn(unsigned n) = 0;
  virtual void setAvailOut(unsigned n) = 0;
  virtual void setAvailIn(unsigned n) = 0;
  virtual void setNextOut(byte* n) = 0;
  virtual void setNextIn(byte* n) = 0;

  virtual void zip2(byte* start, unsigned len, bool finish) = 0;

  /* Compressed data is stored in a linked list of ZipData objects.
     During the Zobstream object's lifetime, the list is only ever
     added to, never shortened. */
  struct ZipData {
    ZipData() : next(0) { }
    ~ZipData() { delete next; }
    ZipData* next;
    byte data[ZIPDATA_SIZE];
  };
  ZipData* zipBuf; // Start of linked list
  ZipData* zipBufLast; // Last link

private:
  static const unsigned MIN_TODOBUF_SIZE = 256;

//   // Throw a Zerror exception, or bad_alloc() for status==Z_MEM_ERROR
//   inline void throwZerror(int status, const char* zmsg);
  // Pipe contents of todoBuf through zlib into zipBuf
  //  void zip(byte* start, unsigned len, int flush = Z_NO_FLUSH);
  inline void zip(byte* start, unsigned len, bool finish = false);

  //z_stream z;
  byte* todoBuf; // Allocated during open(), deallocated during close()
  unsigned todoBufSize; // Size of todoBuf
  unsigned todoCount; // Offset of 1st unused byte in todoBuf

  bostream* stream;

  unsigned chunkLimVal;

  MD5Sum* md5sum;
};
//______________________________________________________________________

/** Analogous to Zobstream, aware of jigdo file formats - expects a
    number of DATA parts at current stream position. */
class Zibstream {
public:

  /** Interface for gzip and bzip2 implementors. */
  class Impl {
  public:
    virtual unsigned totalOut() const = 0;
    virtual unsigned totalIn() const = 0;
    virtual unsigned availOut() const = 0;
    virtual unsigned availIn() const = 0;
    virtual byte* nextOut() const = 0;
    virtual byte* nextIn() const = 0;
    virtual void setTotalOut(unsigned n) = 0;
    virtual void setTotalIn(unsigned n) = 0;
    virtual void setAvailOut(unsigned n) = 0;
    virtual void setAvailIn(unsigned n) = 0;
    virtual void setNextOut(byte* n) = 0;
    virtual void setNextIn(byte* n) = 0;

    /** Initialize, i.e. inflateInit(). {next,avail}{in,out} must be set up
        before calling this. Sets an internal status flag. */
    virtual void init() = 0;
    /** Finalize, i.e. inflateEnd() */
    virtual void end() = 0;
    /** Re-init, i.e. inflateReset() */
    virtual void reset() = 0;

    /** Sets an internal status flag. */
    virtual void inflate() = 0;
    /** Check status flag: At stream end? */
    virtual bool streamEnd() const = 0;
    /** Check status flag: OK? */
    virtual bool ok() const = 0;

    /** Depending on internal status flag, throw appropriate Zerror. Never
        returns. */
    virtual void throwError() const = 0; /* throws Zerror */
  };
  //________________________________________

  inline explicit Zibstream(unsigned bufSz = 64*1024);
  /** Calls close(), which might throw a Zerror exception! Call
      close() before destroying the object to avoid this. */
  virtual ~Zibstream() { close(); delete buf; if (z != 0) z->end(); delete z; }
  inline Zibstream(bistream& s, unsigned bufSz = 64*1024);
  bool is_open() const { return stream != 0; }
  void close();

  /// Get reference to underlying istream
  bistream& getStream() { return *stream; }

  /// Input 1 character
//   inline Zibstream& get(unsigned char& x);
//   inline Zibstream& get(signed char& x);
  /// Input 32 bit integer in little-endian order
  //Zibstream& get(uint32 x);
  /// Input n characters
//   inline Zibstream& read(const char* x, size_t n);
//   inline Zibstream& read(const signed char* x, size_t n);
//   Zibstream& read(const unsigned char* x, size_t n);
//   inline Zibstream& read(const void* x, size_t n);
  Zibstream& read(byte* x, unsigned n);
  typedef uint64 streamsize;
  /// Number of characters read by last read()
  inline streamsize gcount() const { return gcountVal; gcountVal = 0; }

  bool good() const { return is_open() && buf != 0; }
  bool eof() const { return dataUnc == 0; }
  bool fail() const { return !good(); }
  bool bad() const { return false; }
  operator void*() const { return fail() ? (void*)0 : (void*)(-1); }
  bool operator!() const { return fail(); }

protected:
  void open(bistream& s);

private:
  // Throw a Zerror exception, or bad_alloc() for status==Z_MEM_ERROR
  //inline void throwZerror(int status, const char* zmsg);

//   z_stream z;
  Impl* z;
  bistream* stream;
  mutable streamsize gcountVal;
  unsigned bufSize;
  byte* buf; // Contains compressed data
  uint64 dataLen; // bytes remaining to be read from current DATA part
  uint64 dataUnc; // bytes remaining in uncompressed DATA part
};
//______________________________________________________________________

/* Initialising todoBufSize and todoCount in this way allows us to
   move Assert(is_open) out of the inline functions and into zip() for
   calls to put() and write() */
Zobstream::Zobstream(MD5Sum* md)
    : zipBuf(0), zipBufLast(0), todoBuf(0), todoBufSize(0), todoCount(0),
      stream(0), md5sum(md) {
//   z.zalloc = (alloc_func)0;
//   z.zfree = (free_func)0;
//   z.opaque = 0;
}

// Zobstream::Zobstream(bostream& s, size_t chunkLimit, int level,
//                      int windowBits, int memLevel, size_t todoBufSz,
//                      MD5Sum* md)
//     : todoBuf(0), todoBufSize(0), todoCount(0), stream(0),
//       zipBuf(0), zipBufLast(0), md5sum(md) {
//   z.zalloc = (alloc_func)0;
//   z.zfree = (free_func)0;
//   z.opaque = 0;
//   open(s, chunkLimit, level, windowBits, memLevel, todoBufSz);
// }
//________________________________________

void Zobstream::open(bostream& s, unsigned chunkLimit, unsigned todoBufSz) {
  Assert(!is_open());
  todoBufSize = (MIN_TODOBUF_SIZE > todoBufSz ? MIN_TODOBUF_SIZE :todoBufSz);
  chunkLimVal = chunkLimit;

  todoCount = 0;
  todoBuf = new byte[todoBufSize];

  stream = &s; // Declare as open
  Paranoid(stream != 0);
}

void Zobstream::zip(byte* start, unsigned len, bool finish) {
  zip2(start, len, finish);
  todoCount = 0;
}
//________________________________________

Zobstream& Zobstream::put(unsigned char x) {
  if (todoCount >= todoBufSize) zip(todoBuf, todoCount);
  todoBuf[todoCount] = static_cast<byte>(x);
  ++todoCount;
  return *this;
}

Zobstream& Zobstream::put(signed char x) {
  if (todoCount >= todoBufSize) zip(todoBuf, todoCount);
  todoBuf[todoCount] = static_cast<byte>(x);
  ++todoCount;
  return *this;
}

Zobstream& Zobstream::put(char x) {
  if (todoCount >= todoBufSize) zip(todoBuf, todoCount);
  todoBuf[todoCount] = static_cast<byte>(x);
  ++todoCount;
  return *this;
}

Zobstream& Zobstream::put(int x) {
  if (todoCount >= todoBufSize) zip(todoBuf, todoCount);
  todoBuf[todoCount] = static_cast<byte>(x);
  ++todoCount;
  return *this;
}

Zobstream& Zobstream::write(const byte* x, unsigned n) {
  Assert(is_open());
  if (n > 0) {
    zip(todoBuf, todoCount); // Zip remaining data in todoBuf
    zip(const_cast<byte*>(x), n); // Zip byte array
  }
  return *this;
}
//________________________________________

Zibstream::Zibstream(unsigned bufSz)
    : z(0), stream(0), bufSize(bufSz), buf(0) {
}

Zibstream::Zibstream(bistream& s, unsigned bufSz)
    : z(0), stream(0), bufSize(bufSz), buf(0) {
  // data* will be init'ed by open()
  open(s);
}

#endif
