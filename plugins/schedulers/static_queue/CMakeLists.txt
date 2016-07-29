# Copyright (c) 2016 Satyaki Upadhyay
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

include(HPX_AddLibrary)

macro(add_static_queue_module)
  hpx_debug("add_static_queue_module")
  add_hpx_library(static_queue_scheduler
    PLUGIN
    SOURCES
          "${PROJECT_SOURCE_DIR}/plugins/schedulers/static_queue/static_queue_scheduler_registration.cpp"
    HEADERS
          ${PROJECT_SOURCE_DIR}/hpx/runtime/threads/policies/static_queue_scheduler.hpp
          ${PROJECT_SOURCE_DIR}/hpx/plugins/static_queue_scheduler_plugin_factory.hpp
    FOLDER "Core/Plugins/Schedulers/LocalQueue"
    )
    add_hpx_pseudo_dependencies(core plugins.schedulers.static_queue)
endmacro()

