#include "ast/Expr.hpp"
#include "engine/JIT.hpp"
#include "utility/Constant.hpp"
#include "utility/Report.hpp"
#include "utility/Temporary.hpp"
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Value.h>

namespace abaci::codegen {

using abaci::ast::ExprNode;
using abaci::engine::JIT;
using abaci::utility::AbaciValue;
using abaci::utility::Constants;
using abaci::utility::Type;
using abaci::utility::TypedValue;
using abaci::utility::LocalSymbols;
using abaci::utility::Temporaries;
using llvm::BasicBlock;
using llvm::IRBuilder;
using llvm::Module;
using llvm::StructType;
using llvm::Value;

class ExprCodeGen {
    JIT& jit;
    IRBuilder<>& builder;
    Module& module;
    const Constants& constants;
    LocalSymbols *locals;
    Temporaries *temps;
    mutable std::vector<TypedValue> stack;
    auto pop() const {
        Assert(!stack.empty());
        auto value = stack.back();
        stack.pop_back();
        return value;
    }
    void push(const TypedValue& value) const {
        stack.push_back(value);
    }
public:
    ExprCodeGen(JIT& jit, LocalSymbols *locals, Temporaries *temps)
        : jit{ jit }, builder{ jit.getBuilder() }, module{ jit.getModule() },
        constants{ jit.getConstants() }, locals{ locals }, temps{ temps } {}
    const auto& get() const {
        Assert(stack.size() == 1);
        return stack.front();
    }
    void operator()(const ExprNode& node) const;
    Value *toBoolean(TypedValue& operand) const;
    AbaciValue::Type promote(TypedValue& operand1, TypedValue& operand2) const;
private:
    template<ExprNode::Type N>
    void codeGen(const ExprNode& node) const;
};

} // namespace abaci::codegen
