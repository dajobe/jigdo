/* $Id$ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2003  |  richard@
  | \/¯|  Richard Atterer     |  atterer.net
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

  In-memory, push-oriented decompression of .gz files

  #test-deps util/gunzip.o

*/

#include <config.h>

#include <algorithm>
#include <errno.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string.h>

#include <bstream.hh>
#include <gunzip.hh>
#include <log.hh>
//______________________________________________________________________

int returnCode = 0;

namespace {
  const int FILEBUFSIZE = 4096;
  const int BUFSIZE = 4096;

  struct ToStdout : Gunzip::IO {
    byte buf[BUFSIZE];
    virtual ~ToStdout() { }
    virtual void gunzip_deleted() { }
    virtual void gunzip_data(Gunzip*, byte* decompressed,
                             unsigned size) {
      cout.write(reinterpret_cast<char*>(decompressed), size);
    }
    virtual void gunzip_needOut(Gunzip* self) {
      self->setOut(buf, BUFSIZE);
    }
    virtual void gunzip_succeeded() { }
    virtual void gunzip_failed(string* message) {
      cerr << *message << endl;
    }
  };

  const char* const hexDigits = "0123456789abcdef";
  void toHex(string* o, byte c) {
    switch (c) {
    case 0: *o += "\\0"; break;
    case '\n': *o += "\\n"; break;
    case '"': case '\\': *o += '\\'; *o += c; break;
    default:
      if (c >= ' ' && c <= '~') {
        *o += c;
      } else {
        *o += "\\x";
        *o += hexDigits[unsigned(c) >> 4];
        *o += hexDigits[unsigned(c) & 0xfU];
      }
    }
  }

  inline string escapedString(const string& s) {
    string result;
    for (unsigned i = 0; i < s.length(); ++i)
      toHex(&result, s[i]);
    return result;
  }

  inline string escapedString(const byte* s, unsigned ssize) {
    string result;
    for (unsigned i = 0; i < ssize; ++i)
      toHex(&result, s[i]);
    return result;
  }

  struct ToString : Gunzip::IO {
    unsigned bufsize;
    byte* buf;
    ToString(unsigned bufs) : bufsize(bufs) {
      buf = new byte[bufs + 1];
      buf[bufs] = 0x7fU;
    }
    virtual ~ToString() { delete[] buf; }
    virtual void gunzip_deleted() { }
    virtual void gunzip_data(Gunzip*, byte* decompressed,
                             unsigned size) {
      if (buf[bufsize] != 0x7fU) o += "[BUFFER OVERFLOW]";
      o.append(reinterpret_cast<const char*>(decompressed), size);
    }
    virtual void gunzip_needOut(Gunzip* self) {
      self->setOut(buf, bufsize);
    }
    virtual void gunzip_succeeded() { }
    virtual void gunzip_failed(string* message) {
      o += '[';
      o += *message;
      o += ']';
    }
    string o;
  };

  void decompressFile(const char* name) {
    bifstream f(name);
    byte buf[FILEBUFSIZE];
    ToStdout out;
    Gunzip decompressor(&out);

    while (f) {
      readBytes(f, buf, FILEBUFSIZE);
      unsigned n = f.gcount();
      decompressor.inject(buf, n);
    }
    if (!f.eof()) {
      cerr << strerror(errno) << endl;
    }
  }

  // cbs/ubs: Block size for I/O of (un)compressed data
  void testCase2(const byte* c, unsigned csize, const string& unp,
                 unsigned cbs, unsigned ubs) {
    ToString io(ubs);
    Gunzip decompressor(&io);
    unsigned cleft = csize;
    const byte* cptr = c;
    while (cleft > 0) {
      unsigned count = min(cleft, cbs);
      decompressor.inject(cptr, count);
      cptr += count;
      cleft -= count;
    }
    if (unp == io.o) {
      msg("OK: cbs=%1 ubs=%2 \"%3\"", cbs, ubs, escapedString(unp));
    } else {
      msg("FAILED: cbs=%1 ubs=%2", cbs, ubs);
      msg("  expected \"%1\"", escapedString(unp));
      msg("  but got  \"%1\"", escapedString(io.o));
      returnCode = 1;
    }
  }

  void testCase(const byte* c, unsigned csize, const byte* u, unsigned usize,
                int testNr) {
    string unp(reinterpret_cast<const char*>(u), usize);
    msg("Test case %1: \"%2\"", testNr, escapedString(c, csize));
    testCase2(c, csize, unp, 1, 1);
    testCase2(c, csize, unp, 3, 1);
    testCase2(c, csize, unp, 1, 3);
    testCase2(c, csize, unp, 10, 1);
    testCase2(c, csize, unp, 1, 10);
    testCase2(c, csize, unp, 10, 10);
    testCase2(c, csize, unp, 5, 1024);
    testCase2(c, csize, unp, 1024, 5);
    testCase2(c, csize, unp, 1024, 1024);
  }

} // namespace

int main(int argc, char* argv[]) {

  if (argc == 3 && strcmp("decompress", argv[1]) == 0) {
    decompressFile(argv[2]);
    exit(0);
  }

  /* Special case, nothing to do with testing gunzip: Convert stdin to
     C-style string. */
  if (argc == 2 && strcmp("tohex", argv[1]) == 0) {
    string o;
    int len = 0;
    while (cin) {
      byte c;
      cin.read(reinterpret_cast<char*>(&c), 1);
      if (cin.gcount() == 0) continue;
      toHex(&o, c);
      ++len;
    }
    cout << len << ", \"" << o << '"' << endl;
    return 0;
  } else if (argc == 2) {
    Logger::scanOptions(argv[1], argv[0]);
  }

  msg("Usage: `gunziptest decompress FILENAME.gz' to decompress to stdout\n"
      "       `gunziptest' to test some built-in files\n"
      "       `gunziptest tohex' to convert stdin to escaped string");

  // compressed size, compressed input, unc. size, uncompressed output
  struct Test { int csize; const char* c; int usize; const char* u; };
  const Test test[] = {
    {
      // Rhubarb with embedded filename "rhubarb"
      36, "\x1f\x8b\x08\x08\x8d\xb1\x89>\x02\x03rhubarb\0\x0b\xca(MJ,J\xe2\x02\0l]\x94\x11\x08\0\0\0",
      8, "Rhubarb\n"
    }, {
      // Quick brown fox without filename
      64, "\x1f\x8b\x08\0H\xf9\x8a>\x02\x03\x0b\xc9HU(,\xcdL\xceVH*\xca/\xcfSH\xcb\xafP\xc8*\xcd-(V\xc8/K-R(\x01J\xe7$VU*\xa4\xe4\xa7\xebq\x01\0j\xccP\xeb-\0\0\0",
      45, "The quick brown fox jumps over the lazy dog.\n"
    }, {
      // Non-gzip, 0 gzip header bytes => transparent
      11, "Gunzip test",
      11, "Gunzip test"
    }, {
      // Non-gzip, 1 gzip header byte => transparent
      11, "\x1funzip test",
      11, "\x1funzip test"
    }, {
      // Non-gzip, 2 gzip header bytes => transparent
      11, "\x1f\x8bnzip test",
      11, "\x1f\x8bnzip test"
    }, {
      // Non-gzip, 3 gzip header bytes => error, flag byte invalid
      11, "\x1f\x8b\x08zip test",
      21, "[Decompression error]"
    }, {
      // Long name, null content
      41, "\x1f\x8b\x08\x08\xde\xfd\x8a>\x02\x03""12345678901234567890\0\x03\0\0\0\0\0\0\0\0\0",
      0, ""
    }, {
      // Otherwise correct .gz file, but with a wrong checksum at the end
      47, "\x1f\x8b\x08\x08\xe6\x04\x8b>\x02\x03ssl\0\x0b.I\xcc\xccKQ\x08.\xcd""51T\xf0\xc9\xcc-p\xca\xac\xca\xce,\x01\0\xe7\"\x98'\x17\0\0\0",
      63, "Staind Sum41 LimpBizkit[Decompression error: Checksum is wrong]"
    }, {
      // Two .gz files concatenated
      48, "\x1f\x8b\x08\0s\x06\x8b>\x02\x03K\xcb\xcf\xe7\x02\0\xa8""e2~\x04\0\0\0\x1f\x8b\x08\0u\x06\x8b>\x02\x03KJ,\xe2\x02\0\xe9\xb3\xa2\x04\x04\0\0\0",
      8, "foo\nbar\n"
    }, {
      // EXTRA field set
      102, "\x1f\x8b\x08\x0c""F\x04\x8b>\x02\x03\x04\0\x11\"3Dyadayada\0\x01@\0\xbf\xff\x97\x93\x05\xf1\xed\xa5\xf2\x89\xc6_V\x05\x89""CG\xc2""9\xfe(\xee\x0e\x9f\xbf\x04\xb2$2\xf2\xf0\xd2\xe4\xd6\x8e<\xc8pD&r\x94\xe1l\x99\x80VT\n\xcc\xad\xfd\xc9Y\x95""9h\x17\x19""F\xdeQ\xcb]M?\x89n\xc0\xd4@\0\0\0",
64, "\x97\x93\x05\xf1\xed\xa5\xf2\x89\xc6_V\x05\x89""CG\xc2""9\xfe(\xee\x0e\x9f\xbf\x04\xb2$2\xf2\xf0\xd2\xe4\xd6\x8e<\xc8pD&r\x94\xe1l\x99\x80VT\n\xcc\xad\xfd\xc9Y\x95""9h\x17\x19""F\xdeQ\xcb]M?"
    }, {
      // EXTRA, name, comment all present
      62, "\x1f\x8b\x08\x1c\x96\x0c\x8b>\x02\x03\x05\0\x11\"3DUyadayada\0comment\0s\xc9L-N-\nN,\xa9\xf2N\xcd\xcc\x0bK-J\x02\0h\xb6]\xb8\x12\0\0\0",
      18, "DieserSatzKeinVerb"
    }
  };
  int nrOfTests = sizeof(test) / sizeof(Test);
  for (int i = 0; i < nrOfTests; ++i) {
    testCase(reinterpret_cast<const byte*>(test[i].c), test[i].csize,
             reinterpret_cast<const byte*>(test[i].u), test[i].usize, i);
  }

  if (returnCode == 0)
    msg("OK - all tests succeeded!");
  else
    msg("FAILED - at least one test had an incorrect result!");
  return returnCode;
}
