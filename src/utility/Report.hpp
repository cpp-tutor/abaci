#ifndef Report_hpp
#define Report_hpp

#include "localize/Messages.hpp"
#include <exception>
#include <string>

#ifdef ABACI_USE_STD_FORMAT
#include <format>
using std::vformat;
using std::make_format_args;
#else
#include <fmt/core.h>
#include <fmt/format.h>
using fmt::print;
using fmt::runtime;
#endif

template<typename... Ts>
class AbaciError : public std::exception {
private:
    std::string message{};
public:
    AbaciError(const char *errorString, Ts... args) : message{
#ifdef ABACI_USE_STD_FORMAT
        vformat(errorString, make_format_args(args...))
#else
        format(runtime(errorString), std::forward<Ts>(args)...)
#endif
    } {}
    virtual const char *what() const noexcept override { return message.c_str(); }
};

template<typename... Ts>
class CompilerError : public std::exception {
private:
    std::string message{};
public:
    CompilerError(const char* sourceFile, const int lineNumber, const char *errorString, Ts... args)
#ifdef ABACI_USE_STD_FORMAT
        : message{ vformat(errorString, make_format_args(args...)) }
    {
        message = vformat(CompilerInconsistency, make_format_args(message));
        if (sourceFile != std::string{}) {
            message.append(vformat(SourceFile, make_format_args(sourceFile)));
            if (lineNumber != -1) {
                message.append(vformat(LineNumber, make_format_args(lineNumber)));
            }
        }
    }
#else
        : message{ format(runtime(errorString), std::forward<Ts>(args)...) }
    {
        message = format(runtime(CompilerInconsistency), message);
        if (sourceFile != std::string{}) {
            message.append(format(runtime(SourceFile), sourceFile));
            if (lineNumber != -1) {
                message.append(format(runtime(LineNumber), lineNumber));
            }
        }
    }
#endif
    virtual const char *what() const noexcept override { return message.c_str(); }
};

class AssertError : public std::exception {
private:
    std::string message{};
public:
    AssertError(const char* sourceFile, const int lineNumber, const char *assertion)
#ifdef ABACI_USE_STD_FORMAT
        : message{ vformat(AssertionFailed, make_format_args(assertion)) }
    {
        if (sourceFile != std::string{}) {
            message.append(vformat(SourceFile, make_format_args(sourceFile)));
            if (lineNumber != -1) {
                message.append(vformat(LineNumber, make_format_args(lineNumber)));
            }
        }
    }
#else
        : message{ format(runtime(AssertionFailed), assertion) }
    {
        if (sourceFile != std::string{}) {
            message.append(format(runtime(SourceFile), sourceFile));
            if (lineNumber != -1) {
                message.append(format(runtime(LineNumber), lineNumber));
            }
        }
    }
#endif
    virtual const char *what() const noexcept override { return message.c_str(); }
};

#define LogicError0(errorString) throw AbaciError(errorString)
#define LogicError1(errorString, arg1) throw AbaciError(errorString, arg1)
#define LogicError2(errorString, arg1, arg2) throw AbaciError(errorString, arg1, arg2)
#define UnexpectedError0(errorString) throw CompilerError(__FILE__, __LINE__, errorString)
#define UnexpectedError1(errorString, arg1) throw CompilerError(__FILE__, __LINE__, errorString, arg1)
#define UnexpectedError2(errorString, arg1, arg2) throw CompilerError(__FILE__, __LINE__, errorString, arg1, arg2)
#define Assert(condition) if (!(condition)) throw AssertError(__FILE__, __LINE__, #condition)

#endif
