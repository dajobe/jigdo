#! /usr/bin/gawk -f
#  __   _
#  |_) /|  Copyright (C) 2001  |  richard@
#  | \/¯|  Richard Atterer     |  atterer.net
#  ¯ '` ¯
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License, version 2. See
#  the file COPYING for details.

# Convert the list of Debian mirrors from Debian CVS into entries for
# the [Servers] section of a .jigdo file.

function jigdo(str, comment) {
  if (str in recorded) return;
  recorded[str];
  result = str;
  #result = substr(str, 7)"   "substr(str, 1, 6);
  if (comment) {
    result = result substr("                                              ",
                           length(str)) " # " comment;
  }
  print result;
}

function entry() {
  for (dummy in x) { # Array empty => ignore
    comment = x["Country"];
    if ("Location" in x) {
      # Remove repeated country name from Location
      location = x["Location"];
      loc = index(location, substr(comment, 4));
      if (loc) {
        comment = comment " (" substr(location, 1, loc - 1) \
          substr(location, loc + length(comment) - 3) ")";
      } else {
        comment = comment " (" location ")";
      }
      sub(/[ ,]+\)$| +\)/, ")", comment);
      sub(/[ ,]+\( *\)$/, "", comment);
      sub(/  +/, " ", comment);
    }
    if ("Archive-http" in x)
      jigdo("Debian=http://" x["Site"] x["Archive-http"], comment);
    else if ("Archive-ftp" in x)
      jigdo("Debian=ftp://" x["Site"] x["Archive-ftp"], comment);
    if ("NonUS-http" in x)
      jigdo("Non-US=http://" x["Site"] x["NonUS-http"], comment);
    else if ("NonUS-ftp" in x)
      jigdo("Non-US=ftp://" x["Site"] x["NonUS-ftp"], comment);
    split("", x); # Clear x[]
    return;
  }
}

/^$/ {
  entry();
}

($1 ~ /^[A-Za-z0-9_-]+:$/) {
  field = substr($1, 1, length($1) - 1);
  line = $0; sub(/^[^ ]+ /, "", line);
  x[field] = line;
}

# Entry line continued on new line
/^[ 	]+[^ 	]/ {
  line = $0; sub(/^[ 	]+/, " ", line);
  x[field] = x[field] line;
}

END {
  entry();
}
