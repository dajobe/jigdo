/* $Id$ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2001-2002  |  richard@
  | \/¯|  Richard Atterer          |  atterer.net
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

*/
/** @file

  Convert objects into byte streams and vice versa

  Classes that support serialization should implement the indicated
  member functions; serialize(), unserialize() and serialSizeOf(). It
  is assumed that 'byte' has been typdef'd to an unsigned type which
  represents one byte.

  <pre>
  MyClass {
    template<class Iterator>
    inline Iterator serialize(Iterator i) const;
    template<class ConstIterator>
    inline ConstIterator unserialize(ConstIterator i);
    inline size_t serialSizeOf() const;
  };</pre>

*/

#ifndef SERIALIZE_HH
#define SERIALIZE_HH

#include <bstream.hh>
#include <config.h> /* byte, size_t */
//______________________________________________________________________

/** Store serialized object via iterator. The iterator could be byte*
    or vector<byte>::iterator or SerialOstreamIterator - anything in
    which you can store bytes. There must be enough room to store
    serialSizeOf() bytes, implementers of serialize() need not check
    this.

    The implementation of serialize() must both modify i and return
    its value after the last modification. */
template<class Object, class Iterator>
inline Iterator serialize(const Object& o, Iterator i) {
  return o.serialize(i);
}

/** Assign the contents of the byte stream pointed to by i to the object.
    template<class Object, class ConstIterator> */
template<class Object, class ConstIterator>
inline ConstIterator unserialize(Object& o, ConstIterator i) {
  return o.unserialize(i);
}

/** Return number of bytes needed by this object when serialized. If a
    whole tree of objects is serialized, this may include the
    accumulated serialized sizes of the child objects, too. */
template<class Object>
inline size_t serialSizeOf(const Object& o) {
  return o.serialSizeOf();
}
//______________________________________________________________________

/** Slight variation (and simplification) of std::istream_iterator and
    std::ostream_iterator. You *MUST* use this when reading binary
    data for (un)serializing.

    The reason why the ordinary iterator does not work is that it uses
    "mystream >> byteToRead" which skips over whitespace (!) - this
    one uses "mystream.get(byteToRead)".

    Additionally, because it makes use of peek() this implementation
    allows mixing of Serial*streamIterator with calls to read() on the
    respective stream, without any problems. (This does not work with
    GCC's version, which uses get().) */
class SerialIstreamIterator {
public:
  typedef bistream istream_type;
  typedef byte value_type;
  typedef const byte* pointer;
  typedef const byte& reference;

  SerialIstreamIterator() : stream(0), val(0) { }
  SerialIstreamIterator(istream_type& s) : stream(&s), val(0) { }
  SerialIstreamIterator& operator++() { stream->get(); return *this; }
  SerialIstreamIterator operator++(int) {
    SerialIstreamIterator i = *this; stream->get(); return i; }
  reference operator*() const {
    val = static_cast<byte>(stream->peek()); return val; }
  pointer operator->() const {
    val = static_cast<byte>(stream->peek()); return &val; }

private:
  istream_type* stream;
  mutable byte val;
};
//____________________

/** @see SerialIstreamIterator */
class SerialOstreamIterator {
public:
  typedef bostream ostream_type;
  typedef void value_type;
  typedef void difference_type;
  typedef void pointer;
  typedef void reference;

  SerialOstreamIterator(ostream_type& s) : stream(&s) { }
  SerialOstreamIterator& operator=(const byte val) {
    stream->put(val);
    return *this;
  }
  SerialOstreamIterator& operator*() { return *this; }
  SerialOstreamIterator& operator++() { return *this; }
  SerialOstreamIterator& operator++(int) { return *this; }
private:
  ostream_type* stream;
};
//______________________________________________________________________

// Serializations of common data types. Stores in little-endian.
//________________________________________

/** @name
    Numeric types - append the number of bytes to use (e.g. 4 for 32-bit) */
//@{
template<class NumType, class Iterator>
inline Iterator serialize1(NumType x, Iterator i) {
  *i = x & 0xff; ++i;
  return i;
}
template<class NumType, class ConstIterator>
inline ConstIterator unserialize1(NumType& x, ConstIterator i) {
  x = static_cast<NumType>(*i); ++i;
  return i;
}

template<class NumType, class Iterator>
inline Iterator serialize2(NumType x, Iterator i) {
  *i = x & 0xff; ++i;
  *i = (x >> 8) & 0xff; ++i;
  return i;
}
template<class NumType, class ConstIterator>
inline ConstIterator unserialize2(NumType& x, ConstIterator i) {
  x = static_cast<NumType>(*i); ++i;
  x |= static_cast<NumType>(*i) << 8; ++i;
  return i;
}

template<class NumType, class Iterator>
inline Iterator serialize4(NumType x, Iterator i) {
  *i = x & 0xff; ++i;
  *i = (x >> 8) & 0xff; ++i;
  *i = (x >> 16) & 0xff; ++i;
  *i = (x >> 24) & 0xff; ++i;
  return i;
}
template<class NumType, class ConstIterator>
inline ConstIterator unserialize4(NumType& x, ConstIterator i) {
  x = static_cast<NumType>(*i); ++i;
  x |= static_cast<NumType>(*i) << 8; ++i;
  x |= static_cast<NumType>(*i) << 16; ++i;
  x |= static_cast<NumType>(*i) << 24; ++i;
  return i;
}

template<class NumType, class Iterator>
inline Iterator serialize6(NumType x, Iterator i) {
  *i = x & 0xff; ++i;
  *i = (x >> 8) & 0xff; ++i;
  *i = (x >> 16) & 0xff; ++i;
  *i = (x >> 24) & 0xff; ++i;
  *i = (x >> 32) & 0xff; ++i;
  *i = (x >> 40) & 0xff; ++i;
  return i;
}
template<class NumType, class ConstIterator>
inline ConstIterator unserialize6(NumType& x, ConstIterator i) {
  x = static_cast<NumType>(*i); ++i;
  x |= static_cast<NumType>(*i) << 8; ++i;
  x |= static_cast<NumType>(*i) << 16; ++i;
  x |= static_cast<NumType>(*i) << 24; ++i;
  x |= static_cast<NumType>(*i) << 32; ++i;
  x |= static_cast<NumType>(*i) << 40; ++i;
  return i;
}

template<class NumType, class Iterator>
inline Iterator serialize8(NumType x, Iterator i) {
  *i = x & 0xff; ++i;
  *i = (x >> 8) & 0xff; ++i;
  *i = (x >> 16) & 0xff; ++i;
  *i = (x >> 24) & 0xff; ++i;
  *i = (x >> 32) & 0xff; ++i;
  *i = (x >> 40) & 0xff; ++i;
  *i = (x >> 48) & 0xff; ++i;
  *i = (x >> 56) & 0xff; ++i;
  return i;
}
template<class NumType, class ConstIterator>
inline ConstIterator unserialize8(NumType& x, ConstIterator i) {
  x = static_cast<NumType>(*i); ++i;
  x |= static_cast<NumType>(*i) << 8; ++i;
  x |= static_cast<NumType>(*i) << 16; ++i;
  x |= static_cast<NumType>(*i) << 24; ++i;
  x |= static_cast<NumType>(*i) << 32; ++i;
  x |= static_cast<NumType>(*i) << 40; ++i;
  x |= static_cast<NumType>(*i) << 48; ++i;
  x |= static_cast<NumType>(*i) << 56; ++i;
  return i;
}
//@}
//______________________________________________________________________

#endif
