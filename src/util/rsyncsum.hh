/* $Id$ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2000-2002  |  richard@
  | \/Ø|  Richard Atterer          |  atterer.net
  Ø '` Ø
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

  A 32 or 64 bit rolling checksum

*/

#ifndef RSYNCSUM_HH
#define RSYNCSUM_HH

#ifndef INLINE
#  ifdef NOINLINE
#    define INLINE
#    else
#    define INLINE inline
#  endif
#endif

#include <config.h>

#include <iosfwd>

#include <serialize.hh>
//______________________________________________________________________

/** A 32 bit checksum with the special property that you do not need
    to recalculate the checksum when data is added to the front/end of
    the checksummed area, or removed from it.

    Currently only adding to the end and removing from the front is
    supported. Adding to the end is very slightly faster.

    Many thanks to Andrew Tridgell and Paul Mackerras for the
    algorithm - NB none of rsync's code is used.

    Unless described otherwise, if a method returns an RsyncSum&, then
    this is a reference to the object itself, to allow chaining of
    calls, e.g. obj.addBack(x).addBack(y) */
class RsyncSum {
public:
  /// Initialises the checksum with zero
  RsyncSum() : sum(0) { };
  /// Initialises with the checksum of a memory area
  RsyncSum(const byte* mem, size_t len) : sum(0) { addBack(mem, len); };
  /// Compare two RsyncSum objects
  bool operator==(const RsyncSum& x) const { return get() == x.get(); }
  bool operator!=(const RsyncSum& x) const { return get() != x.get(); }
  bool operator< (const RsyncSum& x) const { return get() < x.get(); }
  bool operator> (const RsyncSum& x) const { return get() > x.get(); }
  bool operator<=(const RsyncSum& x) const { return get() <= x.get(); }
  bool operator>=(const RsyncSum& x) const { return get() >= x.get(); }
  /** Append memory area to end of area covered by the checksum */
  RsyncSum& addBack(const byte* mem, size_t len);
  /// Append one byte at end of area covered by the checksum
  inline RsyncSum& addBack(byte x);
  /** Append the same byte n times at end of area covered by the
      checksum. (addBack() is not overloaded for this in order to
      avoid confusion with removeFront(byte, size_t), which has the
      same signature, but only removes one byte.) */
  inline RsyncSum& addBackNtimes(byte x, size_t n);
  /** Remove memory area from start of area covered by the checksum
      @param len Number of bytes to remove from area, so it will cover
      area-len bytes after call
      @param areaSize Size covered by area before call (necessary for
      calculcations) */
  RsyncSum& removeFront(const byte* mem, size_t len, size_t areaSize);
  /** Remove one byte from start of area covered by checksum */
  inline RsyncSum& removeFront(byte x, size_t areaSize);
  /// Read stored checksum
  uint32 get() const { return sum; }
  /// Reset to initial state
  RsyncSum& reset() { sum = 0; return *this; }
  /// Check whether sum is zero
  bool empty() const { return sum == 0; }
  // Using default dtor & copy ctor
private:
  // Nothing magic about this constant; just a random number which,
  // according to the rsync sources, improves the quality of the
  // checksum.
  static const uint32 CHAR_OFFSET = 0xb593;
  uint32 sum;
};
//________________________________________

/** Like RsyncSum, but the checksum is a 64 bit value. However, the
    checksum quality would only improve very little if exactly the
    same algorithm were used, so this uses an additional 256-word
    lookup table for more "randomness". */
class RsyncSum64 {
public:
  RsyncSum64() : sumLo(0), sumHi(0) { };
  inline RsyncSum64(const byte* mem, size_t len);
  inline bool operator==(const RsyncSum64& x) const;
  inline bool operator!=(const RsyncSum64& x) const;
  inline bool operator< (const RsyncSum64& x) const;
  inline bool operator> (const RsyncSum64& x) const;
  inline bool operator<=(const RsyncSum64& x) const;
  inline bool operator>=(const RsyncSum64& x) const;
  INLINE RsyncSum64& addBack(const byte* mem, size_t len);
  INLINE RsyncSum64& addBack(byte x);
  INLINE RsyncSum64& addBackNtimes(byte x, size_t n);
  RsyncSum64& removeFront(const byte* mem, size_t len, size_t areaSize);
  inline RsyncSum64& removeFront(byte x, size_t areaSize);
   /// Return lower 32 bits of checksum
  uint32 getLo() const { return sumLo; }
   /// Return higher 32 bits of checksum
  uint32 getHi() const { return sumHi; }
  RsyncSum64& reset() { sumLo = sumHi = 0; return *this; }
  bool empty() const { return sumLo == 0 && sumHi == 0; }

  template<class Iterator>
  inline Iterator serialize(Iterator i) const;
  template<class ConstIterator>
  inline ConstIterator unserialize(ConstIterator i);
  inline size_t serialSizeOf() const;

private:
  RsyncSum64& addBack2(const byte* mem, size_t len);
  static const uint32 charTable[256];
  uint32 sumLo, sumHi;
};

INLINE ostream& operator<<(ostream& s, const RsyncSum& r);
INLINE ostream& operator<<(ostream& s, const RsyncSum64& r);
//______________________________________________________________________

RsyncSum& RsyncSum::addBack(byte x) {
  uint32 a = sum;
  uint32 b = sum >> 16;
  a += x + CHAR_OFFSET;
  b += a;
  sum = ((a & 0xffff) + (b << 16)) & 0xffffffff;
  return *this;
}

RsyncSum& RsyncSum::addBackNtimes(byte x, size_t n) {
  uint32 a = sum;
  uint32 b = sum >> 16;
  b += n * a + (n * (n + 1) / 2) * (x + CHAR_OFFSET); // Gauﬂ
  a += n * (x + CHAR_OFFSET);
  sum = ((a & 0xffff) + (b << 16)) & 0xffffffff;
  return *this;
}

RsyncSum& RsyncSum::removeFront(byte x, size_t areaSize) {
  uint32 a = sum;
  uint32 b = sum >> 16;
  b -= areaSize * (x + CHAR_OFFSET);
  a -= x + CHAR_OFFSET;
  sum = ((a & 0xffff) + (b << 16)) & 0xffffffff;
  return *this;
}
//________________________________________

RsyncSum64::RsyncSum64(const byte* mem, size_t len) : sumLo(0), sumHi(0) {
  addBack(mem, len);
};

bool RsyncSum64::operator==(const RsyncSum64& x) const {
  return sumHi == x.sumHi && sumLo == x.sumLo;
}
bool RsyncSum64::operator!=(const RsyncSum64& x) const {
  return sumHi != x.sumHi || sumLo != x.sumLo;
}
bool RsyncSum64::operator< (const RsyncSum64& x) const {
  return (sumHi < x.sumHi) || (sumHi == x.sumHi && sumLo < x.sumLo);
}
bool RsyncSum64::operator> (const RsyncSum64& x) const {
  return (sumHi > x.sumHi) || (sumHi == x.sumHi && sumLo > x.sumLo);
}
bool RsyncSum64::operator<=(const RsyncSum64& x) const {
  return !(*this > x);
}
bool RsyncSum64::operator>=(const RsyncSum64& x) const {
  return !(*this < x);
}


RsyncSum64& RsyncSum64::removeFront(byte x, size_t areaSize) {
  sumHi = (sumHi - areaSize * charTable[x]) & 0xffffffff;
  sumLo = (sumLo - charTable[x]) & 0xffffffff;
  return *this;
}

template<class Iterator>
inline Iterator RsyncSum64::serialize(Iterator i) const {
  i = serialize4(sumLo, i);
  i = serialize4(sumHi, i);
  return i;
}
template<class ConstIterator>
inline ConstIterator RsyncSum64::unserialize(ConstIterator i) {
  i = unserialize4(sumLo, i);
  i = unserialize4(sumHi, i);
  return i;
}
inline size_t RsyncSum64::serialSizeOf() const { return 8; }

#ifndef NOINLINE
#  include <rsyncsum.ih> /* NOINLINE */
#endif

#endif
