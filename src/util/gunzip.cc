/* $Id$ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2003  |  richard@
  | \/¯|  Richard Atterer     |  atterer.net
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

  In-memory, push-oriented decompression of .gz files

*/

#include <config.h>

#include <algorithm>
#include <iostream>
#include <string.h>

#include <debug.hh>
#include <gunzip.hh>

#ifndef DEBUG_GUNZIP
#  define DEBUG_GUNZIP (DEBUG && 0)
#endif
//______________________________________________________________________

void Gunzip::error(const char* msg) {
  string err = _("Decompression error");
  if (msg != 0) {
    err += ": ";
    err += msg;
  }
  state = ERROR;
  io->gunzip_failed(&err);
}
//______________________________________________________________________

Gunzip::Gunzip(IO* ioPtr) : io(ioPtr), state(INIT0), headerFlags(0),
                            skip(0) {
  z.next_in = 0;
  z.avail_in = 0;
  z.next_out = 0;
  z.avail_out = 0;
  z.zalloc = (alloc_func)0;
  z.zfree = (free_func)0;
  z.opaque = 0;
  // Undocumented zlib feature - the following comment is from gzio.c:
  /* windowBits is passed < 0 to tell that there is no zlib header. Note that
     in this case inflate *requires* an extra "dummy" byte after the
     compressed stream in order to complete decompression and return
     Z_STREAM_END. Here the gzip CRC32 ensures that 4 bytes are present after
     the compressed stream. */
  int ok = inflateInit2(&z, -MAX_WBITS);
  if (ok != Z_OK) error(z.msg);

  // Initialize checksum of uncompressed data
  crc = crc32(0L, Z_NULL, 0);
}
//______________________________________________________________________

Gunzip::~Gunzip() {
  inflateEnd(&z); // Ignore errors
}
//______________________________________________________________________

void Gunzip::inject(const byte* compressed, unsigned size) {
  if (state == ERROR) return;

  Assert(z.avail_in == 0);
  z.next_in = (byte*)compressed;
  z.avail_in = size;

  byte b;

  while (z.avail_in > 0) {

    if (skip > 0) { // Ignore "skip" bytes of input
      if (skip <= z.avail_in) {
        z.next_in += skip;
        z.avail_in -= skip;
        if (DEBUG_GUNZIP)
          cerr << "Gunzip: Skipped " << skip << endl;
        skip = 0;
        if (z.avail_in == 0) break;
      } else {
        if (DEBUG_GUNZIP)
          cerr << "Gunzip: Skipped~ " << z.avail_in << endl;
        z.next_in += z.avail_in;
        skip -= z.avail_in;
        z.avail_in = 0;
        break;
      }
    }

    Paranoid(z.avail_in > 0);
    switch (state) {

    case INIT0:
      // Start
      crc = crc32(0L, Z_NULL, 0); // Init checksum
      if (DEBUG_GUNZIP)
        cerr << "Gunzip:INIT0: Need byte 31, got " << unsigned(*z.next_in)
             << endl;
      //cerr << unsigned(z.next_in[1]) << ' ' << unsigned(z.next_in[2]) << ' ' << unsigned(z.next_in[3]) << endl;
      if (*z.next_in != '\x1f') {
        state = TRANSPARENT;
        break;
      }
      state = INIT1;
      nextInByte();
      if (z.avail_in == 0) break;

    case INIT1:
      // Found first .gz ID byte \x1f, look for second \x8b
      if (DEBUG_GUNZIP)
        cerr << "Gunzip:INIT1: Need byte 139, got " << unsigned(*z.next_in)
             << endl;
      if (static_cast<byte>(*z.next_in) != 0x8bU) {
        outputByte('\x1f');
        state = TRANSPARENT;
        break;
      }
      state = HEADER_CM;
      nextInByte();
      if (z.avail_in == 0) break;

    case HEADER_CM:
      // Found .gz ID bytes \x1f\x8b, compression method byte follows
      if (DEBUG_GUNZIP)
        cerr << "Gunzip:HEADER_CM: Need byte 8, got " << unsigned(*z.next_in)
             << endl;
      if (*z.next_in != 8) {
        outputByte('\x1f');
        outputByte('\x8b');
        state = TRANSPARENT;
        break;
      }
      state = HEADER_FLG;
      nextInByte();
      if (z.avail_in == 0) break;

    case HEADER_FLG:
      // Found .gz ID bytes \x1f\x8b\x08, now read flag byte
      headerFlags = nextInByte();
      if (DEBUG_GUNZIP)
        cerr << "Gunzip:HEADER_FLG: " << hex << unsigned(headerFlags) << dec
             << endl;
      if ((headerFlags & 0xe0) != 0) {
        error(0); // Reserved flags non-zero => error
        return;
      }
      state = HEADER_FEXTRA0;
      skip = 4  // skip MTIME field (Modification TIME)
           + 1  // skip XFL (eXtra FLags)
           + 1; // skip OS (Operating System)
      break;

    case HEADER_FEXTRA0:
      // If FEXTRA flag set, read XLEN
      if ((headerFlags & (1<<2)) == 0) {
        state = HEADER_FNAME; // FEXTRA not set
        break;
      }
      if (DEBUG_GUNZIP) cerr << "Gunzip:FEXTRA0" << endl;
      data = nextInByte(); // Lower 8 bits of XLEN (eXtra LENgth)
      state = HEADER_FEXTRA1;
      if (z.avail_in == 0) break;

    case HEADER_FEXTRA1:
      // If FEXTRA flag set, read XLEN
      data |= (nextInByte() << 8); // Upper 8 bits of XLEN (eXtra LENgth)
      if (DEBUG_GUNZIP) cerr << "Gunzip:FEXTRA1: XLEN=" << data << endl;
      state = HEADER_FNAME;
      skip = data; // Skip contents of "extra field"
      break;

    case HEADER_FNAME:
      // If FNAME flag set, skip subsequent original filename
      if ((headerFlags & (1<<3)) == 0) {
        state = HEADER_FCOMMENT; // FNAME not set
        break;
      }
      // Skip null-terminated original filename
      while (z.avail_in > 0) {
        byte b = nextInByte();
        if (DEBUG_GUNZIP)
          cerr << "Gunzip:FNAME: Skipping name: " << unsigned(b) << endl;
        if (b == 0) {
          state = HEADER_FCOMMENT;
          break;
        }
      }
      if (z.avail_in == 0) break;

    case HEADER_FCOMMENT:
      // If FCOMMENT flag set, skip subsequent comment
      if ((headerFlags & (1<<4)) == 0) {
        state = ZLIB; // FCOMMENT not set
        if ((headerFlags & (1<<1)) != 0)
          skip = 2; // FHCRC set - skip header checksum
        break;
      }
      // Skip null-terminated comment
      while (z.avail_in > 0) {
        b = nextInByte();
        if (DEBUG_GUNZIP)
          cerr << "Gunzip:FCOMMENT: Skipping comment: " << unsigned(b)
               << endl;
        if (b == 0) {
          state = ZLIB;
          if ((headerFlags & (1<<1)) != 0)
            skip = 2; // FHCRC set - skip header checksum
          break;
        }
      }
      break;

    case ZLIB:
      // Pass compressed data to zlib
      while (z.avail_in > 0) {
        needOutByte();
        Bytef* oldNextOut = z.next_out;
        int ok = inflate(&z, Z_NO_FLUSH); // Decompress!
        if (z.next_out > oldNextOut) {
          crc = crc32(crc, oldNextOut, z.next_out - oldNextOut);
          io->gunzip_data(this, oldNextOut, z.next_out - oldNextOut);
        }
        if (ok == Z_OK) {
          if (DEBUG_GUNZIP) cerr << "Gunzip:ZLIB: Z_OK" << endl;
        } else if (ok == Z_STREAM_END) {
          // Re-initialize decompressor
          if (inflateReset(&z) != Z_OK) {
            error(z.msg);
            return;
          }
          if (DEBUG_GUNZIP) cerr << "Gunzip:ZLIB: Z_STREAM_END" << endl;
          // End of zlib stream reached, now verify checksum
          state = TRAILER_CRC0;
          break;
        } else { // Z_NEED_DICT, Z_DATA_ERROR, Z_STREAM_ERROR, Z_BUF_ERROR
          if (DEBUG_GUNZIP) cerr << "Gunzip:ZLIB: error " << ok << endl;
          error(z.msg);
          return;
        }
      }
      break;

    case TRAILER_CRC0:
      // Compare checksum byte
      if (DEBUG_GUNZIP)
        cerr << "TRAILER_CRC0: crc=" << hex << crc << dec << endl;
      if ((b = nextInByte()) != (crc & 0xffU)) {
        if (DEBUG_GUNZIP)
          cerr << "TRAILER_CRC0 failed: need " << hex << (crc & 0xffU)
               << ", got " << unsigned(b) << dec << endl;
        error(_("Checksum is wrong"));
        return;
      }
      if (DEBUG_GUNZIP) cerr << "TRAILER_CRC0 ok" << endl;
      state = TRAILER_CRC1;
      if (z.avail_in == 0) break;

    case TRAILER_CRC1:
      // Compare checksum byte
      if (nextInByte() != ((crc >> 8) & 0xffU)) {
        if (DEBUG_GUNZIP) cerr << "TRAILER_CRC1 failed" << endl;
        error(_("Checksum is wrong"));
        return;
      }
      if (DEBUG_GUNZIP) cerr << "TRAILER_CRC1 ok" << endl;
      state = TRAILER_CRC2;
      if (z.avail_in == 0) break;

    case TRAILER_CRC2:
      // Compare checksum byte
      if (nextInByte() != ((crc >> 16) & 0xffU)) {
        if (DEBUG_GUNZIP) cerr << "TRAILER_CRC2 failed" << endl;
        error(_("Checksum is wrong"));
        return;
      }
      if (DEBUG_GUNZIP) cerr << "TRAILER_CRC2 ok" << endl;
      state = TRAILER_CRC3;
      if (z.avail_in == 0) break;

    case TRAILER_CRC3:
      // Compare checksum byte
      if (nextInByte() != ((crc >> 24) & 0xffU)) {
        if (DEBUG_GUNZIP) cerr << "TRAILER_CRC3 failed" << endl;
        error(_("Checksum is wrong"));
        return;
      }
      if (DEBUG_GUNZIP) cerr << "TRAILER_CRC3 ok" << endl;
      // Skip 4 bytes of length of uncompressed, then expect another .gz file
      skip = 4;
      state = INIT0;
      break;

    case TRANSPARENT:
      // Pass data through unmodified
      if (DEBUG_GUNZIP) cerr << "Gunzip:TRANSPARENT" << endl;
      while (z.avail_in > 0) {
        needOutByte();
        unsigned s = min(z.avail_in, z.avail_out);
        Bytef* oldNextOut = z.next_out;
        memmove(z.next_out, z.next_in, s);
        z.avail_in -= s;
        z.avail_out -= s;
        z.next_in += s;
        z.next_out += s;
        io->gunzip_data(this, oldNextOut, s);
      }
      break;

    default:
      Paranoid(false);
      error("Bug");
      if (DEBUG_GUNZIP) cerr << "Unknown state " << state << endl;
      return;

    } // endswitch (state)

  } // endwhile (z.avail_in > 0)

}
