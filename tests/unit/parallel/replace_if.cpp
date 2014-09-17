//  Copyright (c) 2014 Hartmut Kaiser
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <hpx/hpx_init.hpp>
#include <hpx/hpx.hpp>
#include <hpx/include/parallel_replace.hpp>
#include <hpx/util/lightweight_test.hpp>

#include "test_utils.hpp"

////////////////////////////////////////////////////////////////////////////
struct equal_f
{
    equal_f(std::size_t val) : val_(val) {}

    bool operator()(std::size_t lhs) const
    {
        return lhs == val_;
    }

    std::size_t val_;
};

template <typename ExPolicy, typename IteratorTag>
void test_replace_if(ExPolicy const& policy, IteratorTag)
{
    BOOST_STATIC_ASSERT(hpx::parallel::is_execution_policy<ExPolicy>::value);

    typedef std::vector<std::size_t>::iterator base_iterator;
    typedef test::test_iterator<base_iterator, IteratorTag> iterator;

    std::vector<std::size_t> c(10007);
    std::vector<std::size_t> d(c.size());
    std::iota(boost::begin(c), boost::end(c), std::rand());
    std::copy(boost::begin(c), boost::end(c), boost::begin(d));

    std::size_t idx = std::rand() % c.size();

    hpx::parallel::replace_if(policy,
        iterator(boost::begin(c)), iterator(boost::end(c)),
        equal_f(c[idx]), c[idx]+1);

    std::replace_if(boost::begin(d), boost::end(d), equal_f(d[idx]), d[idx]+1);

    std::size_t count = 0;
    HPX_TEST(std::equal(boost::begin(c), boost::end(c), boost::begin(d),
        [&count](std::size_t v1, std::size_t v2) -> bool {
            HPX_TEST_EQ(v1, v2);
            ++count;
            return v1 == v2;
        }));
    HPX_TEST_EQ(count, d.size());
}

template <typename IteratorTag>
void test_replace_if(hpx::parallel::parallel_task_execution_policy, IteratorTag)
{
    typedef std::vector<std::size_t>::iterator base_iterator;
    typedef test::test_iterator<base_iterator, IteratorTag> iterator;

    std::vector<std::size_t> c(10007);
    std::vector<std::size_t> d(c.size());
    std::iota(boost::begin(c), boost::end(c), std::rand());
    std::copy(boost::begin(c), boost::end(c), boost::begin(d));

    std::size_t idx = std::rand() % c.size();

    hpx::future<void> f =
        hpx::parallel::replace_if(hpx::parallel::par_task,
            iterator(boost::begin(c)), iterator(boost::end(c)),
            equal_f(c[idx]), c[idx]+1);
    f.wait();

    std::replace_if(boost::begin(d), boost::end(d), equal_f(d[idx]), d[idx]+1);

    std::size_t count = 0;
    HPX_TEST(std::equal(boost::begin(c), boost::end(c), boost::begin(d),
        [&count](std::size_t v1, std::size_t v2) -> bool {
            HPX_TEST_EQ(v1, v2);
            ++count;
            return v1 == v2;
        }));
    HPX_TEST_EQ(count, d.size());
}

template <typename IteratorTag>
void test_replace_if()
{
    using namespace hpx::parallel;
    test_replace_if(seq, IteratorTag());
    test_replace_if(par, IteratorTag());
    test_replace_if(par_vec, IteratorTag());
    test_replace_if(par(task), IteratorTag());

    test_replace_if(execution_policy(seq), IteratorTag());
    test_replace_if(execution_policy(par), IteratorTag());
    test_replace_if(execution_policy(par_vec), IteratorTag());
    test_replace_if(execution_policy(par(task)), IteratorTag());
}

void replace_if_test()
{
    test_replace_if<std::random_access_iterator_tag>();
    test_replace_if<std::forward_iterator_tag>();
}

///////////////////////////////////////////////////////////////////////////////
template <typename ExPolicy, typename IteratorTag>
void test_replace_if_exception(ExPolicy const& policy, IteratorTag)
{
    BOOST_STATIC_ASSERT(hpx::parallel::is_execution_policy<ExPolicy>::value);

    typedef std::vector<std::size_t>::iterator base_iterator;
    typedef test::decorated_iterator<base_iterator, IteratorTag>
        decorated_iterator;

    std::vector<std::size_t> c(10007);
    std::iota(boost::begin(c), boost::end(c), std::rand());

    bool caught_exception = false;
    try {
        hpx::parallel::replace_if(policy,
            decorated_iterator(
                boost::begin(c),
                [](){ throw std::runtime_error("test"); }),
            decorated_iterator(boost::end(c)),
            equal_f(42), std::size_t(43));
        HPX_TEST(false);
    }
    catch (hpx::exception_list const& e) {
        caught_exception = true;
        test::test_num_exceptions<ExPolicy, IteratorTag>::call(policy, e);
    }
    catch (...) {
        HPX_TEST(false);
    }

    HPX_TEST(caught_exception);
}

template <typename IteratorTag>
void test_replace_if_exception(hpx::parallel::parallel_task_execution_policy, IteratorTag)
{
    typedef std::vector<std::size_t>::iterator base_iterator;
    typedef test::decorated_iterator<base_iterator, IteratorTag>
        decorated_iterator;

    std::vector<std::size_t> c(10007);
    std::iota(boost::begin(c), boost::end(c), std::rand());

    bool caught_exception = false;
    try {
        hpx::future<void> f =
            hpx::parallel::replace_if(hpx::parallel::par_task,
                decorated_iterator(
                    boost::begin(c),
                    [](){ throw std::runtime_error("test"); }),
                decorated_iterator(boost::end(c)),
                equal_f(42), std::size_t(43));
        f.get();

        HPX_TEST(false);
    }
    catch (hpx::exception_list const& e) {
        caught_exception = true;
        test::test_num_exceptions<
            hpx::parallel::parallel_task_execution_policy, IteratorTag
        >::call(hpx::parallel::par(task), e);
    }
    catch (...) {
        HPX_TEST(false);
    }

    HPX_TEST(caught_exception);
}

template <typename IteratorTag>
void test_replace_if_exception()
{
    using namespace hpx::parallel;

    // If the execution policy object is of type vector_execution_policy,
    // std::terminate shall be called. therefore we do not test exceptions
    // with a vector execution policy
    test_replace_if_exception(seq, IteratorTag());
    test_replace_if_exception(par, IteratorTag());
    test_replace_if_exception(par(task), IteratorTag());

    test_replace_if_exception(execution_policy(seq), IteratorTag());
    test_replace_if_exception(execution_policy(par), IteratorTag());
    test_replace_if_exception(execution_policy(par(task)), IteratorTag());
}

void replace_if_exception_test()
{
    test_replace_if_exception<std::random_access_iterator_tag>();
    test_replace_if_exception<std::forward_iterator_tag>();
}

//////////////////////////////////////////////////////////////////////////////
template <typename ExPolicy, typename IteratorTag>
void test_replace_if_bad_alloc(ExPolicy const& policy, IteratorTag)
{
    BOOST_STATIC_ASSERT(hpx::parallel::is_execution_policy<ExPolicy>::value);

    typedef std::vector<std::size_t>::iterator base_iterator;
    typedef test::decorated_iterator<base_iterator, IteratorTag>
        decorated_iterator;

    std::vector<std::size_t> c(10007);
    std::iota(boost::begin(c), boost::end(c), std::rand());

    bool caught_bad_alloc = false;
    try {
        hpx::parallel::replace_if(policy,
            decorated_iterator(
                boost::begin(c),
                [](){ throw std::bad_alloc(); }),
            decorated_iterator(boost::end(c)),
            equal_f(42), std::size_t(43));
        HPX_TEST(false);
    }
    catch (std::bad_alloc const&) {
        caught_bad_alloc = true;
    }
    catch (...) {
        HPX_TEST(false);
    }

    HPX_TEST(caught_bad_alloc);
}

template <typename IteratorTag>
void test_replace_if_bad_alloc(hpx::parallel::parallel_task_execution_policy, IteratorTag)
{
    typedef std::vector<std::size_t>::iterator base_iterator;
    typedef test::decorated_iterator<base_iterator, IteratorTag>
        decorated_iterator;

    std::vector<std::size_t> c(10007);
    std::iota(boost::begin(c), boost::end(c), std::rand());

    bool caught_bad_alloc = false;
    try {
        hpx::future<void> f =
            hpx::parallel::replace_if(hpx::parallel::par_task,
                decorated_iterator(
                    boost::begin(c),
                    [](){ throw std::bad_alloc(); }),
                decorated_iterator(boost::end(c)),
                equal_f(42), std::size_t(43));
        f.get();

        HPX_TEST(false);
    }
    catch(std::bad_alloc const&) {
        caught_bad_alloc = true;
    }
    catch(...) {
        HPX_TEST(false);
    }

    HPX_TEST(caught_bad_alloc);
}

template <typename IteratorTag>
void test_replace_if_bad_alloc()
{
    using namespace hpx::parallel;

    // If the execution policy object is of type vector_execution_policy,
    // std::terminate shall be called. therefore we do not test exceptions
    // with a vector execution policy
    test_replace_if_bad_alloc(seq, IteratorTag());
    test_replace_if_bad_alloc(par, IteratorTag());
    test_replace_if_bad_alloc(par(task), IteratorTag());

    test_replace_if_bad_alloc(execution_policy(seq), IteratorTag());
    test_replace_if_bad_alloc(execution_policy(par), IteratorTag());
    test_replace_if_bad_alloc(execution_policy(par(task)), IteratorTag());
}

void replace_if_bad_alloc_test()
{
    test_replace_if_bad_alloc<std::random_access_iterator_tag>();
    test_replace_if_bad_alloc<std::forward_iterator_tag>();
}

int hpx_main()
{
    replace_if_test();
    replace_if_exception_test();
    replace_if_bad_alloc_test();
    return hpx::finalize();
}

int main(int argc, char* argv[])
{
    std::vector<std::string> cfg;
    cfg.push_back("hpx.os_threads=" +
        boost::lexical_cast<std::string>(hpx::threads::hardware_concurrency()));

    HPX_TEST_EQ_MSG(hpx::init(argc, argv, cfg), 0,
        "HPX main exited with non-zero status");

    return hpx::util::report_errors();
}