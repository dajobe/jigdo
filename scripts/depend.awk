#! /usr/bin/env awk -f
#  __   _
#  |_) /|  Copyright (C) 2000  |  richard@
#  | \/¯|  Richard Atterer     |  atterer.net
#  ¯ '` ¯
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License, version 2. See
#  the file COPYING for details.

# Syntax: awk -f depend.awk srcdir subdir1 subdir2... - file1 file2...

# Reads all the files and looks for #include directives. If any file
# being included can be found in srcdir/subdir or ./subdir, it is also
# recursively scanned. Finally, writes a list of dependencies for each
# file to srcdir+"Makedeps" (can also append the list of dependencies
# to Makefile and Makefile.in; see output = "..." below)

# Supports my two-level or three-level #include hierarchy:
#   - in "fh" (forward decl) files, may only include other "fh" files
#   - in "hh" (normal hdr) files, may include "fh" and "hh" files
#   - in "ih" (inline defs) files, may include "fh", "hh" and "ih" files
# Furthermore, depending on whether NOINLINE has been #define'd,
# INLINE functions are either compiled inline or non-inline. The
# latter is useful during development, to avoid excessive
# recompilation after changes to "hh" headers. The idea is that the
# implementations in "ih" will need many more other headers than the
# decls in "hh". IMPORTANT: The generated dependencies assume that you
# *have* defined NOINLINE during development.

# Differences to "makedepend" behaviour: ignores ".ih" files unless
# included from ".cc" files; never takes #ifdef and friends into
# account; does not scan include dirs in specified order, but instead
# scans all include dirs for each file; Untested behaviour if the same
# source filename appears more than once in different source
# directories.

# Another extension: If outputting a rule for a file matching
# *-test.o, outputs an additional rule for direct compilation of
# *-test
#______________________________________________________________________

# recursively find dependencies for specified file.
function makeDeps(file) {
  finalDeps = makeDeps2(file);
  return substr(finalDeps, 2, length(finalDeps) - 2);
}

function makeDeps2(file, l_deps, l_dir, l_recurseFile, l_line, l_exists,
                   l_lineNr, l_fileType, l_includedType) {
  if (deps[file] == "-") return ""; # avoid infinite recursion
  else if (deps[file] != "") return deps[file]; # already know about that
  deps[file] = "-";

  l_deps = " "; l_lineNr = 0;
  l_fileType = substr(file, length(file) - 1); # e.g. "cc" or ".h"
  while ((getline l_line < file) == 1) { # read each line of file
    exists[file]; # have read at least 1 line, so the file is there
    ++l_lineNr;
    if (l_line !~ /^[ \t]*\#[ \t]*include[ \t]+["<][a-zA-Z0-9.-]+[">]/)
      continue;
    # found #include line
    match(l_line, /["<][a-zA-Z0-9.-]+[">]/);
    l_includedType = substr(l_line, RSTART + RLENGTH - 3, 2);

    l_exists = 0;
    # skip for loop to ignore .ih files except when included from .cc
    if (l_fileType == "cc" || l_includedType != "ih") {
      for (l_dir in includeDir) { # try each in include path
        l_recurseFile = substr(l_line, RSTART + 1, RLENGTH - 2);
        split(makeDeps2(l_dir l_recurseFile), newDeps); # recurse
        # eliminate duplicates by only adding new files to l_deps
        for (i in newDeps) {
          if (index(l_deps, " " newDeps[i] " ") == 0)
            l_deps = l_deps newDeps[i] " ";
        }
        newDep = includeDir[l_dir] l_recurseFile;
        if ((l_dir l_recurseFile) in exists) {
          l_exists = 1;
          if (index(l_deps, " " newDep " ") == 0) l_deps = l_deps newDep " ";
        }
      }
    }
    # maybe complain about wrong includes
    if (l_line !~ /\/\* *NOINLINE/ && l_exists \
        && type[l_fileType] < type[l_includedType])
      printf("%s:%d: #including file `%s' violates policy\n",
             file, l_lineNr, l_recurseFile);
  }
  close(file);
  deps[file] = l_deps;
  return l_deps;
}
#______________________________________________________________________

# Read file until separator line found, set depContent to rest
function readSepFile(file, l_line, l_ret) {
  l_ret = "";
  while ((getline l_line < file) == 1) {
    l_ret = l_ret l_line "\n";
    if (l_line == "# DO NOT DELETE THIS LINE -- make depend depends on it."){
      depContent = "";
      while ((getline l_line < file) == 1)
        depContent = depContent l_line "\n";
      close(file);
      return l_ret;
    }
  }
  print "depend.awk: No separator line found in `" file "'!?";
  exit(1);
}

# Read entire file contents into depContent
function readFile(file, l_RS) {
  l_RS = RS;
  RS = "\x7f";
  getline depContent < file;
  close(file);
  RS = l_RS;
  return "";
}
#______________________________________________________________________

BEGIN {
  # Output filename. Special case: output="" means: Append
  # dependencies to Makefile and Makefile.in
  output = "Makedeps";

  # lower number => may include all with higher number
  type["cc"] = type[".c"] = 4;
  type["ih"] = 3;
  type["hh"] = type[".h"] = 2;
  type["fh"] = 1;

  arg = 0;
  srcDir = ARGV[++arg]; # first arg is directory containing sources
  if (substr(srcDir, length(srcDir)) != "/") srcDir = srcDir "/";
  if (substr(srcDir, 1, 2) == "./") srcDir = substr(srcDir, 3);

  includeDir[srcDir] = ""; # look in source dir
  includeDir[""] = ""; # also look in current dir and its subdirs
  while (ARGV[++arg] != "-") { # read subdirectories until "-"
    includeDir[srcDir ARGV[arg] "/"] = ARGV[arg] "/";
    includeDir[ARGV[arg] "/"] = ARGV[arg] "/";
  }

  if (output) {
    md = srcDir output;
    readFile(md);
    newDepContent = "";
  } else {
    # Makefile/Makefile.in
    mf1 = srcDir "Makefile.in";
    mf2 = "Makefile";
    makeFileInContent = readSepFile(mf1);
    newDepContent = "\n";
  }

  # build dependencies
  while (++arg < ARGC) {
    f = ARGV[arg]; sub(/^\.\//, "", f);
    base = f; sub(/\.cc?$/, "", base);
    target = base ".o";
    newDepContent = newDepContent \
                    sprintf("%s: %s %s\n", target, f, makeDeps(srcDir f));
    if (substr(base, length(base) - 4) == "-test")
      newDepContent = newDepContent \
                      sprintf("%s: %s\n\t$(LD) -o %s $(LDFLAGS) %s\n",
                              base, target, base, target);
  }

  # write files
  if (depContent == newDepContent) {
    print "No dependency changes";
  } else {
    if (output) {
      system("mv -f " md " " md ".bak");
      printf("%s", newDepContent) > md;
      print "Updated `" md "'";
    } else {
      # Makefile/Makefile.in
      makeFileContent = readSepFile(mf2);
      system("mv -f " mf1 " " mf1 ".bak");
      printf("%s%s", makeFileInContent, newDepContent) > mf1;
      printf("%s%s", makeFileContent, newDepContent) > mf2;
      print "Updated `" srcDir "Makefile.in' and `Makefile'";
    }
  }
}
