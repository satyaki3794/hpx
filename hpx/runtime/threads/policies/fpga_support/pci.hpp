//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2009 Maciej Brodowicz
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
//////////////////////////////////////////////////////////////////////////////


#ifndef _PCI_H
#define _PCI_H 1

#include <map>
#include <boost/filesystem.hpp>


// wildcard for matching devices and BARs
#define PCI_ANY (-1)
// root of PCI device tree in /sys (Linux)
#define SYSPATH "/sys/bus/pci/devices"

namespace fs = boost::filesystem;

namespace PCI
{
  // device parameters
  struct DevInfo
  {
    int vendor_, device_, class_;
    int sub_vendor_, sub_device_;
    int bar_mask_;

    DevInfo():
      vendor_(PCI_ANY), device_(PCI_ANY), class_(PCI_ANY),
      sub_vendor_(PCI_ANY), sub_device_(PCI_ANY), bar_mask_(0) { }
  };

  // region entry
  struct Region
  {
    void *addr_;  // address in app space
    int fd_;      // file descriptor for mmap
    size_t size_; // mapped region size

    Region(): addr_(0), fd_(-1), size_(0) { }
  };

  // device access
  class Device
  {
    fs::path dev_path_;
    DevInfo info_;
    std::map<int, Region> regmap_;
    Region regcfg_;

    void read(fs::path const&, char *, int);
    void get_info(fs::path const&);
    void find(DevInfo const&);
    Region map_region(std::string const&);
    Region const& map_bar(int);

  public:
    Device(DevInfo const&, std::string const& syspath = "");
    ~Device();

    inline DevInfo const& info() const {return info_;}

    Region const& bar_region(int barnum = PCI_ANY);
    Region const& config_region();
  };


  //// userspace support to access kernel driver
  namespace kdev
  {
    // "special" device numbers
    enum DevNum
    {
      DEV_NONE = 0xffff,
      DEV_ANY  = -1
    };

    void *map(int, int&);
  } // namespace kdev
} // namespace PCI

#endif // _PCI_H
