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
#include <hpx/util/static.hpp>
#include <hpx/runtime/threads/policies/lockfree_queue_backends.hpp>

#include <hpx/runtime/threads/policies/fpga_support/pci.hpp>
#include <hpx/runtime/threads/policies/fpga_support/queue_defs.hpp>

#include <boost/cstdint.hpp>
#include <boost/static_assert.hpp>
#include <boost/atomic/atomic.hpp>
#include <boost/type_traits/is_same.hpp>

namespace hpx { namespace threads { namespace policies
{
    namespace detail
    {
        // default PCI BAR number used by APX project
        const int DEFAULT_APX_BAR = 2;

        template <typename T = boost::uint64_t>
        inline T *cmd2addr(boost::uint8_t *base, int qn, unsigned cmd)
        {
            return reinterpret_cast<T *>(
                base + (qn << (MQ_REQ_BITS + 3)) + (cmd << 3));
        }

        template <typename T>
        bool to_fpga(boost::uint8_t *bar, int queue_num, unsigned cmd,
            T const& data)
        {
            HPX_ASSERT(
                !((cmd & (MQ_REQ_GET_HEAD | MQ_REQ_GET_LAST)) ||
                  (cmd == MQ_REQ_GET_CNT) || (cmd == MQ_REQ_GET_STAT))
            );

            volatile T *addr = cmd2addr<T>(bar, queue_num, cmd);

            PCI::verb("writing to PCI memory space: addr(%llx), data(%llx)", addr, data);
            *addr = data;

            // FIXME: error handling?

            return true;
        }

        template <typename T>
        bool from_fpga(boost::uint8_t *bar, int queue_num, unsigned cmd,
            T& data)
        {
            HPX_ASSERT(
                (cmd & (MQ_REQ_GET_HEAD | MQ_REQ_GET_LAST)) ||
                (cmd == MQ_REQ_GET_CNT) || (cmd == MQ_REQ_GET_STAT)
            );

            volatile T const*addr = cmd2addr<T>(bar, queue_num, cmd);

            PCI::verb("reading from PCI memory space: addr(%llx)", addr);
            data = *addr;

            PCI::verb("read from PCI memory space: addr(%llx), data(%llx)", addr, data);

            // FIXME: error handling?

            return true;
        }

        ///////////////////////////////////////////////////////////////////////
        inline boost::uint8_t* get_pci_device_base(int bar = DEFAULT_APX_BAR);

        struct pci_device
        {
            pci_device(int bar = DEFAULT_APX_BAR)
              : info_(0x10ee), // default vendor ID: Xilinx
                device_(info_)
            {}

            struct tag {};

            static pci_device& get()
            {
                util::static_<pci_device, pci_device::tag> pcidevice;
                return pcidevice.get();
            }

            PCI::DevInfo info_;
            PCI::Device device_;
        };


        inline PCI::Region const& get_pci_device_region(int bar)
        {
            pci_device& device = pci_device::get();
            return device.device_.bar_region(bar);
        }

        inline boost::uint8_t* get_pci_device_base(int bar)
        {
            return reinterpret_cast<boost::uint8_t*>(
                get_pci_device_region(bar).addr_);
        }
    }

    ///////////////////////////////////////////////////////////////////////////
    struct fpga_queue
    {
        fpga_queue(boost::uint64_t device_no)
          : base_(detail::get_pci_device_base()),
            device_no_(static_cast<int>(device_no))
        {
            HPX_ASSERT(0 != base_);
            HPX_ASSERT(boost::uint64_t(-1) != device_no);

            // reset this queue
            PCI::verb("performing reset: addr(%llx)",
                detail::cmd2addr(base_, device_no_, MQ_REQ_RESET));
            *detail::cmd2addr(base_, device_no_, MQ_REQ_RESET) = 0;

            // make sure we have a sufficient number of queues available which
            // support a sufficiently wide word storage
            HPX_ASSERT(*detail::cmd2addr(base_, 0, MQ_REQ_GET_NQ) >
                static_cast<boost::uint64_t>(device_no_));
            HPX_ASSERT(*detail::cmd2addr(base_, 0, MQ_REQ_GET_WSIZE) >= 64ul);
        }

        bool push_left(boost::uint64_t data)
        {
            return detail::to_fpga(base_, device_no_, MQ_REQ_SET_HEAD, data);
        }

        bool push_right(boost::uint64_t data)
        {
            return detail::to_fpga(base_, device_no_, MQ_REQ_SET_LAST, data);
        }

        bool pop_left(boost::uint64_t& data)
        {
            return detail::from_fpga(base_, device_no_, MQ_REQ_GET_HEAD, data);
        }

        bool pop_right(boost::uint64_t& data)
        {
            return detail::from_fpga(base_, device_no_, MQ_REQ_GET_LAST, data);
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
    // FPGA queue without overflow
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
            return queue_.push_left(reinterpret_cast<boost::uint64_t>(val));
        }

        bool pop(reference val, bool steal = true)
        {
            if (steal)
                return queue_.pop_left(reinterpret_cast<boost::uint64_t&>(val));
            return queue_.pop_right(reinterpret_cast<boost::uint64_t&>(val));
        }

        bool empty()
        {
            return queue_.empty();
        }

        boost::uint64_t max_items() const
        {
            return queue_.max_items();
        }

      private:
        container_type queue_;
    };

    struct fpga_fifo
    {
        template <typename T>
        struct apply
        {
            BOOST_STATIC_ASSERT(sizeof(T) == sizeof(boost::uint64_t));
            typedef fpga_queue_backend<T> type;
        };
    };

    ///////////////////////////////////////////////////////////////////////////
    template <typename T>
    struct fpga_queue_with_overflow_backend
    {
        typedef fpga_queue container_type;
        typedef T value_type;
        typedef T& reference;
        typedef T const& const_reference;
        typedef boost::uint64_t size_type;

        fpga_queue_with_overflow_backend(
                size_type initial_size = 0,
                size_type num_thread = size_type(-1))
          : fpga_queue_(initial_size, num_thread),
            count_(0),
            fpga_queue_max_size_(fpga_queue_.max_items()),
            overflow_queue_(initial_size, num_thread)
        {}

        bool push(const_reference val, bool other_end = false)
        {
            // If limit of hardware queue is reached, push into overflow,
            // otherwise push into hardware queue.
            if (++count_ >= static_cast<boost::int64_t>(fpga_queue_max_size_))
                return overflow_queue_.push(val, other_end);

            return fpga_queue_.push(val, other_end);
        }

        bool pop(reference val, bool steal = true)
        {
            // If there is at least one item in the hardware queue, take it,
            // otherwise return false.
            if (count_-- < 0)
            {
                ++count_;
                return false;
            }

            bool result = fpga_queue_.pop(val, steal);

            // Move one item from the overflow queue into the hardware queue,
            // if possible.
            value_type next_val;
            if (overflow_queue_.pop(next_val))
            {
                // If limit of hardware queue is reached, push into overflow,
                // otherwise push into hardware queue.
                if (++count_ >= static_cast<boost::int64_t>(fpga_queue_max_size_))
                    overflow_queue_.push(next_val, true);
                else
                    fpga_queue_.push(next_val);
            }

            return result;
        }

        bool empty() const
        {
            return count_.load() <= 0;
        }

    private:
        typedef typename fpga_fifo::apply<T>::type fpga_queue_type;
        typedef typename lockfree_fifo::apply<T>::type overflow_queue_type;

        fpga_queue_type fpga_queue_;
        std::atomic<boost::int64_t> count_;
        std::size_t fpga_queue_max_size_;
        overflow_queue_type overflow_queue_;
    };

    struct fpga_overflow_fifo
    {
        template <typename T>
        struct apply
        {
            BOOST_STATIC_ASSERT(sizeof(T) == sizeof(boost::uint64_t));
            typedef fpga_queue_with_overflow_backend<T> type;
        };
    };
}}}

#endif
#endif


