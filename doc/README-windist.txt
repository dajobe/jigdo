
Jigsaw Download (jigdo) for Windows
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Jigsaw Download homepage:   <http://atterer.net/jigdo/>
Debian CD images via jigdo: <http://www.debian.org/CD/jigdo-cd/>
                            <richard@
Written by Richard Atterer:  atterer.net>

jigdo-lite
~~~~~~~~~~
This program enables you to retrieve big files (for example, CD
images) that someone offers for download in the form of ".jigdo"
files.

First, locate a site which offers such files with a ".jigdo" extension
- see <http://www.debian.org/CD/jigdo-cd/> for CD and DVD images of
Debian Linux in jigdo format. Next, download the image as follows:

 - Double-click on the "jigdo-lite.bat" file.
 - The script will ask for the URL of a ".jigdo" file - enter the URL
   of your choice.
 - Follow the instructions printed by jigdo-lite - hopefully they will
   be self-explanatory.

Making jigdo-lite use your proxy
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
To make jigdo-lite use your proxy for its downloads, first
double-click on "jigdo-lite.bat" to start the program. As soon as the
input prompt has appeared, abort the program again and close the
command window.

You will find that jigdo-lite has created a file called
"jigdo-lite-settings.txt" in the same directory as this README.txt
file. Load this into an editor and find the line that starts with
"wgetOpts". The following switches can be added to the line:

-e ftp_proxy=http://LOCAL-PROXY:PORT/
-e http_proxy=http://LOCAL-PROXY:PORT/
--proxy-user=USER
--proxy-passwd=PASSWORD

Of course, you only need --proxy-user/password if your proxy requires
authentication. The switches need to be added to the end of the
wgetOpts line *before* the final ' character. All options must be on
one line.

----------------------------------------------------------------------

Copyright (C) 2001-2005  |  richard@
Richard Atterer          |  atterer.net

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2, as
  published by the Free Software Foundation.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

Please note: The copyright notice in the file COPYING only applies to
the text of the GNU General Public License; the copyright of the
individual source files is as specified at the top of each file and
above. Also note that the code is licensed under GPL _version_2_ and
no other version. Special licensing for my (RA's) code is available on
request.
