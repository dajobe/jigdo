/* $Id$ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2002  |  richard@
  | \/¯|  Richard Atterer     |  atterer.net
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

  Helper class for mktemplate - queue of partially matched files

*/

#include <config.h>
#include <mktemplate.hh>
#include <partialmatch.hh>
#include <partialmatch.ih>
//______________________________________________________________________

#if DEBUG

void MkTemplate::PartialMatchQueue::consistencyCheck() const {
  int count = 0;
  for (PartialMatch* i = head; i != 0 && count <= MAX_MATCHES;
       i = i->next()) {
    Assert(i->next() == 0 || *i <= *(i->next()));
    ++count;
  }
  for (PartialMatch* i = freeHead; i != 0 && count <= MAX_MATCHES;
       i = i->next()) ++count;
  Assert(count == MAX_MATCHES);
}

#endif
