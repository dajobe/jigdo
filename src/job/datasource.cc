/* $Id$ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2003  |  richard@
  | \/¯|  Richard Atterer     |  atterer.net
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

  Interface for objects returning data from the network or from disk

*/

#include <datasource.hh>

using namespace Job;

DataSource::~DataSource() {
  if (io) io->job_deleted();
}
