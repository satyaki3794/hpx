//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2009 Maciej Brodowicz
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying 
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
//////////////////////////////////////////////////////////////////////////////


#ifndef _PCI_CONFIG_H
#define _PCI_CONFIG_H 1

#include <stdint.h>
#include <string.h>
#include <boost/assign/list_inserter.hpp>
#include <boost/assign/list_of.hpp>
#include <boost/variant.hpp>
#include "error.h"
#include "pci.h"
#include "util.h"


namespace pci { namespace config
{
  //// PCI configuration space description

  // named contiguous subfield of a configuration parameter
  struct Field
  {
    uint8_t parsize_; // in bytes: containing parameter size
    uint8_t offset_, size_; // in bits

    Field(uint8_t parsz, uint8_t offs, uint8_t bitlen):
      parsize_(parsz), offset_(offs), size_(bitlen)
    { // eliminate suspect accesses
      if (parsz > 8)
        throw RuntimeError("Field ctor", "bit field size exceeds 64 bits (not supported yet)");
      if (offset_+size_ > parsz*8)
        throw RuntimeError("Field ctor", "bit field specification crosses containing buffer boundary");
    }

    inline uint64_t mask() const {return ((1ULL << size_)-1) << offset_;}
    void rdbits(uint8_t *buf) const
    { // buf is INOUT
      uint64_t tmp = 0;
      memcpy(&tmp, buf, parsize_);
      tmp = (tmp & mask()) >> offset_;
      memcpy(buf, &tmp, parsize_);
    }
    void wrbits(uint8_t *val, uint8_t *param) const
    { // val is IN (LSB aligned value), param is INOUT (modified parameter)
      uint64_t v1 = 0, p1 = 0;
      memcpy(&v1, val, parsize_); memcpy(&p1, param, parsize_);
      p1 = (p1 & ~mask()) | (v1 << offset_);
      memcpy(param, &p1, parsize_);
    }
  };

  // maps field ID to field descriptor
  typedef std::map<char const *, Field> field_map;


  //// simple metaprogramming to save typing and avoid mistakes;
  // single config space access with 1, 2, or 4 bytes
  template<uint8_t sz>
  struct ConfAtom
  {
    inline void read(pci_device *dev, uint16_t offs, uint8_t *buf) const
    {
      BOOST_STATIC_ASSERT(!sz);
    }
    inline void write(pci_device *dev, uint16_t offs, uint8_t *buf) const
    {
      BOOST_STATIC_ASSERT(!sz);
    }
  };
  // supported interfaces
# define OPEXC(op, sz, buf) do { \
  if (pci_device_cfg_## op ## _u ## sz (dev, buf, offs)) \
    throw RuntimeError("Entry::" #op "()", \
                       "pci_device_cfg_" #op "_u" #sz "() failed at offset %u", offs); \
  } while (0)

  // configuration space read/write functions using libpciaccess API
  template<>
  inline void ConfAtom<1>::read(pci_device *dev, uint16_t offs, uint8_t *buf) const
  {
    OPEXC(read, 8, buf);
  }
  template<>
  inline void ConfAtom<1>::write(pci_device *dev, uint16_t offs, uint8_t *buf) const
  {
    OPEXC(write, 8, *buf);
  }
  template<>
  inline void ConfAtom<2>::read(pci_device *dev, uint16_t offs, uint8_t *buf) const
  {
    uint16_t tmp;
    OPEXC(read, 16, &tmp); memcpy(buf, &tmp, 2);
  }
  template<>
  inline void ConfAtom<2>::write(pci_device *dev, uint16_t offs, uint8_t *buf) const
  {
    uint16_t tmp;
    memcpy(&tmp, buf, 2); OPEXC(write, 16, tmp);
  }
  template<>
  inline void ConfAtom<4>::read(pci_device *dev, uint16_t offs, uint8_t *buf) const
  {
    uint32_t tmp;
    OPEXC(read, 32, &tmp); memcpy(buf, &tmp, 4);
  }
  template<>
  inline void ConfAtom<4>::write(pci_device *dev, uint16_t offs, uint8_t *buf) const
  {
    uint32_t tmp;
    memcpy(&tmp, buf, 4); OPEXC(write, 32, tmp);
  }

  // translates access of arbitrary size into minimal sequence of
  // low-level accesses
  template<uint8_t size>
  struct ConfUnit
  {
    enum {bytes = (size >= 4? 4: (size >= 2? 2: (size > 0? 1: 0)))};

    ConfAtom<bytes> atom;
    ConfUnit<size-bytes> rest;

    inline void read(pci_device *dev, uint16_t offs, uint8_t *buf) const
    {
      atom.read(dev, offs, buf); rest.read(dev, offs+bytes, buf+bytes);
    }
    inline void write(pci_device *dev, uint16_t offs, uint8_t *buf) const
    {
      atom.write(dev, offs, buf); rest.write(dev, offs+bytes, buf+bytes);
    }
  };

  // recursion stop
  template<> struct ConfUnit<0>
  {
    inline void read(pci_device *dev, uint16_t offs, uint8_t *buf) const { }
    inline void write(pci_device *dev, uint16_t offs, uint8_t *buf) const { }
  };

  // named config parameter
  template<uint8_t sz>
  struct Entry
  {
    uint16_t offset_; // in bytes
    field_map fields_; // supported fields

    Entry(uint16_t offs = 0): offset_(offs) { }
    Entry(uint16_t offs, field_map const& flds):
      offset_(offs), fields_(flds) { }

    uint16_t size() const {return sz;}
    void read(pci_device *dev, uint8_t *buf, Field const *fp = 0) const
    {
      ConfUnit<sz> u;
      u.read(dev, offset_, buf);
      if (fp) fp->rdbits(buf);
    }
    void write(pci_device *dev, uint8_t *buf, Field const *fp = 0) const
    {
      ConfUnit<sz> u;
      if (fp && fp->size_ < sz*8)
      { // bitfield modification
        uint8_t tmp[sz];
        u.read(dev, offset_, tmp);
        fp->wrbits(buf, tmp);
        u.write(dev, offset_, tmp);
      }
      else u.write(dev, offset_, buf); // full parameter override
    }

    // for comparison of variants; field maps are not condsidered
    bool operator==(Entry const& e) const {return offset_ == e.offset_;}
  };

  // arbitrary access (this operates only on bytes)
  struct AnyEntry
  {
    static int const MAXTRIES = 3;
    uint16_t offset_, size_; // in bytes

    AnyEntry(uint16_t offs = 0, uint16_t sz = 1): offset_(offs), size_(sz) { }

    inline uint16_t size() const {return size_;}
    inline bool operator==(AnyEntry const& e) const
    {
      return offset_ == e.offset_ && size_ == e.size_;
    }

    void read(pci_device *dev, uint8_t *buf, Field const *fp = 0) const;
    void write(pci_device *dev, uint8_t *buf, Field const *fp = 0) const;
  };

  typedef boost::variant<AnyEntry, Entry<1>, Entry<2>, Entry<3>, Entry<4>, Entry<8> > Parameter;
  typedef std::map<char const *, Parameter> parameter_map;


  // description of parameters and field layout in configuration space
  struct ConfInfo: public parameter_map
  {
    ConfInfo()
    {
      boost::assign::insert(*this)
        ("VENDOR_ID", Parameter(Entry<2>(0)))
        ("DEVICE_ID", Parameter(Entry<2>(2)))
        ("COMMAND", Parameter(Entry<2>(4,
          boost::assign::map_list_of
            ("IO_SPACE", Field(2, 0, 1))
            ("MEMORY_SPACE", Field(2, 1, 1))
            ("BUS_MASTER", Field(2, 2, 1))
            ("SPECIAL_CYCLES", Field(2, 3, 1))
            ("MEMORY_WRITE_INVALIDATE_ENABLE", Field(2, 4, 1))
            ("VGA_PALETTE_SNOOP", Field(2, 5, 1))
            ("PARITY_ERROR_RESPONSE", Field(2, 6, 1))
            ("SERR_ENABLE", Field(2, 8, 1))
            ("FAST_BACK_TO_BACK_ENABLE", Field(2, 9, 1))
            ("INTERRUPT_DISABLE", Field(2, 10, 1)))))
        ("STATUS", Parameter(Entry<2>(6,
          boost::assign::map_list_of
            ("INTERRUPT_STATUS", Field(2, 3, 1))
            ("CAPABILITIES_LIST", Field(2, 4, 1))
            ("66MHZ_CAPABLE", Field(2, 5, 1))
            ("FAST_BACK_TO_BACK_CAPABLE", Field(2, 7, 1))
            ("MASTER_DATA_PARITY_ERROR", Field(2, 8, 1))
            ("DEVSEL_TIMING", Field(2, 9, 2))
            ("SIGNALED_TARGET_ABORT", Field(2, 11, 1))
            ("RECEIVED_TARGET_ABORT", Field(2, 12, 1))
            ("RECEIVED_MASTER_ABORT", Field(2, 13, 1))
            ("SIGNALED_SYSTEM_ERROR", Field(2, 14, 1))
            ("DETECTED_PARITY_ERROR", Field(2, 15, 1)))))
        ("REVISION_ID", Parameter(Entry<1>(8)))
        ("CLASS_CODE", Parameter(Entry<3>(9,
          boost::assign::map_list_of
            ("INTERFACE", Field(3, 0, 8))
            ("SUBCLASS_CODE", Field(3, 8, 8))
            ("BASE_CLASS_CODE", Field(3, 16, 8)))))
        ("CACHE_LINE_SIZE", Parameter(Entry<1>(0xc)))
        ("MASTER_LATENCY_TIMER", Parameter(Entry<1>(0xd)))
        ("HEADER_TYPE", Parameter(Entry<1>(0xe,
          boost::assign::map_list_of
            ("LAYOUT", Field(1, 0, 7))
            ("MULTIFUNCTION", Field(1, 7, 1)))))
        ("BIST", Parameter(Entry<1>(0xf,
          boost::assign::map_list_of
            ("COMPLETION_CODE", Field(1, 0, 4))
            ("START", Field(1, 6, 1))
            ("BIST_CAPABLE", Field(1, 7, 1)))))
        ("BAR0", Parameter(Entry<4>(0x10,
          boost::assign::map_list_of
            ("PREFETCHABLE", Field(4, 3, 1))
            ("64BIT", Field(4, 2, 1))
            ("IO", Field(4, 0, 1)))))
        ("BAR1", Parameter(Entry<4>(0x14,
          boost::assign::map_list_of
            ("PREFETCHABLE", Field(4, 3, 1))
            ("64BIT", Field(4, 2, 1))
            ("IO", Field(4, 0, 1)))))
        ("BAR2", Parameter(Entry<4>(0x18,
          boost::assign::map_list_of
            ("PREFETCHABLE", Field(4, 3, 1))
            ("64BIT", Field(4, 2, 1))
            ("IO", Field(4, 0, 1)))))
        ("BAR3", Parameter(Entry<4>(0x1c,
          boost::assign::map_list_of
            ("PREFETCHABLE", Field(4, 3, 1))
            ("64BIT", Field(4, 2, 1))
            ("IO", Field(4, 0, 1)))))
        ("BAR4", Parameter(Entry<4>(0x20,
          boost::assign::map_list_of
            ("PREFETCHABLE", Field(4, 3, 1))
            ("64BIT", Field(4, 2, 1))
            ("IO", Field(4, 0, 1)))))
        ("BAR5", Parameter(Entry<4>(0x24,
          boost::assign::map_list_of
            ("PREFETCHABLE", Field(4, 3, 1))
            ("64BIT", Field(4, 2, 1))
            ("IO", Field(4, 0, 1)))))
        ("CARDBUS_POINTER", Parameter(Entry<4>(0x28)))
        ("SUBSYSTEM_VENDOR_ID", Parameter(Entry<2>(0x2c)))
        ("SUBSYSTEM_ID", Parameter(Entry<2>(0x2e)))
        ("EXPANSION_ROM", Parameter(Entry<4>(0x30,
          boost::assign::map_list_of
            ("ENABLE", Field(4, 0, 1))
            ("BASE_ADDRESS", Field(4, 11, 21)))))
        ("CAPABILITIES_POINTER", Parameter(Entry<1>(0x34)))
        ("INTERRUPT_LINE", Parameter(Entry<1>(0x3c)))
        ("INTERRUPT_PIN", Parameter(Entry<1>(0x3d)))
        ("MIN_GNT", Parameter(Entry<1>(0x3e)))
        ("MAX_LAT", Parameter(Entry<1>(0x3f)))
      ;
    }
  };

  static ConfInfo const config;


  // convenience struct to describe single bit-level access
  struct Accessor
  {
    Parameter const *param_;
    Field const *field_;

    Accessor(Parameter const *pp = 0, Field const *fp = 0):
      param_(pp), field_(fp) { }
  };

  //// Parameter visitors

  // field id extractor
  struct GetFields: public boost::static_visitor<field_map const *>
  {
    template<typename T>
    field_map const *operator()(T& arg) const {return &(arg.fields_);}
    // AnyEntry doesn't have fields_
    field_map const *operator()(AnyEntry const& arg) const {return 0;}
  };
  // parameter size
  struct GetSize: public boost::static_visitor<uint16_t>
  {
    template<typename T>
    uint16_t operator()(T& arg) const {return arg.size();}
  };
  // read from config space
  struct Read: public boost::static_visitor<>
  {
    pci_device *dev_;
    uint8_t *buf_;
    Field const *fp_;

    Read(pci_device *dev, uint8_t *buf, Field const *fp = 0):
      dev_(dev), buf_(buf), fp_(fp) { }
    template<typename T>
    void operator()(T& arg) const {arg.read(dev_, buf_, fp_);}
  };
  // write to config space
  struct Write: public boost::static_visitor<>
  {
    pci_device *dev_;
    uint8_t *buf_;
    Field const *fp_;

    Write(pci_device *dev, uint8_t *buf, Field const *fp = 0):
      dev_(dev), buf_(buf), fp_(fp) { }
    template<typename T>
    void operator()(T& arg) const {arg.write(dev_, buf_, fp_);}
  };

} /* namespace config */ } /* namespace pci */

#endif // _PCI_CONFIG_H
