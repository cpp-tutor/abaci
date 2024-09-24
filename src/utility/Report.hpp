#ifndef Report_hpp
#define Report_hpp

#include "localize/Messages.hpp"
#include <exception>
#include <string>

#ifdef ABACI_USE_STD_FORMAT
#include <format>
#else
#include <fmt/core.h>
#include <fmt/format.h>
#endif

template<typename... Ts>
class AbaciError : public std::exception {
private:
    std::string message{};
public:
    AbaciError(const char *errorString, Ts... args) : message{
#ifdef ABACI_USE_STD_FORMAT
        std::vformat(errorString, std::make_format_args(args...))
#else
        fmt::format(fmt::runtime(errorString), std::forward<Ts>(args)...)
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
        : message{ std::vformat(errorString, std::make_format_args(args...)) }
    {
        message = std::vformat(CompilerInconsistency, std::make_format_args(message));
        if (sourceFile != std::string{}) {
            message.append(std::vformat(SourceFile, std::make_format_args(sourceFile)));
            if (lineNumber != -1) {
                message.append(std::vformat(LineNumber, std::make_format_args(lineNumber)));
            }
        }
    }
#else
        : message{ fmt::format(fmt::runtime(errorString), std::forward<Ts>(args)...) }
    {
        message = fmt::format(fmt::runtime(CompilerInconsistency), message);
        if (sourceFile != std::string{}) {
            message.append(fmt::format(fmt::runtime(SourceFile), sourceFile));
            if (lineNumber != -1) {
                message.append(fmt::format(fmt::runtime(LineNumber), lineNumber));
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
        : message{ std::vformat(AssertionFailed, std::make_format_args(assertion)) }
    {
        if (sourceFile != std::string{}) {
            message.append(std::vformat(SourceFile, std::make_format_args(sourceFile)));
            if (lineNumber != -1) {
                message.append(std::vformat(LineNumber, std::make_format_args(lineNumber)));
            }
        }
    }
#else
        : message{ fmt::format(fmt::runtime(AssertionFailed), assertion) }
    {
        if (sourceFile != std::string{}) {
            message.append(fmt::format(fmt::runtime(SourceFile), sourceFile));
            if (lineNumber != -1) {
                message.append(fmt::format(fmt::runtime(LineNumber), lineNumber));
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
