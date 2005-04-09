/* $Id$ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2003  |  richard@
  | \/¯|  Richard Atterer     |  atterer.net
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

*//** @file

  Reference-counted bfstream

*/

#ifndef BSTREAM_COUNTED_HH
#define BSTREAM_COUNTED_HH

#include <config.h>

#include <fstream>

#include <smartptr.hh>
#include <bstream.hh>

/** A bfstream which can be referenced with a SmartPtr. */
class BfstreamCounted : virtual public SmartPtrBase, public bfstream {
public:
  BfstreamCounted(const char* name, ios::openmode mode)
    : bfstream(name, mode) { }
};

#endif
