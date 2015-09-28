////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2009 Maciej Brodowicz
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
//////////////////////////////////////////////////////////////////////////////

#ifndef _ERROR_H
#define _ERROR_H 1

#include <stdarg.h>
#include <string>
#include <exception>

// call wrapper w/return code check
#define CALL(fn, expected) do {                                 \
  int rc = (fn);                                                \
  if (rc != (expected))                                         \
    error(1, "\"" #fn "\" failed with return code %d", rc);     \
  } while (0)

namespace PCI
{
  // prototypes
  void error(int rc, char const *fmt, ...);

  // classes
  // customized exception classes to be used by test codes
  class ExceptionBase: public std::exception
  {
  protected:
    std::string info_;

    void print(char const *fmt, va_list ap)
    {
      if (fmt)
      { // kludge to process formatted printout of unknown size
        std::vector<char> tmp;
        size_t sz = 2*strlen(fmt)+1;
        for (sz = strlen(fmt)*2; ; sz *= 2)
        {
          tmp.resize(sz, 0);
          vsnprintf(&tmp[0], tmp.size(), fmt, ap);
          if (tmp.back() == 0)
            break; // printout fits in buffer (including terminating '\0')
        }
        info_ += &tmp[0];
      }
    }

  public:
    ExceptionBase(bool cf = false) { }
    virtual ~ExceptionBase() throw() { }

    virtual char const *what() const throw() {return info_.c_str();}
  };

  // exception thrown by test apps when encountering runtime error
  class RuntimeError: public ExceptionBase
  {
  public:
    RuntimeError(char const *where = 0, char const *fmt = 0, ...)
    {
      va_list ap;
      va_start(ap, fmt);

      std::string info_ = "runtime error ";
      if (where && where[0])
      { // location information available
        info_ += " in "; info_ += where;
      }
      info_ += ":\n>>> ";
      print(fmt, ap);
      // grab errno from environment (if set)
      if (errno)
      {
          info_ += " (errno: "; info_ += strerror(errno); info_ += ")";
      }
    }

    virtual ~RuntimeError() throw() { }
  };

  // exception thrown on invalid command line or input options
  class OptionError: public ExceptionBase
  {
  public:
    OptionError(char const *where = 0, char const *fmt = 0, ...)
    {
      va_list ap;
      va_start(ap, fmt);

      std::string info_ = "runtime error ";
      if (where && where[0])
      { // location information available
        info_ += " in "; info_ += where;
      }
      info_ += ":\n>>> ";
      print(fmt, ap);
      // grab errno from environment (if set)
      if (errno)
      {
        info_ += " (errno: "; info_ += strerror(errno); info_ += ")";
      }
   }

    virtual ~OptionError() throw() { }
  };
}

#endif // _ERROR_H


