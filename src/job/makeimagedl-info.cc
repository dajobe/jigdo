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

#include <config.h>

#include <compat.hh>
#include <jigdoconfig.hh>
#include <makeimagedl.hh>
//______________________________________________________________________

using namespace Job;

DEBUG_UNIT("makeimagedl-info")
//______________________________________________________________________

namespace {
  struct ImageInfoParse {
    ImageInfoParse(string* o, bool e, const char** s)
      : output(o), escapedText(e), subst(s), depth(0) { }
    string* output;
    bool escapedText;
    const char** subst;
    int depth;
  };

  // For ImageInfo - verify that the XML is correct
  void parseStartElem(GMarkupParseContext* /*context*/,
      const gchar* elem, const gchar** attrNames,
      const gchar** /*attrValues*/, gpointer user_data, GError** error) {
    debug("imageInfo parse: <%1>", elem);
    ImageInfoParse* x = static_cast<ImageInfoParse*>(user_data);
    ++x->depth;
    if (attrNames != 0 && attrNames[0] != 0) {
      g_set_error(error, g_markup_error_quark(),
                  G_MARKUP_ERROR_UNKNOWN_ATTRIBUTE, " ");
      return;
    }
    if (strcmp("x", elem) == 0 && x->depth == 1)
      return; // Ignore dummy outer <x> element
    const char* s;
    if      (strcmp("b",     elem) == 0) s = x->subst[MakeImageDl::B];
    else if (strcmp("i",     elem) == 0) s = x->subst[MakeImageDl::I];
    else if (strcmp("tt",    elem) == 0) s = x->subst[MakeImageDl::TT];
    else if (strcmp("u",     elem) == 0) s = x->subst[MakeImageDl::U];
    else if (strcmp("big",   elem) == 0) s = x->subst[MakeImageDl::BIG];
    else if (strcmp("small", elem) == 0) s = x->subst[MakeImageDl::SMALL];
    else if (strcmp("br",    elem) == 0) s = x->subst[MakeImageDl::BR];
    else {
      g_set_error(error, g_markup_error_quark(),
                  G_MARKUP_ERROR_UNKNOWN_ELEMENT, " ");
      return;
    }
    *x->output += s;
  }

  void parseEndElem(GMarkupParseContext* /*context*/,
      const gchar* elem, gpointer user_data, GError** error) {
    debug("imageInfo parse: </%1>", elem);
    ImageInfoParse* x = static_cast<ImageInfoParse*>(user_data);
    --x->depth;
    if (strcmp("x", elem) == 0 && x->depth == 0)
      return; // Ignore dummy outer </x> element
    if (strcmp("br", elem) == 0) return;
    const char* s;
    if      (strcmp("b",     elem) == 0) s = x->subst[MakeImageDl::B_];
    else if (strcmp("i",     elem) == 0) s = x->subst[MakeImageDl::I_];
    else if (strcmp("tt",    elem) == 0) s = x->subst[MakeImageDl::TT_];
    else if (strcmp("u",     elem) == 0) s = x->subst[MakeImageDl::U_];
    else if (strcmp("big",   elem) == 0) s = x->subst[MakeImageDl::BIG_];
    else if (strcmp("small", elem) == 0) s = x->subst[MakeImageDl::SMALL_];
    else {
      g_set_error(error, g_markup_error_quark(),
                  G_MARKUP_ERROR_UNKNOWN_ELEMENT, " ");
      return;
    }
    *x->output += s;
  }

  void parseText(GMarkupParseContext* context, const gchar* text,
      gsize textLen, gpointer user_data, GError** error) {
    ImageInfoParse* x = static_cast<ImageInfoParse*>(user_data);
    debug("imageInfo parseText: \"%1\"", string(text, textLen));
    if (textLen == 0) return;
    if (strcmp(g_markup_parse_context_get_element(context), "br") == 0) {
      // <br> element must be empty
      g_set_error(error, g_markup_error_quark(),
                  G_MARKUP_ERROR_INVALID_CONTENT, " ");
      return;
    }
    if (x->escapedText) {
      x->output->append(text, textLen); // Leave escaped
    } else {
      // Unescape. NB UTF-8
      const char* p = text;
      const char* end = text + textLen;
      while (p < end) {
        if (*p != '&') {
          *x->output += *p;
          ++p;
          continue;
        }
        if (p + 4 <= end && strncmp(p + 1, "lt;", 3) == 0) {
          *x->output += '<';
          p += 4;
          continue;
        }
        if (p + 4 <= end && strncmp(p + 1, "gt;", 3) == 0) {
          *x->output += '>';
          p += 4;
          continue;
        }
        if (p + 5 <= end && strncmp(p + 1, "amp;", 4) == 0) {
          *x->output += '&';
          p += 5;
          continue;
        }
        *x->output += *p;
        ++p;
      }
    }
  }

  // Comments, processing instructions and CDATA not allowed
  void parseComment(GMarkupParseContext* /*context*/,
      const gchar* /*passthrough_text*/, gsize /*textLen*/,
      gpointer /*user_data*/, GError** error) {
    debug("imageInfo parseComment");
    g_set_error(error, g_markup_error_quark(),
                G_MARKUP_ERROR_INVALID_CONTENT, " ");
  }

} // namespace

void MakeImageDl::imageInfo(string* output, bool escapedText,
                            const char* subst[13]) const {
  Paranoid(output != 0 && subst != 0);

  unsigned outputLen = output->length();
  ImageInfoParse myData(output, escapedText, subst);
  static GMarkupParser p = { &parseStartElem, &parseEndElem, &parseText,
                             &parseComment, 0 };
  GMarkupParseContext* context =
    g_markup_parse_context_new(&p, (GMarkupParseFlags)0, &myData, 0);
  GError* err = 0;
  if (g_markup_parse_context_parse(context, "<x>", 3, &err)
      && g_markup_parse_context_parse(context, imageInfoVal.data(),
                                      imageInfoVal.length(), &err)
      && g_markup_parse_context_parse(context, "</x>", 4, &err)
      && g_markup_parse_context_end_parse(context, &err)) { }
  g_markup_parse_context_free(context);
  if (err != 0) {
    g_error_free(err);
    output->resize(outputLen);
    if (escapedText) {
      debug("imageInfo: error, escaping whole ImageInfo (%1)", err->code);
      for (string::const_iterator i = imageInfoVal.begin(),
             e = imageInfoVal.end(); i != e; ++i) {
        if (*i == '<') *output += "&lt;";
        else if (*i == '>') *output += "&gt;";
        else if (*i == '&') *output += "&amp;";
        else *output += *i;
      }
    } else {
      *output += imageInfoVal;
    }
  }
}
