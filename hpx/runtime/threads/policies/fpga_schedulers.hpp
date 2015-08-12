//  Copyright (c) 2015 Hartmut Kaiser
//  Copyright (c) 2015 Maciej Brodowicz
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#if !defined(HPX_THREADS_FPGA_SCHEDULER_AUG_12_2015_1011AM)
#define HPX_THREADS_FPGA_SCHEDULER_AUG_12_2015_1011AM

#include <hpx/config.hpp>

#if defined(HPX_HAVE_FPGA_QUEUES)

#include <hpx/runtime/threads/policies/lockfree_queue_backends.hpp>
#include <hpx/runtime/threads/policies/local_queue_scheduler.hpp>
#include <hpx/runtime/threads/policies/local_priority_queue_scheduler.hpp>
#include <hpx/runtime/threads/policies/fpga_queue_backend.hpp>

#include <boost/thread/mutex.hpp>

///////////////////////////////////////////////////////////////////////////////
namespace hpx { namespace threads { namespace policies
{
    typedef local_queue_scheduler<
            boost::mutex,
            fpga_overflow_fifo,
            lockfree_fifo,          // FIFO staged queuing
            lockfree_lifo           // LIFO terminated queuing
        > local_fpga_scheduler;

    typedef local_priority_queue_scheduler<
            boost::mutex,
            fpga_overflow_fifo,
            lockfree_fifo,          // FIFO staged queuing
            lockfree_lifo           // LIFO terminated queuing
        > local_fpga_priority_scheduler;
}}}

#endif
#endif
