/* $Id$ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2003  |  richard@
  | \/¯|  Richard Atterer     |  atterer.net
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

  Download & interpret .jigdo, download parts, assemble image

*/

#ifndef MAKEIMAGE_HH
#define MAKEIMAGE_HH

#include <config.h>

#include <debug.hh>
#include <jigdoconfig.hh>
#include <nocopy.hh>
//______________________________________________________________________

/** Download & interpret .jigdo, download parts, assemble image. MakeImage is
    the "core" class, other components use it for part of the work, e.g.
    MakeImageDl uses SingleURLs for file downloads, then passes the data to
    MakeImage.

    Below, arrow "A->B" means "A uses B"
    <pre>

    GtkMakeImage(GTK GUI)  CursesMakeImage(curses GUI)[0]
                   |        |
                   V        V
                  MakeImageDl  "jigdo-file make-image"[1]
                           |     |
                           V     V
                          MakeImage

    [0] Curses GUI non-existent so far
    [1] ATM, jigdo-file uses its own implementation of MakeImage's
        functionality, TODO fix that.
    </pre>

    MakeImage: Everything for turning many data sources into one final image
    <ul>

      <li>Maintains .jigdo file contents, but does not download the .jigdo
      data - someone else must do this and pass the file to MakeImage.

      <li>Stores name of .template file once present, opens & reads it.

      <li>Access to list of MD5 sums which are still needed to complete the
      image, i.e. list of parts left to download.

      <li>Lookup of MD5 sum, gives list of URLs for file, with indication WRT
      which server is preferred by the user.

      <li>Stores name of output image, creates image and writes to it.

      <li>You can give it chunks of downloaded data from *any* part, it'll
      write the output image as far as necessary (with zero areas for as yet
      non-downloaded data) and write out the downloaded data.

      <li>For resuming partial downloads of files in the image, can query how
      many bytes of the data for a part have been passed to MakeImage, and
      request some of those bytes for the checks of an overlapping resume.

      <li>Non-blocking operation for .template unpacking: If it needs to
      write lots of data to disc, will not do this during one long-lasting
      call to it, but will queue the disc-intensive request and expect you to
      call it back whenever it should do the next chunk of work.

      <li>While a disc request is active, downloaded data from any part can
      still be given to MakeImage. If the respective section of the image has
      already been written out (filled with zeroes), writes the data to disc.
      Otherwise, buffers the data. It'll indicate when the amount of buffered
      data exceeds a certain limit (=> the downloader *should* pause
      downloading), but will continue to accept and buffer further data.

      <li>Blocking operation when writing downloaded data: Usually,
      downloaded data of parts is not buffered in memory. In this case, will
      always write out the complete supplied data to disc in one go,
      regardless of the size of the downloaded chunk of data.

    </ul>

    MakeImageDl: Everything related to downloads
    <ul>

      <li>Pushes .jigdo file contents from a SingleURL to the MakeImage

      <li>Downloads .template via SingleURL, notifies MakeImage once done

      <li>"Owner" of the layout of the temporary directory, only component
      that directly makes modifications to this dir. (MakeImage only writes
      to the image file.)

      <li>Starts further SingleURLs for download of individual parts.

      <li>Automatic server selection: For servers which were rated equally
      acceptable by the user, measures their speed, then prefers the faster
      ones (but does not completely stop using the slower ones).

    </ul>

    GtkMakeImage: Is notified by MakeImageDl when anything interesting
    happens, updates GTK+ widgets. Also pauses, continues etc. the
    MakeImageDl. */
class MakeImage : NoCopy {
public:

  /** jigdoFile argument is only used for displaying error messages when
      scanning the .jigdo file contents.
      @param destDir destination directory that the final image should be
      written to. Initially, MakeImage will create a temporary dir (name
      based on jigdoFile leafname) to store administrative data in. */
  inline MakeImage(const string& jigdoFile, //const string& destDir,
                   JigdoConfig::ProgressReporter* jigdoErrors);
  inline ~MakeImage();

  /** Set where to report JigdoConfig errors, overwriting any value passed as
      jigdoErrors to the ctor. */
  inline void setReporter(JigdoConfig::ProgressReporter* jigdoErrors);

  /** Public data member: Contents of .jigdo file. The component using this
      MakeImage should retrieve the .jigdo data and add it here with
      configFile().push_back(line) for each line of the file. Call
      config.rescan() when done. */
  JigdoConfig config;
  ConfigFile& configFile() { return config.configFile(); }

private:
  // Don't copy
  inline MakeImage& operator=(const MakeImage&);
};
//______________________________________________________________________

MakeImage::MakeImage(const string& jigdoFile, //const string& destDir,
                     JigdoConfig::ProgressReporter* jigdoErrors)
  : config(jigdoFile, new ConfigFile(), *jigdoErrors) { }

MakeImage::~MakeImage() { }

void MakeImage::setReporter(JigdoConfig::ProgressReporter* jigdoErrors) {
  config.setReporter(*jigdoErrors);
}

#endif
