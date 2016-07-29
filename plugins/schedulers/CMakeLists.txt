# Copyright (c) 2016 Satyaki Upadhyay
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

set(scheduler_plugins
    local_priority_queue
    local_queue
    static_priority_queue
    static_queue)

foreach(type ${scheduler_plugins})
  add_hpx_pseudo_target(plugins.schedulers.${type})
  add_subdirectory(${type})
  add_hpx_pseudo_dependencies(plugins.schedulers plugins.schedulers.${type})
endforeach()

macro(add_scheduler_modules)
  add_local_priority_queue_module()
  add_local_queue_module()
  add_static_priority_queue_module()
  add_static_queue_module()
endmacro()
