#include <boost/test/unit_test.hpp>
#include "TestJIT.hpp"

struct Test method[] = {
    { R"(
        class c(n)
            fn show()
                print this.n
            endfn
            fn add(m)
                print m + this.n
            endfn
            fn get()
                return this.n
            endfn
            fn sub(m)
                return this.n - m
            endfn
            fn set(m)
                this.n <- m
            endfn
        endclass

        let a <- c(1.1j)
        a.show()
        a.add(2.2)
        print a.get()
        print a.sub(2.2)
        let b <- a
        b.show()
        b.set(3.3j)
        b.show()
        print a.n,
        print b.n
    )", "", TestJIT::Result::OK, 1+R"(
0+1.1j
2.2+1.1j
0+1.1j
-2.2+1.1j
0+1.1j
0+3.3j
0+1.1j 0+3.3j)" },
};

BOOST_AUTO_TEST_CASE( method_tests )
{
    for (const auto &test : method) {
        TestJIT jit;
        auto result = jit.test(test.program, test.input);
        BOOST_TEST(result == test.result);
        BOOST_TEST(jit.getOutput().starts_with(test.output));
    }
}