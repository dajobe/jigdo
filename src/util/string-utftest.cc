/* $Id$ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2003  |  richard@
  | \/Ø|  Richard Atterer     |  atterer.net
  Ø '` Ø
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

  Helper functions for dealing with UTF-8 strings

*/

#include <config.h>

#include <glib.h>
#include <stdlib.h>
#include <iostream>
#include <string>

#include <string-utf.hh>
//______________________________________________________________________

#if 0
gchar *
g_locale_to_utf8 (const gchar  *opsysstring,
                  gssize        len,
                  gsize        *bytes_read,
                  gsize        *bytes_written,
                  GError      **error)
{
  const char *charset;

  if (g_get_charset (&charset))
    return strdup_len (opsysstring, len, bytes_read, bytes_written, error);
  else
    return g_convert (opsysstring, len,
                      "UTF-8", charset, bytes_read, bytes_written, error);
}
#endif
//____________________

namespace {

int returnCode = 0;

void test(const char* correct, const string& generated) {
  if (generated == correct) {
    cout << "OK: \"" << generated << "\"\n";
    return;
  }
  cout << "FAILED:\n"
       << "  expected \"" << correct << "\"\n"
       << "  but got  \"" << generated << "\"\n";
  returnCode = 1;
}

}
//____________________

int main() {
  /*
  const char* charset;
  g_get_charset(&charset);
  cout << "glib thinks that your locale uses the following character "
    "encoding: " << charset << "\nI'm setting CHARSET=ISO-8859-15 just for "
    "this test\n";
  */

  // For this test, make glib assume 8-bit locale
  //putenv("CHARSET=ISO-8859-15");

  string s("string");
  test("1 >42 foo 43% 666 1024 X ¬© string string ",
       subst("1 >%1 foo %2%% %3 %4 %5 %6 %7 %8 %9",
             42, 43U, 666L, 1024UL, 'X', "¬©", s, &s));

  // Arg not valid UTF-8, so it is cut off at first invalid char
  test("2 &Wah <>&W Woo!",
       subst("2 &Wah %1 Woo!", "<>&W‰‰‰h"));

  // As above, but escape <>&
  test("3 &Wah &lt;&gt;&amp;W Woo!",
       subst("3 &Wah %E1 Woo!", "<>&W‰‰‰h"));
  // Let's try this again with a valid UTF-8 string
  test("4 &Wah W√§&lt;¬©&gt;&amp;h Woo!",
       subst("4 &Wah %E1 Woo!", "W√§<¬©>&h"));

  // When using F, the thing is not checked, producing invalid UTF-8
  test("5 Wah W‰‰‰h <Woo!",
       subst("5 Wah %F1 <Woo!", "W‰‰‰h"));

  // But we can have subst() convert the string for us
  test("6 Wah W√§√§√§h Woo!",
       subst("6 Wah %L1 %2", "W‰‰‰h", "Woo!"));

  // convert from ISO-8859-1 and escape <>&
  test("7 Wah <b>W√§&lt;√§&gt;√§h&amp;</b> &lt; Woo!",
       subst("7 Wah <b>%FELL1</b> &lt; Woo!", "W‰<‰>‰h&"));

  // escape, but assume that arg is UTF-8
  test("8 Wah <b>Wo&lt;o&gt;oh&amp;</b> &lt; Woo!",
       subst("8 Wah <b>%FE1</b> &lt; Woo!", "Wo<o>oh&"));

  return returnCode;
}
