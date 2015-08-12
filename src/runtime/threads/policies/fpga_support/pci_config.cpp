//////////////////////////////////////////////////////////////////////////////
//
// Support for PCI configuration space access
//
// Copyright (c) 2009 Maciej Brodowicz
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
//////////////////////////////////////////////////////////////////////////////

#include <hpx/config.hpp>

#if defined(HPX_HAVE_FPGA_QUEUES)
#include "pci_config.h"
#include "util.h"


namespace pci { namespace config
{
  // arbitrary read/write support in config space
  void AnyEntry::read(pci_device *dev, uint8_t *buf, Field const *fp) const
  {
    if (fp)
      throw RuntimeError("AnySize::read()", "accesses to arbitrary bit fields not supported");

    pciaddr_t remain = size_;
    pciaddr_t offs = offset_;
    int faults = 0;
    while (remain > 0)
    {
      pciaddr_t n;
      if (pci_device_cfg_read(dev, buf, offs, remain, &n))
        throw RuntimeError("AnyEntry::read()",
                           "pci_device_cfg_read() failed");
      if (!n)
      {
        if (++faults >= MAXTRIES)
          throw RuntimeError("AnyEntry::read()",
                             "exceeded maximum number of retries");
        continue;
      }
      remain -= n; buf += n; offs += n;
    }
  }

  void AnyEntry::write(pci_device *dev, uint8_t *buf, Field const *fp) const
  {
    if (fp)
      throw RuntimeError("AnySize::write()", "accesses to arbitrary bit fields not supported");

    pciaddr_t remain = size_;
    pciaddr_t offs = offset_;
    int faults = 0;
    while (remain > 0)
    {
      pciaddr_t n;
      if (pci_device_cfg_write(dev, buf, offs, remain, &n))
        throw RuntimeError("AnyEntry::write()",
                           "pci_device_cfg_write() failed");
      if (!n)
      {
        if (++faults >= MAXTRIES)
          throw RuntimeError("AnyEntry::write()",
                             "exceeded maximum number of retries");
        continue;
      }
      remain -= n; buf += n; offs += n;
    }
  }

}}

#endif
