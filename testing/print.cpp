#define BOOST_TEST_MODULE abaci
#include <boost/test/unit_test.hpp>
#include "TestJIT.hpp"

struct Test print_statement[] = {
    { "print \"Abaci\" + \"JIT\"", "", TestJIT::Result::OK, "AbaciJIT" },
    { "print 1 + 2, 4, 5;", "", TestJIT::Result::OK, "3 4 5" },
    { "print 5 + 3j", "", TestJIT::Result::OK, "5+3j" },
};

BOOST_AUTO_TEST_CASE( print_statement_tests )
{
    for (const auto &test : print_statement) {
        TestJIT jit;
        auto result = jit.test(test.program, test.input);
        BOOST_TEST(result == test.result);
        BOOST_TEST(jit.getOutput().starts_with(test.output));
    }
}