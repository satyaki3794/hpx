//////////////////////////////////////////////////////////////////////////////
//
// PCI wrapper for mmap'd access to devices via /sys
//
// Copyright (c) 2009, 2014 Maciej Brodowicz
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
//////////////////////////////////////////////////////////////////////////////

#include <hpx/config.hpp>

#if defined(HPX_HAVE_FPGA_QUEUES)
#if (defined(__linux) || defined(linux) || defined(__linux__)) && \
    !defined(__bgq__) && !defined(__powerpc__)

#include <fcntl.h>
#include <sys/mman.h>
#include <iostream>
#include <iterator>
#include <algorithm>
#include <string>
#include <fcntl.h>
#include <sys/mman.h>
// for non-portable format directives
#define __STDC_FORMAT_MACROS 1
#include <inttypes.h>
#include <hpx/runtime/threads/policies/fpga_support/pci.hpp>
//#include "util.h"
#include <hpx/runtime/threads/policies/fpga_support/error.hpp>
#include <hpx/runtime/threads/policies/fpga_support/apx.hpp>

namespace PCI
{
  // verbose printouts
  bool verbose = true;

  // read small file
  void Device::read(boost::filesystem::path const& p, char *buf, int sz)
  {
    FILE *f = fopen(p.c_str(), "r");
    if (!f)
      throw RuntimeError("PCI::Device::read()",
                         "cannot open %s for reading", p.c_str());
    size_t n = fread(buf, 1, sz, f);
    buf[n] = 0; // terminator
    fclose(f);
  }

  // fill in device info
  void Device::get_info(boost::filesystem::path const& pd)
  {
    char s[16];
    boost::filesystem::path fn = pd/"vendor";
    read(fn, s, 15);
    info_.vendor_ = strtol(s, 0, 0);

    fn = pd/"device";
    read(fn, s, 15);
    info_.device_ = strtol(s, 0, 0);

    fn = pd/"subsystem_vendor";
    read(fn, s, 15);
    info_.sub_vendor_ = strtol(s, 0, 0);

    fn = pd/"subsystem_device";
    read(fn, s, 15);
    info_.sub_device_ = strtol(s, 0, 0);

    fn = pd/"class";
    read(fn, s, 15);
    info_.class_ = strtol(s, 0, 0);

    info_.bar_mask_ = 0;
    if (boost::filesystem::is_regular_file(pd/"resource0")) info_.bar_mask_ |= 1<<0;
    if (boost::filesystem::is_regular_file(pd/"resource1")) info_.bar_mask_ |= 1<<1;
    if (boost::filesystem::is_regular_file(pd/"resource2")) info_.bar_mask_ |= 1<<2;
    if (boost::filesystem::is_regular_file(pd/"resource3")) info_.bar_mask_ |= 1<<3;
    if (boost::filesystem::is_regular_file(pd/"resource4")) info_.bar_mask_ |= 1<<4;
    if (boost::filesystem::is_regular_file(pd/"resource5")) info_.bar_mask_ |= 1<<5;
  }

  // find matching PCI device
  void Device::find(DevInfo const& di)
  {
    if (dev_path_.empty()) dev_path_ = SYSPATH;

    if (boost::filesystem::is_directory(dev_path_))
    { // device directory: iterate over devices
      boost::filesystem::directory_iterator it = boost::filesystem::directory_iterator(dev_path_);
      for (; it != boost::filesystem::directory_iterator(); it++)
        if (boost::filesystem::is_directory(*it))
        {
          get_info(*it);
          if (di.vendor_ != PCI_ANY && info_.vendor_ != di.vendor_) continue;
          if (di.device_ != PCI_ANY && info_.device_ != di.device_) continue;
          if (di.class_ != PCI_ANY && info_.class_ != di.class_) continue;
          if (di.sub_vendor_ != PCI_ANY && info_.sub_vendor_ != di.sub_vendor_) continue;
          if (di.sub_device_ != PCI_ANY && info_.sub_device_ != di.sub_device_) continue;

          verb("Entry at %s with vendor ID 0x%04x and device ID 0x%04x matches",
               it->path().filename().c_str(), info_.vendor_, info_.device_);
          dev_path_ = it->path();
          return;
        }
      // no matching device found
      throw RuntimeError("PCI::Device::find()",
                         "could not find the requested PCI device");
    }
    else if (boost::filesystem::is_regular_file(dev_path_))
    { // direct resource path
      if (!boost::filesystem::is_regular_file(dev_path_))
        throw RuntimeError("PCI::Device::find()",
                           "file %s doesn't exist", dev_path_.c_str());
      std::string base = dev_path_.filename().native();
      char last = *(base.end()-1);
      if (last < '0' || last > '9')
        throw RuntimeError("PCI::Device::find()",
                           "unsupported format of resource file %s", base.c_str());
      // set device path to the containing parent directory
      dev_path_ = dev_path_.parent_path();
      get_info(dev_path_);
      map_bar(last-'0');
    }
    else
      RuntimeError("PCI::Device::find()",
                   "device path %s does not exist", dev_path_.c_str());
  }

  // map PCI resource in app's address space
  Region Device::map_region(std::string const& name)
  {
    verb("Attempting to map %s", name.c_str());
    Region reg;
    boost::filesystem::path dp = dev_path_/name;
    reg.fd_ = open(dp.c_str(), O_RDWR | O_SYNC);
    if (reg.fd_ < 0)
      throw RuntimeError("PCI::Device::map_region()",
                         "cannot open %s", dp.c_str());
    reg.size_ = boost::filesystem::file_size(dp);
    reg.addr_ = mmap(0, reg.size_, PROT_READ | PROT_WRITE, MAP_SHARED, reg.fd_, 0);
    if (reg.addr_ == MAP_FAILED)
      throw RuntimeError("PCI::Device::map_region()",
                         "mmap failed for region \"%s\"", name.c_str());
    verb("Mapped %lu bytes at %p", reg.size_, reg.addr_);
    return reg;
  }

  // map memory associated with specific BAR into app's address space
  Region const& Device::map_bar(int barn)
  {
    if (regmap_.find(barn) != regmap_.end())
      return regmap_[barn];

    std::string base("resource");
    base += '0'+barn;
    regmap_[barn] = map_region(base);
    return regmap_[barn];
  }

  // find PCI device given its attributes
  Device::Device(DevInfo const& info, std::string const& path):
    dev_path_(path)
  {
    // find first matching device
    find(info);
  }

  Device::~Device()
  {
    std::map<int, Region>::iterator it;
    for (it = regmap_.begin(); it != regmap_.end(); ++it)
    {
      munmap(it->second.addr_, it->second.size_);
      close(it->second.fd_);
    }
  }

  // return mapped BAR region (or map it implicitly if not done before)
  Region const& Device::bar_region(int barn)
  {
    if (barn == PCI_ANY)
    { // any mapped region (lowest indexes first)
      if (!regmap_.size())
      {
        for (int i = 0; i < 6; i++)
          if (info_.bar_mask_ & (1<<i))
            return map_bar(i);
      }
      return regmap_.begin()->second;
    }
    else // specific BAR requested
      return map_bar(barn);
  }

  // return region for configuration space
  Region const& Device::config_region()
  {
    if (!regcfg_.addr_)
    {
      regcfg_ = map_region("config");
    }
    return regcfg_;
  }


  //// access through kernel driver
  namespace kdev
  {
    // open kernel device associated with requested accelerator and mmap its resources
    void *map(int devno, int& fd)
    {
      int i, first = 0, last = APX_DEVNUM_RANGE-1;
      // set scan bounds when specific board is requested
      if (devno != DEV_NONE && devno != DEV_ANY) first = last = devno;

      for (i = first; i <= last; i++)
      {
        char devnm[64];
        snprintf(devnm, 64, "/dev/" APX_ACC_DEVSTR, i);
        verb("Attempting to open %s", devnm);
        if ((fd = open(devnm, O_RDWR)) >= 0)
        {
          // one page is enough for testing
          void *addr = mmap(NULL, sysconf(_SC_PAGE_SIZE), PROT_READ | PROT_WRITE,
                            MAP_SHARED | MAP_NORESERVE, fd, 0);
          if (addr != reinterpret_cast<void *>(-1))
          {
            verb("Success, mmap returned address %p", addr);
            return addr;
          }

          // mapping failed: release the device
          verb("Mapping failed with errno=%d (%s)\n", errno, strerror(errno));
          close(fd); fd = -1;
        }
      }

      return 0;
    }
  } // namespace kdev
} // namespace PCI

#endif
#endif
