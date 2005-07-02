/* $Id$ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2001-2002  |  richard@
  | \/¯|  Richard Atterer          |  atterer.net
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

*//** @file

  Implementation of the different jigdo-file commands. To be used only
  by main() in jigdo-file.cc

*/

#ifndef JIGDO_FILE_CMD_HH
#define JIGDO_FILE_CMD_HH

#include <config.h>

#include <iosfwd>
#include <string>

#include <jigdoconfig.hh>
#include <scan.hh>
#include <md5sum.hh>
#include <mkimage.hh>
#include <mktemplate.hh>
//______________________________________________________________________

/** class for "pointer to any *Reporter class", with disambiguation
    members */
struct AnyReporter : public MkTemplate::ProgressReporter,
                     public JigdoCache::ProgressReporter,
                     public JigdoDesc::ProgressReporter,
                     public MD5Sum::ProgressReporter,
                     public JigdoConfig::ProgressReporter {
  virtual void error(const string& message) {
    MD5Sum::ProgressReporter::error(message);
  }
  virtual void info(const string& message) {
    MD5Sum::ProgressReporter::info(message);
  }
  virtual void coutInfo(const string& message) {
    cout << message << endl;
  }
};
//______________________________________________________________________

/** Class providing functionality only to jigdo-file.cc */
class JigdoFileCmd {
  friend int main(int argc, char* argv[]);
  //________________________________________

  enum Command {
    MAKE_TEMPLATE, MAKE_IMAGE,
    PRINT_MISSING, PRINT_MISSING_ALL,
    SCAN, VERIFY, LIST_TEMPLATE, MD5SUM
  };
  //________________________________________

  // Command line options, to be used by the jigdo-file commands
# if WINDOWS
  static const char* const binaryName;
# else
  friend const string& binName();
  static string binaryName; // of the program
# endif

  // Names of files given on command line, and of --files-from files
  static RecurseDir fileNames;
  static string imageFile;
  static string jigdoFile;
  static string templFile;
  static string jigdoMergeFile;
  static string cacheFile;
  static size_t optCacheExpiry; // Expiry time for cache in seconds
  static vector<string> optLabels; // Strings of the form "Label=/some/path"
  static vector<string> optUris;   // "Label=http://some.server/"
  static size_t blockLength; // of rsync algorithm, is also minimum file size
  static size_t md5BlockLength;
  static size_t readAmount;
  static int optZipQuality;
  static bool optBzip2;
  static bool optForce; // true => Silently delete existent output
  static bool optMkImageCheck; // true => check MD5sums
  static bool optScanWholeFile; // false => read only first block
  static bool optAddImage; // true => Add [Image] section to output .jigdo
  static bool optAddServers; // true => Add [Servers] to output .jigdo
  static bool optHex; // true => Use hex not base64 output for md5/ls cmds
  static string optDebug; // list of debug msg to turn on, or all/help
  // Reporter is defined in config.h and is the base of all other *Reporter's
  static AnyReporter* optReporter;
  static string optMatchExec;
  //________________________________________

  /** Defined in jigdo-file.cc - reads command line options and sets
      the static vars above, returns command requested by user. Will
      throw Cleanup() for things like --help, --version or invalid cmd
      line args. */
  static Command cmdOptions(int argc, char* argv[]);
  //________________________________________

  /** @{ Functions corresponding to the jigdo-file commands, defined in
      jigdo-file-cmd.cc */
  static int makeTemplate();
  static int makeImage();
  static int printMissing(Command command = PRINT_MISSING);
  static int scanFiles();
  static int verifyImage();
  static int listTemplate();
  static int md5sumFiles();
  /*@}*/

  /* @{ Helper functions for the above functions, only to be used in
     jigdo-file-cmd.cc */
  static int addLabels(JigdoCache& cache);
  static void addUris(ConfigFile& config);
  static bool printMissing_lookup(JigdoConfig& jc, const string& query,
                                  bool printAll);
  /*@}*/
};
//______________________________________________________________________

/** Convenience function: Return name of executable, for printing in
    error messages etc. */
#if WINDOWS
inline const char* binName() {
  return "jigdo-file";
}
#else
inline const string& binName() {
  return JigdoFileCmd::binaryName;
}
#endif

/** Prints "jigdo-file: Try `jigdo-file --help' or `man jigdo-file'
    for more information", then throws Cleanup(3). */
extern void exit_tryHelp();
//______________________________________________________________________

#endif
