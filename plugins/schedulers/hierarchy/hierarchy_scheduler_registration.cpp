//  Copyright (c) 2016 Satyaki Upadhyay
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <hpx/runtime/threads/policies/hierarchy_scheduler.hpp>
#include <hpx/plugins/hierarchy_scheduler_plugin_factory.hpp>


///////////////////////////////////////////////////////////////////////////////
// Add this once for each module
HPX_REGISTER_SCHEDULER_PLUGIN_MODULE_DYNAMIC();

// Add this once for each supported plugin type
typedef hpx::threads::policies::hierarchy_scheduler<> h_sh;
HPX_REGISTER_HIERARCHY_SCHEDULER_PLUGIN_FACTORY(h_sh, hierarchy_scheduler);
