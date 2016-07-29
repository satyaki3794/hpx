//  Copyright (c) 2016 Satyaki Upadhyay
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <hpx/runtime/threads/policies/static_queue_scheduler.hpp>
#include <hpx/plugins/static_queue_scheduler_plugin_factory.hpp>


///////////////////////////////////////////////////////////////////////////////
// Add this once for each module
HPX_REGISTER_SCHEDULER_PLUGIN_MODULE_DYNAMIC();

// Add this once for each supported plugin type
typedef hpx::threads::policies::static_queue_scheduler<> s_sh;
HPX_REGISTER_SQ_SCHEDULER_PLUGIN_FACTORY(s_sh, static_queue_scheduler);
