#include <boost/test/unit_test.hpp>
#include "TestJIT.hpp"

struct Test expression[] = {
    { "let a = 1 print a = 1", "", TestJIT::Result::OK, "true" },
    { "let b <- 1.1 b <- b + 1.1 print b = 2.2", "", TestJIT::Result::OK, "true" },
    { "let c <- 1 + 2j c <- c * 2 print c", "", TestJIT::Result::OK, "2+4j" },
    { "let d <- \"Aba\" + \"ci\" d <- d + \"Progr\" + \"am\" print d", "", TestJIT::Result::OK, "AbaciProgram" },
    { "let e <- [int] e <- e + [1, 2] print !e, ?e;", "", TestJIT::Result::OK, "2 [int]" },
    { "let f = 2 f <- f + 1", "", TestJIT::Result::ExceptionThrown, "" },
    { "let g <- [1.1, 2.2, 3.3] g[1] <- nil print !g, g[1];", "", TestJIT::Result::OK, "2 3.3" },
};

BOOST_AUTO_TEST_CASE( expression_tests )
{
    for (const auto &test : expression) {
        TestJIT jit;
        auto result = jit.test(test.program, test.input);
        BOOST_TEST(result == test.result);
        BOOST_TEST(jit.getOutput().starts_with(test.output));
    }
}