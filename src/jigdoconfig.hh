/* $Id$ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2001-2002  |  richard@
  | \/¯|  Richard Atterer          |  atterer.net
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

  Representation for config data in a .jigdo file - based on ConfigFile

  Mostly, this class just "forwards" requests by making the
  appropriate calls to the ConfigFile object, with one exception: It
  caches the labels in the [Servers] section and detects loops.

*/

#ifndef JIGDOCONFIG_HH
#define JIGDOCONFIG_HH

#include <map>
#include <string>
#include <vector>

#include <configfile.hh>
//______________________________________________________________________

class JigdoConfig {
private:
  /* multimap doesn't make guarantees about order of inserted values
     with equal key, which needs to be preserved in our case. */
  typedef map<string,vector<string> > Map;
  //________________________________________

public:
  class ProgressReporter {
  public:
    virtual ~ProgressReporter() { }
    virtual void error(const string& message);
    virtual void info(const string& message);
  };
  //________________________________________

  // Open file for input and create a ConfigFile object
  JigdoConfig(const char* jigdoFile, ProgressReporter& pr);
  JigdoConfig(const string& jigdoFile, ProgressReporter& pr);
  /* Take over possession of existing ConfigFile - configFile will be
     deleted in ~JigdoConfig()! jigdoFile argument is not used except
     for error messages. */
  JigdoConfig(const char* jigdoFile, ConfigFile* configFile,
              ProgressReporter& pr);
  JigdoConfig(const string& jigdoFile, ConfigFile* configFile,
              ProgressReporter& pr);
  ~JigdoConfig() { delete config; }
  ConfigFile& configFile() { return *config; }
  //________________________________________

  /** Prepare internal map from label name to URIs. Is called
      automatically during JigdoConfig(), but must be called manually
      afterwards whenever any entry in the ConfigFile's "[Servers]"
      section changes. */
  void rescan();

  /** Change reporter for error messages */
  inline void setReporter(ProgressReporter& pr);

  /** Given an URI-style string like "MyServer:dir/foo/file.gz", do
      label lookup (looking for [Servers] entries like "MyServer=...")
      and return the resulting strings, e.g.
      "ftp://mysite.com/dir/foo/file.gz". The class will enumerate all
      the possible URIs. NB: After a jc.rescan(), no further calls to
      next() of existing Lookups for that JigdoConfig are allowed.
      Also, query must stay valid throughout the lifetime of Lookup,
      since a reference to it is maintained. */
  class Lookup {
  public:
    inline Lookup(const JigdoConfig& jc, const string& query);
    /** If true returned, result has been overwritten with next value.
        Otherwise, end of list has been reached and result is
        unchanged. */
    inline bool next(string& result);
    // Default copy ctor and dtor
  private:
    const JigdoConfig& config;
    // "MyServer:dir/foo/file.gz" or null: next() returns false
    const string* uri;
    string::size_type colon; // Offset of ':' in query
    // Pointer in list for "MyServer" mappings; pointer to end of list
    vector<string>::const_iterator cur, end;
  };
  friend class Lookup;
  //________________________________________

private:

  /* Adds filename and line number before reporting. This is used by
     JigdoConfig to talk to ConfigFile. */
  struct ForwardReporter : ConfigFile::ProgressReporter {
    inline ForwardReporter(JigdoConfig::ProgressReporter& pr,
                           const string& file);
    inline ForwardReporter(JigdoConfig::ProgressReporter& pr,
                           const char* file);
    virtual ~ForwardReporter() { }
    virtual void error(const string& message, const size_t lineNr = 0);
    virtual void info(const string& message, const size_t lineNr = 0);
    JigdoConfig::ProgressReporter* reporter;
    string fileName;
  };
  //________________________________________

  struct ServerLine {
    ConfigFile::iterator line;
    size_t labelStart, labelEnd, valueStart;
  };
  Map::iterator rescan_addLabel(list<ServerLine>& entries,
                                const string& label, bool& printError);
  inline void rescan_makeSubst(list<ServerLine>& entries, Map::iterator mapl,
                               const ServerLine& l, bool& printError);

  ConfigFile* config;
  Map serverMap;
  ForwardReporter freporter;
};
//______________________________________________________________________

JigdoConfig::ForwardReporter::ForwardReporter(
    JigdoConfig::ProgressReporter& pr, const string& file)
  : reporter(&pr), fileName(file) { }

JigdoConfig::ForwardReporter::ForwardReporter(
    JigdoConfig::ProgressReporter& pr, const char* file)
  : reporter(&pr), fileName(file) { }

void JigdoConfig::setReporter(ProgressReporter& pr) {
  freporter.reporter = &pr;
}
//________________________________________

JigdoConfig::Lookup::Lookup(const JigdoConfig& jc, const string& query)
    : config(jc), uri(&query), colon(query.find(':')), cur() {
  /* colon == string::npos means: The URI doesn't contain a ':', or it
     does but the label before it ("MyServer") isn't listed in the
     mapping, or the label is listed but the corresponding
     vector<string> is empty. In all these cases, the Lookup will only
     return one string - the original query. */
  if (colon != string::npos) {
    string label(query, 0, colon);
    Map::const_iterator vec = config.serverMap.find(label);
    ++colon;
    if (vec != config.serverMap.end() && vec->second.size() != 0) {
      cur = vec->second.begin();
      end = vec->second.end();
    } else {
      colon = string::npos;
    }
  }
}

bool JigdoConfig::Lookup::next(string& result) {
  if (uri == 0) return false;
  if (colon == string::npos) { // Only return one value
    result = *uri;
    uri = 0;
    return true;
  }
  // Iterate through vector
  result = *cur;
  result.append(*uri, colon, string::npos);
  ++cur;
  if (cur == end) uri = 0;
  return true;
}

#endif
