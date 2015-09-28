// Copyright (c) 2011 Maciej Brodowicz
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying 
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef _APX_DRIVER_PUBLIC_H
#define _APX_DRIVER_PUBLIC_H

/////////////////////////////////////////////////////////////////////////////
//// control data values (64-bit)

// magic: \0-terminated "AccelPX" string (little endian)
#define APX_MAGIC 0x0058506c65636341
// register layout revision (major - 2 bytes, minor - 1 byte, tiny - 1 byte);
// zero major is experimental
#define APX_VERSION 0x00000100

//// metadata in control space (always implemented)
// magic (RO)
#define APX_CTL_MAGIC              0
// register index for hardware revision (RO)
#define APX_CTL_VERSION_OFFSET     1
// register index for start of board control registers (RO)
#define APX_CTL_CTLREG_BASE_OFFSET 2
// register index for control space size (number of registers) (RO)
#define APX_CTL_CTLREG_SIZE_OFFSET 3
// register index for start of accelerator control registers (RO)
#define APX_CTL_ACCREG_BASE_OFFSET 4
// register index for accelerator control size (RO)
#define APX_CTL_ACCREG_SIZE_OFFSET 5
// register index for start of DMA control registers (RO)
#define APX_CTL_DMAREG_BASE_OFFSET 6
// register index for DMA control size (RO)
#define APX_CTL_DMAREG_SIZE_OFFSET 7

//// control space descriptor registers (relative to CTLREG_BASE)
// available control capabilities (RO)
#define APX_CTL_CAP_OFFSET         0
// command channel (RW)
#define APX_CTL_CMD_OFFSET         1
// data channel (RW)
#define APX_CTL_DATA_OFFSET        2
// auxiliary data channels (RW)
#define APX_CTL_AUX0_OFFSET        3
#define APX_CTL_AUX1_OFFSET        4

//// control register section for the accelerator resource
// accelerator capabilities (RO)
#define APX_CTL_ACCCAP_OFFSET 0
// thread parameter limits in 16-bit fields; from LSB to MSB: number of
// thread blocks, active registers per block, block stride (RO)
#define APX_CTL_ACCPAR_OFFSET 1

//// accelerator capability flags

#define APX_ACCEL_CAP_____

/////////////////////////////////////////////////////////////////////////////
//// device definitions

// device name strings (under "/dev/")
#define APX_ACC_DEVSTR "apx%d_acc"
#define APX_CTL_DEVSTR "apx%d_ctl"
#define APX_MEM_DEVSTR "apx%d_mem"

//// minor device number range (for scanning)
#define APX_DEVNUM_RANGE 128

//// default PCI vendor and device ID
#define APX_VENDOR_ID 0x10ee
#define APX_DEVICE_ID 0x5850

//// BAR numbers for active apertures
#define APX_ACC_BARNUM 0
#define APX_CTL_BARNUM 2
#define APX_MEM_BARNUM 4

#endif // _APX_DRIVER_PUBLIC_H


