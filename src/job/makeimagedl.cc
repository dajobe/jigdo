/* $Id$ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2003  |  richard@
  | \/¯|  Richard Atterer     |  atterer.net
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

  Download .jigdo/.template and file URLs

*/

#include <errno.h>

#include <config.h>

#include <stdio.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fstream>
#include <memory>

#include <cached-url.hh>
#include <compat.hh>
#include <jigdo-io.hh>
#include <log.hh>
#include <makeimagedl.hh>
#include <md5sum.hh>
#include <mimestream.hh>
#include <string.hh>
#include <uri.hh>
#include <url-mapping.hh>
//______________________________________________________________________

using namespace Job;

DEBUG_UNIT("makeimagedl")

// const char* Job::MakeImageDl::destDescTemplateVal =
//     _("Cache entry %1  --  %2");
//______________________________________________________________________

namespace {

  /* Normally, when an URL is already in the cache, we send out an
     If-Modified-Since request to check whether it has changed on the server.
     Exception: If cache entry was created no longer than this many seconds
     ago, do *not* send any request, just assume it has not changed on the
     server. */
  const time_t CACHE_ENTRY_AGE_NOIFMODSINCE = 5;

  /* Name prefix to use when creating temporary directory. A checksum of the
     source .jigdo URL will be appended. */
  const char* const TMPDIR_PREFIX = "jigdo-";

}

MakeImageDl::MakeImageDl(/*IO* ioPtr,*/ const string& jigdoUri,
                         const string& destination)
    : io(/*ioPtr*/), stateVal(DOWNLOADING_JIGDO),
      jigdoUrl(jigdoUri), jigdoIo(0), childrenVal(), dest(destination),
      tmpDirVal(), mi(),
      imageNameVal(), imageInfoVal(), imageShortInfoVal(), templateUrlVal(),
      templateMd5Val(0), callbackId(0) {
  // Remove all trailing '/' from dest dir, even if result empty
  unsigned destLen = dest.length();
  while (destLen > 0 && dest[destLen - 1] == DIRSEP) --destLen;
  dest.resize(destLen);

  /* Create name of temporary dir.
     Directory name: "jigdo-" followed by md5sum of .jigdo URL */
  MD5Sum md;
  md.update(reinterpret_cast<const byte*>(jigdoUri.data()),
            jigdoUri.length()).finish();

  Base64String b64;
  b64.result() = dest;
  b64.result() += DIRSEP;
  b64.result() += TMPDIR_PREFIX;
  // Halve number of bits by XORing bytes. (Would a real CRC64 be better??)
  byte cksum[8];
  const byte* digest = md.digest();
  for (int i = 0; i < 8; ++i)
    cksum[i] = digest[i] ^ digest[i + 8];
  b64.write(cksum, 8).flush();
  tmpDirVal.swap(b64.result());
}
//______________________________________________________________________

Job::MakeImageDl::~MakeImageDl() {
  debug("~MakeImageDl");
  if (callbackId != 0) g_source_remove(callbackId);
  killAllChildren();
  delete jigdoIo;
  delete templateMd5Val;
}
//______________________________________________________________________

void Job::MakeImageDl::killAllChildren() {
  // Delete all our children. NB ~Child will remove child from the list
  while (!childrenVal.empty()) {
    Child* x = childrenVal.front().get();
#   if DEBUG
    /* childFailed() or childSucceeded() MUST always be called for a Child.
       Exception: May delete the MakeImageDl without calling this for its
       children. */
    if (!x->childSuccFail)
      debug("childFailed()/Succeeded() not called for %1",
            x->source() ? x->source()->location() : "[deleted source]");
    x->childSuccFail = true; // Avoid failed assert
#   endif
    debug("~MakeImageDl: delete %1", x);
    delete x;
  }
}
//______________________________________________________________________

void MakeImageDl::run() {
  Assert(!finalState());

  // Now create tmpdir
  int status = compat_mkdir(tmpDir().c_str());
  if (status != 0) {
    if (errno == EEXIST) {
      // FIXME: Resume if EEXIST
      msg("RESUME NOT YET SUPPORTED, SORRY");
    } else {
      string error = subst(_("Could not create temporary directory: "
                             "%L1"), strerror(errno));
      generateError(error);
      return;
    }
  }
  writeReadMe();

  // Run initial .jigdo download, will start other downloads as needed
  string leafname;
  auto_ptr<Child> childDl(childFor(jigdoUrl, 0, &leafname));
  if (childDl.get() != 0) {
    string info = _("Retrieving .jigdo");
//     string destDesc = subst(destDescTemplate(), leafname, info);
    //x auto_ptr<DataSource::IO> frontend(0);
    //xif (io)
    //x  frontend.reset(io->makeImageDl_new(childDl->source(), jigdoUrl,
    //x                                     destDesc) );
    jigdoIo = new JigdoIO(childDl.get(), jigdoUrl/*, frontend.get()*/);
    //x childDl->setChildIo(jio);
    //x frontend.release();
    //x if (io) io->job_message(&info);
    IOSOURCE_SEND(IO, io, job_message, (info));
    childDl->source()->io.addListener(*jigdoIo);
    (childDl.release())->source()->run();
  }
}
//______________________________________________________________________

void MakeImageDl::writeReadMe() {
  string readmeName = tmpDir();
  readmeName += DIRSEP;
  readmeName += "ReadMe.txt";
  ofstream f(readmeName.c_str());
  f << subst(_(
    "Jigsaw Download - half-finished download\n"
    "\n"
    "This directory contains the data for a half-finished download of a\n"
    ".jigdo file. Do not change or delete any of the files in this\n"
    "directory! (Of course you can delete the entire directory if you do\n"
    "not want to continue with the download.)\n"
    "\n"
    "If the jigdo application was stopped and you want it to resume this\n"
    "download, simply enter again the same values you used the first time.\n"
    "\n"
    "In the \"URL\" field, enter:\n"
    "  %1\n"
    "\n"
    "In the \"Save to\" field, enter the parent directory of the directory\n"
    "containing this file. Unless you have moved it around, the correct\n"
    "value is:\n"
    "  %2\n"
    "\n"
    "\n"
    "[Download]\n"
    "URL=%1\n"
    "Destination=%2\n"
    ), jigdoUri(), dest);
}
//______________________________________________________________________

/* Return filename for content md5sum cache entry:
   "/home/x/jigdo-blasejfwe/c-nGJ2hQpUNCIZ0fafwQxZmQ" */
string MakeImageDl::cachePathnameContent(const MD5& md, bool isFinished,
                                         bool isC) {
  Base64String x;
  x.result() = tmpDir();
  x.result() += DIRSEP;
  if (isC) x.result() += 'c'; else x.result() += 'u';
  if (isFinished) x.result() += '-'; else x.result() += '~';
  x.write(md, 16);
  x.flush();
  return x.result();
}

/* Return filename for URL cache entry:
   "/home/x/jigdo-blasejfwe/u-nGJ2hQpUNCIZ0fafwQxZmQ" */
string MakeImageDl::cachePathnameUrl(const string& url, bool isFinished) {
  Base64String x;
  x.result() = tmpDir();
  x.result() += DIRSEP;
  x.result() += 'u';
  if (isFinished) x.result() += '-'; else x.result() += '~';
  static MD5Sum nameMd;
  nameMd.reset()
        .update(reinterpret_cast<const byte*>(url.data()), url.length())
        .finishForReuse();
  x.write(nameMd.digest(), 16);
  x.flush();
  return x.result();
}

// void MakeImageDl::appendLeafname(string* s, bool contentMd, const MD5& md) {
//   Base64String x;
//   *s += (contentMd ? "c-" : "u-");
//   (*s).swap(x.result());
//   x.write(md, 16).flush();
//   (*s).swap(x.result());
// }
//______________________________________________________________________

MakeImageDl::Child* MakeImageDl::childFor(const string& url, const MD5* md,
                                          string* leafnameOut) {
  debug("childFor: %L1, md %2",
        url, (md == 0 ? string("unknown") : md->toString()));

  msg("TODO: Maintain cache of active children; if URL requested 2nd time, "
      "add child which waits for 1st to finish");

  *leafnameOut = "TODO makeimagedl.cc:235";

  if (md != 0) {
    // "c-": Check whether data with that _checksum_ already finished in cache
    string filename = cachePathnameContent(*md);
    string destDesc = subst(_("Cache entry %1"), filename);
    struct stat fileInfo;
    if (stat(filename.c_str(), &fileInfo) == 0) {
      Child* c = childForCompletedContent(fileInfo, filename, md);
      IOSOURCE_SEND(IO, io, makeImageDl_new, (c->source(), url, destDesc));
      return c;
    }
  }

// Check whether file already present in cache, i.e. in tmpDir
//   string filename = tmpDir();
//   filename += DIRSEP;
//   filename += *leafname;

  // "u-": Check whether data with that _source_URL_ already finished in cache
  string filename = cachePathnameUrl(url);
  string destDesc = subst(_("Cache entry %1"), filename);
  struct stat fileInfo;
  if (stat(filename.c_str(), &fileInfo) == 0) {
    Child* c = childForCompletedUrl(fileInfo, filename, md);
    IOSOURCE_SEND(IO, io, makeImageDl_new, (c->source(), url, destDesc));
    return c;
  }

  /* statResult != 0, we assume this means "no such file or directory".
     "u~": Now check whether a download is already under way, or if a
     half-finished download was aborted earlier. */
  toggleLeafname(&filename);
//   toggleLeafname(leafname);
  if (stat(filename.c_str(), &fileInfo) == 0) {
    Child* c = childForSemiCompleted(fileInfo, filename);
    IOSOURCE_SEND(IO, io, makeImageDl_new, (c->source(), url, destDesc));
    return c;
  }

  /* Neither the complete nor the partial data is in the cache, so start a
     new download. */
  debug("childFor: New download to %L1", filename);
  BfstreamCounted* f = new BfstreamCounted(filename.c_str(),
                                    ios::binary|ios::in|ios::out|ios::trunc);
  if (!*f) {
    string err = subst(_("Could not open `%L1' for output: %L2"),
                       filename, strerror(errno));
    generateError(err);
    return 0;
  }
  auto_ptr<SingleUrl> dl(new SingleUrl(url));
  dl->setDestination(f, 0, 0);
  Child* c = new Child(this, &childrenVal, dl.get(), md);
  IOSOURCE_SEND(IO, io, makeImageDl_new, (dl.get(), url, destDesc));
  dl.release();
  return c;
}
//______________________________________________________________________

// Cache contains already completed download for requested URL/md5sum
MakeImageDl::Child* MakeImageDl::childForCompletedUrl(
    const struct stat& fileInfo, const string& filename, const MD5* md) {
  if (!S_ISREG(fileInfo.st_mode)) {
    // Something messed with the cache dir
    string err = subst(_("Invalid cache entry: `%L1' is not a file"),
                       filename);
    generateError(err);
    return 0;
  }

  int cacheEntryAge = time(0) - fileInfo.st_mtime;
  debug("childFor: cache entry is %1 secs old", cacheEntryAge);

  cacheEntryAge = 0; // TODO - FIXME - remove this once if-mod-since impl.

  if (cacheEntryAge < CACHE_ENTRY_AGE_NOIFMODSINCE) {
    // Data fetched very recently - no need to go on the net, imm. return it
    debug("childFor: already have %L1", filename);
    auto_ptr<CachedUrl> dl(new CachedUrl(filename, 0));
    Child* c = new Child(this, &childrenVal, dl.get(), md);
    dl.release();
    return c;
  } else {
    /* Data for URL fetched before. If less than IF_MOD_SINCE seconds ago,
       just return it, else do an If-Modified-Since request. */
    debug("childFor: if-mod-since %L1", filename);
    Assert(false); return 0;

  }
}
//____________________

// Cache contains already completed download for requested URL/md5sum
MakeImageDl::Child* MakeImageDl::childForCompletedContent(
    const struct stat& fileInfo, const string& filename, const MD5* md) {
  if (!S_ISREG(fileInfo.st_mode)) {
    // Something messed with the cache dir
    string err = subst(_("Invalid cache entry: `%L1' is not a file"),
                       filename);
    generateError(err);
    return 0;
  }

  // Data with that MD5 known - no need to go on the net, imm. return it
  debug("childFor: already have md %L1", filename);
  auto_ptr<CachedUrl> dl(new CachedUrl(filename, 0));
  Child* c = new Child(this, &childrenVal, dl.get(), md);
  dl.release();
  return c;
}
//______________________________________________________________________

MakeImageDl::Child* MakeImageDl::childForSemiCompleted(
    const struct stat& fileInfo, const string& filename) {
  if (!S_ISREG(fileInfo.st_mode)) {
    // Something messed with the cache dir
    string err = subst(_("Invalid cache entry: `%L1' is not a file"),
                       filename);
    generateError(err);
    return 0;
  }
  debug("childFor: already have partial %L1", filename);
  Assert(false); return 0;
#if 0
  if (anotherdownloadunderway) {
    return cloneofotherdownload; // What if other d/l is aborted by user
  } else {
    // Do a resume + If-Modified-Since
    return x;
  }
#endif
}
//______________________________________________________________________

/* Called by Child when the download has succeeded: Rename a '~' download to
   '-' to indicate that it is complete. */
void MakeImageDl::singleUrlFinished(Child* c) {
# if DEBUG
  c->childSuccFail = true;
# endif

  Paranoid(dynamic_cast<SingleUrl*>(c->source()) != 0);

  debug("singleUrlFinished: %1", c->source()->location());
  string srcName = cachePathnameUrl(c->source()->location(), false); // "u~"
  string destName;

  if (c->checkContent) {
    // Compare desired md to actual mdCheck
    c->mdCheck.finish();
    if (c->md == c->mdCheck) {
      debug("singleUrlFinished: Checksum OK: %1", c->md.toString());
      destName = cachePathnameContent(c->md); // Rename to "c-"
    } else {
      debug("singleUrlFinished: Checksum mismatch, computed=%1 expected=%2",
            c->mdCheck.toString(), c->md.toString());
      // In this case, let code below rename to "u-"
    }
  }
  if (destName.empty())
    destName = cachePathnameUrl(c->source()->location()); // Rename to "u-"

  debug("singleUrlFinished: mv %1 %2", srcName, destName);

  // On Windows, cannot rename open file, so ensure it is closed
  c->deleteSource();

  int status = rename(srcName.c_str(), destName.c_str());
  if (status != 0) {
    string destName2(destName, tmpDir().length() + 1, string::npos);
    string err = subst(_("Could not rename `%L1' to `%L2': %L3"),
                       srcName, destName2, strerror(errno));
    generateError(err);
    return;
  }
}
//______________________________________________________________________

void MakeImageDl::childFailed(Child* c, DataSource::IO*) {
# if DEBUG
  c->childSuccFail = true;
# endif
  //x if (frontend != 0)
  //x   io->makeImageDl_finished(childDl->source(), frontend);
  IOSOURCE_SEND(IO, io, makeImageDl_finished, (c->source()));

  // Delete partial output file if it is empty
  if (dynamic_cast<SingleUrl*>(c) != 0) {
    string filename = cachePathnameUrl(c->source()->location(), false);
    c->deleteSource();
    debug("rm -f %1", filename);
    remove(filename.c_str());
  }

  // FIXME: Uncomment if() once if-mod-since resume implemented
//   struct stat fileInfo;
//   if (stat(name.c_str(), &fileInfo) == 0 && fileInfo.st_size == 0) {
//   debug("rm -f %1", name);
//   remove(name.c_str());
//   } else {
//     debug("NO rm -f %1", name);
//   }

  // Must not delete any part of JigdoIO tree while any other is still live
  //x if (dynamic_cast<JigdoIO*>(childDl->childIo()) == 0) delete childDl;
}
//______________________________________________________________________

/* Info from first [Image] in include tree available - display it, start
   template download now if possible */
void MakeImageDl::setImageSection(string* imageName, string* imageInfo,
    string* imageShortInfo, string* templateUrl, MD5** templateMd5) {
  debug("setImageSection templateUrl=%1", templateUrl);
  Paranoid(!haveImageSection());
  imageNameVal.swap(*imageName);
  imageInfoVal.swap(*imageInfo);
  imageShortInfoVal.swap(*imageShortInfo);
  templateUrlVal.swap(*templateUrl);
  templateMd5Val = *templateMd5; *templateMd5 = 0;

  //x if (io) io->makeImageDl_haveImageSection();
  IOSOURCE_SEND(IO, io, makeImageDl_haveImageSection, ());
}
//______________________________________________________________________

void MakeImageDl::jigdoFinished() {
  debug("jigdoFinished");
  callbackId = g_idle_add_full(G_PRIORITY_HIGH_IDLE, &jigdoFinished_callback,
                               (gpointer)this, NULL);
}

gboolean MakeImageDl::jigdoFinished_callback(gpointer mi) {
  MakeImageDl* self = static_cast<MakeImageDl*>(mi);
  self->callbackId = 0;
  self->jigdoFinished2();
  return FALSE; // "Don't call me again"
}

/* All .jigdo data available now - this is called by JigdoIO. At this point,
   the only children that we ever started were all SingleUrls with JigdoIOs
   attached to them, so we can delete all jigdo-downloading children simply
   by deleting all children. */
void MakeImageDl::jigdoFinished2() {
  debug("jigdoFinished2");

#if 0
  // Delete all JigdoIO objects
  delete jigdoIo;
  jigdoIo = 0;

  typedef ChildList::iterator Iter;
  Iter i = childrenVal.begin();
  while (i != childrenVal.end()) {
    Child* x = i->get();
    ++i;
#   if DEBUG
    if (!x->childSuccFail)
      debug("childFailed()/Succeeded() not called for %1",
            x->source() ? x->source()->location() : "[deleted source]");
    x->childSuccFail = true; // Avoid failed assert
    urlMap.dumpJigdoInfo();
#   endif
    delete x;
#if 0 /* IOPtr */
    if (dynamic_cast<JigdoIO*>(x->childIo()) != 0) {
#     if DEBUG
      if (!x->childSuccFail)
        debug("childFailed()/Succeeded() not called for %1",
              x->source() ? x->source()->location() : "[deleted source]");
      x->childSuccFail = true; // Avoid failed assert
      urlMap.dumpJigdoInfo();
#     endif
      debug("jigdoFinished: delete %1", x);
      delete x;
    }
#endif
  }
#endif

  if (finalState()) return; // I.e. there was an error

  Paranoid(stateVal == DOWNLOADING_JIGDO);
  stateVal = DOWNLOADING_TEMPLATE;

  /* Template download. Relative template URLs have already been made
     absolute by jigdo-io */
  if (isRealUrl(templateUrlVal)) {
    // Template is a single absolute or relative URL
    debug("Template: <%1>", templateUrlVal);
    // Run .template download
    string leafname;
    auto_ptr<Child> childDl(childFor(templateUrlVal, templateMd5Val,
                                     &leafname));
    if (childDl.get() != 0) {
      string info = _("Retrieving .template");
//       string destDesc = subst(destDescTemplate(), leafname, info);
//x      if (io) {
//         DataSource::IO* frontend =
//           io->makeImageDl_new(childDl->source(), templUrl, destDesc);
//         //x childDl->setChildIo(frontend);
//         childDl->source()->io.addListener(*frontend);
//         io->job_message(&info);
//       }
      IOSOURCE_SEND(IO, io, job_message, (info));
      //nah: childDl->source()->io.addListener(new TemplateIO(this));
      (childDl.release())->source()->run();
    }
  } else {
    debug("Template: %1", templateUrlVal);
    // Template is a Label:something mapping
    generateError("Label:some.template unimplemented", ERROR);
    debug("Label:some.template unimplemented");
  }

}
//______________________________________________________________________

/* Template download finished. This just means that the data was downloaded,
   need to verify its md5sum if appropriate. */
// void MakeImageDl::templateFinished() {
//   debug("MakeImageDl::templateFinished()");
//   if (finalState()) return; // I.e. there was an error
//   Paranoid(stateVal == DOWNLOADING_TEMPLATE);

//   stateVal = DOWNLOADING____;
// }
//______________________________________________________________________

void MakeImageDl::Child::job_deleted() { }

void MakeImageDl::Child::job_succeeded() {
  debug("Child::job_succeeded: %1", source()->location());
# if DEBUG
  childSuccFail = true;
# endif
  IOSOURCE_SEND(MakeImageDl::IO, master()->io,
                makeImageDl_finished, (source()));

  // For SingleUrls, maybe rename cache entry
  if (dynamic_cast<SingleUrl*>(source()) != 0)
    master()->singleUrlFinished(this);
  // singleUrlSucceeded() calls this - also call it for other sources
  deleteSource();

//   if (master()->state() == DOWNLOADING_TEMPLATE)
//     master()->templateFinished();
}

void MakeImageDl::Child::job_failed(const string&) { }

void MakeImageDl::Child::job_message(const string&) { }

void MakeImageDl::Child::dataSource_dataSize(uint64) { }

void MakeImageDl::Child::dataSource_data(const byte* data, unsigned size,
                                         uint64) {
  // Desired checksum is in md; calculate actual checksum in mdCheck
  if (checkContent)
    mdCheck.update(data, size);
}
