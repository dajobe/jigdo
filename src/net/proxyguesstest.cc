/* $Id$ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2003  |  richard@
  | \/¯|  Richard Atterer     |  atterer.net
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

  Find proxy URLs by reading env vars and various browsers' config

*/

#include <config.h>

#include <glibwww.hh>
#include <iostream>
#include <proxyguess.hh>
//______________________________________________________________________

int main() {
  cout << "proxyguesstest" << endl;
  glibwww_init("proxyguesstest", "0.8.15");
  proxyGuess();
}
