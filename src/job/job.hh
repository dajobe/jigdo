/* $Id$ -*- C++ -*-
  __   _
  |_) /|  Copyright (C) 2003  |  richard@
  | \/¯|  Richard Atterer     |  atterer.net
  ¯ '` ¯
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2. See
  the file COPYING for details.

  A job is a certain task - just the application logic, *no* user interface.
  All interaction with the rest of the system (input/output, user
  interaction) happens via an IO object.

*/

#ifndef JOB_HH
#define JOB_HH

#include <config.h>

#include <debug.hh>
#include <ilist.hh>

#include <string>
//______________________________________________________________________

namespace Job {
  class IO;
}
//______________________________________________________________________

//#define _DEPRECATED __attribute__((deprecated))
//#define _DEPRECATED

/** Base class for interaction between the outside world and the job. For
    example, depending on the IO object you register with a job, you can
    control the job via a gtk app or from within a command line utility.

    An IO class is implemented by anyone interested in the information, and
    an instance registered with IOSource::addListener(), which appends a
    pointer to the instance to its list of listening objects. If the listener
    is deleted, it is *automatically* removed from the list it is on.

    The messages are always in valid UTF-8. Their text is *never* "quoted",
    e.g. "<" is not replaced with "&lt;". Neither do they contain any markup.

    The names of all methods here start with "job_". If a child class
    Job::SomeClass::IO adds any further methods, their name starts with
    "someClass_". This makes it easy to see which methods are introduced
    where. */
class Job::IO : public IListBase {
public:

  virtual ~IO() { }

  //x inline virtual Job::IO* job_removeIo(Job::IO* rmIo) _DEPRECATED;

  /** Remove yourself from the IOSource you are listening to, if any. */
  void removeListener() { iList_remove(); }

  /** Called by the IOSource when it is deleted or when a different IO object
      is registered with it. If the IO object considers itself owned by its
      job, it can delete itself. */
  virtual void job_deleted() = 0;

  /** Called when the job has successfully completed its task. */
  virtual void job_succeeded() = 0;

  /** Called when the job fails. The only remaining sensible action after
      getting this is probably to delete the job object. */
  virtual void job_failed(const string& message) = 0;

  /** Informational message. */
  virtual void job_message(const string& message) = 0;
};

//x Job::IO* Job::IO::job_removeIo(Job::IO* rmIo) {
//   if (rmIo != this) return this;
//   return 0;
// }
//______________________________________________________________________

// For IOSource<SomeIO> io, use
// IOSOURCE_SEND(SomeIO, io, job_failed, ("it failed"));
// It is OK if any called object deletes itself in response to the call
#define IOSOURCE_SEND(_ioClass, _ioObj, _functionName, _args) \
  do { \
    IList<_ioClass>& _listeners = (_ioObj).listeners(); \
    IList<_ioClass>::iterator _i = _listeners.begin(), _e = _listeners.end(); \
    while (_i != _e) { \
      _ioClass* _listObj = &*_i; ++_i; _listObj->_functionName _args; \
    } \
  } while (false)
// #define IOSOURCE_SEND(_ioClass, _ioObj, _functionName, _args)
//   do {
//     IList<_ioClass>& _listeners = (_ioObj).listeners();
//     for (IList<_ioClass>::iterator _i = _listeners.begin(),
//          _e = _listeners.end(); _i != _e; ++_i) {
//       _i->_functionName _args;
//     }
//   } while (false)
// Same thing, but const:
#define IOSOURCE_SENDc(_ioClass, _ioObj, _functionName, _args) \
  do { \
    const IList<_ioClass>& _listeners = (_ioObj).listeners(); \
    IList<_ioClass>::const_iterator _i = _listeners.begin(), \
                                    _e = _listeners.end(); \
    while (_i != _e) { \
      _ioClass* _listObj = &*_i; ++_i; _listObj->_functionName _args; \
    } \
  } while (false)
// Same thing, but for template function
#define IOSOURCE_SENDt(_ioClass, _ioObj, _functionName, _args) \
  do { \
    IList<_ioClass>& _listeners = (_ioObj).listeners(); \
    typename IList<_ioClass>::iterator _i = _listeners.begin(), \
                                       _e = _listeners.end(); \
    while (_i != _e) { \
      _ioClass* _listObj = &*_i; ++_i; _listObj->_functionName _args; \
    } \
  } while (false)
//________________________________________

/** In your job class, use the template to generate a public member:<pre>

    class MyJob {
    public:
      class IO { ... };
      IOSource<IO> io;
      MyJob(IO* ioPtr, ...) : io(), ... { io.addListener(ioPtr); ... }
    };

    </pre>*/
template<class SomeIO>
class IOSource : NoCopy {
public:
  IOSource() : list() { }
  /** Does not delete the listeners. */
  //~IOSource() { typename IList<SomeIO>::const_iterator _i; }
  ~IOSource() { IOSOURCE_SENDt(SomeIO, *this, job_deleted, ()); }
  void addListener(SomeIO& l) { Assert(&l != 0); list.push_back(l); }
  const IList<SomeIO>& listeners() const { return list; };
  IList<SomeIO>& listeners() { return list; };
  bool empty() const { return list.empty(); }

private:
  IList<SomeIO> list;
};
//______________________________________________________________________

#if 0
template<class SomeIO>
class IOPtr {
public:
  inline IOPtr(SomeIO* io) _DEPRECATED;
  ~IOPtr() { if (ptr != 0) ptr->job_deleted(); }

  SomeIO& operator*()  { return *ptr; }
  SomeIO* operator->() { return ptr; }
  operator bool()      { return ptr != 0; }
  SomeIO* get()        { return ptr; }
  /** Set up pointer to IO object. Must not already have been set up. */
  void set(SomeIO* io) {
    Paranoid(ptr == 0);
    ptr = io;
  }
  /** Remove pointer from chain of IO objects. */
  void remove(SomeIO* rmIo) {
    Paranoid(ptr != 0);
    Job::IO* newPtr = ptr->job_removeIo(rmIo);
    // Upcast is OK if the job_removeIo() implementation plays by the rules
    Paranoid(newPtr == 0 || dynamic_cast<SomeIO*>(newPtr) != 0);
    ptr = static_cast<SomeIO*>(newPtr);
  }

  /** Like set(0), but doesn't call the IO object's job_deleted() method */
  void release() { ptr = 0; }

private:
  SomeIO* ptr;
};

template<class SomeIO>
IOPtr<SomeIO>::IOPtr(SomeIO* io) : ptr(io) { }
#endif

#endif
