## The text below is written to the README file that is distributed in
## the tarballs with statically linked binaries (created with "make
## bindist"), jigdo-bin-x.y.z.tar.bz2

Jigsaw Download (jigdo)
~~~~~~~~~~~~~~~~~~~~~~~
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

 - Open a command line terminal and change directory to the same
   directory as this README (using the command "cd directoryname").
 - Run the jigdo-lite script, by entering "./jigdo-lite".
 - The script will ask for the URL of a ".jigdo" file - enter the URL
   of your choice.
 - Follow the instructions printed by jigdo-lite - hopefully they will
   be self-explanatory.

You can also have a look at the file "jigdo-lite.html" for a little
documentation. "jigdo-file.html" contains a detailed technical
description of jigdo.

Making jigdo-lite use your proxy
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
To make jigdo-lite use your proxy for its downloads, first start
"jigdo-lite" as described above. As soon as the input prompt has
appeared, abort the program again by pressing Ctrl-C.

You will find that jigdo-lite has created a file called ".jigdo-lite"
in your home directory. Load this into an editor and find the line
that starts with "wgetOpts". The following switches can be added to
the line:

-e ftp_proxy=http://LOCAL-PROXY:PORT/
-e http_proxy=http://LOCAL-PROXY:PORT/
--proxy-user=USER
--proxy-passwd=PASSWORD

Of course, you only need --proxy-user/password if your proxy requires
authentication. The switches need to be added to the end of the
wgetOpts line *before* the final ' character. All options must be on
one line.


jigdo-mirror
~~~~~~~~~~~~
The included jigdo-mirror script is for people who have a full Debian
FTP mirror on their harddisc as well as a mirror of the Debian jigdo
files, and who want to create all the images offered by these jigdo
files. Additionally, if the script is run at regular intervals,
updated/added/deleted jigdo files are detected. See
"jigdo-mirror.html" for more information.

----------------------------------------------------------------------

Copyright (C) 2001-2004  |  richard@
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
