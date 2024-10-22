#define BOOST_TEST_MODULE abaci_expression
#include <boost/test/unit_test.hpp>
#include "TestJIT.hpp"

struct Test tests[] = {
  { "print \"Abaci\" + \"JIT\"", "", TestJIT::Result::OK, "AbaciJIT" },
};

BOOST_AUTO_TEST_CASE( binary_operators )
{
  for (const auto &test : tests) {
    TestJIT jit;
    auto result = jit.test(test.program, test.input);
    BOOST_TEST(result == test.result);
    BOOST_TEST(jit.getOutput().starts_with(test.output));
  }
}