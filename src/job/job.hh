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
    e.g. "<" is not replaced with "&lt;". Neither are they allowed to contain
    any markup.

    The names of all methods here start with "job_". If a child class
    Job::SomeClass::IO adds any further methods, their name starts with
    "someClass_". This makes it easy to see which methods are introduced
    where. */
class Job::IO {
public:

  virtual ~IO() { }

  /** Called by the job when it is deleted or when a different IO object is
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

  SomeIO& operator*()  const throw() { return *ptr; }
  SomeIO* operator->() const throw() { return ptr; }
  operator bool()      const throw() { return ptr != 0; }
  SomeIO* get()        const throw() { return ptr; }
  /** Calls the IO object's job_deleted() method before overwriting the
      value, except if the old and new IO are identical */
  void set(SomeIO* io) {
    if (ptr == io) return;
    if (ptr != 0) ptr->job_deleted();
    ptr = io;
  }
  /** Like set(0), but doesn't call the IO object's job_deleted() method */
  void release() { ptr = 0; }

private:
  SomeIO* ptr;
};

#endif
