#ifndef TestJIT_hpp
#define TestJIT_hpp

#include <string>
#include <memory>
#include <ostream>

struct TestJITImpl;

class TestJIT {
    std::unique_ptr<TestJITImpl> impl;
public:
    TestJIT();
    ~TestJIT();
    enum class Result{ SyntaxError, ExceptionThrown, OK };
    Result test(const std::string& programText, const std::string& inputText);
    const std::string getOutput();
    void clearFunctions();
};

inline std::ostream& operator<<(std::ostream& os, TestJIT::Result result) {
    switch (result) {
        case TestJIT::Result::SyntaxError:
            return os << "SyntaxError";
        case TestJIT::Result::ExceptionThrown:
            return os << "ExceptionThrown";
        case TestJIT::Result::OK:
            return os << "OK";
        default:
            return os << "???";
    }
}

struct Test {
    std::string program;
    std::string input;
    TestJIT::Result result;
    std::string output;
};

#endif