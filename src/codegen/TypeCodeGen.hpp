#ifndef TypeCodeGen_hpp
#define TypeCodeGen_hpp

#include "ast/Expr.hpp"
#include "ast/Stmt.hpp"
#include "utility/Type.hpp"
#include "utility/Context.hpp"
#include "utility/Report.hpp"
#include "engine/Cache.hpp"
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>
#include <vector>
#include <optional>

namespace abaci::codegen {

using llvm::IRBuilder;
using llvm::Module;
using abaci::ast::ExprNode;
using abaci::ast::StmtNode;
using abaci::ast::StmtList;
using abaci::engine::Cache;
using abaci::utility::AbaciValue;
using abaci::utility::Type;
using abaci::utility::Variable;
using abaci::utility::Context;
using abaci::utility::LocalSymbols;
using abaci::utility::Temporaries;

class TypeEvalGen {
    mutable std::vector<Type> stack;
    Context *context;
    Cache *cache;
    LocalSymbols *locals;
    auto pop() const {
        Assert(!stack.empty());
        auto value = stack.back();
        stack.pop_back();
        return value;
    }
    void push(const Type& value) const {
        stack.push_back(value);
    }
public:
    TypeEvalGen() = delete;
    TypeEvalGen(Context *context, Cache *cache, LocalSymbols *locals = nullptr)
        : context{ context }, cache{ cache }, locals{ locals } {}
    Type get() const {
        Assert(stack.size() == 1);
        return stack.front();
    }
    void operator()(const ExprNode& expr) const;
    AbaciValue::Type promote(const Type& operand1, const Type& operand2) const;
};

class TypeCodeGen {
public:
    enum FunctionType { NotAFunction, FreeFunction, Method };
private:
    Context *context;
    Cache *cache;
    mutable LocalSymbols *locals;
    mutable Temporaries *temps = nullptr;
    FunctionType functionType;
    mutable std::optional<Type> returnType;
public:
    TypeCodeGen() = delete;
    TypeCodeGen(Context *context, Cache *cache, LocalSymbols *locals = nullptr, FunctionType functionType = NotAFunction)
        : context{ context }, cache{ cache }, locals{ locals }, functionType{ functionType } {}
    void operator()(const StmtList& stmts) const;
    void operator()(const StmtNode& stmt) const;
    Type get() const {
        if (returnType.has_value()) {
            return returnType.value();
        }
        else {
            return AbaciValue::None;
        }
    }
private:
    template<typename T>
    void codeGen(const T&) const;
};

} // namespace abaci::codegen

#endif
