/* $Id$ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2000-2002  |  richard@
  | \/¯|  Richard Atterer          |  atterer.net
  ¯ '` ¯
  "Ported" to C++ by RA. Actual MD5 code taken from glibc

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

  Quite secure 128-bit checksum

*/

#ifndef MD5SUM_HH
#define MD5SUM_HH

#ifndef INLINE
#  ifdef NOINLINE
#    define INLINE
#    else
#    define INLINE inline
#  endif
#endif

#include <config.h>

#include <cstdlib>
#include <iosfwd>
#include <string>

#include <bstream.hh>
#include <debug.hh>
#include <md5sum.fh>
//______________________________________________________________________

/** Container for an already computed MD5Sum.

    Objects of this class are smaller than MD5Sum objects by one
    pointer. As soon as the checksum calculation of an MD5Sum object
    has finish()ed, the pointer is no longer needed. If you need to
    store a large number of calculated MD5Sums, it may be beneficial
    to assign the MD5Sum to an MD5 to save space. */
class MD5 {
public:
  MD5() { }
  inline MD5(const MD5Sum& md);
  /** 16 bytes of MD5 checksum */
  byte sum[16];
  /** Allows you to treat the object exactly like a pointer to a byte
      array */
  operator byte*() { return sum; }
  operator const byte*() const { return sum; }
  /** Assign an MD5Sum */
  inline MD5& operator=(const MD5Sum& md);
  inline bool operator<(const MD5& x) const;
  /** Clear contents to zero */
  inline MD5& clear();
  /** Convert to string */
  INLINE string toString() const;

  template<class Iterator>
  inline Iterator serialize(Iterator i) const;
  template<class ConstIterator>
  inline ConstIterator unserialize(ConstIterator i);
  inline size_t serialSizeOf() const { return 16; }

  // Default copy ctor
private:
  bool operator_less2(const MD5& x) const;
  static const byte zero[16];
};

inline bool operator==(const MD5& a, const MD5& b);
inline bool operator!=(const MD5& a, const MD5& b) { return !(a == b); }

/// Output MD5 as Base64 digest
INLINE ostream& operator<<(ostream& s, const MD5& r);
//______________________________________________________________________

/** A 128-bit, cryptographically strong message digest algorithm.

    Unless described otherwise, if a method returns an MD5Sum&, then
    this is a reference to the object itself, to allow chaining of
    calls. */
class MD5Sum {
  friend class MD5;
public:
  class ProgressReporter;

  /** Initialise the checksum */
  inline MD5Sum();
  /** Initialise with another checksum instance */
  MD5Sum(const MD5Sum& md);
  ~MD5Sum() { delete p; }
  /** Assign another checksum instance */
  MD5Sum& operator=(const MD5Sum& md);
  /** Tests for equality. Note: Will only return true if both message
      digest operations have been finished and their MD5 sums are the
      same. */
  inline bool operator==(const MD5Sum& md) const;
  inline bool operator!=(const MD5Sum& md) const;
  inline bool operator==(const MD5& md) const { return sum == md; }
  inline bool operator!=(const MD5& md) const { return sum != md; }
  /** Reset checksum object to the same state as immediately after its
      creation. You must call when reusing an MD5Sum object - call it
      just before the first update() for the new checksum. */
  inline MD5Sum& reset();
  /** Process bytes with the checksum algorithm. May lead to some
      bytes being temporarily buffered internally. */
  inline MD5Sum& update(const byte* mem, size_t len);
  /// Add a single byte. NB, not implemented efficiently ATM
  inline MD5Sum& update(byte x) { update(&x, 1); return *this; }
  /** Process remaining bytes in internal buffer and create the final
      checksum.
      @return Pointer to the 16-byte checksum. */
  inline MD5Sum& finish();
  /** Exactly the same behaviour as finish(), but is more efficient if
      you are going to call reset() again in the near future to re-use
      the MD5Sum object.
      @return Pointer to the 16-byte checksum. */
  inline MD5Sum& finishForReuse();
  /** Deallocate buffers like finish(), but don't generate the final
      sum */
  inline MD5Sum& abort();
  /** Return 16 byte buffer with checksum. Warning: Returns junk if
      checksum not yet finish()ed or flush()ed. */
  inline const byte* digest() const;

  /** Convert to string */
  INLINE string toString() const;

  /** Read data from file and update() this checksum with it.
      @param s The stream to read from
      @param size Total number of bytes to read
      @param r Reporter object
      @return Number of bytes read (==size if no error) */
  uint64 updateFromStream(bistream& s, uint64 size,
      size_t bufSize = 128*1024, ProgressReporter& pr = noReport);

  /* Serializing an MD5Sum is only allowed after finish(). The
     serialization is compatible with that of MD5. */
  template<class Iterator>
  inline Iterator serialize(Iterator i) const;
  template<class ConstIterator>
  inline ConstIterator unserialize(ConstIterator i);
  inline size_t serialSizeOf() const { return sum.serialSizeOf(); }

private:
  struct md5_ctx {
    uint32 A, B, C, D;
    uint32 total[2];
    uint32 buflen;
    char buffer[128] __attribute__ ((__aligned__ (__alignof__ (uint32))));
  };

  /// Default reporter: Only prints error messages to stderr
  static ProgressReporter noReport;

  // These functions are the original glibc API
  static void md5_init_ctx(md5_ctx* ctx);
  static void md5_process_bytes(const void* buffer, size_t len,
                                struct md5_ctx* ctx);
  static byte* md5_finish_ctx(struct md5_ctx* ctx, byte* resbuf);
  static byte* md5_read_ctx(const md5_ctx *ctx, byte* resbuf);
  static void md5_process_block(const void* buffer, size_t len,
                                md5_ctx* ctx);
  MD5 sum;
  struct md5_ctx* p; // null once MD creation is finished

# if DEBUG
  /* After finish(ForReuse)(), must call reset() before the next
     update(). OTOH, must only call digest() after finish(). Enforce
     this rule by keeping track of the state. */
  bool finished;
# endif
};

inline bool operator==(const MD5& a, const MD5Sum& b) { return b == a; }
inline bool operator!=(const MD5& a, const MD5Sum& b) { return b != a; }

/// Output MD5Sum as Base64 digest
INLINE ostream& operator<<(ostream& s, const MD5Sum& r);
//______________________________________________________________________

/** Class allowing JigdoCache to convey information back to the
    creator of a JigdoCache object. */
class MD5Sum::ProgressReporter {
public:
  virtual ~ProgressReporter() { }
  /// General-purpose error reporting.
  virtual void error(const string& message);
  /// Like error(), but for purely informational messages.
  virtual void info(const string& message);
  /// Called when data is read during updateFromStream()
  virtual void readingMD5(uint64 offInStream, uint64 size);
};
//______________________________________________________________________

bool MD5Sum::operator==(const MD5Sum& md) const {
# if DEBUG
  Paranoid(this->finished && md.finished);
# endif
  return sum == md.sum;
}
bool MD5Sum::operator!=(const MD5Sum& md) const {
# if DEBUG
  Paranoid(this->finished && md.finished);
# endif
  return sum != md.sum;
}

MD5Sum::MD5Sum() {
  p = new md5_ctx();
  md5_init_ctx(p);
# if DEBUG
  finished = false;
# endif
}

MD5Sum& MD5Sum::reset() {
  if (p == 0) p = new md5_ctx();
  md5_init_ctx(p);
# if DEBUG
  finished = false;
# endif
  return *this;
}

MD5Sum& MD5Sum::update(const byte* mem, size_t len) {
  Paranoid(p != 0);
# if DEBUG
  Paranoid(!finished); // Don't forget to call reset() before update()
# endif
  md5_process_bytes(mem, len, p);
  return *this;
}

MD5Sum& MD5Sum::finish() {
  Paranoid(p != 0 );
  md5_finish_ctx(p, sum);
  delete p;
  p = 0;
# if DEBUG
  finished = true;
# endif
  return *this;
}

MD5Sum& MD5Sum::finishForReuse() {
  Paranoid(p != 0  );
  md5_finish_ctx(p, sum);
# if DEBUG
  finished = true;
# endif
  return *this;
}

MD5Sum& MD5Sum::abort() {
  delete p;
  p = 0;
# if DEBUG
  finished = false;
# endif
  return *this;
}

const byte* MD5Sum::digest() const {
# if DEBUG
  Paranoid(finished); // Call finish() first
# endif
  return sum.sum;
}

template<class Iterator>
inline Iterator MD5Sum::serialize(Iterator i) const {
# if DEBUG
  Paranoid(finished ); // Call finish() first
# endif
  return sum.serialize(i);
}
template<class ConstIterator>
inline ConstIterator MD5Sum::unserialize(ConstIterator i) {
# if DEBUG
  finished = true;
# endif
  return sum.unserialize(i);
}
//____________________

MD5& MD5::operator=(const MD5Sum& md) {
# if DEBUG
  Paranoid(md.finished); // Call finish() first
# endif
  *this = md.sum;
  return *this;
}

bool MD5::operator<(const MD5& x) const {
  if (sum[0] < x.sum[0]) return true;
  if (sum[0] > x.sum[0]) return false;
  return operator_less2(x);
}

// inline bool operator<(const MD5& a, const MD5& b) {
//   return a.operator<(b);
// }

MD5::MD5(const MD5Sum& md) { *this = md.sum; }

bool operator==(const MD5& a, const MD5& b) {
  // How portable is this?
  return memcmp(a.sum, b.sum, 16 * sizeof(byte)) == 0;
# if 0
  return a.sum[0] == b.sum[0] && a.sum[1] == b.sum[1]
    && a.sum[2] == b.sum[2] && a.sum[3] == b.sum[3]
    && a.sum[4] == b.sum[4] && a.sum[5] == b.sum[5]
    && a.sum[6] == b.sum[6] && a.sum[7] == b.sum[7]
    && a.sum[8] == b.sum[8] && a.sum[9] == b.sum[9]
    && a.sum[10] == b.sum[10] && a.sum[11] == b.sum[11]
    && a.sum[12] == b.sum[12] && a.sum[13] == b.sum[13]
    && a.sum[14] == b.sum[14] && a.sum[15] == b.sum[15];
# endif
}

MD5& MD5::clear() {
  byte* x = sum;
  *x++ = 0; *x++ = 0; *x++ = 0; *x++ = 0;
  *x++ = 0; *x++ = 0; *x++ = 0; *x++ = 0;
  *x++ = 0; *x++ = 0; *x++ = 0; *x++ = 0;
  *x++ = 0; *x++ = 0; *x++ = 0; *x++ = 0;
  return *this;
}

template<class Iterator>
inline Iterator MD5::serialize(Iterator i) const {
  for (int j = 0; j < 16; ++j) { *i = sum[j]; ++i; }
  return i;
}
template<class ConstIterator>
inline ConstIterator MD5::unserialize(ConstIterator i) {
  for (int j = 0; j < 16; ++j) { sum[j] = *i; ++i; }
  return i;
}

#ifndef NOINLINE
#  include <md5sum.ih> /* NOINLINE */
#endif

#endif
