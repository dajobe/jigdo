/* $Id$ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2000-2002  |  richard@
  | \/¯|  Richard Atterer          |  atterer.net
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

  Command line utility to create .jigdo and .template files

*/

#include <config.h>

#include <fstream>
#include <glibc-getopt.h>
#include <errno.h>
#if ENABLE_NLS
#  include <locale.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <zlib.h>

#include <compat.hh>
#include <configfile.hh>
#include <debug.hh>
#include <jigdo-file-cmd.hh>
#include <jigdoconfig.hh>
#include <log.hh>
#include <mkimage.hh>
#include <mktemplate.hh>
#include <recursedir.hh>
#include <scan.hh>
#include <string.hh>
//______________________________________________________________________

RecurseDir JigdoFileCmd::fileNames;
string JigdoFileCmd::imageFile;
string JigdoFileCmd::jigdoFile;
string JigdoFileCmd::templFile;
string JigdoFileCmd::jigdoMergeFile;
string JigdoFileCmd::cacheFile;
size_t JigdoFileCmd::optCacheExpiry = 60*60*24*30; // default: 30 days
vector<string> JigdoFileCmd::optLabels;
vector<string> JigdoFileCmd::optUris;
size_t JigdoFileCmd::blockLength    =   1*1024U;
size_t JigdoFileCmd::md5BlockLength = 128*1024U - 55;
size_t JigdoFileCmd::readAmount     = 128*1024U;
int JigdoFileCmd::optZipQuality = Z_BEST_COMPRESSION;
bool JigdoFileCmd::optForce = false;
bool JigdoFileCmd::optMkImageCheck = true;
bool JigdoFileCmd::optAddImage = true;
bool JigdoFileCmd::optAddServers = true;
bool JigdoFileCmd::optHex = false;
string JigdoFileCmd::optDebug;
AnyReporter* JigdoFileCmd::optReporter = 0;
string JigdoFileCmd::optMatchExec;
#if !WINDOWS
string JigdoFileCmd::binaryName; // of the program
#endif
//______________________________________________________________________

namespace {

// Absolute minimum for --min-length (i.e. blockLength), in bytes
const size_t MINIMUM_BLOCKLENGTH = 256;

char optHelp = '\0';
bool optVersion = false;

// Return value of main(), for "delayed error exit"
int returnValue = 0;

// Size of image file (zero if stdin), for percentage progress reports
static uint64 imageSize;
//______________________________________________________________________

/// Progress report class that writes informational messages to cerr
class MyProgressReporter : public AnyReporter {
public:
  MyProgressReporter(bool prog) : printProgress(prog) { }

  // Length of "100% 9999k/9999k " part before "scanning ..." etc message
  static const unsigned PROGRESS_WIDTH = 23;

  virtual void error(const string& message) { print(message); }
  virtual void info(const string& message) { print(message); }
  virtual void coutInfo(const string& message);
  virtual void scanningFile(const FilePart* file, uint64 offInFile) {
    if (!printProgress) return;
    string m;
    append(m, 100 * offInFile / file->size(), 3); // 3
    m += '%'; // 1 char
    append(m, offInFile / 1024, 8); // >= 8 chars
    m += "k/"; // 2 chars
    append(m, file->size() / 1024); // want >= 8 chars
    m += 'k'; // 1 char
    if (m.size() < 3+1+8+2+8+1)
      m += "          " + 10 - (3+1+8+2+8+1 - m.size());
    Paranoid(m.length() == PROGRESS_WIDTH);
    m += _("scanning");
    m += " `";
    m += file->leafName();
    m += '\'';
    print(m, false);
  }
  virtual void scanningImage(uint64 offset) {
    if (!printProgress) return;
    string m;
    if (imageSize != 0) {
      append(m, 100 * (totalAll + offset) / (totalAll + imageSize), 3); // 3
      m += '%'; // 1 char
    } else {
      m += "    ";
    }
    append(m, offset / 1024, 8); // >= 8 chars
    m += 'k'; // 1 char
    if (imageSize != 0) {
      m += '/'; // 1 char
      append(m, imageSize / 1024); // want >= 8 chars
      m += 'k'; // 1 char
    }
    if (m.size() < 3+1+8+1+1+8+1)
      m += "          " + 10 - (3+1+8+1+1+8+1 - m.size());
    Paranoid(m.length() == PROGRESS_WIDTH);
    m += _("scanning image");
    print(m, false);
  }
  virtual void readingMD5(uint64 offInStream, uint64 size) {
    if (!printProgress) return;
    string m;
    append(m, 100 * offInStream / size, 3); // 3
    m += '%'; // 1 char
    append(m, offInStream / 1024, 8); // >= 8 chars
    m += "k/"; // 2 chars
    append(m, size / 1024); // want >= 8 chars
    m += 'k'; // 1 char
    if (m.size() < 3+1+8+2+8+1)
      m += "          " + 10 - (3+1+8+2+8+1 - m.size());
    Paranoid(m.length() == PROGRESS_WIDTH);
    m += _("verifying image");
    print(m, false);
  }
  virtual void writingImage(uint64 written, uint64 totalToWrite,
                            uint64 imgOff, uint64 imgSize) {
    if (!printProgress) return;
    string m;
    append(m, 100 * written / totalToWrite, 3); // 3
    m += '%'; // 1 char
    append(m, imgOff / 1024, 8); // >= 8 chars
    m += "k/"; // 2 chars
    append(m, imgSize / 1024); // want >= 8 chars
    m += 'k'; // 1 char
    if (m.size() < 3+1+8+2+8+1)
      m += "          " + 10 - (3+1+8+2+8+1 - m.size());
    Paranoid(m.length() == PROGRESS_WIDTH);
    m += _("writing image");
    print(m, false);
  }
  virtual void abortingScan() {
    string m(_("Error scanning image - abort"));
    print(m);
  }
  virtual void matchFound(const FilePart* file, uint64 offInImage) {
    string m = subst(_("Match of `%1' at offset %2"),
                     file->leafName(), offInImage);
    print(m);
  }
  virtual void finished(uint64 imageSize) {
    if (!printProgress) return;
    string m = subst(_("Finished - image size is %1 bytes."), imageSize);
    print(m);
  }
private:
  static void print(string s, bool addNewline = true);
  bool printProgress;
  static string prevLine;
  static uint64 totalAll;
};
//________________________________________

string MyProgressReporter::prevLine;
uint64 MyProgressReporter::totalAll = 0;

/* Print the string to stderr. Repeatedly overwrite the same line with
   different progress reports if requested with addNewline==false. If
   the tail of the message is the same as a previous message printed
   on the same line, do not print the tail. */
void MyProgressReporter::print(string s, bool addNewline) {
  size_t screenWidth = static_cast<size_t>(ttyWidth()) - 1;
  // For progress messages, truncate if too long
  if (!addNewline && screenWidth != 0 && s.length() > screenWidth) {
    string::size_type tick = s.find("`", PROGRESS_WIDTH);
    if (tick != string::npos)
      s.replace(tick + 1, s.length() - screenWidth + 3, "...", 3);
    if (s.length() > screenWidth) {
      s.resize(screenWidth);
      s[screenWidth - 1] = '$';
    }
  }

  if (s.size() != prevLine.size()) {
    // Print new message, maybe pad with spaces to overwrite rest of old one
    cerr << s;
    if (s.size() < prevLine.size()) {
      size_t nrSpaces = prevLine.size() - s.size();
      while (nrSpaces >= 10) { cerr << "          "; nrSpaces -= 10; }
      if (nrSpaces > 0) cerr << ("         " + 9 - nrSpaces);
    }
  } else {
    /* Same length as previous - to reduce cursor flicker, only redraw
       as much as necessary */
    int i = s.size() - 1;
    while (i >= 0 && s[i] == prevLine[i]) --i;
    for (int j = 0; j <= i; ++j) cerr << s[j];
  }

  // Should the message just printed be overwritten on next call?
  if (addNewline) {
    // No, leave it visible
    cerr << endl;
    prevLine = "";
  } else {
    // Yes, overwrite with next message
    cerr << '\r';
    prevLine = s;
  }
}
//________________________________________

/* If stdout is redirected to a file, the file should just contain the
   progress reports, none of the padding space chars. */
void MyProgressReporter::coutInfo(const string& message) {
  cout << message;
  if (message.size() < prevLine.size()) {
    size_t nrSpaces = prevLine.size() - message.size();
    while (nrSpaces >= 10) { cerr << "          "; nrSpaces -= 10; }
    if (nrSpaces > 0) cerr << ("         " + 9 - nrSpaces);
  }
  cout << endl;
  cerr << '\r';
  /* Ensure good-looking progress reports even if cout goes to a file
     and cerr to the terminal. */
  if (message.size() > prevLine.size())
    prevLine.resize(message.size(), '\0');
  else
    prevLine[prevLine.size() - 1] = '\0';
}
//________________________________________

/// Progress report class that writes informational messages to cerr
class MyGrepProgressReporter : public AnyReporter {
public:
  // Default error()/info() will print to cerr
  virtual void matchFound(const FilePart* file, uint64 offInImage) {
    cout << offInImage << ' ' << file->getPath() << file->leafName() << endl;
  }
};
//________________________________________

/// Progress report class that writes informational messages to cerr
class MyQuietProgressReporter : public AnyReporter {
  virtual void info(const string&) { }
};
//________________________________________

MyProgressReporter reporterDefault(true);
MyProgressReporter reporterNoprogress(false);
MyGrepProgressReporter reporterGrep;
MyQuietProgressReporter reporterQuiet;
//______________________________________________________________________

inline void printUsage(bool detailed, size_t blockLength,
                       size_t md5BlockLength, size_t readAmount) {
  if (detailed) {
    cout << _(
    "\n"
    "Copyright (C) 2001-2003 Richard Atterer <http://atterer.net>\n"
    "This program is free software; you can redistribute it and/or modify\n"
    "it under the terms of the GNU General Public License, version 2. See\n"
    "the file COPYING or <http://www.gnu.org/copyleft/gpl.html> for details.\n"
    "\n");
  }
  cout << subst(_(
    "\n"
    "Usage: %1 COMMAND [OPTIONS] [FILES...]\n"
    "Commands:\n"
    "  make-template mt Create template and jigdo from image and files\n"
    "  make-image mi    Recreate image from template and files (can merge\n"
    "                   files in >1 steps, uses `IMG%2tmp' for --image=IMG)\n"
    "  print-missing pm After make-image, print files still missing for\n"
    "                   the image to be completely recreated\n"),
    binName(), EXTSEPS);
  if (detailed) cout << _(
    "  print-missing-all pma\n"
    "                   Print all URIs for each missing file\n"
    "  scan sc          Update cache with information about supplied files\n");
  cout << _(
    "  verify ver       Check whether image matches checksum from template\n"
    "  md5sum md5       Print MD5 checksums similar to md5sum(1)\n");
  if (detailed) {
    cout << _(
    "  list-template ls Print low-level listing of contents of template\n"
    "                   data or tmp file\n");
  }
  cout << subst(_(
    "\n"
    "Important options:\n"
    "  -i  --image=FILE Output/input filename for image file\n"
    "  -j  --jigdo=FILE Input/output filename for jigdo file\n"
    "  -t  --template=FILE\n"
    "                   Input/output filename for template file\n"
    "  -T  --files-from=FILE\n"
    "                   Read further filenames from FILE (`-' for stdin)\n"
    "  -r  --report=default|noprogress|quiet|grep\n"
    "                   Control format of status reports to stderr (or\n"
    "                   stdout in case of `grep')\n"
    "  -f  --force      Silently delete existent output files\n"
    "      --label Label=%1%2path\n"
    "                   [make-template] Replace name of input file\n"
    "                   `%1%2path%3a%2file%4txt' (note the `%3') with\n"
    "                   `Label:a/file%4txt' in output jigdo\n"
    "      --uri Label=http://www.site.com\n"
    "                   [make-template] Add mapping from Label to given\n"
    "                   URI instead of default `file:' URI\n"
    "                   [print-missing] Override mapping in input jigdo\n"
    "  -0 to -9         Set amount of compression in output template\n"
    "      --cache=FILE Store/reload information about any files scanned\n"),
    (WINDOWS ? "C:" : ""), DIRSEPS, SPLITSEP, EXTSEPS);
  if (detailed) {
    cout << _(
      "      --no-cache   Do not cache information about scanned files\n"
      "      --cache-expiry=SECONDS[h|d|w|m|y]\n"
      "                   Remove cache entries if last access was longer\n"
      "                   ago than given amount of time [default 30 days]\n"
      "  -h  --help       Output short help\n"
      "  -H  --help-all   Output this help\n");
  } else {
    cout << _("  -h  --help       Output this help\n"
              "  -H  --help-all   Output more detailed help\n");
  }
  cout << _("  -v  --version    Output version info") << endl;
  if (detailed) {
    cout << subst(_(
    "\n"
    "Further options: (can append 'k', 'M', 'G' to any BYTES argument)\n"
    "  --merge=FILE     [make-template] Add FILE contents to output jigdo\n"
    "  --no-force       Do not delete existent output files [default]\n"
    "  --min-length=BYTES [default %1]\n"
    "                   [make-template] Minimum length of files to search\n"
    "                   for in image data\n"
    "  --md5-block-size=BYTES [default %2]\n"
    "                   Uninteresting internal parameter -\n"
    "                   jigdo-file enforces: min-length < md5-block-size\n"
    "  --readbuffer=BYTES [default %3k]\n"
    "                   Amount of data to read at a time\n"
    "  --check-files [default]\n"
    "                   [make-image] Verify checksum of files written to\n"
    "                   image\n"
    "  --no-check-files [make-image] Do not verify checksums\n"
    "  --image-section [default]\n"
    "  --no-image-section\n"
    "  --servers-section [default]\n"
    "  --no-servers-section\n"
    "                   [make-template] When creating the jigdo file, do\n"
    "                   or do not add the sections `[Image]' or `[Servers]'\n"
    "  --debug[=all|=UNIT1,UNIT2...|=help]\n"
    "                   Print debugging information for all units, or for\n"
    "                   specified units, or print list of units.\n"
    "                   Can use `~', e.g. `all,~libwww'\n"
    "  --no-debug       No debugging info [default]\n"
    "  --match-exec=CMD [make-template] Execute command when files match\n"
    "                   CMD is passed to a shell, with environment set up:\n"
    "                   LABEL, LABELPATH, MATCHPATH, LEAF, MD5SUM, FILE\n"
    "                   e.g. 'mkdir -p \"${LABEL:-.}/$MATCHPATH\" && ln -f \"$FILE\" \"${LABEL:-.}/$MATCHPATH$LEAF\"'\n"
    "  --no-hex [default]\n"
    "  --hex            [md5sum, list-template] Output checksums in \n"
    "                   hexadecimal, not Base64\n"),
    blockLength, md5BlockLength, readAmount / 1024) << endl;
  }
  return;
}
//______________________________________________________________________

/* Like atoi(), but tolerate exactly one suffix 'k', 'K', 'm', 'M',
   'g' or 'G' for kilo, mega, giga */
size_t scanMemSize(const char* str) {
  const char* s = str;
  size_t x = 0;
  while (*s >= '0' && *s <= '9') {
    x = 10 * x + *s - '0';
    ++s;
  }
  switch (*s) { // Fallthrough mania!
  case 'g': case 'G':
    x = x * 1024;
  case 'm': case 'M':
    x = x * 1024;
  case 'k': case 'K':
    x = x * 1024;
    if (*++s == '\0') return x;
  default:
    cerr << subst(_("%1: Invalid size specifier `%2'"), binName(), str)
         << endl;
    throw Cleanup(3);
  case '\0': return x;
  }
}

/* Like atoi(), but tolerate exactly one suffix 'h', 'd', 'w', 'm',
   'y' for hours, days, weeks, months, years. Returns seconds. Special
   value "off" results in 0 to be returned. */
size_t scanTimespan(const char* str) {
  if (strcmp(str, "off") == 0) return 0;
  const char* s = str;
  size_t x = 0;
  while (*s >= '0' && *s <= '9') {
    x = 10 * x + *s - '0';
    ++s;
  }
  switch (*s) {
  case 'h': case 'H': x = x * 60 * 60; ++s; break;
  case 'd': case 'D': x = x * 60 * 60 * 24; ++s; break;
  case 'w': case 'W': x = x * 60 * 60 * 24 * 7; ++s; break;
  case 'm': case 'M': x = x * 60 * 60 * 24 * 30; ++s; break;
  case 'y': case 'Y': x = x * 60 * 60 * 24 * 365; ++s; break;
  }
  if (*s == '\0') return x;
  cerr << subst(_("%1: Invalid time specifier `%2'"), binName(), str)
       << endl;
  throw Cleanup(3);
}
//______________________________________________________________________

/* Try creating a filename in dest by stripping any file extension
   from source and appending ext.
   Only deduceName() should call deduceName2(). */
void deduceName2(string& dest, const char* ext, const string& src) {
  Paranoid(dest.empty());
  string::size_type lastDot = src.find_last_of(EXTSEP);
  if (lastDot != string::npos) {
    if (src.find_first_of(DIRSEP, lastDot + 1) != string::npos)
      lastDot = string::npos;
  }
  dest.assign(src, 0U, lastDot);
  dest += ext;
  if (dest == src) dest = "";
}

inline void deduceName(string& dest, const char* ext, const string& src) {
  if (!dest.empty()) return;
  deduceName2(dest, ext, src);
}
//______________________________________________________________________

enum {
  LONGOPT_BUFSIZE = 0x100, LONGOPT_NOFORCE, LONGOPT_MINSIZE,
  LONGOPT_MD5SIZE, LONGOPT_MKIMAGECHECK, LONGOPT_NOMKIMAGECHECK,
  LONGOPT_LABEL, LONGOPT_URI, LONGOPT_ADDSERVERS, LONGOPT_NOADDSERVERS,
  LONGOPT_ADDIMAGE, LONGOPT_NOADDIMAGE, LONGOPT_NOCACHE, LONGOPT_CACHEEXPIRY,
  LONGOPT_MERGE, LONGOPT_HEX, LONGOPT_NOHEX, LONGOPT_DEBUG, LONGOPT_NODEBUG,
  LONGOPT_MATCHEXEC
};

// Deal with command line switches
JigdoFileCmd::Command JigdoFileCmd::cmdOptions(int argc, char* argv[]) {
# if !WINDOWS
  binaryName = argv[0];
# endif
  bool error = false;
  optReporter = &reporterDefault;

  while (true) {
    static const struct option longopts[] = {
      { "cache",              required_argument, 0, 'c' },
      { "cache-expiry",       required_argument, 0, LONGOPT_CACHEEXPIRY },
      { "check-files",        no_argument,       0, LONGOPT_MKIMAGECHECK },
      { "debug",              optional_argument, 0, LONGOPT_DEBUG },
      { "files-from",         required_argument, 0, 'T' }, // "-T" like tar's
      { "force",              no_argument,       0, 'f' },
      { "help",               no_argument,       0, 'h' },
      { "help-all",           no_argument,       0, 'H' },
      { "hex",                no_argument,       0, LONGOPT_HEX },
      { "image",              required_argument, 0, 'i' },
      { "image-section",      no_argument,       0, LONGOPT_ADDIMAGE },
      { "jigdo",              required_argument, 0, 'j' },
      { "label",              required_argument, 0, LONGOPT_LABEL },
      { "md5-block-size",     required_argument, 0, LONGOPT_MD5SIZE },
      { "match-exec",         required_argument, 0, LONGOPT_MATCHEXEC },
      { "merge",              required_argument, 0, LONGOPT_MERGE },
      { "min-length",         required_argument, 0, LONGOPT_MINSIZE },
      { "no-cache",           no_argument,       0, LONGOPT_NOCACHE },
      { "no-check-files",     no_argument,       0, LONGOPT_NOMKIMAGECHECK },
      { "no-debug",           no_argument,       0, LONGOPT_NODEBUG },
      { "no-force",           no_argument,       0, LONGOPT_NOFORCE },
      { "no-hex",             no_argument,       0, LONGOPT_NOHEX },
      { "no-image-section",   no_argument,       0, LONGOPT_NOADDIMAGE },
      { "no-servers-section", no_argument,       0, LONGOPT_NOADDSERVERS },
      { "readbuffer",         required_argument, 0, LONGOPT_BUFSIZE },
      { "report",             required_argument, 0, 'r' },
      { "servers-section",    no_argument,       0, LONGOPT_ADDSERVERS },
      { "template",           required_argument, 0, 't' },
      { "uri",                required_argument, 0, LONGOPT_URI },
      { "version",            no_argument,       0, 'v' },
      { 0, 0, 0, 0 }
    };

    int c = getopt_long(argc, argv, "0123456789hHvT:i:j:t:c:fr:",
                        longopts, 0);
    if (c == -1) break;

    switch (c) {
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
      optZipQuality = c - '0'; break;
    case 'h': case 'H': optHelp = c; break;
    case 'v': optVersion = true; break;
    case 'T': fileNames.addFilesFrom(
                strcmp(optarg, "-") == 0 ? "" : optarg); break;
    case 'i': imageFile = optarg; break;
    case 'j': jigdoFile = optarg; break;
    case 't': templFile = optarg; break;
    case LONGOPT_MERGE: jigdoMergeFile = optarg; break;
    case 'c': cacheFile = optarg; break;
    case LONGOPT_NOCACHE: cacheFile.erase(); break;
    case LONGOPT_CACHEEXPIRY: optCacheExpiry = scanTimespan(optarg); break;
    case 'f': optForce = true; break;
    case LONGOPT_NOFORCE: optForce = false; break;
    case LONGOPT_MINSIZE:    blockLength = scanMemSize(optarg); break;
    case LONGOPT_MD5SIZE: md5BlockLength = scanMemSize(optarg); break;
    case LONGOPT_BUFSIZE:     readAmount = scanMemSize(optarg); break;
    case 'r':
      if (strcmp(optarg, "default") == 0) {
        optReporter = &reporterDefault;
      } else if (strcmp(optarg, "noprogress") == 0) {
        optReporter = &reporterNoprogress;
      } else if (strcmp(optarg, "quiet") == 0) {
        optReporter = &reporterQuiet;
      } else if (strcmp(optarg, "grep") == 0) {
        optReporter = &reporterGrep;
      } else {
        cerr << subst(_("%1: Invalid argument to --report (allowed: "
                        "default noprogress quiet grep)"), binName())
             << '\n';
        error = true;
      }
      break;
    case LONGOPT_MKIMAGECHECK: optMkImageCheck = true; break;
    case LONGOPT_NOMKIMAGECHECK: optMkImageCheck = false; break;
    case LONGOPT_ADDSERVERS: optAddServers = true; break;
    case LONGOPT_NOADDSERVERS: optAddServers = false; break;
    case LONGOPT_ADDIMAGE: optAddImage = true; break;
    case LONGOPT_NOADDIMAGE: optAddImage = false; break;
    case LONGOPT_LABEL: optLabels.push_back(string(optarg)); break;
    case LONGOPT_URI: optUris.push_back(string(optarg)); break;
    case LONGOPT_HEX: optHex = true; break;
    case LONGOPT_NOHEX: optHex = false; break;
    case LONGOPT_DEBUG:
      if (optarg) optDebug = optarg; else optDebug = "all";
      break;
    case LONGOPT_NODEBUG: optDebug.erase(); break;
    case LONGOPT_MATCHEXEC: optMatchExec = optarg; break;
    case '?': error = true;
    case ':': break;
    default:
      msg("getopt returned %1", static_cast<int>(c));
      break;
    }
  }

  if (error) exit_tryHelp();

  if (optHelp != '\0' || optVersion) {
    if (optVersion) cout << "jigdo-file version " JIGDO_VERSION << endl;
    if (optHelp != '\0') printUsage(optHelp == 'H', blockLength,
                                    md5BlockLength, readAmount);
    throw Cleanup(0);
  }

# if WINDOWS
  Logger::scanOptions(optDebug, binName());
# else
  Logger::scanOptions(optDebug, binName().c_str());
# endif
  //______________________________

  // Silently correct invalid blockLength/md5BlockLength args
  if (blockLength < MINIMUM_BLOCKLENGTH) blockLength = MINIMUM_BLOCKLENGTH;
  if (blockLength >= md5BlockLength) md5BlockLength = blockLength + 1;
  // Round to next k*64+55 for efficient MD5 calculation
  md5BlockLength = ((md5BlockLength + 63 - 55) & ~63U) + 55;

  Paranoid(blockLength >= MINIMUM_BLOCKLENGTH
           && blockLength < md5BlockLength);
  //______________________________

  // Complain if name of command isn't there
  if (optind >= argc) {
    cerr << subst(_("%1: Please specify a command"), binName()) << '\n';
    exit_tryHelp();
  }

  // Find Command code corresponding to command string on command line :)
  Command result;
  {
    const char* command = argv[optind++];
    struct CodesEntry { char* name; Command code; };
    const CodesEntry codes[] = {
      { "make-template",     MAKE_TEMPLATE },
      { "mt",                MAKE_TEMPLATE },
      { "make-image",        MAKE_IMAGE },
      { "mi",                MAKE_IMAGE },
      { "print-missing",     PRINT_MISSING },
      { "pm",                PRINT_MISSING },
      { "print-missing-all", PRINT_MISSING_ALL },
      { "pma",               PRINT_MISSING_ALL },
      { "verify",            VERIFY },
      { "ver",               VERIFY },
      { "scan",              SCAN },
      { "sc",                SCAN },
      { "list-template",     LIST_TEMPLATE },
      { "ls",                LIST_TEMPLATE },
      { "md5sum",            MD5SUM },
      { "md5",               MD5SUM }
    };

    const CodesEntry *c = codes;
    const CodesEntry *end = codes+sizeof(codes)/sizeof(CodesEntry);
    while (true) {
      if (strcmp(command, c->name) == 0) {
        result = c->code;
        break;
      }
      ++c;
      if (c == end) {
        string allCommands;
        for (const CodesEntry *c = codes,
              *end = codes+sizeof(codes)/sizeof(CodesEntry); c != end; ++c) {
          allCommands += ' ';
          allCommands += c->name;
        }
        cerr << subst(_("%1: Invalid command `%2'\n(Must be one of:%3)"),
                      binName(), command, allCommands) << '\n';
        exit_tryHelp();
      }
    }
  }
  //____________________

  while (optind < argc) fileNames.addFile(argv[optind++]);

# if 0
  /* If no --files-from given and no files on command line, assume we
     are to read any list of filenames from stdin. */
  if (fileNames.empty()) fileNames.addFilesFrom("");
# endif
  //____________________

  // If --image, --jigdo or --template not given, create name from other args
  if (!imageFile.empty() && imageFile != "-") {
    deduceName(jigdoFile, EXTSEPS"jigdo", imageFile);
    deduceName(templFile, EXTSEPS"template", imageFile);
  } else if (!jigdoFile.empty() && jigdoFile != "-") {
    deduceName(imageFile, "", jigdoFile);
    deduceName(templFile, EXTSEPS"template", jigdoFile);
  } else if (!templFile.empty() && templFile != "-") {
    deduceName(imageFile, "", templFile);
    deduceName(jigdoFile, EXTSEPS"jigdo", templFile);
  }

  if (imageFile != "-") {
    struct stat fileInfo;
    if (stat(imageFile.c_str(), &fileInfo) == 0)
      imageSize = fileInfo.st_size;
  }
  //____________________

  if (msg) {
    msg("Image file: %1", imageFile);
    msg("Jigdo:      %1", jigdoFile);
    msg("Template:   %1", templFile);
  }

  return result;
}
//______________________________________________________________________

void outOfMemory() {
  cerr << subst(_("%1: Out of memory - aborted."), binName()) << endl;
  exit(3);
}
//______________________________________________________________________

} // local namespace

void exit_tryHelp() {
  cerr << subst(_("%1: Try `%1 -h' or `man jigdo-file' for more "
                  "information"), binName()) << endl;
  throw Cleanup(3);
}
//______________________________________________________________________

int main(int argc, char* argv[]) {

# if ENABLE_NLS
  setlocale (LC_ALL, "");
  bindtextdomain(PACKAGE, PACKAGE_LOCALE_DIR);
  textdomain(PACKAGE);
# endif
# if DEBUG
  Logger::setEnabled("general");
# else
  Debug::abortAfterFailedAssertion = false;
# endif

  try {
    set_new_handler(outOfMemory);
    JigdoFileCmd::Command command = JigdoFileCmd::cmdOptions(argc, argv);
    switch (command) {
    case JigdoFileCmd::MAKE_TEMPLATE:
      returnValue = JigdoFileCmd::makeTemplate(); break;
    case JigdoFileCmd::MAKE_IMAGE:
      returnValue = JigdoFileCmd::makeImage();    break;
    case JigdoFileCmd::PRINT_MISSING:
    case JigdoFileCmd::PRINT_MISSING_ALL:
      returnValue = JigdoFileCmd::printMissing(command); break;
    case JigdoFileCmd::SCAN:
      returnValue = JigdoFileCmd::scanFiles();    break;
    case JigdoFileCmd::VERIFY:
      returnValue = JigdoFileCmd::verifyImage();  break;
    case JigdoFileCmd::LIST_TEMPLATE:
      returnValue = JigdoFileCmd::listTemplate(); break;
    case JigdoFileCmd::MD5SUM:
      returnValue = JigdoFileCmd::md5sumFiles();  break;
    }
  }
  catch (bad_alloc) { outOfMemory(); }
  catch (Cleanup c) {
    msg("[Cleanup %1]", c.returnValue);
    return c.returnValue;
  }
  catch (Error e) {
    string err = binName(); err += ": "; err += e.message;
    JigdoFileCmd::optReporter->error(err);
    return 3;
  }
  catch (...) { // Uncaught exception - this should not happen(tm)
    string err = binName(); err += ": Unknown error";
    JigdoFileCmd::optReporter->error(err);
    return 3;
  }
  msg("[exit(%1)]", returnValue);
  return returnValue;
}
