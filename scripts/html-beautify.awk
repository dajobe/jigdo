#! /usr/bin/env awk -f
#  __   _
#  |_) /|  Copyright (C) 2000  |  richard@
#  | \/¯|  Richard Atterer     |  atterer.net
#  ¯ '` ¯
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License, version 2. See
#  the file COPYING for details.

function appendWord(word, spaceAfterWord) {
  #print "appendWord \"" word "\" \"" spaceAfterWord "\"";
  if (prevSpaceAfterWord == "\n") {
    # Linebreak while inside <pre>
    doc = doc substr(indentStr, 1, ind) docLine "\n";
    docLine = word;
    ind = 0;
    prevSpaceAfterWord = spaceAfterWord;
    return;
  }

  if (ind + length(docLine) + length(word) < curMaxLen) {
    # Append
    if (word != "" || doPreserve > 0)
      docLine = docLine prevSpaceAfterWord word;
    prevSpaceAfterWord = spaceAfterWord;
  } else {
    # New line
    if (docLine != "") doc = doc substr(indentStr, 1, ind) docLine "\n";
    #print docLine;
    docLine = word;
    ind = nextInd;
    prevSpaceAfterWord = spaceAfterWord;
  }
}
#______________________________________________________________________

BEGIN {
  get = ARGV[1];
  put = ARGV[2];
  maxLen = 75;
  indent = 1;
  indentStr = "                    "; # Won't indent by more than this
  killClass = 1; # If nonzero, remove all " class=...>" attributes

  # Only tags that come with closing tags are allowed!
  tags["html"]=1; tags["body"]=1; tags["head"]; tags["title"];
  tags["div"]; tags["h1"]; tags["h2"]; tags["h3"]; tags["h4"]; tags["h5"];
  tags["h6"]; tags["p"]; tags["dl"]; tags["dt"]; tags["dd"]; tags["table"];
  tags["tr"]; tags["td"];
  preserve["pre"];

  maxTagLength = 100;
  curMaxLen = maxLen;
  # Join lines
  getline rest < get;
  while ((getline line < get) == 1)
    rest = rest line "\n";

  if (killClass)
    gsub(/[ \t\n]+(class|CLASS)=("[^"]*"|'[^']*')[ \t\n]*>/, ">", rest); #"

  # Split lines at whitespace and some tags
  nextInd = ind = 0; # Nr of characters of indentation
  doc = ""; # Ouput document
  docLine = ""; # Current line to append words to
  doPreserve = 0; # Nesting level of <pre>
  while (match(rest, /([ \n\t]+|< *(\/ *)?)/)) {
    #print "MATCH \"" substr(rest, RSTART, RLENGTH) "\"";
    #print "xxx "nextInd" " substr(rest, 1, 90);

    if (substr(rest, RSTART, 1) == "<") {
      # Tag found
      tagName = tolower(substr(rest, RSTART + RLENGTH, maxTagLength));
      gsub(/[^a-z0-9].*$/, "", tagName);
      closing = index(substr(rest, RSTART + 1, RLENGTH - 1), "/");

      # Is tag <pre>?
      if (tagName in preserve) {
        if (closing && doPreserve > 0) {
          --doPreserve;
          if (doPreserve == 0) {
            curMaxLen = maxLen; nextInd = nonpreserveInd;
          }
        }
        if (!closing) {
          # Disable indentation while inside <pre>
          if (doPreserve == 0) { nonpreserveInd = nextInd; nextInd = 0; }
          ++doPreserve; curMaxLen = 9999999;
        }
      }

      if (!(tagName in tags)) {
        # No known tag name
        appendWord(substr(rest, 1, RSTART + RLENGTH + length(tagName) - 1),
                   "");
        rest = substr(rest, RSTART + RLENGTH + length(tagName));
        continue;
      } else if (closing) {
        #print "---/" tagName;
        # Closing tag
        if (tags[tagName] == 0) {
          appendWord(substr(rest, 1, RSTART + RLENGTH + length(tagName) \
                            - 1), "");
          nextInd -= indent;
        } else {
          nextInd -= indent;
          appendWord(substr(rest, 1, RSTART - 1), "");
          curMaxLen = 0; # Force new line with next appendWord()
          appendWord(substr(rest, RSTART, RLENGTH + length(tagName)), "");
          curMaxLen = maxLen;
        }
      } else {
        #print "--- " tagName;
        # Opening tag
        appendWord(substr(rest, 1, RSTART - 1), "");
        curMaxLen = 0; # Force new line with next appendWord()
        appendWord(substr(rest, RSTART, RLENGTH + length(tagName)), "");
        curMaxLen = maxLen;
        nextInd += indent;
      }
      rest = substr(rest, RSTART + RLENGTH + length(tagName));
      continue;
    }

    # Whitespace
    #print "dop " doPreserve ", RSTART=" RSTART ", RLENGTH=" RLENGTH;
    if (doPreserve) {
      # Preserve spaces and newlines in output
      appendWord(substr(rest, 1, RSTART - 1), substr(rest, RSTART, 1));
      rest = substr(rest, RSTART + 1);
    } else {
      # Wrap words
      if (substr(rest, RSTART + RLENGTH, 1) == ">")
        appendWord(substr(rest, 1, RSTART - 1), "");
      else
        appendWord(substr(rest, 1, RSTART - 1), " ");
      rest = substr(rest, RSTART + RLENGTH);
    }

  }
  doc = doc substr(indentStr, 1, ind) docLine rest;

  print doc;

}
