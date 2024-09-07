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
#include <fmt/core.h>
#include <fmt/format.h>
#include <fmt/ostream.h>
#include <iostream>
#include <fstream>
#include <string>
using fmt::print;
using fmt::runtime;

int main(const int argc, const char **argv) {
    if (argc == 2) {
        std::ifstream input_file{ argv[1] };
        if (input_file) {
            std::string input_text;
            std::getline(input_file, input_text, '\0');
            abaci::ast::StmtList ast;
            abaci::utility::Constants constants;
            abaci::utility::Context context(std::cin, std::cout, std::cerr, constants);
            abaci::engine::Cache functions;
            abaci::utility::Temporaries temps;
            try {
                if (abaci::parser::parse_block(input_text, ast, *(context.error), &constants)) {
                    abaci::codegen::TypeCodeGen type_code_gen(&context, &functions);
                    for (const auto& stmt : ast) {
                        type_code_gen(stmt);
                    }
                    abaci::engine::JIT jit("Abaci", "program", &context, &functions);
                    abaci::codegen::StmtCodeGen code_gen(jit, &temps);
                    for (const auto& stmt : ast) {
                        code_gen(stmt);
                    }
                    auto programFunc = jit.getExecFunction(&context);
                    programFunc();
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
    print(std::cout, runtime(InitialPrompt), Version, EXIT);
    std::getline(std::cin, input);

    abaci::ast::StmtNode ast;
    abaci::utility::Constants constants;
    abaci::utility::Context context(std::cin, std::cout, std::cerr, constants);
    abaci::engine::Cache functions;
    while (!input.ends_with(EXIT)) {
        std::string more_input = "\n";
        while (!abaci::parser::test_statement(input) && !more_input.empty()) {
            print(std::cout, "{}", ContinuationPrompt);
            std::getline(std::cin, more_input);
            input += '\n' + more_input;
        }
        if (abaci::parser::test_statement(input)) {
            while (!input.empty() && abaci::parser::parse_statement(input, ast, *(context.error), &constants)) {
                try {
                    abaci::codegen::TypeCodeGen type_code_gen(&context, &functions);
                    type_code_gen(ast);
                    abaci::engine::JIT jit("Abaci", "program", &context, &functions);
                    abaci::utility::Temporaries temps;
                    abaci::codegen::StmtCodeGen code_gen(jit, &temps);
                    code_gen(ast);
                    temps.deleteTemporaries(jit);
                    auto stmtFunc = jit.getExecFunction(&context);
                    stmtFunc();
                }
                catch (std::exception& error) {
                    print(std::cout, "{}\n", error.what());
                }
            }
            more_input = input;
        }
        else {
            print(std::cout, "{}\n", SyntaxError);
            more_input.clear();
        }
        print(std::cout, "{}", more_input.empty() ? InputPrompt : ContinuationPrompt);
        std::getline(std::cin, input);
        if (!more_input.empty()) {
            input = more_input + '\n' + input;
        }
    }
    return 0;
}
