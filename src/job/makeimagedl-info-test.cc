/* $Id$ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2003  |  richard@
  | \/¯|  Richard Atterer     |  atterer.net
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

  #test-deps job/makeimagedl-info.o net/uri.o

*/

#include <config.h>

#include <string>

// #include <cached-url.hh>
// #include <jigdo-io.hh>
#include <debug.hh>
#include <log.hh>
#include <makeimagedl.hh>
#include <url-mapping.hh>
//______________________________________________________________________

using namespace Job;

MakeImageDl::MakeImageDl(/*IO* ioPtr,*/ const string& jigdoUri,
                         const string& destination)
    : io(/*ioPtr*/), stateVal(DOWNLOADING_JIGDO),
      jigdoUrl(jigdoUri), childrenVal(), dest(destination),
      tmpDirVal(), mi(),
      imageNameVal(), imageInfoVal(), imageShortInfoVal(), templateUrlVal(),
      templateMd5Val(0) { }
Job::MakeImageDl::~MakeImageDl() { }

void MakeImageDl::setImageSection(string* imageName, string* imageInfo,
    string* imageShortInfo, string* templateUrl, MD5** templateMd5) {
  msg("setImageSection templateUrl=%1", templateUrl);
  Paranoid(!haveImageSection());
  imageNameVal.swap(*imageName);
  imageInfoVal.swap(*imageInfo);
  imageShortInfoVal.swap(*imageShortInfo);
  templateUrlVal.swap(*templateUrl);
  templateMd5Val = *templateMd5; *templateMd5 = 0;

  //x if (io) io->makeImageDl_haveImageSection();
  IOSOURCE_SEND(IO, io, makeImageDl_haveImageSection, ());
}
//======================================================================

namespace {

  const char* const hexDigits = "0123456789abcdef";
  void escapedChar(string* o, byte c) {
    switch (c) {
    case 0: *o += "\\0"; break;
    case '\n': *o += "\\n"; break;
    case '\t': *o += "\\t"; break;
    case '"': case '\\': *o += '\\'; *o += c; break;
    default:
      if (c >= ' ' && c <= '~') {
        *o += c;
      } else {
        *o += "\\x";
        *o += hexDigits[unsigned(c) >> 4];
        *o += hexDigits[unsigned(c) & 0xfU];
      }
    }
  }

  inline string escapedString(const string& s) {
    string result;
    for (unsigned i = 0; i < s.length(); ++i)
      escapedChar(&result, s[i]);
    return result;
  }

}
//______________________________________________________________________

string testImageInfo(const char* subst[], bool escapedText,
                     const char* text, const char* expected) {
  string imageShortInfo;
  string templateUrl;
  MD5* templateMd5 = 0;

  MakeImageDl m("http://url/", "");
  string imageName = "image.iso";
  string imageInfo = text;
  m.setImageSection(&imageName, &imageInfo, &imageShortInfo, &templateUrl,
                    &templateMd5);
  string info = " ";
  m.imageInfo(&info, escapedText, subst);
  msg("\"%1\"", escapedString(info));
  Assert(info == expected);
  return info;
}
//______________________________________________________________________

int main(int argc, char* argv[]) {
  if (argc == 2) Logger::scanOptions(argv[1], argv[0]);

  const char* gtk[] = {
    "<b>", "</b>", // <b>, </b>
    "<i>", "</i>", // <i>, </i>
    "<tt>", "</tt>", // <tt>, </tt>
    "<u>", "</u>", // <u>, </u>
    "<span size=\"large\">", "</span>", // <big>, </big>
    "<span size=\"small\">", "</span>", // <small>, </small>
    "\n" // <br/>
  };
  const char* tex[] = {
    "\\textbf{", "}", // <b>, </b>
    "\\textit{", "}", // <i>, </i>
    "\\texttt{", "}", // <tt>, </tt>
    "\\underline{", "}", // <u>, </u>
    "\\large{", "}", // <big>, </big>
    "\\small{", "}", // <small>, </small>
    "\\\\" // <br/>
  };

  testImageInfo(gtk, true,
                "'Sun, <16> \"Mar 2003 & 04:45:40 -0700'",
                " 'Sun, &lt;16&gt; \"Mar 2003 &amp; 04:45:40 -0700'");
  testImageInfo(tex, false,
                "'Sun, <16> \"Mar 2003 & 04:45:40 -0700'",
                " 'Sun, <16> \"Mar 2003 & 04:45:40 -0700'");

  testImageInfo(gtk, true,
                "<b>Let</b> <big>him</big> <small>who</small><br/><br></br>"
                "<i>hath</i> <tt>understanding</tt> <u>reckon</u>",
                " <b>Let</b> <span size=\"large\">him</span> "
                "<span size=\"small\">who</span>\n\n<i>hath</i> "
                "<tt>understanding</tt> <u>reckon</u>");
  testImageInfo(tex, false,
                "<b>Let</b> <big>him</big> <small>who</small><br/><br></br>"
                "<i>hath</i> <tt>understanding</tt> <u>reckon</u>",
                " \\textbf{Let} \\large{him} \\small{who}\\\\\\\\"
                "\\textit{hath} \\texttt{understanding} \\underline{reckon}");

  testImageInfo(gtk, true,
                "br must be empty <br> </br>",
                " br must be empty &lt;br&gt; &lt;/br&gt;");

  testImageInfo(tex, false,
                "Ugh, </x> does no good",
                " Ugh, </x> does no good");

  testImageInfo(gtk, true,
                "<b>Now <i>let <big>mee<br/>ee</big> entertain</i> you</b>",
                " <b>Now <i>let <span size=\"large\">mee\nee</span> "
                "entertain</i> you</b>");

  testImageInfo(tex, false,
                "<blubb>x</blubb>",
                " <blubb>x</blubb>");

  testImageInfo(gtk, true,
                "<b x=\"\">nobold</b>",
                " &lt;b x=\"\"&gt;nobold&lt;/b&gt;");

  testImageInfo(gtk, true,
                "nocomment<!-- x -->",
                " nocomment&lt;!-- x --&gt;");

  /* CDATA sections are supported by glib. However, our parseComment()
     creates an error if it sees one, because glib doesn't strip the
     "<![CDATA[" and "]]>" strings - not useful! */
  testImageInfo(gtk, true,
                "<b><![CDATA[This <i>is</i> & quoted]]></b>",
                " &lt;b&gt;&lt;![CDATA[This &lt;i&gt;is&lt;/i&gt; &amp; "
                "quoted]]&gt;&lt;/b&gt;");
  // " <b>This &lt;i&gt;is&lt;/i&gt; &amp; quoted</b>"

  return 0;
}
