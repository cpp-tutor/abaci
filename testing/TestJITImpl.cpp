#include "TestJITImpl.hpp"

using abaci::codegen::StmtCodeGen;
using abaci::codegen::TypeCodeGen;
using abaci::engine::JIT;
using abaci::parser::parseBlock;

TestJIT::Result TestJITImpl::test(const std::string& programText, const std::string& inputText) {
    input.str(inputText);
    auto start = programText.cbegin();
    try {
        std::ostringstream error;
        if (parseBlock(programText, ast, error, start, &constants)) {
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
            return TestJIT::Result::OK;
        }
        else {
            return TestJIT::Result::SyntaxError;
        }
    }
    catch (std::exception& error) {
        return TestJIT::Result::ExceptionThrown;
    }
}