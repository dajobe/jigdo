/* $Id$ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2001-2005  |  richard@
  | \/¯|  Richard Atterer          |  atterer.net
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

  Zlib/bzlib2 compression layer which integrates with C++ streams

*/

#include <config.h>

#include <errno.h>
#include <string.h>
#include <zlib.h>

#include <algorithm>
#include <fstream>
#include <new>

#include <log.hh>
#include <md5sum.hh>
#include <serialize.hh>
#include <string.hh>
#include <zstream.hh>
#include <zstream-gz.hh>
#include <zstream-bz.hh>
// struct ZibstreamBz : Zibstream::Impl { };
//______________________________________________________________________

DEBUG_UNIT("zstream")
//________________________________________

void Zobstream::close() {
  if (!is_open()) return;
  try {
    zip(todoBuf, todoCount, Z_FINISH); // Flush out remain. buffer contents
    deflateEnd();
  } catch (Zerror) {
    zipBufLast = zipBuf;
    // Deallocate memory
    delete[] todoBuf;
    todoBuf = 0;
    todoBufSize = todoCount = 0; // Important; cf Zobstream()
    stream = 0;
    /* Only report errors *after* marking the stream as closed, to avoid
       another exception being thrown when the Zobstream object goes out of
       scope and ~Zobstream calls close() again. */
    throw;
  }

  zipBufLast = zipBuf;

  // Deallocate memory
  delete[] todoBuf;
  todoBuf = 0;
  todoBufSize = todoCount = 0; // Important; cf Zobstream()
  stream = 0;
}
//______________________________________________________________________

// Write compressed, flushed data to output stream
void Zobstream::writeZipped(unsigned partId) {
  debug("Writing %1 bytes compressed, was %2 uncompressed",
        totalOut(), totalIn());

  // #Bytes     Value   Description
  // ----------------------------------------------------------------------
  //  4       dataID  "ID for the part: 'DATA' = the hex bytes 44 41 54 41"
  //  6       dataLen "Length of part, i.e. length of compressed data + 16"
  //  6       dataUnc "Number of bytes of *uncompressed* data of this part"
  // dataLen-16       "Compressed data"
  byte buf[16];
  byte* p = buf;
  serialize4(partId, p); // DATA or BZIP
  uint64 l = totalOut() + 16;
  serialize6(l, p + 4);
  l = totalIn();
  serialize6(l, p + 10);
  writeBytes(*stream, buf, 16);
  if (!stream->good())
    throw Zerror(0, string(_("Could not write template data")));
  if (md5sum != 0) md5sum->update(buf, 16);

  ZipData* zd = zipBuf;
  unsigned len;
  while (true) {
    Paranoid(zd != 0);
    len = (totalOut() < ZIPDATA_SIZE ? totalOut() : ZIPDATA_SIZE);
    writeBytes(*stream, zd->data, len);
    if (md5sum != 0) md5sum->update(zd->data, len);
    if (!stream->good())
      throw Zerror(0, string(_("Could not write template data")));
    zd = zd->next;
    if (len < ZIPDATA_SIZE || zd == 0) break;
    setTotalOut(totalOut() - ZIPDATA_SIZE);
  }

  zipBufLast = zipBuf;
  setNextOut(zipBuf->data);
  setAvailOut(ZIPDATA_SIZE);
  setTotalIn(0);
  setTotalOut(0);
  // NB: next_in, avail_in are left alone

  deflateReset(); // Might throw
}
//______________________________________________________________________

Zobstream& Zobstream::put(uint32 x) {
  if (todoCount > todoBufSize - 4) zip(todoBuf, todoCount);
  todoBuf[todoCount] = static_cast<byte>(x & 0xff);
  ++todoCount;
  todoBuf[todoCount] = static_cast<byte>((x >> 8) & 0xff);
  ++todoCount;
  todoBuf[todoCount] = static_cast<byte>((x >> 16) & 0xff);
  ++todoCount;
  todoBuf[todoCount] = static_cast<byte>((x >> 24) & 0xff);
  ++todoCount;
  return *this;
}

//======================================================================

void Zibstream::open(bistream& s) {
  Assert(!is_open());
  Paranoid(buf == 0);
  buf = new byte[bufSize];

//   z = new ZibstreamGz(); // TODO switch for each block
//   z->setNextIn(0);
//   z->setNextOut(0);
//   z->setAvailIn(0);
//   z->setAvailOut(0);
//   z->init();
//   if (!z->ok()) z->throwError();

  dataLen = dataUnc = 0;
  stream = &s;
}

void Zibstream::close() {
  if (!is_open()) return;

  if (z != 0) z->end();

  // Deallocate memory
  delete[] buf;
  buf = 0;
  stream = 0;

  /* Only report errors *after* marking the stream as closed, to avoid
     another exception being thrown when the Zibstream object goes out of
     scope and ~Zibstream calls close() again. */
  if (!z->ok()) z->throwError();
}
//________________________________________

Zibstream& Zibstream::read(byte* dest, unsigned n) {
  gcountVal = 0; // in case n == 0
  if (!good()) return *this;
  nextOut = dest;
  availOut = n;

  //cerr << "Zibstream::read: " << n << " to read, avail_in=" << z->availIn()
  //     << endl;
  SerialIstreamIterator in(*stream);
  while (availOut > 0) {
    //____________________

    /* If possible, uncompress into destination buffer. Handling this
       case first for speed */
    if (z != 0 && z->availIn() != 0) {
      byte* oldNextOut = nextOut;
      z->inflate(&nextOut, &availOut);
      gcountVal = nextOut - dest;
      dataUnc -= nextOut - oldNextOut;
      debug("read: avail_out=%1 dataLen=%2 dataUnc=%3 - inflated %4",
            availOut, dataLen, dataUnc, nextOut - oldNextOut);
      Assert(dataUnc > 0 || z->streamEnd() || z->ok());
      if (z->availOut() == 0) break;
      if (!z->ok() && !z->streamEnd())
        z->throwError();
      continue;
    }
    //____________________

    // Need to read another DATA part?
    if (dataLen == 0) {
      Assert(dataUnc == 0);
      streamsize prevPos = stream->tellg();
      unsigned id;
      unserialize4(id, in);
      if (!*stream || (id != DATA && id != BZIP)) {
        // Reached end of file or a non-DATA/BZIP part
        stream->seekg(prevPos, ios::beg);
        delete[] buf;
        buf = 0; // Causes fail() == true
        throw Zerror(0, string(_("Corrupted input data")));
      }

//       Assert(dataUnc == 0);
//       const char* hdr = "DATA";
//       const char* cur = hdr;
//       byte x;
//       while (*cur != '\0' && *stream) {
//         x = stream->get(); // Any errors handled below, after end of while()
//         //debug("read: cur=%1, infile=%2 @%3", int(*cur), x,
//         //      implicit_cast<uint64>(stream->tellg()) - 1);
//         if (*cur != x) { // Reached end of file or non-DATA part
//           stream->seekg(hdr - cur, ios::cur);
//           delete[] buf;
//           buf = 0; // Causes fail() == true
//           throw Zerror(0, string(_("Corrupted input data")));
//         }
//         ++cur;
//       }
      unserialize6(dataLen, in);
      dataLen -= 16;
      unserialize6(dataUnc, in);
#     if 0
      cerr << "Zibstream::read: avail_out=" << availOut
           << " dataLen=" << dataLen
           << " dataUnc=" << dataUnc << " - new DATA part" << endl;
#     endif
      if (dataUnc == 0 || !*stream) {
        delete[] buf;
        buf = 0;
        throw Zerror(0, string(_("Corrupted input data")));
      }

      // Decide whether to (re)allocate inflater
      // One or both out of gz/bz will be null
      ZibstreamGz* gz = dynamic_cast<ZibstreamGz*>(z);
      ZibstreamBz* bz = dynamic_cast<ZibstreamBz*>(z);
      if ((id == DATA && gz == 0)
          || (id == BZIP && bz == 0)) {
        if (z != 0) {
          // Delete old, unneeded inflater
          z->end();
          if (!z->ok()) z->throwError();
          delete z;
        }
        // Allocate and init new one
        if (id == DATA)
          z = new ZibstreamGz();
        else
          z = new ZibstreamBz();
        z->setNextIn(0);
        z->setAvailIn(0);
        z->init();
        if (!z->ok()) z->throwError();
      } else {
        // Nothing changed - just recycle old inflater
        z->reset();
        if (!z->ok()) z->throwError();
      }

    } // endif (dataLen == 0) // Need to read another DATA part?
    //____________________

    // Read data from file into buffer?
    unsigned toRead = (dataLen < bufSize ? dataLen : bufSize);
    byte* b = &buf[0];
    z->setNextIn(b);
    z->setAvailIn(toRead);
    dataLen -= toRead;
    while (*stream && toRead > 0) {
      readBytes(*stream, b, toRead);
      unsigned n = stream->gcount();
      b += n;
      toRead -= n;
    }
#   if 0
    cerr << "Zibstream::read: avail_out=" << z->availOut()
         << " dataLen=" << dataLen
         << " dataUnc=" << dataUnc << " - read "
         << z->availIn() << " from file to buf" << endl;
#   endif
    if (!*stream) {
      delete[] buf;
      buf = 0;
      string err = subst(_("Error reading compressed data - %1"),
                         strerror(errno));
      throw Zerror(0, err);
    }
    //____________________

  } // endwhile (availOut > 0)

# if 0
  cerr << "Zibstream::read: avail_out=" << z->availOut()
       << " dataLen=" << dataLen
       << " dataUnc=" << dataUnc << " - returns, gcount=" << gcountVal
       << " avail_in=" << z->availIn()
       << endl;
# endif

  return *this;
}
