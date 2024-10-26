#include <boost/test/unit_test.hpp>
#include "TestJIT.hpp"

struct Test block[] = {
    { R"(
        let n <- 10
        while n > 0
            print n,
        n <- n - 1
        endwhile

        let s = "Blastoff!"
        print s)",
    "", TestJIT::Result::OK, "10 9 8 7 6 5 4 3 2 1 Blastoff!" },
    { R"(
        let i <- 3
        repeat
            let j = i + 4j
            print j
            i <- i * 2
        until i > 20)",
    "", TestJIT::Result::OK, "3+4j\n6+4j\n12+4j\n" },
    { R"(
        let n <- 5
        print n, 
        case true
            when 0 <= n < 5
                print "is between 0 and 4"
            when 5 <= n < 10
                print "is between 5 and 9"
            else
                print "is below 0 or above 9"
        endcase)",
    "", TestJIT::Result::OK, "5 is between 5 and 9" },
};

BOOST_AUTO_TEST_CASE( block_tests )
{
    for (const auto &test : block) {
        TestJIT jit;
        auto result = jit.test(test.program, test.input);
        BOOST_TEST(result == test.result);
        BOOST_TEST(jit.getOutput().starts_with(test.output));
    }
}