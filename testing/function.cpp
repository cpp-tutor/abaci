#include <boost/test/unit_test.hpp>
#include "TestJIT.hpp"

struct Test function[] = {
    { R"(
        fn a()
            print "Abaci"
        endfn

        a())",
    "", TestJIT::Result::OK, "Abaci" },
    { R"(
        fn show(n)
            print n
        endfn

        show(1)
        show(1.1)
        show(1 - 1j))",
    "", TestJIT::Result::OK, "1\n1.1\n1-1j\n" },
    { R"(
        fn difference(c, d)
            if c < d
                return d - c
            else
                return c - d
            endif
        endfn

        print difference(2, 5)
        print difference(4.4, 1.1))",
    "", TestJIT::Result::OK, "3\n3.3\n" },
};

BOOST_AUTO_TEST_CASE( function_tests )
{
    for (const auto &test : function) {
        TestJIT jit;
        auto result = jit.test(test.program, test.input);
        BOOST_TEST(result == test.result);
        BOOST_TEST(jit.getOutput().starts_with(test.output));
    }
}