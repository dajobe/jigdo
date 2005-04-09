/* $Id$ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2003  |  richard@
  | \/¯|  Richard Atterer     |  atterer.net
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

*//** @file

  subst("Format %1, %2", arg1, arg2) creates strings with the arguments
  filled in, and does so in a safer way than sprintf() and friends.

  This is the same functionality as in string.hh, except that UTF-8 strings
  are handled by subst():

    - Output string is always valid UTF-8
    - Format string is assumed to be in valid UTF-8
    - "%F1" arg is assumed to be in valid UTF-8 (is copied over unvalidated)
    - "%1"  arg is untrusted UTF-8, will be validated while substituting it
    - "%L1" arg is assumed to be in the OS locale, is converted into UTF-8

  Additionally:
    - "%E1" arg: The characters <>& are escaped with their entities
      &lt; &gt; &amp; when substituting the value. Useful if the string will
      be rendered by Pango. The escaping is limited to this single
      substitution, the format string or other substituted values can still
      contain tags. NB: Newlines, tabs, delete (0x7f) etc. are not removed.

  Modifiers can be combined, except that F is ignored when you use L.
  Modifiers only have an effect for string substitution, not for
  int/long/char etc.

  If untrusted UTF-8 turns out to be invalid, only the valid prefix (if any)
  of the invalid arg is substituted.

  Substituting single chars only makes sense if the char is an ASCII
  character.

  In a nutshell:<br>
    F = fast UTF-8 string substitution<br>
    L = locale-format string<br>
    E = escape <>&

*/

#ifndef STRING_UTF_HH
#define STRING_UTF_HH

#include <string.hh> /* Same interface, different implementation */

#endif
