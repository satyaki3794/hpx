//  Copyright (c) 2015 Hartmut Kaiser
//  Copyright (c) 2015 Maciej Brodowicz
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#if !defined(HPX_THREADS_FPGA_QUEUE_BACKEND_JUL_28_2015_0101PM)
#define HPX_THREADS_FPGA_QUEUE_BACKEND_JUL_28_2015_0101PM

#include <hpx/config.hpp>

#if defined(HPX_HAVE_FPGA_QUEUES)
#include <hpx/util/assert.hpp>

#include <immintrin.h>

#include <boost/cstdint.hpp>
#include <boost/static_assert.hpp>
#include <boost/atomic/atomic.hpp>
#include <boost/type_traits/is_same.hpp>

namespace hpx { namespace threads { namespace policies
{
    namespace detail
    {
        template <typename T = boost::uint64_t>
        inline T *cmd2addr(boost::uint8_t *base, int qn, unsigned cmd)
        {
            return reinterpret_cast<T *>(
                base + (qn << (MQ_REQ_BITS + 3)) + (cmd << 3));
        }

        template <typename T>
        bool to_fpga(boost::uint8_t *bar, int queue_num, unsigned cmd,
            T const* data)
        {
            HPX_ASSERT(
                !((cmd & (MQ_REQ_GET_HEAD | MQ_REQ_GET_LAST)) ||
                  (cmd == MQ_REQ_GET_CNT) || (cmd == MQ_REQ_GET_STAT))
            );

            T *addr = cmd2addr<T>(bar, queue_num, cmd);

            // write to PCI memory space
            *addr = *data;

            // FIXME: error handling?

            return true;
        }

        template <typename T>
        bool from_fpga(boost::uint8_t *bar, int queue_num, unsigned cmd,
            T* data)
        {
            HPX_ASSERT(
                (cmd & (MQ_REQ_GET_HEAD | MQ_REQ_GET_LAST)) ||
                (cmd == MQ_REQ_GET_CNT) || (cmd == MQ_REQ_GET_STAT)
            );

            T const*addr = cmd2addr<T>(bar, queue_num, cmd);

            // read from PCI memory space
            *data = *addr;

            // FIXME: error handling?

            return true;
        }
    }

    ///////////////////////////////////////////////////////////////////////////
    struct fpga_queue
    {
        fpga_queue(boost::uint64_t device_no)
          : base_(0), device_no_(static_cast<int>(device_no)), count_(0)
        {
            PCI::DevInfo ;
            info.vendor_ = APX_VENDOR_ID;
            info.device_ = device_no;

            PCI::Device device(info);
            base_ = reinterpret_cast<boost::uint8_t>(
                device.bar_region(info.bar_).addr_);

            HPX_ASSERT(0 != base_);

            // make sure we have a sufficient number of queues available which
            // support a sufficiently wide word storage
            HPX_ASSERT(*cmd2addr(base_, 0, MQ_REQ_GET_NQ) > device_no_);
            HPX_ASSERT(*cmd2addr(base_, 0, MQ_REQ_GET_WSIZE) >= 64);

            // reset this queue
            *cmd2addr(base_, device_no_, MQ_REQ_RESET) = 0;
        }

        bool push_left(boost::uint64_t data)
        {
            detail::to_fpga(base_, device_no_, MQ_REQ_SET_HEAD, &data);
        }

        bool push_right(boost::uint64_t)
        {
            return detail::to_fpga(base_, device_no_, MQ_REQ_SET_LAST, &data);
        }

        bool pop_left(boost::uint64_t& data)
        {
            return detail::to_fpga(base_, device_no_, MQ_REQ_GET_HEAD, &data);
        }

        bool pop_left(boost::uint64_t& data)
        {
            return detail::to_fpga(base_, device_no_, MQ_REQ_GET_LAST, &data);
        }

        bool empty() const
        {
            boost::uint64_t count = 0;
            detail::from_fpga(base_, device_no_, MQ_REQ_GET_CNT, count);
            return 0 == count;
        }

        boost::uint64_t max_items() const
        {
            boost::uint64_t size = 0;
            detail::from_fpga(base_, device_no_, MQ_REQ_GET_SIZE, size);
            return size;
        }

    private:
        boost::uint8_t* base_;
        int device_no_;
    };

    ///////////////////////////////////////////////////////////////////////////
    template <typename T>
    struct fpga_queue_backend
    {
        typedef fpga_queue container_type;
        typedef T value_type;
        typedef T& reference;
        typedef T const& const_reference;
        typedef boost::uint64_t size_type;

        fpga_queue_backend(
                size_type initial_size = 0,
                size_type num_thread = size_type(-1))
          : queue_(num_thread)
        {}

        bool push(const_reference val, bool other_end = false)
        {
            if (other_end)
                return queue_.push_right(reinterpret_cast<boost::uint64_t>(val));
            return queue_.push_left(reinterpret_cast<boost::uint64_t>(val)));
        }

        bool pop(reference val, bool steal = true)
        {
            if (steal)
                return queue_.pop_left(reinterpret_cast<boost::uint64_t&>(val)));
            return queue_.pop_right(reinterpret_cast<boost::uint64_t&>(val)));
        }

        bool empty()
        {
            return queue_.empty();
        }

      private:
        container_type queue_;
    };

    ///////////////////////////////////////////////////////////////////////////
    struct fpga_fifo
    {
        template <typename T>
        struct apply
        {
            BOOST_STATIC_ASSERT(sizeof(T) == sizeof(boost::uint64_t));
            typedef fpga_queue_backend<T> type;
        };
    };
}}}

#endif
#endif


