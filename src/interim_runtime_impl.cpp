//  Copyright (c) 2007-2015 Hartmut Kaiser
//  Copyright (c)      2011 Bryce Lelbach
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <hpx/config.hpp>
#include <hpx/state.hpp>
#include <hpx/exception.hpp>
#include <hpx/interim_runtime_impl.hpp>
#include <hpx/util/bind.hpp>
#include <hpx/util/logging.hpp>
#include <hpx/util/set_thread_name.hpp>
#include <hpx/util/thread_mapper.hpp>
#include <hpx/util/apex.hpp>
#include <hpx/runtime/agas/big_boot_barrier.hpp>
#include <hpx/runtime/get_config_entry.hpp>
#include <hpx/runtime/components/console_error_sink.hpp>
#include <hpx/runtime/components/server/console_error_sink.hpp>
#include <hpx/runtime/components/runtime_support.hpp>
#include <hpx/runtime/shutdown_function.hpp>
#include <hpx/runtime/startup_function.hpp>
#include <hpx/runtime/threads/coroutines/detail/context_impl.hpp>
#include <hpx/runtime/threads/interim_threadmanager_impl.hpp>
#include <hpx/runtime/threads/policies/scheduler_base.hpp>
#include <hpx/lcos/latch.hpp>

#include <boost/cstdint.hpp>
#include <boost/exception_ptr.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/condition.hpp>
#include <boost/ref.hpp>

#include <iostream>
#include <list>
#include <mutex>
#include <sstream>
#include <string>
#include <utility>
#include <vector>
#include <memory>

#if defined(_WIN64) && defined(_DEBUG) && !defined(HPX_HAVE_FIBER_BASED_COROUTINES)
#include <io.h>
#endif

namespace hpx {

    ///////////////////////////////////////////////////////////////////////////
    // There is no need to protect these global from thread concurrent access
    // as they are access during early startup only.

    extern std::list<startup_function_type> global_pre_startup_functions;
    extern std::list<startup_function_type> global_startup_functions;

    extern std::list<shutdown_function_type> global_pre_shutdown_functions;
    extern std::list<shutdown_function_type> global_shutdown_functions;

    ///////////////////////////////////////////////////////////////////////////
    // void register_pre_startup_function(startup_function_type f)
    // {
    //     runtime* rt = get_runtime_ptr();
    //     if (nullptr != rt) {
    //         if (rt->get_state() > state_pre_startup) {
    //             HPX_THROW_EXCEPTION(invalid_status,
    //                 "register_pre_startup_function",
    //                 "Too late to register a new pre-startup function.");
    //             return;
    //         }
    //         rt->add_pre_startup_function(std::move(f));
    //     }
    //     else {
    //         global_pre_startup_functions.push_back(std::move(f));
    //     }
    // }

    // void register_startup_function(startup_function_type f)
    // {
    //     runtime* rt = get_runtime_ptr();
    //     if (nullptr != rt) {
    //         if (rt->get_state() > state_startup) {
    //             HPX_THROW_EXCEPTION(invalid_status,
    //                 "register_startup_function",
    //                 "Too late to register a new startup function.");
    //             return;
    //         }
    //         rt->add_startup_function(std::move(f));
    //     }
    //     else {
    //         global_startup_functions.push_back(std::move(f));
    //     }
    // }

    // void register_pre_shutdown_function(shutdown_function_type f)
    // {
    //     runtime* rt = get_runtime_ptr();
    //     if (nullptr != rt) {
    //         if (rt->get_state() > state_pre_shutdown) {
    //             HPX_THROW_EXCEPTION(invalid_status,
    //                 "register_pre_shutdown_function",
    //                 "Too late to register a new pre-shutdown function.");
    //             return;
    //         }
    //         rt->add_pre_shutdown_function(std::move(f));
    //     }
    //     else {
    //         global_pre_shutdown_functions.push_back(std::move(f));
    //     }
    // }

    // void register_shutdown_function(shutdown_function_type f)
    // {
    //     runtime* rt = get_runtime_ptr();
    //     if (nullptr != rt) {
    //         if (rt->get_state() > state_shutdown) {
    //             HPX_THROW_EXCEPTION(invalid_status,
    //                 "register_shutdown_function",
    //                 "Too late to register a new shutdown function.");
    //             return;
    //         }
    //         rt->add_shutdown_function(std::move(f));
    //     }
    //     else {
    //         global_shutdown_functions.push_back(std::move(f));
    //     }
    // }

    ///////////////////////////////////////////////////////////////////////////
    interim_runtime_impl::interim_runtime_impl(
            util::runtime_configuration & rtcfg,
            std::shared_ptr<threads::policies::scheduler_base> scheduler,
            runtime_mode locality_mode, std::size_t num_threads,
            threads::policies::init_affinity_data const& init_affinity)
      : runtime(rtcfg, init_affinity),
        scheduler_(scheduler),
        mode_(locality_mode), result_(0), num_threads_(num_threads),
        main_pool_(1,
            util::bind(&interim_runtime_impl::init_tss, This(), "main-thread",
                util::placeholders::_1, util::placeholders::_2, false),
            util::bind(&interim_runtime_impl::deinit_tss, This()), "main_pool"),
        io_pool_(rtcfg.get_thread_pool_size("io_pool"),
            util::bind(&interim_runtime_impl::init_tss, This(), "io-thread",
                util::placeholders::_1, util::placeholders::_2, true),
            util::bind(&interim_runtime_impl::deinit_tss, This()), "io_pool"),
        timer_pool_(rtcfg.get_thread_pool_size("timer_pool"),
            util::bind(&interim_runtime_impl::init_tss, This(), "timer-thread",
                util::placeholders::_1, util::placeholders::_2, true),
            util::bind(&interim_runtime_impl::deinit_tss, This()), "timer_pool"),
        notifier_(interim_runtime_impl::
            get_notification_policy("worker-thread")),
        thread_manager_(
            new hpx::threads::interim_threadmanager_impl(
                timer_pool_, scheduler_, notifier_, num_threads)),
        parcel_handler_(rtcfg, thread_manager_.get(),
            util::bind(&interim_runtime_impl::init_tss, This(), "parcel-thread",
                util::placeholders::_1, util::placeholders::_2, true),
            util::bind(&interim_runtime_impl::deinit_tss, This())),
        agas_client_(parcel_handler_, ini_, mode_),
        init_logging_(ini_, mode_ == runtime_mode_console, agas_client_),
        applier_(parcel_handler_, *thread_manager_)
    {
        components::server::get_error_dispatcher().register_error_sink(
            &interim_runtime_impl::default_errorsink, default_error_sink_);

        // in AGAS v2, the runtime pointer (accessible through get_runtime
        // and get_runtime_ptr) is already initialized at this point.
        applier_.init_tss();

#if defined(HPX_HAVE_SECURITY)
        // once all has been initialized, finalize security data for bootstrap
        this->init_security();
#endif
        // now, launch AGAS and register all nodes, launch all other components
        agas_client_.initialize(
            parcel_handler_, boost::uint64_t(runtime_support_.get()),
            boost::uint64_t(memory_.get()));
        parcel_handler_.initialize(agas_client_, &applier_);

        applier_.initialize(boost::uint64_t(runtime_support_.get()),
        boost::uint64_t(memory_.get()));

#if defined(HPX_HAVE_SECURITY)
        // enable parcel capability checking
        applier_.enable_verify_capabilities();
#endif

        // copy over all startup functions registered so far
        for (startup_function_type& f : global_pre_startup_functions)
        {
            add_pre_startup_function(std::move(f));
        }
        global_pre_startup_functions.clear();

        for (startup_function_type& f : global_startup_functions)
        {
            add_startup_function(std::move(f));
        }
        global_startup_functions.clear();

        for (shutdown_function_type& f : global_pre_shutdown_functions)
        {
            add_pre_shutdown_function(std::move(f));
        }
        global_pre_shutdown_functions.clear();

        for (shutdown_function_type& f : global_shutdown_functions)
        {
            add_shutdown_function(std::move(f));
        }
        global_shutdown_functions.clear();

        // set state to initialized
        set_state(state_initialized);
    }

    ///////////////////////////////////////////////////////////////////////////
    interim_runtime_impl::~interim_runtime_impl()
    {
        LRT_(debug) << "~interim_runtime_impl(entering)";

        runtime_support_->delete_function_lists();

        // stop all services
        parcel_handler_.stop();     // stops parcel pools as well
        thread_manager_->stop();    // stops timer_pool_ as well
        io_pool_.stop();

        // unload libraries
        runtime_support_->tidy();

        LRT_(debug) << "~interim_runtime_impl(finished)";
    }

    int pre_main(hpx::runtime_mode);

    threads::thread_state_enum
    interim_runtime_impl::run_helper(
        util::function_nonser<runtime::hpx_main_function_type> func, int& result)
    {
        LBT_(info) << "(2nd stage) interim_runtime_impl::run_helper: launching pre_main";

        // Change our thread description, as we're about to call pre_main
        threads::set_thread_description(threads::get_self_id(), "pre_main");

        // Finish the bootstrap
        result = hpx::pre_main(mode_);
        if (result) {
            LBT_(info) << "interim_runtime_impl::run_helper: bootstrap "
                          "aborted, bailing out";
            return threads::terminated;
        }

        LBT_(info) << "(4th stage) interim_runtime_impl::run_helper: bootstrap complete";
        set_state(state_running);

        parcel_handler_.enable_alternative_parcelports();

        // reset all counters right before running main, if requested
        if (get_config_entry("hpx.print_counter.startup", "0") == "1")
        {
            bool reset = false;
            if (get_config_entry("hpx.print_counter.reset", "0") == "1")
                reset = true;

            error_code ec(lightweight);     // ignore errors
            evaluate_active_counters(reset, "startup", ec);
        }

        // Connect back to given latch if specified
        std::string connect_back_to(
            get_config_entry("hpx.on_startup.wait_on_latch", ""));
        if (!connect_back_to.empty())
        {
            // inform launching process that this locality is up and running
            hpx::lcos::latch l;
            l.connect_to(connect_back_to);
            l.count_down_and_wait();
        }

        // Now, execute the user supplied thread function (hpx_main)
        if (!!func) {
            // Change our thread description, as we're about to call hpx_main
            threads::set_thread_description(threads::get_self_id(), "hpx_main");

            // Call hpx_main
            result = func();
        }
        return threads::terminated;
    }

    int interim_runtime_impl::start(
        util::function_nonser<hpx_main_function_type> const& func, bool blocking)
    {
#if defined(_WIN64) && defined(_DEBUG) && !defined(HPX_HAVE_FIBER_BASED_COROUTINES)
        // needs to be called to avoid problems at system startup
        // see: http://connect.microsoft.com/VisualStudio/feedback/ViewFeedback.aspx?FeedbackID=100319
        _isatty(0);
#endif
        // {{{ early startup code - local

        // initialize instrumentation system
        util::apex_init();

        LRT_(info) << "cmd_line: " << get_config().get_cmd_line();

        LBT_(info) << "(1st stage) interim_runtime_impl::start: booting locality " //-V128
                   << here() << " on " << num_threads_ << " OS-thread"
                   << ((num_threads_ == 1) ? "" : "s");

        // start runtime_support services
        runtime_support_->run();
        LBT_(info) << "(1st stage) interim_runtime_impl::start: started "
                      "runtime_support component";

        // start the io pool
        io_pool_.run(false);
        LBT_(info) << "(1st stage) interim_runtime_impl::start: started the application "
                      "I/O service pool";

        // start the thread manager
        thread_manager_->run(num_threads_);
        LBT_(info) << "(1st stage) interim_runtime_impl::start: started threadmanager";
        // }}}

        // invoke the AGAS v2 notifications
        agas::get_big_boot_barrier().trigger();

        // {{{ launch main
        // register the given main function with the thread manager
        LBT_(info) << "(1st stage) interim_runtime_impl::start: launching run_helper "
                      "HPX thread";

        threads::thread_init_data data(
            util::bind(&interim_runtime_impl::run_helper, this, func,
                boost::ref(result_)),
            "run_helper", 0, threads::thread_priority_normal, std::size_t(-1),
            threads::get_stack_size(threads::thread_stacksize_large));

        threads::thread_id_type id = threads:: invalid_thread_id;
        thread_manager_->register_thread(data, id);
        this->runtime::starting();
        // }}}

        // block if required
        if (blocking)
            return wait();     // wait for the shutdown_action to be executed

        // Register this thread with the runtime system to allow calling certain
        // HPX functionality from the main thread.
        init_tss("main-thread", 0, "", false);

        return 0;   // return zero as we don't know the outcome of hpx_main yet
    }

    int interim_runtime_impl::start(bool blocking)
    {
        util::function_nonser<hpx_main_function_type> empty_main;
        return start(empty_main, blocking);
    }

    ///////////////////////////////////////////////////////////////////////////
    void interim_runtime_impl::wait_helper(
        boost::mutex& mtx, boost::condition_variable& cond, bool& running)
    {
        // signal successful initialization
        {
            std::lock_guard<boost::mutex> lk(mtx);
            running = true;
            cond.notify_all();
        }

        // register this thread with any possibly active Intel tool
        HPX_ITT_THREAD_SET_NAME("main-thread#wait_helper");

        // set thread name as shown in Visual Studio
        util::set_thread_name("main-thread#wait_helper");

#if defined(HPX_HAVE_APEX)
        apex::register_thread("main-thread#wait_helper");
#endif
        // wait for termination
        runtime_support_->wait();

        // stop main thread pool
        main_pool_.stop();
    }

    int interim_runtime_impl::wait()
    {
        LRT_(info) << "interim_runtime_impl: about to enter wait state";

        // start the wait_helper in a separate thread
        boost::mutex mtx;
        boost::condition_variable cond;
        bool running = false;

        boost::thread t (util::bind(
                &interim_runtime_impl::wait_helper,
                this, boost::ref(mtx), boost::ref(cond), boost::ref(running)
            ));

        // wait for the thread to run
        {
            boost::unique_lock<boost::mutex> lk(mtx);
            while (!running)
                cond.wait(lk);
        }

        // use main thread to drive main thread pool
        main_pool_.thread_run(0);

        // block main thread
        t.join();

        LRT_(info) << "interim_runtime_impl: exiting wait state";
        return result_;
    }

    ///////////////////////////////////////////////////////////////////////////
    // First half of termination process: stop thread manager,
    // schedule a task managed by timer_pool to initiate second part
    void interim_runtime_impl::stop(bool blocking)
    {
        LRT_(warning) << "interim_runtime_impl: about to stop services";

        // flush all parcel buffers, stop buffering parcels at this point
        //parcel_handler_.do_background_work(true);

        // execute all on_exit functions whenever the first thread calls this
        this->runtime::stopping();

        // stop interim_runtime_impl services (threads)
        thread_manager_->stop(false);    // just initiate shutdown

        if (threads::get_self_ptr())
        {
            // schedule task on separate thread to execute stopped() below
            // this is necessary as this function (stop()) might have been called
            // from a HPX thread, so it would deadlock by waiting for the thread
            // manager
            boost::mutex mtx;
            boost::condition_variable cond;
            boost::unique_lock<boost::mutex> l(mtx);

            boost::thread t(util::bind(&interim_runtime_impl::stopped, this, blocking,
                boost::ref(cond), boost::ref(mtx)));
            cond.wait(l);

            t.join();
        }
        else
        {
            runtime_support_->stopped();         // re-activate shutdown HPX-thread
            thread_manager_->stop(blocking);     // wait for thread manager

            // this disables all logging from the main thread
            deinit_tss();

            LRT_(info) << "interim_runtime_impl: stopped all services";
        }

        // stop the rest of the system
        parcel_handler_.stop(blocking);     // stops parcel pools as well
        io_pool_.stop();                    // stops io_pool_ as well

        deinit_tss();
    }

    // Second step in termination: shut down all services.
    // This gets executed as a task in the timer_pool io_service and not as
    // a HPX thread!
    void interim_runtime_impl::stopped(
        bool blocking, boost::condition_variable& cond, boost::mutex& mtx)
    {
        // wait for thread manager to exit
        runtime_support_->stopped();         // re-activate shutdown HPX-thread
        thread_manager_->stop(blocking);     // wait for thread manager

        // this disables all logging from the main thread
        deinit_tss();

        LRT_(info) << "interim_runtime_impl: stopped all services";

        std::lock_guard<boost::mutex> l(mtx);
        cond.notify_all();                  // we're done now
    }

    ///////////////////////////////////////////////////////////////////////////
    void interim_runtime_impl::report_error(
        std::size_t num_thread, boost::exception_ptr const& e)
    {
        // Early and late exceptions, errors outside of HPX-threads
        if (!threads::get_self_ptr() || !threads::threadmanager_is(state_running))
        {
            // report the error to the local console
            detail::report_exception_and_continue(e);

            // store the exception to be able to rethrow it later
            {
                std::lock_guard<boost::mutex> l(mtx_);
                exception_ = e;
            }

            // initiate stopping the runtime system
            runtime_support_->notify_waiting_main();
            stop(false);

            return;
        }

        // The components::console_error_sink is only applied at the console,
        // so the default error sink never gets called on the locality, meaning
        // that the user never sees errors that kill the system before the
        // error parcel gets sent out. So, before we try to send the error
        // parcel (which might cause a double fault), print local diagnostics.
        components::server::console_error_sink(e);

        // Report this error to the console.
        naming::gid_type console_id;
        if (agas_client_.get_console_locality(console_id))
        {
            if (agas_client_.get_local_locality() != console_id) {
                components::console_error_sink(
                    naming::id_type(console_id, naming::id_type::unmanaged), e);
            }
        }

        components::stubs::runtime_support::terminate_all(
            naming::get_id_from_locality_id(HPX_AGAS_BOOTSTRAP_PREFIX));
    }

    void interim_runtime_impl::report_error(
        boost::exception_ptr const& e)
    {
        return report_error(hpx::get_worker_thread_num(), e);
    }

    void interim_runtime_impl::rethrow_exception()
    {
        if (state_.load() > state_running)
        {
            std::lock_guard<boost::mutex> l(mtx_);
            if (exception_)
            {
                boost::exception_ptr e = exception_;
                exception_ = boost::exception_ptr();
                boost::rethrow_exception(e);
            }
        }
    }

    ///////////////////////////////////////////////////////////////////////////
    int interim_runtime_impl::run(
        util::function_nonser<hpx_main_function_type> const& func)
    {
        // start the main thread function
        start(func);

        // now wait for everything to finish
        wait();
        stop();

        parcel_handler_.stop();      // stops parcelport for sure

        rethrow_exception();
        return result_;
    }

    ///////////////////////////////////////////////////////////////////////////
    int interim_runtime_impl::run()
    {
        // start the main thread function
        start();

        // now wait for everything to finish
        int result = wait();
        stop();

        parcel_handler_.stop();      // stops parcelport for sure

        rethrow_exception();
        return result;
    }

    ///////////////////////////////////////////////////////////////////////////
    void interim_runtime_impl::default_errorsink(
        std::string const& msg)
    {
        // log the exception information in any case
        LERR_(always) << "default_errorsink: unhandled exception: " << msg;

        std::cerr << msg << std::endl;
    }

    ///////////////////////////////////////////////////////////////////////////
    threads::policies::callback_notifier interim_runtime_impl::
        get_notification_policy(char const* prefix)
    {
        typedef void (interim_runtime_impl::*report_error_t)(
            std::size_t, boost::exception_ptr const&);

        using util::placeholders::_1;
        using util::placeholders::_2;
        return notification_policy_type(
            util::bind(&interim_runtime_impl::init_tss, This(), prefix, _1, _2, false),
            util::bind(&interim_runtime_impl::deinit_tss, This()),
            util::bind(static_cast<report_error_t>(&interim_runtime_impl::report_error),
                This(), _1, _2));
    }

    void interim_runtime_impl::init_tss_ex(
        char const* context, std::size_t num, char const* postfix,
        bool service_thread, error_code& ec)
    {
        // initialize our TSS
        this->runtime::init_tss();

        // initialize applier TSS
        applier_.init_tss();

        // set the thread's name, if it's not already set
        if (nullptr == runtime::thread_name_.get())
        {
            std::string* fullname = new std::string(context);
            if (postfix && *postfix)
                *fullname += postfix;
            *fullname += "#" + std::to_string(num);
            runtime::thread_name_.reset(fullname);

            char const* name = runtime::thread_name_.get()->c_str();

            // initialize thread mapping for external libraries (i.e. PAPI)
            thread_support_->register_thread(name, ec);

            // initialize coroutines context switcher
            hpx::threads::coroutines::thread_startup(name);

            // register this thread with any possibly active Intel tool
            HPX_ITT_THREAD_SET_NAME(name);

            // set thread name as shown in Visual Studio
            util::set_thread_name(name);

#if defined(HPX_HAVE_APEX)
            apex::register_thread(name);
#endif
        }

        // if this is a service thread, set its service affinity
        if (service_thread)
        {
            // FIXME: We don't set the affinity of the service threads on BG/Q,
            // as this is causing a hang (needs to be investigated)
#if !defined(__bgq__)
            threads::mask_cref_type used_processing_units =
                thread_manager_->get_used_processing_units();

            // --hpx:bind=none  should disable all affinity definitions
            if (threads::any(used_processing_units))
            {
                this->topology_.set_thread_affinity_mask(
                    this->topology_.get_service_affinity_mask(
                        used_processing_units));
            }
#endif
        }
    }

    void interim_runtime_impl::deinit_tss()
    {
        // initialize coroutines context switcher
        hpx::threads::coroutines::thread_shutdown();

        // reset applier TSS
        applier_.deinit_tss();

        // reset our TSS
        this->runtime::deinit_tss();

        // reset PAPI support
        thread_support_->unregister_thread();

        // reset thread local storage
        runtime::thread_name_.reset();
    }

    naming::gid_type
    interim_runtime_impl::get_next_id(std::size_t count)
    {
        return id_pool_.get_id(count);
    }

    void interim_runtime_impl::
        add_pre_startup_function(startup_function_type f)
    {
        runtime_support_->add_pre_startup_function(std::move(f));
    }

    void interim_runtime_impl::
        add_startup_function(startup_function_type f)
    {
        runtime_support_->add_startup_function(std::move(f));
    }

    void interim_runtime_impl::
        add_pre_shutdown_function(shutdown_function_type f)
    {
        runtime_support_->add_pre_shutdown_function(std::move(f));
    }

    void interim_runtime_impl::
        add_shutdown_function(shutdown_function_type f)
    {
        runtime_support_->add_shutdown_function(std::move(f));
    }

    bool interim_runtime_impl::
        keep_factory_alive(components::component_type type)
    {
        return runtime_support_->keep_factory_alive(type);
    }

    hpx::util::io_service_pool*
    interim_runtime_impl::
        get_thread_pool(char const* name)
    {
        HPX_ASSERT(name != nullptr);

        if (0 == std::strncmp(name, "io", 2))
            return &io_pool_;
        if (0 == std::strncmp(name, "parcel", 6))
            return parcel_handler_.get_thread_pool(name);
        if (0 == std::strncmp(name, "timer", 5))
            return &timer_pool_;
        if (0 == std::strncmp(name, "main", 4)) //-V112
            return &main_pool_;

        return nullptr;
    }

    /// Register an external OS-thread with HPX
    bool interim_runtime_impl::
        register_thread(char const* name, std::size_t num, bool service_thread,
            error_code& ec)
    {
        if (nullptr != runtime::thread_name_.get())
            return false;       // already registered

        std::string thread_name(name);
        thread_name += "-thread";

        init_tss_ex(thread_name.c_str(), num, nullptr, service_thread, ec);

        return !ec ? true : false;
    }

    /// Unregister an external OS-thread with HPX
    bool interim_runtime_impl::
        unregister_thread()
    {
        if (nullptr == runtime::thread_name_.get())
            return false;       // never registered

        deinit_tss();
        return true;
    }

    ///////////////////////////////////////////////////////////////////////////
    // threads::policies::callback_notifier
    //     get_notification_policy(char const* prefix)
    // {
    //     return get_runtime().get_notification_policy(prefix);
    // }
}

///////////////////////////////////////////////////////////////////////////////

