/* $Id$ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2001-2002  |  richard@
  | \/¯|  Richard Atterer          |  atterer.net
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

*/

#include <config.h>

#include <iostream>
#include <string>
#include <vector>

#include <debug.hh>
#include <download.hh>
#include <glibc-getopt.h>
#include <glibwww.hh>
#include <gui.hh>
#include <jobline.hh>
#include <proxyguess.hh>
#include <string-utf.hh>
#include <support.hh>

#if DEBUG
#  include <string.h>
#  include <unistd.h>
#endif
//______________________________________________________________________

#if WINDOWS
string packageDataDir;
#endif
//______________________________________________________________________

namespace {

#if WINDOWS
const char* binaryName = "jigdo";
#else
const char* binaryName;
#endif

vector<string> optUris;
enum OptProxy { GUESS, ON, OFF } optProxy = GUESS;
string optDebug;

void tryHelp() {
  cerr << subst(_("%L1: Try `%L1 -h' or `man jigdo' for more "
                  "information"), binaryName) << endl;
  exit(3);
}

enum {
  LONGOPT_DEBUG = 0x100, LONGOPT_NODEBUG
};

inline void cmdOptions(int argc, char* argv[]) {
  bool optHelp = false;
  bool optVersion = false;

  if (!WINDOWS) binaryName = argv[0];
  bool error = false;
  while (true) {
    static const struct option longopts[] = {
      { "debug",              optional_argument, 0, LONGOPT_DEBUG },
      { "help",               no_argument,       0, 'h' },
      { "no-debug",           no_argument,       0, LONGOPT_NODEBUG },
      { "proxy",              required_argument, 0, 'Y' },
      { "version",            no_argument,       0, 'v' },
      { 0, 0, 0, 0 }
    };
    int c = getopt_long(argc, argv, "hvY:", longopts, 0);
    if (c == -1) break;
    switch (c) {
    case 'h': optHelp = true; break;
    case 'v': optVersion = true; break;
    case 'Y':
      if (strcmp(optarg, "guess") == 0) optProxy = GUESS;
      else if (strcmp(optarg, "on") == 0) optProxy = ON;
      else if (strcmp(optarg, "off") == 0) optProxy = OFF;
      else cerr << subst(_("%L1: Please specify `on', `off' or `guess' after"
                           " --proxy"), binaryName) << endl;
      break;
    case LONGOPT_DEBUG:
      if (optarg) optDebug = optarg; else optDebug = "all";
      break;
    case LONGOPT_NODEBUG: optDebug.erase(); break;
    case '?': error = true;
    case ':': break;
    default:
      msg("getopt returned %1", static_cast<int>(c));
      break;
    }
  }
  if (error) tryHelp();
  if (optHelp || optVersion) {
    if (optVersion) cout << "jigdo version " JIGDO_VERSION << endl;
    if (optHelp) cout << subst(_(
    "Usage: %L1 [OPTIONS] [URL]\n"
    "Options:\n"
    "  -h  --help       Output help\n"
    "  -Y  --proxy=on/off/guess [guess]\n"
    "                   Turn proxy on or off, or guess from Mozilla/KDE/\n"
    "                   wget/lynx settings\n"
    "  -v  --version    Output version info\n"
    "  --debug[=all|=UNIT1,UNIT2...|=help]\n"
    "                   Print debugging information for all units, or for\n"
    "                   specified units, or print list of units.\n"
    "                   Can use `~', e.g. `all,~libwww'\n"
    "  --no-debug       No debugging info [default]\n"),
    binaryName) << endl;
    exit(0);
  }

  Logger::scanOptions(optDebug, binaryName);

  while (optind < argc) optUris.push_back(argv[optind++]);
}

#if WINDOWS
inline void getPackageDataDir() {
  char buf[MAX_PATH];
  GetModuleFileName(NULL, buf, MAX_PATH);
  char* end = strrchr(buf, '\\');
  packageDataDir.append(buf, end - buf + 1);
}

void noPrintHandler(const char*) {
  return;
}

void noLogHandler(const gchar*, GLogLevelFlags, const gchar*, gpointer) {
  return;
}
#endif

} // local namespace
//______________________________________________________________________

int main (int argc, char *argv[]) {
# if WINDOWS
  getPackageDataDir();
  if (!DEBUG) {
    // Switch off error messages, to avoid that the console window is opened
    g_set_print_handler(noPrintHandler);
    g_set_printerr_handler(noPrintHandler);
    g_log_set_handler(0, static_cast<GLogLevelFlags>(-1), noLogHandler, 0);
  }
# endif
# if ENABLE_NLS
  bindtextdomain(PACKAGE, packageLocaleDir);
  textdomain(PACKAGE);
  bind_textdomain_codeset(PACKAGE, "UTF-8");
# endif
# if DEBUG
  Logger::setEnabled("general");
# else
  Debug::abortAfterFailedAssertion = false;
# endif

  try {
    // Initialize GTK+ and display window
    gtk_set_locale();
    gtk_init(&argc, &argv);
    {
#     if !WINDOWS
      add_pixmap_directory("../gfx");
      add_pixmap_directory("gfx");
#     endif
      string pixDir = packageDataDir; pixDir += "pixmaps";
      add_pixmap_directory(pixDir.c_str());
    }
    GUI::create();
    cmdOptions(argc, argv);
    gtk_widget_show(GUI::window.window);

    // Initialize networking code
    Download::init();
    if (optProxy != OFF) glibwww_parse_proxy_env();
    if (optProxy == GUESS) proxyGuess();

    // Start downloads of any URIs specified on command line
    const char* dest = g_get_current_dir();
    for (vector<string>::const_iterator i = optUris.begin(),
           e = optUris.end(); i != e; ++i)
      JobLine::create(i->c_str(), dest);
    optUris.clear();
    g_free((gpointer)dest);

    gtk_main(); // Here be dragons
  }
  catch (Cleanup c) {
    msg("[Cleanup %1]", c.returnValue);
    return c.returnValue;
  }

# if DEBUG && !WINDOWS
  const char* preload = getenv("LD_PRELOAD");
  if (preload != 0 && strstr(preload, "libmemintercept") != 0) {
    msg("Detected memprof - doing sleep() to allow you to find leaks");
    while (true) sleep(1024);
  }
# endif
  return 0;
}
