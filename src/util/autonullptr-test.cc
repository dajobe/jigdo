/* $Id$ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2004  |  richard@
  | \/¯|  Richard Atterer     |  atterer.net
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify it
  under the terms of the GNU General Public License, version 2. See the file
  COPYING for details.

  A pointer which gets set to null if the pointed-to object is deleted

  #test-deps

*/

#include <autonullptr.hh>
#include <debug.hh>
//______________________________________________________________________

namespace {

  struct X : public AutoNullPtrBase<X> {
    int memb;
  };

}

int main() {
  X* x = new X();
  AutoNullPtr<X> ptr(x);
  Assert(ptr.get() == x);
  Assert(ptr == x);
  Assert(ptr); // non-null
  Assert(ptr != 0); // non-null
  (*ptr).memb = 0;
  Assert(ptr->memb == 0);

  {
    AutoNullPtr<X> ptr2 = ptr;
    Assert(ptr2 == x && ptr2 == ptr);
    delete x;
    Assert(ptr == 0);
    Assert(ptr2 == 0);

    x = new X();
    ptr = x;
  } // Destroy ptr2
  delete x;

  return 0;
}
