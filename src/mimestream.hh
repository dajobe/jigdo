/* $Id$ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2000-2002  |  richard@
  | \/¯|  Richard Atterer          |  atterer.net
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

  Convert binary data to/from ASCII using Base64 encoding

*/

#ifndef MIMESTREAM_HH
#define MIMESTREAM_HH

#ifndef INLINE
#  ifdef NOINLINE
#    define INLINE
#    else
#    define INLINE inline
#  endif
#endif

#include <config.h>

#include <iostream>
#include <string>

#include <debug.hh>
//______________________________________________________________________

/** Convert binary data to Base64 and output. Note that this does
    *not* implement the RFC2045 requirement that lines of text be no
    longer than 76 characters each. Furthermore, by default the data
    is not terminated with any '='.

    Output is a class offering the following:
       void put(char c); // Output one ASCII character
       typedef implementation_defined ResultType;
       ResultType result(); // Is called by Base64Out::result()
*/
template <class Output>
class Base64Out {
public:
  Base64Out() : bits(0) { }
  typename Output::ResultType result() { return out.result(); }

  /// Output operators
  Base64Out<Output>& operator<<(char x) { return put(x); }
  Base64Out<Output>& operator<<(signed char x) { return put(x); }
  Base64Out<Output>& operator<<(unsigned char x) { return put(x); }
  /// Output the low 8 bits of an integer
  Base64Out<Output>& operator<<(int x) { return put(x); }
  /// Output 32 bit integer in little-endian order
  Base64Out<Output>& operator<<(uint32 x) { return put(x); }

  /// Output null-terminated string
  inline Base64Out<Output>& operator<<(const char* x);
  inline Base64Out<Output>& operator<<(const signed char* x);
  Base64Out<Output>& operator<<(const unsigned char* x);
  inline Base64Out<Output>& operator<<(const void* x);

  /// Output 1 character
  inline Base64Out<Output>& put(unsigned char x);
  inline Base64Out<Output>& put(signed char x);
  /// Output the low 8 bits of an integer
  inline Base64Out<Output>& put(int x);
  inline Base64Out<Output>& put(char x);
  /// Output 32 bit integer in little-endian order
  Base64Out<Output>& put(uint32 x);
  /// Output n characters
  inline Base64Out<Output>& write(const char* x, size_t n);
  inline Base64Out<Output>& write(const signed char* x, size_t n);
  Base64Out<Output>& write(const unsigned char* x, size_t n);
  inline Base64Out<Output>& write(const void* x, size_t n);

  /// This is *not* a no-op.
  Base64Out<Output>& flush();
  /** Output the appropriate number of '=' characters (0, 1 or 2)
      given how many bytes were fed into the Base64Out<Output> object. */
  Base64Out<Output>& trailer(streamsize n);

  /** A bit of a hack for jigdo: If true, switch from Base64 output to
      hexadecimal output. Default is false. */
  static bool hex;

private:
  /* The first line (commented out) is the correct string for MIME
     base64 encoding. We use the second instead because b64 strings
     might be used as filenames for jigdo at one point. Additionally,
     "+" or "/" looks weird in the .jigdo file. */
  static const char* const code;
  static const char* const hexCode;
  int bits;
  uint32 data;
  Output out;
};
//______________________________________________________________________

class Base64StringOut {
public:
  void put(char c) { val += c; }
  typedef string& ResultType;
  string& result() { return val; }
  const string& result() const { return val; }
private:
  string val;
};

/** A string which you can output to with "str << 1234" or
    "str.write(buf, 4096)". */
typedef Base64Out<Base64StringOut> Base64String;
//______________________________________________________________________

// Support functions to allow things like "b64Stream << flush;"

template <class Output>
inline Base64Out<Output>& flush(Base64Out<Output>& s) {
  return s.flush();
}

template <class Output>
inline Base64Out<Output>& operator<<(Base64Out<Output>& s,
    Base64Out<Output>& (*m)(Base64Out<Output>&)) {
  return (*m)(s);
}
//______________________________________________________________________

template <class Output>
bool Base64Out<Output>::hex = false;
template <class Output>
const char* const Base64Out<Output>::code =
    //"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
template <class Output>
const char* const Base64Out<Output>::hexCode = "0123456789abcdef";

template <class Output>
Base64Out<Output>& Base64Out<Output>::operator<<(const char* x) {
  return (*this) << reinterpret_cast<const unsigned char*>(x);
}
template <class Output>
Base64Out<Output>& Base64Out<Output>::operator<<(const signed char* x) {
  return (*this) << reinterpret_cast<const unsigned char*>(x);
}
template <class Output>
Base64Out<Output>& Base64Out<Output>::operator<<(const void* x) {
  return (*this) << static_cast<const unsigned char*>(x);
}

template <class Output>
Base64Out<Output>& Base64Out<Output>::put(unsigned char x) {
  if (hex) {
    out.put(hexCode[(x >> 4) & 15U]);
    out.put(hexCode[x & 15U]);
    return *this;
  }
  data = (data << 8) | x;
  bits += 2;
  out.put(code[(data >> bits) & 63U]);
  if (bits >= 8) {
    bits -= 6;
    out.put(code[(data >> bits) & 63U]);
  }
  return *this;
}
template <class Output>
Base64Out<Output>& Base64Out<Output>::put(signed char x) {
  return put(static_cast<unsigned char>(x));
}
template <class Output>
Base64Out<Output>& Base64Out<Output>::put(char x) {
  return put(static_cast<unsigned char>(x));
}
template <class Output>
Base64Out<Output>& Base64Out<Output>::put(int x) {
  return put(static_cast<unsigned char>(x));
}

template <class Output>
Base64Out<Output>& Base64Out<Output>::write(const char* x, size_t n) {
  return write(reinterpret_cast<const unsigned char*>(x), n);
}
template <class Output>
Base64Out<Output>& Base64Out<Output>::write(const signed char* x, size_t n) {
  return write(reinterpret_cast<const unsigned char*>(x), n);
}
template <class Output>
Base64Out<Output>& Base64Out<Output>::write(const void* x, size_t n) {
  return write(static_cast<const unsigned char*>(x), n);
}

template <class Output>
Base64Out<Output>& Base64Out<Output>::put(uint32 x) {
  (*this)
    .put(static_cast<unsigned char>(x & 0xff))
    .put(static_cast<unsigned char>((x >> 8) & 0xff))
    .put(static_cast<unsigned char>((x >> 16) & 0xff))
    .put(static_cast<unsigned char>((x >> 24) & 0xff));
  return *this;
}

template <class Output>
Base64Out<Output>& Base64Out<Output>::flush() {
  if (bits > 0) {
    data <<= 6 - bits;
    out.put(code[data & 63U]);
  }
  bits = 0;
  return *this;
}

template <class Output>
Base64Out<Output>& Base64Out<Output>::trailer(streamsize n) {
  int rest = n % 3;
  if (rest == 1)
    (*stream) << '=';
  if (rest >= 1)
    (*stream) << '=';
  return *this;
}

// Output null-terminated string
template <class Output>
Base64Out<Output>& Base64Out<Output>::operator<<(const unsigned char* x) {
  while (*x != '\0')
    (*this) << static_cast<byte>(*x++);
  return *this;
}

// Output n characters
template <class Output>
Base64Out<Output>& Base64Out<Output>::write(const unsigned char* x,
                                            size_t n) {
  for (size_t i = 0; i < n; ++i)
    (*this) << static_cast<byte>(*x++);
  return *this;
}
//______________________________________________________________________

#if 0
/** Convert binary data to Base64 while outputting. Note that this
    does *not* implement the RFC2045 requirement that lines of text be
    no longer than 76 characters each. Furthermore, by default the
    data is not terminated with any '='. */
class Base64ostream { // Will not derive from ostream, just imitating it
public:
  Base64ostream() : stream(0) { }
  ~Base64ostream() { flush(); }
  explicit Base64ostream(ostream& s) : bits(0), stream(&s) { }
  bool is_open() const { return stream != 0; }
  void open(ostream& s) { Assert(!is_open()); bits = 0; stream = &s; }
  void close();

  /// Output operators
  Base64ostream& operator<<(char x) { return put(x); }
  Base64ostream& operator<<(signed char x) { return put(x); }
  Base64ostream& operator<<(unsigned char x) { return put(x); }
  /// Output the low 8 bits of an integer
  Base64ostream& operator<<(int x) { return put(x); }
  /// Output 32 bit integer in little-endian order
  Base64ostream& operator<<(uint32 x) { return put(x); }

  /// Output null-terminated string
  inline Base64ostream& operator<<(const char* x);
  inline Base64ostream& operator<<(const signed char* x);
  Base64ostream& operator<<(const unsigned char* x);
  inline Base64ostream& operator<<(const void* x);

  /// Get reference to underlying ostream
  ostream& getStream() { return *stream; }
  /** Implicit conversion to the underlying ostream always flushes the
      Base64ostream first */
  operator ostream&() { flush(); return getStream(); }

  /// Output 1 character
  inline Base64ostream& put(unsigned char x);
  inline Base64ostream& put(signed char x);
  /// Output the low 8 bits of an integer
  inline Base64ostream& put(int x);
  inline Base64ostream& put(char x);
  /// Output 32 bit integer in little-endian order
  Base64ostream& put(uint32 x);
  /// Output n characters
  inline Base64ostream& write(const char* x, size_t n);
  inline Base64ostream& write(const signed char* x, size_t n);
  Base64ostream& write(const unsigned char* x, size_t n);
  inline Base64ostream& write(const void* x, size_t n);

  /** Standard ostream operations are forwarded to actual stream. NB:
      This implementation is probably not 100% correct WRT the
      semantic definition of these functions. */
  bool good() const { return stream != 0 && stream->good(); }
  bool eof() const { return stream != 0 && stream->eof(); }
  bool fail() const { return stream != 0 && stream->fail(); }
  bool bad() const { return stream != 0 && stream->bad(); }
  operator void*() const {
    return reinterpret_cast<void*>(fail() ? -1 : 0); }
  bool operator!() const { return fail(); }
  /** flush() is not forwarded. Instead, it terminates the Base64
      string. NB: This is *not* a no-op. */
  Base64ostream& flush();
  /** Output the appropriate number of '=' characters (0, 1 or 2)
      given how many bytes were fed into the Base64ostream object. */
  Base64ostream& trailer(streamsize n);

private:
  static const byte code[];
  int bits;
  uint32 data;
  ostream* stream;
};
#endif /* 0 */

#endif
