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

  ATM there's no need for a common base class for all jobs, so they're only
  grouped together in the Job namespace.

*/

#ifndef JOB_HH
#define JOB_HH

#include <config.h>

#include <debug.hh>

#include <string>
//______________________________________________________________________

namespace Job {
  class IO;
}
//______________________________________________________________________

/** Base class for interaction between the outside world and the job. For
    example, depending on the IO object you register with a job, you can
    control the job via a gtk app or from within a command line utility.

    A note about the message strings passed to jobFailed() and jobMessage():
    They are not const, and the IO child class can modify them in whatever
    way it likes. The most useful action is to copy the contents away with
    swap(), e.g. with myMessage.swap(*message);

    The messages are always in valid UTF-8. Their text is *never* "quoted",
    e.g. "<" is not replaced with "&lt;". Neither do they contain any markup.

    The names of all methods here start with "job_". If a child class
    Job::SomeClass::IO adds any further methods, their name starts with
    "someClass_". This makes it easy to see which methods are introduced
    where. */
class Job::IO {
public:

  virtual ~IO() { }

  /** Called from IOPtr.remove(), gets as argument the object that was passed
      to it, returns the new pointer to store inside the IOPtr. Override the
      default implementation like this to implement chained IOs:

      <pre>
        IO* child;
        virtual Job::IO* job_removeIo(Job::IO* rmIo) {
          if (rmIo == this) {
            IO* c = child;
            child = 0;
            delete this; // May of course omit this if not desired
            return c;
          } else if (child != 0) {
            Job::IO* c = child->job_removeIo(rmIo);
            Paranoid(c == 0 || dynamic_cast<IO*>(c) != 0);
            child = static_cast<IO*>(c);
          }
          return this;
        }
        virtual void job_deleted() {
          if (child != 0) child->job_deleted();
          delete this; // May of course omit this if not desired
        }
      </pre>

      However, in the normal, simple case that there isn't a child, use:

      <pre>
        virtual Job::IO* job_removeIo(Job::IO* rmIo) {
          if (rmIo != this) return this; // Or just: Paranoid(rmIo == this);
          delete this; // May of course omit this if not desired
          return 0;    // New value to be stored inside IOPtr
        }
      </pre>

      *Constructing* chains of IOs is not handled here, you must provide some
      way to do this in your derived class.

      Important: If you return a non-null value, it must point to an object
      of your *derived* class, even though it is just passed as a plain IO*.
      There is a runtime check (if DEBUG=1) in the IOPtr code for this.

      The default impl doesn't delete this: */
  virtual Job::IO* job_removeIo(Job::IO* rmIo) {
    if (rmIo != this) return this;
    return 0;
  }

  /** Called by the IOPtr when it is deleted or when a different IO object is
      registered with it. If the IO object considers itself owned by its job,
      it can delete itself. */
  virtual void job_deleted() = 0;

  /** Called when the job has successfully completed its task. */
  virtual void job_succeeded() = 0;

  /** Called when the job fails. The only remaining sensible action after
      getting this is probably to delete the job object. */
  virtual void job_failed(string* message) = 0;

  /** Informational message. */
  virtual void job_message(string* message) = 0;
};
//______________________________________________________________________

/** For all classes which are "jobs", this provides easy, consistent
    management of a pointer to an IO object. In your job class, use the
    template to generate a public member:<pre>

    class MyJob {
    public:
      class IO { ... };
      IOPtr<IO> io;
      MyJob(IO* ioPtr, ...) : io(ioPtr), ... { ... }
    };

    </pre>*/
template<class SomeIO>
class IOPtr {
public:
  IOPtr(SomeIO* io) : ptr(io) { }
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

#endif
