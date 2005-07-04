/* $Id$ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2005  |  richard@
  | \/¯|  Richard Atterer     |  atterer.net
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify it
  under the terms of the GNU General Public License, version 2. See the file
  COPYING for details.

*//** @file

  Avoid clash between ftruncate() between Win32 and gtk

*/

#ifndef UNISTD_JIGDO_HH

/* Grr, on Windows, include/glib-2.0/glib/gwin32.h #defines ftruncate to
   g_win32_ftruncate, which causes trouble with unistd's ftruncate()
   declaration. */
#ifdef ftruncate
#  undef ftruncate
#endif

#include <unistd.h>

#endif
