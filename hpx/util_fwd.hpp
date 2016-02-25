//  Copyright (c) 2007-2014 Hartmut Kaiser
//  Copyright (c) 2011      Bryce Lelbach
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

/// \file util_fwd.hpp

#ifndef HPX_UTIL_FWD_HPP
#define HPX_UTIL_FWD_HPP

#include <hpx/config.hpp>

#include <string>

namespace hpx { namespace util
{
    /// \cond NOINTERNAL
    class HPX_EXPORT io_service_pool;
    class HPX_EXPORT runtime_configuration;

    struct binary_filter;

    class HPX_EXPORT section;

    /// \brief Expand INI variables in a string
    HPX_API_EXPORT std::string expand(std::string const& expand);

    /// \brief Expand INI variables in a string
    HPX_API_EXPORT void expand(std::string& expand);

    /// \endcond
}}

#endif
