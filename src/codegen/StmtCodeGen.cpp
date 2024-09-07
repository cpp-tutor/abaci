#include "StmtCodeGen.hpp"
#include "ExprCodeGen.hpp"
#include "utility/Operator.hpp"
#include "localize/Keywords.hpp"
#include "localize/Messages.hpp"
#include "engine/Utility.hpp"
#include <llvm/IR/Constants.h>

using namespace llvm;

namespace abaci::codegen {

using abaci::ast::ExprNode;
using abaci::ast::ExprList;
using abaci::ast::FunctionValueCall;
using abaci::ast::StmtNode;
using abaci::ast::StmtList;
using abaci::ast::CommentStmt;
using abaci::ast::PrintStmt;
using abaci::ast::PrintList;
using abaci::ast::InitStmt;
using abaci::ast::AssignStmt;
using abaci::ast::IfStmt;
using abaci::ast::WhileStmt;
using abaci::ast::RepeatStmt;
using abaci::ast::CaseStmt;
using abaci::ast::WhenStmt;
using abaci::ast::Function;
using abaci::ast::FunctionCall;
using abaci::ast::ReturnStmt;
using abaci::ast::ExprFunction;
using abaci::ast::Class;
using abaci::ast::DataAssignStmt;
using abaci::ast::MethodCall;
using abaci::ast::ExpressionStmt;
using abaci::utility::Operator;
using abaci::utility::GlobalSymbols;
using abaci::utility::typeToScalar;
using abaci::utility::addConstToType;
using abaci::utility::isConstant;
using abaci::utility::TypeInstance;
using abaci::utility::TypeBase;
using abaci::engine::getContextValue;
using abaci::engine::storeMutableValue;
using abaci::engine::storeGlobalValue;

void StmtCodeGen::operator()(const StmtList& stmts, BasicBlock *exit_block) const {
    if (!stmts.empty()) {
        locals = const_cast<LocalSymbols*>(static_cast<const LocalSymbols*>(&stmts));
        locals->makeVariables(jit);
        temps = const_cast<Temporaries*>(static_cast<const Temporaries*>(&stmts));
        Assert(temps->size() == 0);
        for (const auto& stmt : stmts) {
            (*this)(stmt);
        }
        if (!dynamic_cast<const ReturnStmt*>(stmts.back().get())) {
            temps->deleteTemporaries(jit);
            locals->destroyVariables(jit);
            if (exit_block) {
                builder.CreateBr(exit_block);
            }
        }
        temps->clear();
        locals->clear();
    }   
    else {
        if (exit_block) {
            builder.CreateBr(exit_block);
        }
    }
}

template<>
void StmtCodeGen::codeGen([[maybe_unused]] const CommentStmt& comment) const {
}

template<>
void StmtCodeGen::codeGen(const PrintStmt& print) const {
    PrintList print_data{ print.expression };
    print_data.insert(print_data.end(), print.format.begin(), print.format.end());
    Value *context = getContextValue(jit);
    for (auto field : print_data) {
        switch (field.index()) {
            case 0: {
                ExprCodeGen expr(jit, locals, temps);
                expr(std::get<ExprNode>(field));
                auto result = expr.get();
                switch (typeToScalar(result.second)) {
                    case AbaciValue::Boolean:
                        builder.CreateCall(module.getFunction("printValueBoolean"), { context, result.first });
                        break;
                    case AbaciValue::Integer:
                        builder.CreateCall(module.getFunction("printValueInteger"), { context, result.first });
                        break;
                    case AbaciValue::Floating:
                        builder.CreateCall(module.getFunction("printValueFloating"), { context, result.first });
                        break;
                    case AbaciValue::Complex:
                        builder.CreateCall(module.getFunction("printValueComplex"), { context, result.first });
                        break;
                    case AbaciValue::String:
                        builder.CreateCall(module.getFunction("printValueString"), { context, result.first });
                        break;
                    case AbaciValue::Instance:
                        builder.CreateCall(module.getFunction("printValueInstance"), { context, result.first });
                        break;
                    default:
                        UnexpectedError0(BadType);
                }
                break;
            }
            case 1:
                switch (std::get<Operator>(field)) {
                    case Operator::Comma:
                        builder.CreateCall(module.getFunction("printComma"), { context });
                        break;
                    case Operator::SemiColon:
                        break;
                    default:
                        UnexpectedError0(BadOperator);
                }
                break;
            default:
                UnexpectedError0(BadPrint);
                break;
        }
    }
    if (!print_data.empty() && print_data.back().index() == 1) {
        if (std::get<Operator>(print_data.back()) != Operator::SemiColon && std::get<Operator>(print_data.back()) != Operator::Comma) {
            builder.CreateCall(module.getFunction("printLn"), { context });
        }
    }
    else {
        builder.CreateCall(module.getFunction("printLn"), { context });
    }
}

template<>
void StmtCodeGen::codeGen(const InitStmt& define) const {
    ExprCodeGen expr(jit, locals, temps);
    expr(define.value);
    auto result = expr.get();
    if (locals) {
        auto index = locals->getIndex(define.name.get(), true).second;
        if (index == LocalSymbols::noVariable) {
            UnexpectedError1(VarNotExist, define.name.get());
        }
        else if (define.assign == Operator::Equal) {
            Assert(locals->getType(index) == addConstToType(result.second));
        }
        else {
            Assert(locals->getType(index) == result.second);
        }
        if (temps->isTemporary(result.first)) {
            temps->removeTemporary(result.first);
            storeMutableValue(jit, locals->getValue(index), result.first);
        }
        else {
            storeMutableValue(jit, locals->getValue(index), cloneValue(jit, result.first, result.second));
        }
    }
    else {
        auto globals = jit.getRuntimeContext().globals;
        auto globalIndex = globals->getIndex(define.name.get());
        if (globalIndex == GlobalSymbols::noVariable) {
            UnexpectedError1(VarNotExist, define.name.get());
        }
        else if (define.assign == Operator::Equal) {
            Assert(globals->getType(globalIndex) == addConstToType(result.second));
        }
        else {
            Assert(globals->getType(globalIndex) == result.second);
        }
        if (temps->isTemporary(result.first)) {
            temps->removeTemporary(result.first);
            storeGlobalValue(jit, globalIndex, result.first);
        }
        else {
            storeGlobalValue(jit, globalIndex, cloneValue(jit, result.first, result.second));
        }
    }
}

template<>
void StmtCodeGen::codeGen(const AssignStmt& assign) const {
    ExprCodeGen expr(jit, locals, temps);
    expr(assign.value);
    auto result = expr.get();
    auto [ vars, index ] = locals ? locals->getIndex(assign.name.get()) : std::pair{ nullptr, LocalSymbols::noVariable };
    if (index != LocalSymbols::noVariable) {
        if (isConstant(vars->getType(index))) {
            UnexpectedError1(NoConstantAssign, assign.name.get());
        }
        Assert(vars->getType(index) == result.second);
        destroyValue(jit, loadMutableValue(jit, locals->getValue(index), result.second), result.second);
        if (temps->isTemporary(result.first)) {
            temps->removeTemporary(result.first);
            storeMutableValue(jit, locals->getValue(index), result.first);
        }
        else {
            storeMutableValue(jit, locals->getValue(index), cloneValue(jit, result.first, result.second));
        }
    }
    else {
        auto globals = jit.getRuntimeContext().globals;
        auto globalIndex = globals->getIndex(assign.name.get());
        if (globalIndex == GlobalSymbols::noVariable) {
            UnexpectedError1(VarNotExist, assign.name.get());
        }
        else if (isConstant(globals->getType(globalIndex))) {
            UnexpectedError1(NoConstantAssign, assign.name.get());
        }
        destroyValue(jit, loadGlobalValue(jit, globalIndex, result.second), result.second);
        Assert(globals->getType(globalIndex) == result.second);
        if (temps->isTemporary(result.first)) {
            temps->removeTemporary(result.first);
            storeGlobalValue(jit, globalIndex, result.first);
        }
        else {
            storeGlobalValue(jit, globalIndex, cloneValue(jit, result.first, result.second));
        }
    }
}

template<>
void StmtCodeGen::codeGen(const IfStmt& if_stmt) const {
    ExprCodeGen expr(jit, locals, temps);
    expr(if_stmt.condition);
    auto result = expr.get();
    Value *condition;
    if (typeToScalar(result.second) == AbaciValue::Boolean) {
        condition = result.first;
    }
    else {
        condition = expr.toBoolean(result);
    }
    BasicBlock *true_block = BasicBlock::Create(jit.getContext(), "", jit.getFunction());
    BasicBlock *false_block = BasicBlock::Create(jit.getContext(), "", jit.getFunction());
    BasicBlock *merge_block = BasicBlock::Create(jit.getContext(), "", jit.getFunction());
    builder.CreateCondBr(condition, true_block, false_block);
    builder.SetInsertPoint(true_block);
    (*this)(if_stmt.true_test, merge_block);
    builder.SetInsertPoint(false_block);
    (*this)(if_stmt.false_test, merge_block);
    builder.SetInsertPoint(merge_block);
}

template<>
void StmtCodeGen::codeGen(const WhileStmt& while_stmt) const {
    BasicBlock *pre_block = BasicBlock::Create(jit.getContext(), "", jit.getFunction());
    BasicBlock *loop_block = BasicBlock::Create(jit.getContext(), "", jit.getFunction());
    BasicBlock *post_block = BasicBlock::Create(jit.getContext(), "", jit.getFunction());
    builder.CreateBr(pre_block);
    builder.SetInsertPoint(pre_block);
    ExprCodeGen expr(jit, locals, temps);
    expr(while_stmt.condition);
    auto result = expr.get();
    Value *condition;
    if (typeToScalar(result.second) == AbaciValue::Boolean) {
        condition = result.first;
    }
    else {
        condition = expr.toBoolean(result);
    }
    builder.CreateCondBr(condition, loop_block, post_block);
    builder.SetInsertPoint(loop_block);
    (*this)(while_stmt.loop_block);
    builder.CreateBr(pre_block);
    builder.SetInsertPoint(post_block);
}

template<>
void StmtCodeGen::codeGen(const RepeatStmt& repeat_stmt) const {
    BasicBlock *loop_block = BasicBlock::Create(jit.getContext(), "", jit.getFunction());
    BasicBlock *post_block = BasicBlock::Create(jit.getContext(), "", jit.getFunction());
    builder.CreateBr(loop_block);
    builder.SetInsertPoint(loop_block);
    (*this)(repeat_stmt.loop_block);
    ExprCodeGen expr(jit, locals, temps);
    expr(repeat_stmt.condition);
    auto result = expr.get();
    Value *condition;
    if (typeToScalar(result.second) == AbaciValue::Boolean) {
        condition = result.first;
    }
    else {
        condition = expr.toBoolean(result);
    }
    builder.CreateCondBr(condition, post_block, loop_block);
    builder.SetInsertPoint(post_block);
}

template<>
void StmtCodeGen::codeGen(const CaseStmt& case_stmt) const {
    std::vector<BasicBlock*> case_blocks(case_stmt.matches.size() * 2 + 1 + !case_stmt.unmatched.empty());
    for (auto& block : case_blocks) {
        block = BasicBlock::Create(jit.getContext(), "", jit.getFunction());
    }
    ExprCodeGen expr(jit, locals, temps);
    expr(case_stmt.case_value);
    auto result = expr.get();
    builder.CreateBr(case_blocks.front());
    for (int block_number = 0; const auto& when : case_stmt.matches) {
        builder.SetInsertPoint(case_blocks.at(block_number * 2));
        auto match_result = result;
        ExprCodeGen expr(jit, locals, temps);
        expr(when.expression);
        auto when_result = expr.get();
        Value *is_match;
        switch (expr.promote(when_result, match_result)) {
            case AbaciValue::Boolean:
            case AbaciValue::Integer:
                is_match = builder.CreateICmpEQ(when_result.first, match_result.first);
                break;
            case AbaciValue::Floating:
                is_match = builder.CreateFCmpOEQ(when_result.first, match_result.first);
                break;
            case AbaciValue::Complex: {
                Value *real_value1 = builder.CreateLoad(builder.getDoubleTy(), builder.CreateStructGEP(jit.getNamedType("struct.Complex"), when_result.first, 0));
                Value *imag_value1 = builder.CreateLoad(builder.getDoubleTy(), builder.CreateStructGEP(jit.getNamedType("struct.Complex"), when_result.first, 1));
                Value *real_value2 = builder.CreateLoad(builder.getDoubleTy(), builder.CreateStructGEP(jit.getNamedType("struct.Complex"), match_result.first, 0));
                Value *imag_value2 = builder.CreateLoad(builder.getDoubleTy(), builder.CreateStructGEP(jit.getNamedType("struct.Complex"), match_result.first, 1));
                is_match = builder.CreateAnd(builder.CreateFCmpOEQ(real_value1, real_value2), builder.CreateFCmpOEQ(imag_value1, imag_value2));
                break;
            }
            case AbaciValue::String:
                is_match = builder.CreateCall(module.getFunction("compareString"), { when_result.first, match_result.first });
                break;
            default:
                LogicError0(BadType);
        }
        builder.CreateCondBr(is_match, case_blocks.at(block_number * 2 + 1), case_blocks.at(block_number * 2 + 2));
        builder.SetInsertPoint(case_blocks.at(block_number * 2 + 1));
        (*this)(when.block, case_blocks.back());
        ++block_number;
    }
    if (!case_stmt.unmatched.empty()) {
        builder.SetInsertPoint(case_blocks.at(case_blocks.size() - 2));
        (*this)(case_stmt.unmatched, case_blocks.back());
    }
    builder.SetInsertPoint(case_blocks.back());
}

template<>
void StmtCodeGen::codeGen([[maybe_unused]] const Function& function) const {
}

template<>
void StmtCodeGen::codeGen(const FunctionCall& function_call) const {
    std::vector<Value*> arguments;
    std::vector<Type> types;
    for (const auto& arg : function_call.args) {
        ExprCodeGen expr(jit, locals, temps);
        expr(arg);
        auto result = expr.get();
        arguments.push_back(result.first);
        types.push_back(result.second);
    }
    auto type = jit.getCache()->getFunctionInstantiationType(function_call.name, types);
    builder.CreateCall(module.getFunction(mangled(function_call.name, types)), arguments);
}

template<>
void StmtCodeGen::codeGen(const ReturnStmt& return_stmt) const {
    ExprCodeGen expr(jit, locals, temps);
    expr(return_stmt.expression);
    auto returnValue = expr.get();
    temps->removeTemporary(returnValue.first);
    auto [ vars, index ] = locals->getIndex(RETURN_V);
    Assert(index != LocalSymbols::noVariable);
    storeMutableValue(jit, vars->getValue(index), returnValue.first);
    Temporaries *tmps = temps;
    while (tmps != nullptr) {
        tmps->deleteTemporaries(jit);
        tmps = tmps->getEnclosing();
    }
    LocalSymbols *lcls = locals;
    while (lcls->getEnclosing() != nullptr) {
        lcls->destroyVariables(jit);
        lcls = lcls->getEnclosing();
    }
    builder.CreateBr(exit_block);
}

template<>
void StmtCodeGen::codeGen([[maybe_unused]] const ExprFunction& expression_function) const {
}

template<>
void StmtCodeGen::codeGen([[maybe_unused]] const Class& class_template) const {
}

template<>
void StmtCodeGen::codeGen(const DataAssignStmt& data_assign) const {
    const std::string& name = data_assign.name.get();
    Type type;
    ExprCodeGen expr(jit, locals, temps);
    expr(data_assign.value);
    auto result = expr.get();
    auto [ vars, index ] = locals ? locals->getIndex(name) : std::pair{ nullptr, LocalSymbols::noVariable };
    if (index != LocalSymbols::noVariable) {
        type = vars->getType(index);
        if (isConstant(type)) {
            UnexpectedError1(NoConstantAssign, name);
        }
        auto *value = loadMutableValue(jit, vars->getValue(index), type);
        Value *valuePtr = nullptr;
        for (const auto& member : data_assign.member_list) {
            if (typeToScalar(removeConstFromType(type)) != AbaciValue::Instance) {
                UnexpectedError0(BadObject);
            }
            auto instanceType = std::dynamic_pointer_cast<TypeInstance>(std::get<std::shared_ptr<TypeBase>>(type));
            Assert(instanceType != nullptr);
            auto index = jit.getCache()->getMemberIndex(jit.getCache()->getClass(instanceType->className), member);
            type = instanceType->variableTypes.at(index);
            ArrayType *array = ArrayType::get(builder.getInt64Ty(), instanceType->variableTypes.size());
            Value *arrayPtr = builder.CreateLoad(PointerType::get(array, 0), builder.CreateStructGEP(jit.getNamedType("struct.Instance"), value, 2));
            valuePtr = builder.CreateGEP(array, arrayPtr, { builder.getInt32(0), builder.getInt32(index) });
            value = builder.CreateLoad(typeToLLVMType(jit, type), valuePtr);
        }
        Assert(valuePtr != nullptr);
        Assert(type == result.second);
        destroyValue(jit, value, type);
        if (temps->isTemporary(result.first)) {
            temps->removeTemporary(result.first);
            builder.CreateStore(result.first, valuePtr);
        }
        else {
            builder.CreateStore(cloneValue(jit, result.first, type), valuePtr);
        }
    }
    else {
        auto globals = jit.getRuntimeContext().globals;
        auto globalIndex = globals->getIndex(name);
        if (globalIndex == GlobalSymbols::noVariable) {
            UnexpectedError1(VarNotExist, name);
        }
        type = globals->getType(globalIndex);
        if (isConstant(type)) {
            UnexpectedError1(NoConstantAssign, name);
        }
        auto *value = loadGlobalValue(jit, globalIndex, globals->getType(globalIndex));
        Value *valuePtr = nullptr;
        for (const auto& member : data_assign.member_list) {
            if (typeToScalar(removeConstFromType(type)) != AbaciValue::Instance) {
                UnexpectedError0(BadObject);
            }
            auto instanceType = std::dynamic_pointer_cast<TypeInstance>(std::get<std::shared_ptr<TypeBase>>(type));
            Assert(instanceType != nullptr);
            auto index = jit.getCache()->getMemberIndex(jit.getCache()->getClass(instanceType->className), member);
            type = instanceType->variableTypes.at(index);
            ArrayType *array = ArrayType::get(builder.getInt64Ty(), instanceType->variableTypes.size());
            Value *arrayPtr = builder.CreateLoad(PointerType::get(array, 0), builder.CreateStructGEP(jit.getNamedType("struct.Instance"), value, 2));
            valuePtr = builder.CreateGEP(array, arrayPtr, { builder.getInt32(0), builder.getInt32(index) });
            value = builder.CreateLoad(typeToLLVMType(jit, type), valuePtr);
        }
        Assert(valuePtr != nullptr);
        Assert(type == result.second);
        destroyValue(jit, value, type);
        if (temps->isTemporary(result.first)) {
            temps->removeTemporary(result.first);
            builder.CreateStore(result.first, valuePtr);
        }
        else {
            builder.CreateStore(cloneValue(jit, result.first, type), valuePtr);
        }
    }
}

template<>
void StmtCodeGen::codeGen(const MethodCall& method_call) const {
    const std::string& name = method_call.name.get();
    Type type;
    Value *thisPtr = nullptr;
    if (locals) {
        auto [ vars, index ] = locals->getIndex(name);
        if (index != LocalSymbols::noVariable) {
            type = vars->getType(index);
            auto *value = loadMutableValue(jit, vars->getValue(index), type);
            for (const auto& member : method_call.member_list) {
                if (typeToScalar(removeConstFromType(type)) != AbaciValue::Instance) {
                    UnexpectedError0(BadObject);
                }
                auto instanceType = std::dynamic_pointer_cast<TypeInstance>(std::get<std::shared_ptr<TypeBase>>(type));
                Assert(instanceType != nullptr);
                auto index = jit.getCache()->getMemberIndex(jit.getCache()->getClass(instanceType->className), member);
                type = instanceType->variableTypes.at(index);
                ArrayType *array = ArrayType::get(builder.getInt64Ty(), instanceType->variableTypes.size());
                Value *arrayPtr = builder.CreateLoad(PointerType::get(array, 0), builder.CreateStructGEP(jit.getNamedType("struct.Instance"), value, 2));
                Value *valuePtr = builder.CreateGEP(array, arrayPtr, { builder.getInt32(0), builder.getInt32(index) });
                value = builder.CreateLoad(typeToLLVMType(jit, type), valuePtr);
            }
            thisPtr = value;
        }
    }
    auto globals = jit.getRuntimeContext().globals;
    auto globalIndex = globals->getIndex(name);
    if (globalIndex != GlobalSymbols::noVariable) {
        auto *value = loadGlobalValue(jit, globalIndex, globals->getType(globalIndex));
        type = globals->getType(globalIndex);
        for (const auto& member : method_call.member_list) {
            if (typeToScalar(removeConstFromType(type)) != AbaciValue::Instance) {
                UnexpectedError0(BadObject);
            }
            auto instanceType = std::dynamic_pointer_cast<TypeInstance>(std::get<std::shared_ptr<TypeBase>>(type));
            Assert(instanceType != nullptr);
            auto index = jit.getCache()->getMemberIndex(jit.getCache()->getClass(instanceType->className), member);
            type = instanceType->variableTypes.at(index);
            ArrayType *array = ArrayType::get(builder.getInt64Ty(), instanceType->variableTypes.size());
            Value *arrayPtr = builder.CreateLoad(PointerType::get(array, 0), builder.CreateStructGEP(jit.getNamedType("struct.Instance"), value, 2));
            Value *valuePtr = builder.CreateGEP(array, arrayPtr, { builder.getInt32(0), builder.getInt32(index) });
            value = builder.CreateLoad(typeToLLVMType(jit, type), valuePtr);
        }
        thisPtr = value;
    }
    if (thisPtr != nullptr) {
        auto instanceType = std::dynamic_pointer_cast<TypeInstance>(std::get<std::shared_ptr<TypeBase>>(type));
        Assert(instanceType != nullptr);
        std::string method_name = instanceType->className + '.' + method_call.method;
        std::vector<Value*> arguments;
        std::vector<Type> types;
        for (const auto& arg : method_call.args) {
            ExprCodeGen expr(jit, locals, temps);
            expr(arg);
            auto result = expr.get();
            arguments.push_back(result.first);
            types.push_back(result.second);
        }
        auto type = jit.getCache()->getFunctionInstantiationType(method_name, types);
        arguments.insert(arguments.begin(), thisPtr);
        builder.CreateCall(module.getFunction(mangled(method_name, types)), arguments);
    }
    else {
        UnexpectedError0(BadObject);
    }
}

template<>
void StmtCodeGen::codeGen([[maybe_unused]] const ExpressionStmt& expression_stmt) const {
}

void StmtCodeGen::operator()(const StmtNode& stmt) const {
    const auto *stmt_data = stmt.get();
    if (dynamic_cast<const CommentStmt*>(stmt_data)) {
        codeGen(dynamic_cast<const CommentStmt&>(*stmt_data));
    }
    else if (dynamic_cast<const PrintStmt*>(stmt_data)) {
        codeGen(dynamic_cast<const PrintStmt&>(*stmt_data));
    }
    else if (dynamic_cast<const InitStmt*>(stmt_data)) {
        codeGen(dynamic_cast<const InitStmt&>(*stmt_data));
    }
    else if (dynamic_cast<const AssignStmt*>(stmt_data)) {
        codeGen(dynamic_cast<const AssignStmt&>(*stmt_data));
    }
    else if (dynamic_cast<const IfStmt*>(stmt_data)) {
        codeGen(dynamic_cast<const IfStmt&>(*stmt_data));
    }
    else if (dynamic_cast<const WhileStmt*>(stmt_data)) {
        codeGen(dynamic_cast<const WhileStmt&>(*stmt_data));
    }
    else if (dynamic_cast<const RepeatStmt*>(stmt_data)) {
        codeGen(dynamic_cast<const RepeatStmt&>(*stmt_data));
    }
    else if (dynamic_cast<const CaseStmt*>(stmt_data)) {
        codeGen(dynamic_cast<const CaseStmt&>(*stmt_data));
    }
    else if (dynamic_cast<const Function*>(stmt_data)) {
        codeGen(dynamic_cast<const Function&>(*stmt_data));
    }
    else if (dynamic_cast<const FunctionCall*>(stmt_data)) {
        codeGen(dynamic_cast<const FunctionCall&>(*stmt_data));
    }
    else if (dynamic_cast<const ReturnStmt*>(stmt_data)) {
        codeGen(dynamic_cast<const ReturnStmt&>(*stmt_data));
    }
    else if (dynamic_cast<const ExprFunction*>(stmt_data)) {
        codeGen(dynamic_cast<const ExprFunction&>(*stmt_data));
    }
    else if (dynamic_cast<const Class*>(stmt_data)) {
        codeGen(dynamic_cast<const Class&>(*stmt_data));
    }
    else if (dynamic_cast<const DataAssignStmt*>(stmt_data)) {
        codeGen(dynamic_cast<const DataAssignStmt&>(*stmt_data));
    }
    else if (dynamic_cast<const MethodCall*>(stmt_data)) {
        codeGen(dynamic_cast<const MethodCall&>(*stmt_data));
    }
    else if (dynamic_cast<const ExpressionStmt*>(stmt_data)) {
        codeGen(dynamic_cast<const ExpressionStmt&>(*stmt_data));
    }
    else {
        UnexpectedError0(BadStmtNode);
    }
}

} // namespace abaci::codegen
