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
#include <jigdoconfig.hh>
#include <log.hh>
#include <makeimagedl.hh>
#include <md5sum.hh>
#include <mimestream.hh>
#include <url-mapping.hh>
#include <string.hh>
//______________________________________________________________________

using namespace Job;

DEBUG_UNIT("makeimagedl")

const char* Job::MakeImageDl::destDescTemplateVal =
    _("Cache entry %1  --  %2");
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

MakeImageDl::MakeImageDl(IO* ioPtr, const string& jigdoUri,
                         const string& destination)
    : io(ioPtr), stateVal(DOWNLOADING_JIGDO),
      jigdoUrl(jigdoUri), childrenVal(), dest(destination),
      tmpDirVal(), mi(),
      imageNameVal(), imageInfoVal(), imageShortInfoVal(), templateUrlVal(),
      templateMd5Val(0) {
  // Remove all trailing '/' from dest dir, even if result empty
  unsigned destLen = dest.length();
  while (destLen > 0 && dest[destLen - 1] == DIRSEP) --destLen;
  dest.resize(destLen);

  /* Create name of temporary dir.
     Directory name: "jigdo-" followed by md5sum of .jigdo URL */
  MD5Sum md;
  md.update(reinterpret_cast<const byte*>(jigdoUri.c_str()),
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
  // Delete all our children. NB ~Child will remove child from the list
  while (!childrenVal.empty()) {
    Child* x = &childrenVal.front();
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
      generateError(&error);
      return;
    }
  }
  writeReadMe();

  // Run initial .jigdo download, will start other downloads as needed
  string leafname;
  auto_ptr<Child> childDl(childFor(jigdoUrl, 0, &leafname));
  if (childDl.get() != 0) {
    string info = _("Retrieving .jigdo data");
    string destDesc = subst(destDescTemplate(), leafname, info);
    auto_ptr<DataSource::IO> frontend(0);
    if (io)
      frontend.reset(io->makeImageDl_new(childDl->source(), jigdoUrl,
                                         destDesc) );
    JigdoIO* jio = new JigdoIO(childDl.get(), jigdoUrl, frontend.get());
    childDl->setChildIo(jio);
    frontend.release();
    if (io) io->job_message(&info);
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

void MakeImageDl::appendLeafname(string* s, bool contentMd, const MD5& md) {
  Base64String x;
  *s += (contentMd ? "c-" : "u-");
  (*s).swap(x.result());
  x.write(md, 16).flush();
  (*s).swap(x.result());
}
//______________________________________________________________________

MakeImageDl::Child* MakeImageDl::childFor(const string& url, const MD5* md,
                                          string* leafnameOut) {
  debug("childFor: %L1", url);

  msg("TODO: Maintain cache of active children; if URL requested 2nd time, "
      "add child which waits for 1st to finish");

  bool contentMdKnown = (md != 0);
  MD5 cacheMd; // Will contain md5sum of either file URL or file contents
  Base64String b64;
  if (contentMdKnown) {
    // Create leafname from md ("c" for "content")
    b64.result() = "c-";
    b64.write(*md, 16).flush();
    cacheMd = *md;
  } else {
    // Create leafname from url ("u" for "url")
    b64.result() = "u-";
    MD5Sum nameMd;
    nameMd.update(reinterpret_cast<const byte*>(url.c_str()),
                  url.length()).finish();
    b64.write(nameMd.digest(), 16).flush();
    cacheMd = nameMd;
  }
  string* leafname;
  if (leafnameOut == 0) {
    leafname = &b64.result();
  } else {
    leafnameOut->swap(b64.result());
    leafname = leafnameOut;
  }

  // Check whether file already present in cache, i.e. in tmpDir
  string filename = tmpDir();
  filename += DIRSEP;
  filename += *leafname;
  struct stat fileInfo;
  int statResult = stat(filename.c_str(), &fileInfo);
  if (statResult == 0)
    return childForCompleted(fileInfo, filename, contentMdKnown, cacheMd);

  /* statResult != 0, we assume this means "no such file or directory".
     Now check whether a download is already under way, or if a half-finished
     download was aborted earlier. */
  toggleLeafname(&filename);
  toggleLeafname(leafname);
  statResult = stat(filename.c_str(), &fileInfo);
  if (statResult == 0)
    return childForSemiCompleted(fileInfo, filename);

  /* Neither the complete nor the partial data is in the cache, so start a
     new download. */
  debug("childFor: New download to %L1", filename);
  BfstreamCounted* f = new BfstreamCounted(filename.c_str(),
                                    ios::binary|ios::in|ios::out|ios::trunc);
  if (!*f) {
    string err = subst(_("Could not open `%L1' for output: %L2"),
                       leafname, strerror(errno));
    generateError(&err);
    return 0;
  }
  auto_ptr<SingleUrl> dl(new SingleUrl(0, url));
  dl->setDestination(f, 0, 0);
  Child* c = new Child(this, &childrenVal, dl.get(), contentMdKnown,
                       cacheMd);
  dl.release();
  return c;
}
//______________________________________________________________________

// Cache contains already completed download for requested URL/md5sum
MakeImageDl::Child* MakeImageDl::childForCompleted(
    const struct stat& fileInfo, const string& filename, bool contentMdKnown,
    const MD5& cacheMd) {
  if (!S_ISREG(fileInfo.st_mode)) {
    // Something messed with the cache dir
    string err = subst(_("Invalid cache entry: `%L1' is not a file"),
                       filename);
    generateError(&err);
    return 0;
  }

  int cacheEntryAge = time(0) - fileInfo.st_mtime;
  debug("childFor: cache entry is %1 secs old", cacheEntryAge);

  cacheEntryAge = 0; // TODO - FIXME - remove this once if-mod-since impl.

  if (contentMdKnown || cacheEntryAge < CACHE_ENTRY_AGE_NOIFMODSINCE) {
    // Data with that MD5 known - no need to go on the net, imm. return it
    debug("childFor: already have %L1", filename);
    auto_ptr<CachedUrl> dl(new CachedUrl(0, filename, 0));
    Child* c = new Child(this, &childrenVal, dl.get(), contentMdKnown,
                         cacheMd);
    dl.release();
    return c;
  } else {
    /* Data for URL fetched before. If less than IF_MOD_SINCE seconds ago,
       just return it, else do an If-Modified-Since request. */
    debug("childFor: if-mod-since %L1", filename);
    Assert(false); return 0;

  }
}
//______________________________________________________________________

MakeImageDl::Child* MakeImageDl::childForSemiCompleted(
    const struct stat& fileInfo, const string& filename) {
  if (!S_ISREG(fileInfo.st_mode)) {
    // Something messed with the cache dir
    string err = subst(_("Invalid cache entry: `%L1' is not a file"),
                       filename);
    generateError(&err);
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

// frontend can be null
void MakeImageDl::childSucceeded(
    Child* childDl, DataSource::IO* /*childIo*/, DataSource::IO* frontend) {
# if DEBUG
  childDl->childSuccFail = true;
# endif
  if (frontend != 0)
    io->makeImageDl_finished(childDl->source(), frontend);

  if (dynamic_cast<SingleUrl*>(childDl->source()) == 0) return;

  // Rename u~... to u-... for SingleUrls
  string destName = tmpDir();
  destName += DIRSEP;
  appendLeafname(&destName, childDl->contentMd, childDl->md);
  string srcName(destName);
  toggleLeafname(&srcName);
  debug("mv %1 %2", srcName, destName);

  // On Windows, cannot rename open file, so ensure it is closed
  childDl->deleteSource();

  int status = rename(srcName.c_str(), destName.c_str());
  if (status != 0) {
    string destName2(destName, tmpDir().length() + 1, string::npos);
    string err = subst(_("Could not rename `%L1' to `%L2': %L3"),
                       srcName, destName2, strerror(errno));
    generateError(&err);
    return;
  }

  // Must not delete any part of JigdoIO tree while any other is still live
  if (dynamic_cast<JigdoIO*>(childDl->childIo()) == 0) delete childDl;
}

void MakeImageDl::childFailed(
    Child* childDl, DataSource::IO* /*childIo*/, DataSource::IO* frontend) {
# if DEBUG
  childDl->childSuccFail = true;
# endif
  if (frontend != 0)
    io->makeImageDl_finished(childDl->source(), frontend);

  // Delete partial output file if it is empty
  string name = tmpDir();
  name += DIRSEP;
  appendLeafname(&name, childDl->contentMd, childDl->md);
  toggleLeafname(&name);
  childDl->deleteSource();

  // FIXME: Uncomment if() once if-mod-since resume implemented
//   struct stat fileInfo;
//   if (stat(name.c_str(), &fileInfo) == 0 && fileInfo.st_size == 0) {
    debug("rm -f %1", name);
    remove(name.c_str());
//   } else {
//     debug("NO rm -f %1", name);
//   }

  // Must not delete any part of JigdoIO tree while any other is still live
  if (dynamic_cast<JigdoIO*>(childDl->childIo()) == 0) delete childDl;
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

  if (io) io->makeImageDl_haveImageSection();

  /* If the template URL is a regular URL (not a "Label:path/x" string), we
     can immediately start the .template download. */
  unsigned labelLen = JigdoConfig::findLabelColon(templateUrlVal);
  if (labelLen == 0 // relative URL, methinks
      || compat_compare(templateUrlVal, 0, 6, "http:/", 6) == 0
      || compat_compare(templateUrlVal, 0, 5, "ftp:/", 5) == 0) {
    string templ;
    Download::uriJoin(&templ, jigdoUri(), templateUrlVal);
    debug("Template: %1", templ);
  }
}

/* All .jigdo data available now - if we didn't download template above, do
   so now. */
void MakeImageDl::jigdoFinished() {
  debug("jigdoFinished");
  typedef ChildList::iterator Iter;
  Iter i = childrenVal.begin();
  while (i != childrenVal.end()) {
    Child* x = &*i;
    ++i;
    if (dynamic_cast<JigdoIO*>(x->childIo()) != 0) {
#     if DEBUG
      if (!x->childSuccFail)
        debug("childFailed()/Succeeded() not called for %1",
              x->source() ? x->source()->location() : "[deleted source]");
      x->childSuccFail = true; // Avoid failed assert
#     endif
      debug("jigdoFinished: delete %1", x);
      delete x;
    }
  }
}
//______________________________________________________________________

// map<MD5, SmartPtr<PartUrlMapping> > parts;

/* Given an URL-like string of the form "Label:some/path" or
   "http://foo/bar", return the ServerUrlMapping for "Label"/"http". If none
   exists, create one.
   @param url URL-like string
   @param colon Offset of ':' in url, must be >0
   @return new or existent mapping */
ServerUrlMapping* MakeImageDl::findOrCreateServerUrlMapping(
    const string& url, unsigned colon) {
  string label(url, 0, colon);
  ServerMap::iterator i = servers.lower_bound(label);
  if (i != servers.end() && i->first == label)
    return i->second.get(); // "Label" entry present, just return it

  // No entry for "Label" yet, need to create a dummy ServerUrlMapping
  ServerUrlMapping* s = new ServerUrlMapping();
  /* Initialize the url for label "http" with "http:"; addServer() below will
     recognize this special case. */
  servers.insert(i, make_pair(label, makeSmartPtr(s)));
  label += ':';
  s->setUrl(label);
  return s;
}
//____________________

void MakeImageDl::addPart(const MD5& md, vector<string>& value) {
  string& url = value.front();
  debug("addPart %1 -> %2", md.toString(), url);

  PartUrlMapping* p = new PartUrlMapping();
  unsigned colon = JigdoConfig::findLabelColon(url);
  if (colon == 0) {
    p->setUrl(url);
  } else {
    p->setPrepend(findOrCreateServerUrlMapping(url, colon));
    p->setUrl(url, colon + 1);
  }
  // Insert entry in "parts"
  pair<PartMap::iterator, bool> x =
    parts.insert(make_pair(md, makeSmartPtr(p)));
  if (!x.second) {
    // entry for md already present in parts, add p to its linked list
    x.first->second->insertNext(p);
  }
}
//____________________

/* For a line "Foobar=Label:some/path" in the [Servers] section:
   @param label == "Foobar"
   @param value arguments; value.front()=="Label:some/path" */
Status MakeImageDl::addServer(const string& label, vector<string>& value) {
  string& url = value.front();
  debug("addServer %1 -> %2", label, url);

  /* Create entry for "Foobar". We usually create a new ServerUrlMapping,
     except in the case where findOrCreateServerUrlMapping() has created a
     dummy entry during previous processing of a [Parts] section. */
  ServerUrlMapping* s;
  ServerMap::iterator i = servers.lower_bound(label);
  if (i == servers.end() || i->first != label) {
    // Create object and start a new linked list; add list head to "servers"
    s = new ServerUrlMapping();
    servers.insert(i, make_pair(label, makeSmartPtr(s)));
  } else {
    const string& somepath = i->second->url();
    if (!somepath.empty() && somepath[somepath.length() - 1] == ':') {
      // List head is dummy; use it directly
      s = i->second.get();
    } else {
      // Create object and add it to existing linked list
      s = new ServerUrlMapping();
      i->second->insertNext(s);
    }
  }

  /* Process the "Label:some/path" string, maybe adding a dummy
     ServerUrlMapping for "Label". */
  unsigned colon = JigdoConfig::findLabelColon(url);
  if (colon == 0) {
    s->setUrl(url);
  } else {
    ServerUrlMapping* prep = findOrCreateServerUrlMapping(url, colon);
    s->setPrepend(prep);
    s->setUrl(url, colon + 1);
    // Check whether this is a recursive definition
    UrlMapping* i = prep;
    do {
      if (i == s) { // Cycle detected
        // Break cycle, leave s in nonsensical state. Maybe also delete prep
        s->setPrepend(0);
        return FAILED;
      }
      i = i->prepend();
    } while (i != 0);
  }
  return OK;
}
