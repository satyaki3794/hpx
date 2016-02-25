//  Copyright (c) 2007-2014 Hartmut Kaiser
//  Copyright (c) 2011      Bryce Lelbach
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

/// \file hpx_fwd.hpp

#if !defined(HPX_HPX_FWD_MAR_24_2008_1119AM)
#define HPX_HPX_FWD_MAR_24_2008_1119AM

#include <hpx/config.hpp>

#include <cstdlib>
#include <vector>

#include <boost/config.hpp>
#include <boost/version.hpp>

#if BOOST_VERSION < 104900
// Please update your Boost installation (see www.boost.org for details).
#error HPX cannot be compiled with a Boost version earlier than V1.49.
#endif

#if defined(HPX_WINDOWS)
#if !defined(WIN32)
#  define WIN32
#endif
#include <winsock2.h>
#include <windows.h>
#endif

#include <boost/shared_ptr.hpp>
#include <boost/intrusive_ptr.hpp>
#include <boost/cstdint.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/system/error_code.hpp>
#include <boost/type_traits/remove_reference.hpp>

#include <hpx/traits.hpp>
#include <hpx/exception_fwd.hpp>
#include <hpx/traits/component_type_database.hpp>
#include <hpx/lcos/local/once_fwd.hpp>
#include <hpx/lcos_fwd.hpp>
#include <hpx/runtime_fwd.hpp>
#include <hpx/util_fwd.hpp>
#include <hpx/util/function.hpp>
// ^ this has to come before the naming/id_type.hpp below
#include <hpx/util/move.hpp>
#include <hpx/util/unique_function.hpp>
#include <hpx/util/unused.hpp>
#include <hpx/runtime/agas_fwd.hpp>
#include <hpx/runtime/applier_fwd.hpp>
#include <hpx/runtime/components_fwd.hpp>
#include <hpx/runtime/find_here.hpp>
#include <hpx/runtime/launch_policy.hpp>
#include <hpx/runtime/naming_fwd.hpp>
#include <hpx/runtime/naming/id_type.hpp>
#include <hpx/runtime/naming/name.hpp>
#include <hpx/runtime/parcelset_fwd.hpp>
#include <hpx/runtime/runtime_mode.hpp>
#include <hpx/runtime/components/component_type.hpp>
#include <hpx/runtime/threads_fwd.hpp>
#include <hpx/runtime/threads/detail/tagged_thread_state.hpp>
#include <hpx/runtime/threads/thread_enums.hpp>

/// \namespace hpx
///
/// The namespace \a hpx is the main namespace of the HPX library. All classes
/// functions and variables are defined inside this namespace.
namespace hpx
{
    /// \cond NOINTERNAL
    class error_code;

    HPX_EXCEPTION_EXPORT extern error_code throws;

    namespace performance_counters
    {
        struct counter_info;
    }

    /// \endcond

    ///////////////////////////////////////////////////////////////////////////
    // Pulling important types into the main namespace
    using naming::invalid_id;
}

namespace hpx
{
    ///////////////////////////////////////////////////////////////////////////
    /// \brief Start all active performance counters, optionally naming the
    ///        section of code
    ///
    /// \param ec [in,out] this represents the error status on exit, if this
    ///           is pre-initialized to \a hpx#throws the function will throw
    ///           on error instead.
    ///
    /// \note     As long as \a ec is not pre-initialized to \a hpx::throws this
    ///           function doesn't throw but returns the result code using the
    ///           parameter \a ec. Otherwise it throws an instance of
    ///           hpx::exception.
    ///
    /// \note     The active counters are those which have been specified on
    ///           the command line while executing the application (see command
    ///           line option \--hpx:print-counter)
    HPX_API_EXPORT void start_active_counters(error_code& ec = throws);

    /// \brief Resets all active performance counters.
    ///
    /// \param ec [in,out] this represents the error status on exit, if this
    ///           is pre-initialized to \a hpx#throws the function will throw
    ///           on error instead.
    ///
    /// \note     As long as \a ec is not pre-initialized to \a hpx::throws this
    ///           function doesn't throw but returns the result code using the
    ///           parameter \a ec. Otherwise it throws an instance of
    ///           hpx::exception.
    ///
    /// \note     The active counters are those which have been specified on
    ///           the command line while executing the application (see command
    ///           line option \--hpx:print-counter)
    HPX_API_EXPORT void reset_active_counters(error_code& ec = throws);

    /// \brief Stop all active performance counters.
    ///
    /// \param ec [in,out] this represents the error status on exit, if this
    ///           is pre-initialized to \a hpx#throws the function will throw
    ///           on error instead.
    ///
    /// \note     As long as \a ec is not pre-initialized to \a hpx::throws this
    ///           function doesn't throw but returns the result code using the
    ///           parameter \a ec. Otherwise it throws an instance of
    ///           hpx::exception.
    ///
    /// \note     The active counters are those which have been specified on
    ///           the command line while executing the application (see command
    ///           line option \--hpx:print-counter)
    HPX_API_EXPORT void stop_active_counters(error_code& ec = throws);

    /// \brief Evaluate and output all active performance counters, optionally
    ///        naming the point in code marked by this function.
    ///
    /// \param reset       [in] this is an optional flag allowing to reset
    ///                    the counter value after it has been evaluated.
    /// \param description [in] this is an optional value naming the point in
    ///                    the code marked by the call to this function.
    /// \param ec [in,out] this represents the error status on exit, if this
    ///           is pre-initialized to \a hpx#throws the function will throw
    ///           on error instead.
    ///
    /// \note     As long as \a ec is not pre-initialized to \a hpx::throws this
    ///           function doesn't throw but returns the result code using the
    ///           parameter \a ec. Otherwise it throws an instance of
    ///           hpx::exception.
    ///
    /// \note     The output generated by this function is redirected to the
    ///           destination specified by the corresponding command line
    ///           options (see \--hpx:print-counter-destination).
    ///
    /// \note     The active counters are those which have been specified on
    ///           the command line while executing the application (see command
    ///           line option \--hpx:print-counter)
    HPX_API_EXPORT void evaluate_active_counters(bool reset = false,
        char const* description = 0, error_code& ec = throws);

    ///////////////////////////////////////////////////////////////////////////
    /// \brief Create an instance of a binary filter plugin
    ///
    /// \param ec [in,out] this represents the error status on exit, if this
    ///           is pre-initialized to \a hpx#throws the function will throw
    ///           on error instead.
    ///
    /// \note     As long as \a ec is not pre-initialized to \a hpx::throws this
    ///           function doesn't throw but returns the result code using the
    ///           parameter \a ec. Otherwise it throws an instance of
    ///           hpx::exception.
    HPX_API_EXPORT serialization::binary_filter* create_binary_filter(
        char const* binary_filter_type, bool compress,
        serialization::binary_filter* next_filter = 0, error_code& ec = throws);

#if defined(HPX_HAVE_SODIUM)
    namespace components { namespace security
    {
        class certificate;
        class certificate_signing_request;
        class parcel_suffix;
        class hash;

        template <typename T> class signed_type;
        typedef signed_type<certificate> signed_certificate;
        typedef signed_type<certificate_signing_request>
            signed_certificate_signing_request;
        typedef signed_type<parcel_suffix> signed_parcel_suffix;
    }}

#if defined(HPX_HAVE_SECURITY)
    /// \brief Return the certificate for this locality
    ///
    /// \returns This function returns the signed certificate for this locality.
    HPX_API_EXPORT components::security::signed_certificate const&
        get_locality_certificate(error_code& ec = throws);

    /// \brief Return the certificate for the given locality
    ///
    /// \param id The id representing the locality for which to retrieve
    ///           the signed certificate.
    ///
    /// \returns This function returns the signed certificate for the locality
    ///          identified by the parameter \a id.
    HPX_API_EXPORT components::security::signed_certificate const&
        get_locality_certificate(boost::uint32_t locality_id, error_code& ec = throws);

    /// \brief Add the given certificate to the certificate store of this locality.
    ///
    /// \param cert The certificate to add to the certificate store of this
    ///             locality
    HPX_API_EXPORT void add_locality_certificate(
        components::security::signed_certificate const& cert,
        error_code& ec = throws);
#endif
#endif
}

// Including declarations of various API function declarations
#include <hpx/runtime/basename_registration.hpp>
#include <hpx/runtime/trigger_lco.hpp>
#include <hpx/runtime/get_locality_name.hpp>
#include <hpx/runtime/get_locality_id.hpp>
#include <hpx/runtime/get_config_entry.hpp>
#include <hpx/runtime/set_parcel_write_handler.hpp>
#include <hpx/runtime/get_os_thread_count.hpp>
#include <hpx/runtime/get_worker_thread_num.hpp>

#include <hpx/lcos/async_fwd.hpp>
#include <hpx/lcos/async_callback_fwd.hpp>

#endif

