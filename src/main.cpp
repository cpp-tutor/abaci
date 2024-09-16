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
#include <iostream>

#ifdef ABACI_USE_STD_FORMAT
#include <format>
#include <print>
using std::print;
using std::vformat;
#else
#include <fmt/core.h>
#include <fmt/format.h>
#include <fmt/ostream.h>
using fmt::print;
using fmt::runtime;
#endif

int main(const int argc, const char **argv) {
    if (argc == 2) {
        std::ifstream inputFile{ argv[1] };
        if (inputFile) {
            std::string inputText;
            std::getline(inputFile, inputText, '\0');
            abaci::ast::StmtList ast;
            abaci::utility::Constants constants;
            abaci::utility::Context context(std::cin, std::cout, std::cerr, constants);
            abaci::engine::Cache functions;
            abaci::utility::Temporaries temps;
            try {
                if (abaci::parser::parseBlock(inputText, ast, *(context.error), &constants)) {
                    abaci::codegen::TypeCodeGen typeGen(&context, &functions);
                    for (const auto& stmt : ast) {
                        typeGen(stmt);
                    }
                    abaci::engine::JIT jit("Abaci", "program", &context, &functions);
                    abaci::codegen::StmtCodeGen codeGen(jit, &temps);
                    for (const auto& stmt : ast) {
                        codeGen(stmt);
                        temps.destroyTemporaries(jit);
                        temps.clear();
                    }
                    auto programFunction = jit.getExecFunction();
                    programFunction();
                    return 0;
                }
                else {
                    LogicError0(BadParse);
                }
            }
            catch (std::exception& error) {
                print(std::cout, "{}\n", error.what());
                return 1;
            }
        }
    }
    std::string input;
#ifdef ABACI_USE_STD_FORMAT
    std::cout << vformat(InitialPrompt, make_format_args(Version, EXIT));
#else
    print(std::cout, runtime(InitialPrompt), Version, EXIT);
#endif
    std::getline(std::cin, input);

    abaci::ast::StmtNode ast;
    abaci::utility::Constants constants;
    abaci::utility::Context context(std::cin, std::cout, std::cerr, constants);
    abaci::engine::Cache functions;
    while (!std::cin.eof() && !input.ends_with(EXIT)) {
        std::string moreInput = "\n";
        while (!std::cin.eof() && !abaci::parser::testStatement(input) && !moreInput.empty()) {
            print(std::cout, "{}", ContinuationPrompt);
            std::getline(std::cin, moreInput);
            input += '\n' + moreInput;
        }
        if (abaci::parser::testStatement(input)) {
            while (!input.empty() && abaci::parser::parseStatement(input, ast, *(context.error), &constants)) {
                try {
                    abaci::codegen::TypeCodeGen typeGen(&context, &functions);
                    typeGen(ast);
                    abaci::engine::JIT jit("Abaci", "program", &context, &functions);
                    abaci::utility::Temporaries temps;
                    abaci::codegen::StmtCodeGen codeGen(jit, &temps);
                    codeGen(ast);
                    temps.destroyTemporaries(jit);
                    temps.clear();
                    auto stmtFunction = jit.getExecFunction();
                    stmtFunction();
                }
                catch (std::exception& error) {
                    print(std::cout, "{}\n", error.what());
                }
            }
            moreInput = input;
        }
        else {
            print(std::cout, "{}\n", SyntaxError);
            moreInput.clear();
        }
        print(std::cout, "{}", moreInput.empty() ? InputPrompt : ContinuationPrompt);
        std::getline(std::cin, input);
        if (!moreInput.empty()) {
            input = moreInput + '\n' + input;
        }
    }
    return 0;
}
