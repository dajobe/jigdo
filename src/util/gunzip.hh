/* $Id$ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2003  |  richard@
  | \/¯|  Richard Atterer     |  atterer.net
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

*//** @file

  In-memory, push-oriented decompression of .gz files

*/

#ifndef GUNZIP_HH
#define GUNZIP_HH

#include <config.h>

#include <string>
#include <zlib.h>

#include <debug.hh>

#ifdef TRANSPARENT
#  undef TRANSPARENT
#endif
#ifdef ERROR
#  undef ERROR
#endif
//______________________________________________________________________

/** Allows .gz files to be decompressed in memory on the fly as they are
    being downloaded. In contrast to a gzstream-style object where calls
    would be made to the object to obtain decompressed data, with Gunzip
    calls have to be made to "push" new gzipped data to the object once
    available, and the Gunzip object then makes further calls, passing
    decompressed data to an object of your choice.

    Contains code to auto-detect .gz files: If the file starts with
    \\x1f\\x8b\\x08, the data is passed to zlib to decompress. Otherwise, it
    is assumed that the file is not compressed and it is passed to the output
    IO object unmodified.

    Can deal with >1 concatenated .gz files; in this case, simply outputs the
    concatenated uncompressed data. */
class Gunzip {
public:
  //________________________________________

  /** The Gunzip object makes calls to the virtual functions of a class you
      derive from this.
      Abstract class, define the gunzip_* methods in your derived class. */
  class IO {
  public:

    virtual ~IO() { }

    /** Called by the Gunzip object when it is deleted or when a different IO
        object is registered with it. If the IO object considers itself owned
        by its Gunzip, it can delete itself. */
    virtual void gunzip_deleted() = 0;

    /** Called from within Gunzip::inject() after each decompression step.
        @param self Gunzip object this IO object is registered with
        @param decompressed Pointer to "size" new bytes of uncompressed data
        @param size Number of bytes at decompressed */
    virtual void gunzip_data(Gunzip* self, byte* decompressed,
                             unsigned size) = 0;

    /** Called from within Gunzip::inject() if self->availOut()==0 and
        another output buffer is needed. You must call self->setOut() to
        supply it.*/
    virtual void gunzip_needOut(Gunzip* self) = 0;

    /** Called when decompression has successfully finished. */
    //virtual void gunzip_succeeded() = 0;

    /** Called when decompression fails. You can copy the error message away
        with mystring.swap(*message). After the error, further calls to the
        object are not allowed; delete the object. */
    virtual void gunzip_failed(string* message) = 0;
  };
  //________________________________________

  /** Pointer to an IO object, can be changed during the Gunzip object's
      lifetime. */
  class IOPtr { // cf. job.hh
  public:
    IOPtr(IO* io) : ptr(io) { }
    ~IOPtr() { ptr->gunzip_deleted(); }

    IO& operator*()  const throw() { return *ptr; }
    IO* operator->() const throw() { return ptr; }
    operator bool()  const throw() { return ptr != 0; }
    IO* get()        const throw() { return ptr; }
    /** Calls the IO object's gunzip_deleted() method before overwriting the
        value, except if the old and new IO are identical */
    void set(IO* io) {
      if (ptr == io) return;
      if (ptr != 0) ptr->gunzip_deleted();
      ptr = io;
    }
  private:
    IO* ptr;
  };
  IOPtr io;
  //________________________________________

  Gunzip(IO* ioPtr);

  virtual ~Gunzip();

  /** Called by you whenever new compressed data is available. The contents
      of "compressed" need not be preserved after the call returns. */
  void inject(const byte* compressed, unsigned size);

  /** Return current output position, i.e. pointer to address where next
      uncompressed byte will be put. */
  inline byte* nextOut() const;

  /** Return nr of bytes left in output buffer. */
  inline unsigned availOut() const;

  /** Supply an output buffer to place decompressed data into. This *must* be
      called from gunzip_needOut(), but can also be called at other times,
      e.g. from gunzip_data(). */
  inline void setOut(byte* newNextOut, unsigned newAvailOut);

private:
  // Pass zlib error to IO object
  void error(const char* msg);
  // Ensure that at least one output byte is writable in z.next_out/avail_out
  inline void needOutByte();
  // Advance z.next_in/z.avail_in by one byte & return it
  inline byte nextInByte();
  // Output a single byte. Calls needOutByte() and io->data(this,...,1)
  inline void outputByte(byte b);

  /* zlib doesn't support in-memory decompression of .gz files, only of zlib
     streams, so we need to extract the zlib stream from the .gz file. Keep
     track of our progress in a state var. */
  enum {
    INIT0, // Guessing whether or not .gz format: First two bytes \x1f \x8b
    INIT1,
    HEADER_CM, // .gz header: compression method byte
    HEADER_FLG, // .gz header: flag byte
    HEADER_FEXTRA0, // .gz header: optional extra fields
    HEADER_FEXTRA1,
    HEADER_FNAME, // .gz header: Original file name
    HEADER_FCOMMENT, // .gz header: Comment field
    ZLIB, // Found zlib stream, pass to zlib
    TRAILER_CRC0, // Checksum of uncompressed data, after zlib data
    TRAILER_CRC1,
    TRAILER_CRC2,
    TRAILER_CRC3,
    TRANSPARENT, // Final state: Not .gz format, pass to IO unmodified
    ERROR // Final state: zlib error or error in .gz header
  };
  int state;
  byte headerFlags;
  z_stream z;
  unsigned skip; // Nr of bytes to ignore in input stream
  unsigned data; // to accumulate parameters from the stream
  uLong crc; // CRC32 checksum of uncompressed data
};
//______________________________________________________________________

byte* Gunzip::nextOut() const {
  return z.next_out;
}

unsigned Gunzip::availOut() const {
  return z.avail_out;
}

void Gunzip::setOut(byte* newNextOut, unsigned newAvailOut) {
  z.next_out = newNextOut;
  z.avail_out = newAvailOut;
}

void Gunzip::needOutByte() {
  if (availOut() == 0) {
    io->gunzip_needOut(this);
    Paranoid(availOut() > 0);
  }
}

void Gunzip::outputByte(byte b) {
  needOutByte();
  *z.next_out = b;
  --z.avail_out;
  ++z.next_out;
  io->gunzip_data(this, z.next_out - 1, 1);
}

byte Gunzip::nextInByte() {
  byte result = *z.next_in;
  ++z.next_in;
  --z.avail_in;
  return result;
}

#endif
