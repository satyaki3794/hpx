//  Copyright (c) 2016 Satyaki Upadhyay
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#if !defined(HPX_HIERARCHY_SCHEDULER_PLUGIN_FACTORY)
#define HPX_HIERARCHY_SCHEDULER_PLUGIN_FACTORY

#include <hpx/config.hpp>
#include <hpx/plugins/unique_plugin_name.hpp>
#include <hpx/plugins/plugin_registry.hpp>

#include <hpx/runtime/threads/policies/scheduler_base.hpp>
#include <hpx/plugins/scheduler_plugin_factory_base.hpp>

///////////////////////////////////////////////////////////////////////////////
namespace hpx{ namespace plugins
{
    ///////////////////////////////////////////////////////////////////////////
    template <typename SchedulerPlugin>
    struct hierarchy_scheduler_plugin_factory : public scheduler_plugin_factory_base
    {
        hierarchy_scheduler_plugin_factory(hpx::util::section const* global_cfg,
            hpx::util::section const* plugin_cfg, bool is_enabled)
          : is_enabled_(is_enabled)
        {
            // store the configuration settings
            if (NULL != global_cfg)
                global_settings_ = *global_cfg;
            if (NULL != plugin_cfg)
                local_settings_ = *plugin_cfg;
        }

        /// Create a new instance of a example_plugin
        ///
        /// \returns Returns the newly created instance of the plugin
        ///          supported by this factory
        threads::policies::scheduler_base* create(std::size_t num_queues_=1,
            std::size_t num_high_priority_queues_=1,
            std::size_t max_queue_thread_count_=1000,
            std::size_t numa_sensitive_=0,
            char const* description_="hierarchy_scheduler")
        {
            if (is_enabled_){
                typedef hpx::threads::policies::hierarchy_scheduler<> sh;
                typedef sh::init_parameter_type init;
                return new sh(init(num_queues_, num_high_priority_queues_,
                    max_queue_thread_count_, numa_sensitive_, description_));
            }
            return 0;
        }

    protected:
        hpx::util::section global_settings_;
        hpx::util::section local_settings_;
        bool is_enabled_;
    };
}}

///////////////////////////////////////////////////////////////////////////////
/// This macro is used create and to register a minimal example factory with
/// Hpx.Plugin.
#define HPX_REGISTER_HIERARCHY_SCHEDULER_PLUGIN_FACTORY(SchedulerPlugin, pluginname)                                            \
    HPX_REGISTER_SCHEDULER_PLUGIN_FACTORY_BASE(                                                                       \
        hpx::plugins::hierarchy_scheduler_plugin_factory<SchedulerPlugin>, pluginname)           \
    HPX_DEF_UNIQUE_PLUGIN_NAME(                                                                                       \
        hpx::plugins::hierarchy_scheduler_plugin_factory<SchedulerPlugin>, pluginname)           \
    template struct hpx::plugins::hierarchy_scheduler_plugin_factory<SchedulerPlugin>;           \
    HPX_REGISTER_PLUGIN_REGISTRY_2(SchedulerPlugin, pluginname)                                                       \
/**/

#endif

