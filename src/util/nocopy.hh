/* $Id$ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2003  |  richard@
  | \/¯|  Richard Atterer     |  atterer.net
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

  To be used as a base class only - prevents that the derived class can be
  copied. It would be equivalent to add a private copy ctor and a private
  assignment operator to the derived class, but deriving from NoCopy saves
  typing and looks cleaner.

*/

#ifndef NOCOPY_HH
#define NOCOPY_HH

class NoCopy {
protected:
  NoCopy() { }
  ~NoCopy() { }
private:
  NoCopy(const NoCopy&);
  const NoCopy& operator=(const NoCopy&);
};

#endif
