#include "utility/Context.hpp"
#include "utility/Constant.hpp"
#include "ast/Stmt.hpp"
#include "parser/Parse.hpp"
#include "codegen/TypeCodeGen.hpp"
#include "codegen/StmtCodeGen.hpp"
#include "engine/JIT.hpp"
#include "localize/Keywords.hpp"
#include "localize/Messages.hpp"
#include "utility/Temporary.hpp"
#include <fstream>
#include <string>
#include <regex>
#include <iostream>

#ifdef ABACI_USE_STD_FORMAT
#include <format>
#include <print>
#else
#include <fmt/core.h>
#include <fmt/format.h>
#include <fmt/ostream.h>
#endif

using abaci::ast::StmtNode;
using abaci::ast::StmtList;
using abaci::codegen::StmtCodeGen;
using abaci::codegen::TypeCodeGen;
using abaci::engine::Cache;
using abaci::engine::JIT;
using abaci::parser::parseBlock;
using abaci::parser::parseStatement;
using abaci::parser::testStatement;
using abaci::utility::Constants;
using abaci::utility::Context;
using abaci::utility::Temporaries;

std::string processError(std::string&& errorString) {
    const std::string inLine = "In line";
    if (auto multiple = errorString.find(inLine, 1); multiple != std::string::npos) {
        errorString.resize(multiple);
    }
    if (errorString.starts_with(inLine)) {
        std::regex i10nLine{ R"(In line (\d+):\n)" };
        std::smatch matches;
        regex_search(errorString, matches, i10nLine);
        std::string line = matches[1];
#ifdef ABACI_USE_STD_FORMAT
        std::string replaced = std::vformat(ErrorAtLine, std::make_format_args(line));
#else
        std::string replaced = fmt::format(fmt::runtime(ErrorAtLine), line);
#endif
        errorString.replace(0, matches[0].length(), replaced);
    }
    if (errorString.ends_with('\n')) {
        errorString.pop_back();
    }
    return errorString;
}

int main(const int argc, const char **argv) {
    if (argc == 2) {
        std::ifstream inputFile{ argv[1] };
        if (inputFile) {
            std::string inputText;
            std::getline(inputFile, inputText, '\0');
            StmtList ast;
            Constants constants;
            Context context(std::cin, std::cout, constants);
            Cache functions;
            Temporaries temps;
            auto start = inputText.cbegin();
            try {
                std::ostringstream error;
                if (parseBlock(inputText, ast, error, start, &constants)) {
                    TypeCodeGen typeGen(&context, &functions);
                    for (const auto& stmt : ast.statements) {
                        typeGen(stmt);
                    }
                    JIT jit("Abaci", "program", &context, &functions);
                    StmtCodeGen codeGen(jit, &temps);
                    for (const auto& stmt : ast.statements) {
                        codeGen(stmt);
                        temps.destroyTemporaries(jit);
                        temps.clear();
                    }
                    auto programFunction = jit.getExecFunction();
                    programFunction();
                    return 0;
                }
                else {
#ifdef ABACI_USE_STD_FORMAT
                    std::print
#else
                    fmt::print
#endif
                    (std::cerr, "{}\n", !error.str().empty() ? processError(error.str()) : SyntaxError);
                    return 1;
                }
            }
            catch (std::exception& error) {
#ifdef ABACI_USE_STD_FORMAT
                std::print
#else
                fmt::print
#endif
                (std::cout, "{}\n", error.what());
                return 1;
            }
        }
    }
    std::string input, line{ "\n" };
#ifdef ABACI_USE_STD_FORMAT
    std::cout << std::vformat(InitialPrompt, std::make_format_args(Version, EXIT));
#else
    fmt::print(std::cout, fmt::runtime(InitialPrompt), Version, EXIT);
#endif
    std::getline(std::cin, input);

    StmtNode ast;
    Constants constants;
    Context context(std::cin, std::cout, constants);
    Cache functions;
    Temporaries temps;
    while (!std::cin.eof() && !input.ends_with(EXIT)) {
        while (!std::cin.eof() && !testStatement(input) && !line.empty()) {
#ifdef ABACI_USE_STD_FORMAT
            std::print
#else
            fmt::print
#endif
            (std::cout, "{}", ContinuationPrompt);
            std::getline(std::cin, line);
            if (!line.empty()) {
                input += '\n' + line;
            }
        }
        auto start = input.cbegin();
        if (testStatement(input)) {
            std::ostringstream error;
            while (start != input.cend() && parseStatement(input, ast, error, start, &constants)) {
                try {
                    TypeCodeGen typeGen(&context, &functions);
                    typeGen(ast);
                    JIT jit("Abaci", "program", &context, &functions);
                    StmtCodeGen codeGen(jit, &temps);
                    codeGen(ast);
                    temps.destroyTemporaries(jit);
                    temps.clear();
                    auto stmtFunction = jit.getExecFunction();
                    stmtFunction();
                }
                catch (std::exception& error) {
#ifdef ABACI_USE_STD_FORMAT
                    std::print
#else
                    fmt::print
#endif
                    (std::cerr, "{}\n", error.what());
                    start = input.cend();
                    break;
                }
            }
            if (!error.str().empty()) {
#ifdef ABACI_USE_STD_FORMAT
                std::print
#else
                fmt::print
#endif
                (std::cerr, "{}\n", processError(error.str()));
                start = input.cend();
            }
        }
        else {
            StmtList dummy;
            std::ostringstream error;
            parseBlock(input, dummy, error, start, nullptr);
#ifdef ABACI_USE_STD_FORMAT
            std::print
#else
            fmt::print
#endif
            (std::cerr, "{}\n", !error.str().empty() ? processError(error.str()) : SyntaxError);
            start = input.cend();
        }
#ifdef ABACI_USE_STD_FORMAT
        std::print
#else
        fmt::print
#endif
        (std::cout, "{}", (start == input.cend()) ? InputPrompt : ContinuationPrompt);
        if (start != input.cend()) {
            input.assign(start, input.cend());
            std::getline(std::cin, line);
            input += '\n' + line;
        }
        else {
            std::getline(std::cin, input);
        }
        line = "\n";
    }
    return 0;
}
