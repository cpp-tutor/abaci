#include "TestJIT.hpp"
#include "TestJITImpl.hpp"

TestJIT::TestJIT() {
    impl.reset(new TestJITImpl);
}

TestJIT::~TestJIT() {
    impl.reset(nullptr);
}

TestJIT::Result TestJIT::test(const std::string& programText, const std::string& inputText) {
    return impl->test(programText, inputText);
}

const std::string TestJIT::getOutput() {
    return impl->getOutput();
}

void TestJIT::clearFunctions() {
    impl->clearFunctions();
}