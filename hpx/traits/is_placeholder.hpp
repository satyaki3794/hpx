//  Copyright (c) 2013-2015 Agustin Berge
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef HPX_TRAITS_IS_PLACEHOLDER_HPP
#define HPX_TRAITS_IS_PLACEHOLDER_HPP

#include <hpx/config.hpp>

#include <boost/is_placeholder.hpp>

#ifdef HPX_HAVE_CXX11_STD_IS_PLACEHOLDER
#include <functional>
#endif
#include <type_traits>

namespace hpx { namespace traits
{
    template <typename T>
    struct is_placeholder
#ifdef HPX_HAVE_CXX11_STD_IS_PLACEHOLDER
      : std::conditional<
            std::is_placeholder<T>::value != 0,
            std::is_placeholder<T>,
            boost::is_placeholder<T>
        >::type
#else
      : boost::is_placeholder<T>
#endif
    {};
}}

#endif
