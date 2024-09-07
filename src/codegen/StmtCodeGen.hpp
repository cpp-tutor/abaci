#ifndef StmtCodeGen_hpp
#define StmtCodeGen_hpp

#include "ast/Expr.hpp"
#include "ast/Stmt.hpp"
#include "engine/JIT.hpp"
#include "utility/Symbol.hpp"
#include "utility/Temporary.hpp"
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Value.h>

namespace abaci::codegen {

using llvm::IRBuilder;
using llvm::Module;
using llvm::BasicBlock;
using abaci::ast::StmtNode;
using abaci::ast::StmtList;
using abaci::engine::JIT;
using abaci::utility::AbaciValue;
using abaci::utility::Variable;
using abaci::utility::Temporaries;
using abaci::utility::LocalSymbols;
using abaci::utility::TypedValue;

class StmtCodeGen {
    JIT& jit;
    IRBuilder<>& builder;
    Module& module;
    mutable Temporaries *temps;
    BasicBlock *exit_block;
    mutable LocalSymbols *locals;
public:
    StmtCodeGen() = delete;
    StmtCodeGen(JIT& jit, Temporaries *temps = nullptr, LocalSymbols *locals = nullptr, BasicBlock *exit_block = nullptr)
        : jit{ jit }, builder{ jit.getBuilder() }, module{ jit.getModule() }, temps{ temps },
        exit_block{ exit_block }, locals{ locals } {}
    void operator()(const StmtList& stmts, BasicBlock *exit_block = nullptr) const;
    void operator()(const StmtNode& stmt) const;
private:
    template<typename T>
    void codeGen(const T&) const;
};

} // namespace abaci::codegen

#endif
