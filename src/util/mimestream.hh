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

#include <string.h>
#include <iostream>
#include <string>
#include <vector>

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

  /** Output operators */
  Base64Out<Output>& operator<<(char x) { return put(x); }
  Base64Out<Output>& operator<<(signed char x) { return put(x); }
  Base64Out<Output>& operator<<(unsigned char x) { return put(x); }
  /** Output the low 8 bits of an integer */
  Base64Out<Output>& operator<<(int x) { return put(x); }
  /** Output 32 bit integer in little-endian order */
  Base64Out<Output>& operator<<(uint32 x) { return put(x); }

  /** Output null-terminated string */
  inline Base64Out<Output>& operator<<(const char* x);
  inline Base64Out<Output>& operator<<(const signed char* x);
  Base64Out<Output>& operator<<(const unsigned char* x);
  inline Base64Out<Output>& operator<<(const void* x);

  /** Output 1 character */
  inline Base64Out<Output>& put(unsigned char x);
  inline Base64Out<Output>& put(signed char x);
  /** Output the low 8 bits of an integer */
  inline Base64Out<Output>& put(int x);
  inline Base64Out<Output>& put(char x);
  /** Output 32 bit integer in little-endian order */
  Base64Out<Output>& put(uint32 x);
  /** Output n characters */
  inline Base64Out<Output>& write(const char* x, unsigned n);
  inline Base64Out<Output>& write(const signed char* x, unsigned n);
  Base64Out<Output>& write(const unsigned char* x, unsigned n);
  inline Base64Out<Output>& write(const void* x, unsigned n);

  /** This is *not* a no-op. */
  Base64Out<Output>& flush();
  /** Output the appropriate number of '=' characters (0, 1 or 2)
      given how many bytes were fed into the Base64Out<Output> object. */
  Base64Out<Output>& trailer(streamsize n);

  /** A bit of a hack for jigdo: If true, switch from Base64 output to
      hexadecimal output. Default is false. */
  static bool hex;

private:
  /* String for MIME base64 encoding. Not entirely standard because b64
     strings are used as filenames by jigdo. Additionally, "+" or "/" looks
     weird in the .jigdo file. */
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
  bits += 2; // plus 8 new bits, less 6 which we output in next line
  out.put(code[(data >> bits) & 63U]);
  if (bits >= 6) { //8?
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
Base64Out<Output>& Base64Out<Output>::write(const char* x, unsigned n) {
  return write(reinterpret_cast<const unsigned char*>(x), n);
}
template <class Output>
Base64Out<Output>& Base64Out<Output>::write(const signed char* x, unsigned n) {
  return write(reinterpret_cast<const unsigned char*>(x), n);
}
template <class Output>
Base64Out<Output>& Base64Out<Output>::write(const void* x, unsigned n) {
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
                                            unsigned n) {
  for (unsigned i = 0; i < n; ++i)
    (*this) << static_cast<byte>(*x++);
  return *this;
}
//______________________________________________________________________

/** Convert a series of Base64 ASCII strings into binary data.

    Output is a class offering the following:
       void put(byte b); // Output one byte of binary data
       typedef implementation_defined ResultType;
       ResultType result(); // Is called by Base64In::result() */
template <class Output>
class Base64In {
public:
  Base64In() : bits(0) { }
  typename Output::ResultType result() { return out.result(); }

  /** Output operators, for handing in the ASCII Base64 string. */
  Base64In<Output>& operator<<(char x) { return put(x); }
  /** Convert null-terminated string */
  inline Base64In<Output>& operator<<(const char* x);
  /** Convert string */
  inline Base64In<Output>& operator<<(const string& x);
  /** Output 1 character */
  inline Base64In<Output>& put(char x);
  /** Convert given number of characters */
  Base64In<Output>& put(const char* x, unsigned n);

  /** Return the object to its initial state */
  void reset() { bits = 0; }

private:
  static const byte table[];
  int bits;
  uint32 data;
  Output out;
};
//______________________________________________________________________

class Base64StringIn {
public:
  void put(byte b) { val.push_back(b); }
  typedef vector<byte>& ResultType;
  vector<byte>& result() { return val; }
private:
  vector<byte> val;
};

typedef Base64In<Base64StringIn> Base64StringI;
//______________________________________________________________________

// Inverse mapping for both of these:
//"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
//"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
#define x 255
template <class Output>
const byte Base64In<Output>::table[] = {
  //    !   "   #   $   %   &   '   (   )   *   +   ,   -   .   /
    x,  x,  x,  x,  x,  x,  x,  x,  x,  x,  x, 62,  x, 62,  x, 63,
  //0   1   2   3   4   5   6   7   8   9   :   ;   <   =   >   ?
   52, 53, 54, 55, 56, 57, 58, 59, 60, 61,  x,  x,  x,  x,  x,  x,
  //@   A   B   C   D   E   F   G   H   I   J   K   L   M   N   O
    x,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
  //P   Q   R   S   T   U   V   W   x   Y   Z   [   \   ]   ^   _
   15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,  x,  x,  x,  x, 63,
  //`   a   b   c   d   e   f   g   h   i   j   k   l   m   n   o
    x, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
  //p   q   r   s   t   u   v   w   x   y   z   {   |   }   ~   DEL
   41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51,  x,  x,  x,  x,  x
};
#undef x

template <class Output>
Base64In<Output>& Base64In<Output>::operator<<(const char* x) {
  unsigned len = strlen(x);
  return put(x, len);
}

template <class Output>
Base64In<Output>& Base64In<Output>::operator<<(const string& x) {
  return put(x.data(), x.length());
}

template <class Output>
Base64In<Output>& Base64In<Output>::put(char x) {
  return put(&x, 1);
}

template <class Output>
Base64In<Output>& Base64In<Output>::put(const char* x, unsigned n) {
  --x;
  while (n > 0) {
    --n; ++x;
    unsigned code = static_cast<byte>(*x);
    if (code < 32 || code > 127) continue; // Just ignore invalid characters
    code = table[code - 32];
    if (code > 63) continue;
    data = (data << 6) | code;
    bits += 6;
    if (bits >= 8) {
      bits -= 8;
      out.put(static_cast<byte>((data >> bits) & 255U));
    }
  }
  return *this;
}

#endif
