/* $Id$ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2001-2002  |  richard@
  | \/¯|  Richard Atterer          |  atterer.net
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

  Implementation of the different jigdo-file commands

*/

#include <config.h>

#include <fstream>
#include <memory>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd-jigdo.h>
#include <errno.h>

#include <compat.hh>
#include <debug.hh>
#include <jigdo-file-cmd.hh>
#include <mimestream.hh>
#include <recursedir.hh>
#include <string.hh>
//______________________________________________________________________

namespace {

#if !HAVE_WORKING_FSTREAM /* ie istream and bistream are not the same */
/* Open named file or stdin if name is "-". Store pointer to stream obj in
   dest and return it (except when it points to an object which should not be
   deleted by the caller; in this case return null). */
bistream* openForInput(bistream*& dest, const string& name) throw(Cleanup) {
  if (name == "-") {
    dest = &bcin;
    return 0;
  }
  bifstream* fdest = new bifstream(name.c_str(), ios::binary);
  dest = fdest;
  if (!*dest /*|| !fdest->is_open()*/) {
    cerr << subst(_("%1: Could not open `%2' for input: %3"),
                  binName(), name, strerror(errno)) << endl;
    throw Cleanup(3);
  }
  return dest;
}
#endif

/* Open named file or stdin if name is "-". Store pointer to stream obj in
   dest and return it (except when it points to an object which should not be
   deleted by the caller; in this case return null). */
istream* openForInput(istream*& dest, const string& name) throw(Cleanup) {
  if (name == "-") {
    /* EEEEK! There's no standard way to switch mode of cin to binary. (There
       might be an implementation-dependent way? close()ing and re-open()ing
       cin may have strange effects.) */
    dest = reinterpret_cast<istream*>(&cin);
    return 0;
  }
  ifstream* fdest = new ifstream(name.c_str(), ios::binary);
  dest = fdest;
  if (!*dest || !fdest->is_open()) {
    cerr << subst(_("%1: Could not open `%2' for input: %3"),
                  binName(), name, strerror(errno)) << endl;
    throw Cleanup(3);
  }
  return dest;
}

/* Ensure that an output file is not already present. Should use this
   for all files before openForOutput() */
int willOutputTo(const string& name, bool optForce,
                 bool errorMessage = true) {
  if (optForce) return 0;
  struct stat fileInfo;
  int err = stat(name.c_str(), &fileInfo);
  if (err == -1 && errno == ENOENT) return 0;

  if (errorMessage) {
    cerr << subst(_("%1: Output file `%2' already exists - delete it or use "
                    "--force"), binName(), name) << endl;
  }
  return 1;
}

#if !HAVE_WORKING_FSTREAM /* ie istream and bistream are not the same */
bostream* openForOutput(bostream*& dest, const string& name) throw(Cleanup) {
  if (name == "-") {
    dest = &bcout;
    return 0;
  }
  dest = new bofstream(name.c_str(), ios::binary|ios::trunc);
  if (!*dest) {
    cerr << subst(_("%1: Could not open `%2' for output: %3"),
                  binName(), name, strerror(errno)) << endl;
    throw Cleanup(3);
  }
  return dest;
}
#endif

ostream* openForOutput(ostream*& dest, const string& name) throw(Cleanup) {
  if (name == "-") {
    dest = reinterpret_cast<ostream*>(&cout); // EEEEK!
    return 0;
  } else {
    dest = new ofstream(name.c_str(), ios::binary|ios::trunc);
    if (!*dest) {
      cerr << subst(_("%1: Could not open `%2' for output: %3"),
                    binName(), name, strerror(errno)) << endl;
      throw Cleanup(3);
    }
    return dest;
  }
}

} // local namespace
//______________________________________________________________________

/* Read contents of optLabels/optUris and call addLabel() for the
   supplied cache object to set up the label mapping.
   optLabels/optUris is cleared after use. */
int JigdoFileCmd::addLabels(JigdoCache& cache) {
  int result = 0;
  string path, label, uri;

  // Create a map from label name to URI
  map<string, string> uriMap;
  for (vector<string>::iterator i = optUris.begin(), e = optUris.end();
       i != e; ++i) {
    pair<string, string> entry;
    string::size_type firstEquals = i->find('=');
    if (firstEquals == string::npos) {
      cerr << subst(_("%1: Invalid argument to --uri: `%2'"), binaryName, *i)
           << '\n';
      result = 1;
    }
    entry.first.assign(*i, 0U, firstEquals);
    entry.second.assign(*i, firstEquals + 1, string::npos);
    // Add mapping to uriMap
    msg("URI mapping: `%1' => `%2'", entry.first, entry.second);
    uriMap.insert(entry);
  }
  optUris.clear();

  // Go through list of --label arguments and add them to JigdoCache
  for (vector<string>::iterator i = optLabels.begin(), e = optLabels.end();
       i != e; ++i) {
    // Label name is everything before first '=' in argument to --label
    string::size_type firstEquals = i->find('=');
    if (firstEquals == string::npos) {
      cerr << subst(_("%1: Invalid argument to --label: `%2'"), binaryName,
                    *i) << '\n';
      result = 1;
    }
    label.assign(*i, 0U, firstEquals);
    path.assign(*i, firstEquals + 1, string::npos);
    map<string, string>::iterator m = uriMap.find(label); // Lookup
    if (m == uriMap.end()) {
      uri = "file:";
      uri += path;
      compat_swapFileUriChars(uri);
      if (uri[uri.length() - 1] != '/') uri += '/';
      ConfigFile::quote(uri);
    } else {
      uri = m->second;
    }
    cache.addLabel(path, label, uri);
  }
  optLabels.clear();
  return result;
}

/* As above, but add URIs to the beginning of the [Servers] section
   of a ConfigFile. rescan() is only necessary when changing section
   lines. */
void JigdoFileCmd::addUris(ConfigFile& config) {
  // Let ci point to the line before which the label mapping will be inserted
  ConfigFile::iterator ci = config.firstSection("Servers");
  if (ci == config.end()) {
    // No [Servers] section yet; append it at end
    config.insert(ci, string("[Servers]"));
    config.rescan();
  } else {
    ++ci;
  }

  for (vector<string>::iterator i = optUris.begin(), e = optUris.end();
       i != e; ++i) {
    // Just add it, no matter what it contains...
    config.insert(ci, *i);
  }
  optUris.clear();
  return;
}
//______________________________________________________________________

int JigdoFileCmd::makeTemplate() {
  if (imageFile.empty() || jigdoFile.empty() || templFile.empty()) {
    cerr << subst(_("%1"
      " make-template: Not all of --image, --jigdo, --template specified.\n"
      "(Attempt to deduce missing names failed.)\n"), binaryName);
    exit_tryHelp();
  }

  if (fileNames.empty()) {
    optReporter->info(_("Warning - no files specified. The template will "
                        "contain the complete image contents!"));
  }

  // Give >1 error messages if >1 output files not present, hence no "||"
  if (willOutputTo(jigdoFile, optForce)
      + willOutputTo(templFile, optForce) > 0) throw Cleanup(3);

  // Open files
  bistream* image;
  auto_ptr<bistream> imageDel(openForInput(image, imageFile));

  auto_ptr<ConfigFile> cfDel(new ConfigFile());
  ConfigFile* cf = cfDel.get();
  if (!jigdoMergeFile.empty()) { // Load file to add to jigdo output
    istream* jigdoMerge;
    auto_ptr<istream> jigdoMergeDel(openForInput(jigdoMerge,
                                                 jigdoMergeFile));
    *jigdoMerge >> *cf;
    if (jigdoMerge->bad()) {
      string err = subst(_("%1 make-template: Could not read `%2' (%3)"),
                         binaryName, jigdoMergeFile, strerror(errno));
      optReporter->error(err);
      return 3;
    }
  }
  JigdoConfig jc(jigdoFile, cfDel.release(), *optReporter);

  bostream* templ;
  auto_ptr<bostream> templDel(openForOutput(templ, templFile));
  //____________________

  JigdoCache cache(cacheFile, optCacheExpiry, readAmount, *optReporter);
  cache.setParams(blockLength, md5BlockLength);
  cache.setCheckFiles(optCheckFiles);
  if (addLabels(cache)) return 3;
  while (true) {
    try { cache.readFilenames(fileNames); } // Recurse through directories
    catch (RecurseError e) { optReporter->error(e.message); continue; }
    break;
  }
  // Create and run MkTemplate operation
  auto_ptr<MkTemplate>
    op(new MkTemplate(&cache, image, &jc, templ, *optReporter,
                      optZipQuality, readAmount, optAddImage, optAddServers,
                      optBzip2));
  op->setMatchExec(optMatchExec);
  op->setGreedyMatching(optGreedyMatching);
  size_t lastDirSep = imageFile.rfind(DIRSEP);
  if (lastDirSep == string::npos) lastDirSep = 0; else ++lastDirSep;
  string imageFileLeaf(imageFile, lastDirSep);
  lastDirSep = templFile.rfind(DIRSEP);
  if (lastDirSep == string::npos) lastDirSep = 0; else ++lastDirSep;
  string templFileLeaf(templFile, lastDirSep);
  if (op->run(imageFileLeaf, templFileLeaf)) return 3;

  // Write out jigdo file
  ostream* jigdoF;
  auto_ptr<ostream> jigdoDel(openForOutput(jigdoF, jigdoFile));
  *jigdoF << jc.configFile();
  if (jigdoF->bad()) {
    string err = subst(_("%1 make-template: Could not write `%2' (%3)"),
                       binaryName, jigdoFile, strerror(errno));
    optReporter->error(err);
    return 3;
  }

  return 0;
}
//______________________________________________________________________

int JigdoFileCmd::makeImage() {
  if (imageFile.empty() || templFile.empty()) {
    cerr << subst(_(
      "%1 make-image: Not both --image and --template specified.\n"
      "(Attempt to deduce missing names failed.)\n"), binaryName);
    exit_tryHelp();
  }

  if (imageFile != "-" && willOutputTo(imageFile, optForce) > 0) return 3;
  JigdoCache cache(cacheFile, optCacheExpiry, readAmount, *optReporter);
  cache.setParams(blockLength, md5BlockLength);
  while (true) {
    try { cache.readFilenames(fileNames); } // Recurse through directories
    catch (RecurseError e) { optReporter->error(e.message); continue; }
    break;
  }

  string imageTmpFile;
  if (imageFile != "-") {
    imageTmpFile = imageFile;
    imageTmpFile += EXTSEPS"tmp";
  }

  bistream* templ;
  auto_ptr<bistream> templDel(openForInput(templ, templFile));

  try {
    return JigdoDesc::makeImage(&cache, imageFile, imageTmpFile, templFile,
      templ, optForce, *optReporter, readAmount, optMkImageCheck);
  } catch (Error e) {
    string err = binaryName; err += " make-image: "; err += e.message;
    optReporter->error(err);
    return 3;
  }
}
//______________________________________________________________________

int JigdoFileCmd::listTemplate() {
  if (templFile.empty()) {
    cerr << subst(_("%1 list-template: --template not specified.\n"),
                  binaryName);
    exit_tryHelp();
  }
  if (templFile == "-") {
    cerr << subst(_("%1 list-template: Sorry, cannot read from standard "
                    "input.\n"), binaryName);
    exit_tryHelp();
  }

  if (JigdoFileCmd::optHex) Base64String::hex = true;

  // Open file
  bistream* templ;
  auto_ptr<bistream> templDel(openForInput(templ, templFile));

  if (JigdoDesc::isTemplate(*templ) == false)
    optReporter->info(
        _("Warning: This does not seem to be a template file"));

  JigdoDescVec contents;
  try {
    JigdoDesc::seekFromEnd(*templ);
    *templ >> contents;
    contents.list(cout);
    if (!*templ) {
      string err = subst(_("%1 list-template: %2"), binaryName,
                         strerror(errno));
      optReporter->error(err);
      return 3;
    }
  } catch (JigdoDescError e) {
    string err = subst(_("%1: %2"), binaryName, e.message);
    optReporter->error(err);
    return 3;
  }
  return 0;
}
//______________________________________________________________________

int JigdoFileCmd::verifyImage() {
  if (imageFile.empty() || templFile.empty()) {
    cerr << subst(_(
      "%1 verify: Not both --image and --template specified.\n"
      "(Attempt to deduce missing names failed.)\n"), binaryName);
    exit_tryHelp();
  }

  bistream* image;
  auto_ptr<bistream> imageDel(openForInput(image, imageFile));

  JigdoDescVec contents;
  JigdoDesc::ImageInfo* info;
  try {
    bistream* templ;
    auto_ptr<bistream> templDel(openForInput(templ, templFile));

    if (JigdoDesc::isTemplate(*templ) == false)
      optReporter->info(
          _("Warning: This does not seem to be a template file"));

    JigdoDesc::seekFromEnd(*templ);
    *templ >> contents;
    if (!*templ) {
      string err = subst(_("%1 verify: %2"), binaryName, strerror(errno));
      optReporter->error(err);
      return 3;
    }
    info = dynamic_cast<JigdoDesc::ImageInfo*>(contents.back());
    if (info == 0) {
      string err = subst(_("%1 verify: Invalid template data - "
                           "corrupted file?"), binaryName);
      optReporter->error(err);
      return 3;
    }
  } catch (JigdoDescError e) {
    string err = subst(_("%1: %2"), binaryName, e.message);
    optReporter->error(err);
    return 3;
  }

  MD5Sum md; // MD5Sum of image
  md.updateFromStream(*image, info->size(), readAmount, *optReporter);
  md.finish();
  if (*image) {
    image->get();
    if (image->eof() && md == info->md5()) {
      optReporter->info(_("OK: Checksums match, image is good!"));
      return 0;
    }
  }
  optReporter->error(_(
      "ERROR: Checksums do not match, image might be corrupted!"));
  return 2;
}
//______________________________________________________________________

/* Look up a query (e.g. "MyServer:foo/path/bar") in the JigdoConfig
   mapping. Returns true if something was found, and prints out all
   resulting URIs. */
bool JigdoFileCmd::printMissing_lookup(JigdoConfig& jc, const string& query,
                                       bool printAll) {
  JigdoConfig::Lookup l(jc, query);
  string uri;
  if (!l.next(uri)) return false;
  do {
    // Omit "file:" when printing
    if (uri[0] == 'f' && uri[1] == 'i' && uri[2] == 'l'
        && uri[3] == 'e' && uri[4] == ':') {
      string nativeFilename(uri, 5);
      compat_swapFileUriChars(nativeFilename);
      cout << nativeFilename << endl;
    } else {
      cout << uri << '\n';
    }
  } while (printAll && l.next(uri));
  return true;
}
//______________________________

int JigdoFileCmd::printMissing(Command command) {
  if (imageFile.empty() || jigdoFile.empty() || templFile.empty()) {
    cerr << subst(_(
      "%1 make-template: Not all of --image, --jigdo, --template specified.\n"
      "(Attempt to deduce missing names failed.)\n"), binaryName);
    exit_tryHelp();
  }

  bistream* templ;
  auto_ptr<bistream> templDel(openForInput(templ, templFile));

  string imageTmpFile;
  if (imageFile != "-") {
    imageTmpFile = imageFile;
    imageTmpFile += EXTSEPS"tmp";

    // If image file exists, assume that it is complete; print nothing
    struct stat fileInfo;
    int err = stat(imageFile.c_str(), &fileInfo);
    if (err == 0) return 0;
  }

  // Read .jigdo file
  istream* jigdo;
  auto_ptr<istream> jigdoDel(openForInput(jigdo, jigdoFile));
  auto_ptr<ConfigFile> cfDel(new ConfigFile());
  ConfigFile* cf = cfDel.get();
  *jigdo >> *cf;
  JigdoConfig jc(jigdoFile, cfDel.release(), *optReporter);
  // Add any mappings specified on command line
  if (!optUris.empty()) {
    addUris(jc.configFile());
    jc.rescan();
  }

  set<MD5> sums;
  try {
    JigdoDesc::listMissing(sums, imageTmpFile, templFile, templ,
                           *optReporter);
  } catch (Error e) {
    string err = subst(_("%1 print-missing: %2"), binaryName, e.message);
    optReporter->error(err);
    return 3;
  }
  //____________________

  string partsSection = "Parts";
  switch (command) {

  case PRINT_MISSING: {
    // To list just the first URI
    for (set<MD5>::iterator i = sums.begin(), e = sums.end(); i != e; ++i) {
      Base64String m;
      m.write(i->sum, 16).flush();
      string& s(m.result());

      vector<string> words;
      size_t off;
      bool found = false;
      for (ConfigFile::Find f(cf, partsSection, s, &off);
           !f.finished(); off = f.next()) {
        // f.section() points to "[section]" line, or end() if 0th section
        // f.label()   points to "label=..." line, or end() if f.finished()
        // off is offset of part after "label=", or 0
        words.clear();
        ConfigFile::split(words, *f.label(), off);
        // Ignore everything but the first word
        if (printMissing_lookup(jc, words[0], false)) { found = true; break;}
      }
      if (!found) {
        /* No mapping found in [Parts] (this shouldn't happen) - create
           fake "MD5sum:<md5sum>" label line */
        s.insert(0, "MD5Sum:");
        printMissing_lookup(jc, s, false);
      }
    }
    break;
  }

  case PRINT_MISSING_ALL: {
    // To list all URIs for each missing file, separated by empty lines:
    for (set<MD5>::iterator i = sums.begin(), e = sums.end(); i != e; ++i){
      Base64String m;
      m.write(i->sum, 16).flush();
      string& s(m.result());

      vector<string> words;
      size_t off;
      for (ConfigFile::Find f(cf, partsSection, s, &off);
           !f.finished(); off = f.next()) {
        // f.section() points to "[section]" line, or end() if 0th section
        // f.label()   points to "label=..." line, or end() if f.finished()
        // off is offset of part after "label=", or 0
        words.clear();
        ConfigFile::split(words, *f.label(), off);
        // Ignore everything but the first word
        printMissing_lookup(jc, words[0], true);
      }
      // Last resort: "MD5sum:<md5sum>" label line
      s.insert(0, "MD5Sum:");
      printMissing_lookup(jc, s, true);
      cout << endl;
    }
    break;
  }

  default:
    Paranoid(false);

  } // end switch()

  return 0;
}
//______________________________________________________________________

// Enter all file arguments into the cache
int JigdoFileCmd::scanFiles() {
  if (cacheFile.empty()) {
    cerr << subst(_("%1 scan: Please specify a --cache file.\n"),
                  binaryName);
    exit_tryHelp();
  }

  JigdoCache cache(cacheFile, optCacheExpiry, readAmount, *optReporter);
  cache.setParams(blockLength, md5BlockLength);
  if (addLabels(cache)) return 3;
  while (true) {
    try { cache.readFilenames(fileNames); } // Recurse through directories
    catch (RecurseError e) { optReporter->error(e.message); continue; }
    break;
  }
  JigdoCache::iterator ci = cache.begin(), ce = cache.end();
  if (optScanWholeFile) {
    // Cause entire file to be read
    while (ci != ce) { ci->getMD5Sum(&cache); ++ci; }
  } else {
    // Only cause first md5 block to be read; not scanning the whole file
    while (ci != ce) { ci->getSums(&cache, 0); ++ci; }
  }
  return 0;
  // Cache data is written out when the JigdoCache is destroyed
}
//______________________________________________________________________

/* Print MD5 checksums of arguments like md5sum(1), but using our
   Base64-like encoding for the checksum, not hexadecimal like
   md5sum(1). Additionally, try to make use of the cache, and only
   print out the part of any filename following any "//". This is
   actually very similar to scanFiles() above. */
int JigdoFileCmd::md5sumFiles() {
  JigdoCache cache(cacheFile, optCacheExpiry, readAmount, *optReporter);
  cache.setParams(blockLength, md5BlockLength);
  cache.setCheckFiles(optCheckFiles);
  while (true) {
    try { cache.readFilenames(fileNames); } // Recurse through directories
    catch (RecurseError e) { optReporter->error(e.message); continue; }
    break;
  }

  if (JigdoFileCmd::optHex) Base64String::hex = true;

  JigdoCache::iterator ci = cache.begin(), ce = cache.end();
  while (ci != ce) {
    Base64String m;
    // Causes whole file to be read
    const MD5Sum* md = ci->getMD5Sum(&cache);
    if (md != 0) {
      m.write(md->digest(), 16).flush();
      string& s(m.result());
      s += "  ";
      if (ci->getPath() == "/") s += '/';
      s += ci->leafName();
      // Output checksum line
      optReporter->coutInfo(s);
    }
    ++ci;
  }
  return 0;
  // Cache data is written out when the JigdoCache is destroyed
}
