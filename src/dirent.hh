/* $Id$ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2001-2002  |  richard@
  | \/¯|  Richard Atterer          |  atterer.net
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

  Compatibility header for glib+mingw.

*/

#ifndef DIRENT_HH
#define DIRENT_HH

/* Work around clash of definitions of 'struct dirent' and 'struct
   DIR'; both MinGW and glib define them. */
#include <config.h>
#if DIRENT_HACK
#  if DEBUG
#    warning "horrible preprocessor hack to fix glib/MinGW dirent conflict"
#  endif
#  include <dirent.h>
#  include <gtypes.h>
#  define dirent            glibHack_seeConfigH_dirent
#  define DIR               glibHack_seeConfigH_DIR
#  define g_win32_opendir   glibHack_seeConfigH_g_win32_opendir
#  define g_win32_readdir   glibHack_seeConfigH_g_win32_readdir
#  define g_win32_closedir  glibHack_seeConfigH_g_win32_closedir
#  define g_win32_rewinddir glibHack_seeConfigH_g_win32_rewinddir
#  include <glib.h>
#  undef dirent
#  undef DIR
#  undef g_win32_opendir
#  undef g_win32_readdir
#  undef g_win32_closedir
#  undef g_win32_rewinddir
#  undef ftruncate
#  undef opendir
#  undef readdir
#  undef rewinddir
#  undef closedir
#  else
#  include <dirent.h>
#endif


#endif
