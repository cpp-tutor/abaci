#include <boost/test/unit_test.hpp>
#include "TestJIT.hpp"

struct Test class_test[] = {
    { R"(
        class c(a,b) endclass
        let a <- c(1,"A")
        print a.a,
        print a.b
        let b = a
        print b.a,
        print b.b
        a.a <- 9
        print a.a,
        print b.a
    )", "", TestJIT::Result::OK, "1 A\n1 A\n9 1" },
    { R"(
        class c(a,b) endclass
        let d <- c(1.1,c(2.2,3.3))
        print d.a,
        #print d.b
        print d.b.a,
        print d.b.b

        d.b.a <- 9.9
        print d.b.a

        d.b <- c(4.4,5.5)
        print d.b.a,
        print d.b.b
    )", "", TestJIT::Result::OK, "1.1 2.2 3.3\n9.9\n4.4 5.5" },
};

BOOST_AUTO_TEST_CASE( class_tests )
{
    for (const auto &test : class_test) {
        TestJIT jit;
        auto result = jit.test(test.program, test.input);
        BOOST_TEST(result == test.result);
        BOOST_TEST(jit.getOutput().starts_with(test.output));
    }
}