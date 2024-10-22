#include "StmtCodeGen.hpp"
#include "ExprCodeGen.hpp"
#include "utility/Operator.hpp"
#include "localize/Keywords.hpp"
#include "localize/Messages.hpp"
#include "utility/Utility.hpp"
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
using abaci::ast::ListAssignStmt;
using abaci::ast::DataListAssignStmt;
using abaci::utility::Operator;
using abaci::utility::GlobalSymbols;
using abaci::utility::typeToScalar;
using abaci::utility::addConstToType;
using abaci::utility::isConstant;
using abaci::utility::TypeInstance;
using abaci::utility::TypeBase;
using abaci::utility::TypeList;
using abaci::utility::getContextValue;
using abaci::utility::storeMutableValue;
using abaci::utility::storeGlobalValue;

void StmtCodeGen::operator()(const StmtList& stmts, BasicBlock *exitBlock) const {
    if (!stmts.statements.empty()) {
        locals = const_cast<LocalSymbols*>(static_cast<const LocalSymbols*>(&stmts));
        locals->makeVariables(jit);
        Temporaries *enclosingTemps = temps;
        temps = const_cast<Temporaries*>(static_cast<const Temporaries*>(&stmts));
        Assert(temps->size() == 0);
        temps->setEnclosing(enclosingTemps);
        for (const auto& stmt : stmts.statements) {
            (*this)(stmt);
        }
        if (!dynamic_cast<const ReturnStmt*>(stmts.statements.back().get())) {
            temps->destroyTemporaries(jit);
            locals->destroyVariables(jit);
            if (exitBlock) {
                builder.CreateBr(exitBlock);
            }
        }
        temps->clear();
        locals->clear();
        temps = temps->getEnclosing();
        locals = locals->getEnclosing();
    }   
    else {
        if (exitBlock) {
            builder.CreateBr(exitBlock);
        }
    }
}

template<>
void StmtCodeGen::codeGen([[maybe_unused]] const CommentStmt& comment) const {
}

template<>
void StmtCodeGen::codeGen(const PrintStmt& print) const {
    PrintList printData{ print.expression };
    if (print.separator != Operator::None) {
        printData.push_back(print.separator);
    }
    for (const auto& format : print.format) {
        if (std::holds_alternative<Operator>(format.data)) {
            printData.push_back(std::get<Operator>(format.data));
        }
        else {
            printData.push_back(format);
        }
    }
    Value *context = getContextValue(jit);
    for (auto field : printData) {
        switch (field.index()) {
            case 0: {
                ExprCodeGen expr(jit, locals, temps);
                expr(std::get<ExprNode>(field));
                auto result = expr.get();
                switch (typeToScalar(result.second)) {
                    case AbaciValue::None:
                        builder.CreateCall(module.getFunction("printValueNil"), { context, result.first });
                        break;
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
                    case AbaciValue::List: 
                        builder.CreateCall(module.getFunction("printValueList"), { context, result.first });
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
    if (!printData.empty() && printData.back().index() == 1) {
        if (std::get<Operator>(printData.back()) != Operator::SemiColon && std::get<Operator>(printData.back()) != Operator::Comma) {
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
        auto index = locals->getIndex(define.name.name, true).second;
        if (index == LocalSymbols::noVariable) {
            UnexpectedError1(VariableNotExist, define.name.name);
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
        auto globalIndex = globals->getIndex(define.name.name);
        if (globalIndex == GlobalSymbols::noVariable) {
            UnexpectedError1(VariableNotExist, define.name.name);
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
    auto [ variables, index ] = locals ? locals->getIndex(assign.name.name) : std::pair{ nullptr, LocalSymbols::noVariable };
    if (index != LocalSymbols::noVariable) {
        if (isConstant(variables->getType(index))) {
            UnexpectedError1(NoConstantAssign, assign.name.name);
        }
        Assert(variables->getType(index) == result.second);
        destroyValue(jit, loadMutableValue(jit, variables->getValue(index), result.second), result.second);
        if (temps->isTemporary(result.first)) {
            temps->removeTemporary(result.first);
            storeMutableValue(jit, variables->getValue(index), result.first);
        }
        else {
            storeMutableValue(jit, variables->getValue(index), cloneValue(jit, result.first, result.second));
        }
    }
    else {
        auto globals = jit.getRuntimeContext().globals;
        auto globalIndex = globals->getIndex(assign.name.name);
        if (globalIndex == GlobalSymbols::noVariable) {
            UnexpectedError1(VariableNotExist, assign.name.name);
        }
        else if (isConstant(globals->getType(globalIndex))) {
            UnexpectedError1(NoConstantAssign, assign.name.name);
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
void StmtCodeGen::codeGen(const IfStmt& ifStmt) const {
    ExprCodeGen expr(jit, locals, temps);
    expr(ifStmt.condition);
    auto result = expr.get();
    Value *condition;
    if (typeToScalar(result.second) == AbaciValue::Boolean) {
        condition = result.first;
    }
    else {
        condition = expr.toBoolean(result);
    }
    BasicBlock *trueBlock = BasicBlock::Create(jit.getContext(), "", jit.getFunction());
    BasicBlock *falseBlock = BasicBlock::Create(jit.getContext(), "", jit.getFunction());
    BasicBlock *mergeBlock = BasicBlock::Create(jit.getContext(), "", jit.getFunction());
    builder.CreateCondBr(condition, trueBlock, falseBlock);
    builder.SetInsertPoint(trueBlock);
    (*this)(ifStmt.trueBlock, mergeBlock);
    builder.SetInsertPoint(falseBlock);
    (*this)(ifStmt.falseBlock, mergeBlock);
    builder.SetInsertPoint(mergeBlock);
}

template<>
void StmtCodeGen::codeGen(const WhileStmt& whileStmt) const {
    BasicBlock *preBlock = BasicBlock::Create(jit.getContext(), "", jit.getFunction());
    BasicBlock *loopBlock = BasicBlock::Create(jit.getContext(), "", jit.getFunction());
    BasicBlock *postBlock = BasicBlock::Create(jit.getContext(), "", jit.getFunction());
    builder.CreateBr(preBlock);
    builder.SetInsertPoint(preBlock);
    ExprCodeGen expr(jit, locals, temps);
    expr(whileStmt.condition);
    auto result = expr.get();
    Value *condition;
    if (typeToScalar(result.second) == AbaciValue::Boolean) {
        condition = result.first;
    }
    else {
        condition = expr.toBoolean(result);
    }
    builder.CreateCondBr(condition, loopBlock, postBlock);
    builder.SetInsertPoint(loopBlock);
    (*this)(whileStmt.loopBlock);
    builder.CreateBr(preBlock);
    builder.SetInsertPoint(postBlock);
}

template<>
void StmtCodeGen::codeGen(const RepeatStmt& repeatStmt) const {
    BasicBlock *loopBlock = BasicBlock::Create(jit.getContext(), "", jit.getFunction());
    BasicBlock *postBlock = BasicBlock::Create(jit.getContext(), "", jit.getFunction());
    builder.CreateBr(loopBlock);
    builder.SetInsertPoint(loopBlock);
    (*this)(repeatStmt.loopBlock);
    ExprCodeGen expr(jit, locals, temps);
    expr(repeatStmt.condition);
    auto result = expr.get();
    Value *condition;
    if (typeToScalar(result.second) == AbaciValue::Boolean) {
        condition = result.first;
    }
    else {
        condition = expr.toBoolean(result);
    }
    builder.CreateCondBr(condition, postBlock, loopBlock);
    builder.SetInsertPoint(postBlock);
}

template<>
void StmtCodeGen::codeGen(const CaseStmt& caseStmt) const {
    std::vector<std::pair<const ExprNode*,const StmtList*>> matches;
    for (const auto& match : caseStmt.matches) {
#ifdef ABACI_USE_OLDER_BOOST
        matches.emplace_back(&match.expression, &match.block);
#endif
        for (const auto& expression : match.expressions) {
            matches.emplace_back(&expression, &match.block);
        }
    }
    std::vector<BasicBlock*> caseBlocks(matches.size() * 2 + 1 + !caseStmt.unmatched.statements.empty());
    for (auto& block : caseBlocks) {
        block = BasicBlock::Create(jit.getContext(), "", jit.getFunction());
    }
    ExprCodeGen expr(jit, locals, temps);
    expr(caseStmt.caseValue);
    auto result = expr.get();
    builder.CreateBr(caseBlocks.front());
    for (std::size_t blockNumber = 0; const auto& when : matches) {
        builder.SetInsertPoint(caseBlocks.at(blockNumber * 2));
        auto matchResult = result;
        ExprCodeGen expr(jit, locals, temps);
        expr(*(when.first));
        auto whenResult = expr.get();
        Value *isMatch;
        switch (typeToScalar(expr.promote(whenResult, matchResult))) {
            case AbaciValue::Boolean:
            case AbaciValue::Integer:
                isMatch = builder.CreateICmpEQ(whenResult.first, matchResult.first);
                break;
            case AbaciValue::Floating:
                isMatch = builder.CreateFCmpOEQ(whenResult.first, matchResult.first);
                break;
            case AbaciValue::Complex: {
                Value *realValue1 = builder.CreateLoad(builder.getDoubleTy(), builder.CreateStructGEP(jit.getNamedType("struct.Complex"), whenResult.first, 0));
                Value *imagValue1 = builder.CreateLoad(builder.getDoubleTy(), builder.CreateStructGEP(jit.getNamedType("struct.Complex"), whenResult.first, 1));
                Value *realValue2 = builder.CreateLoad(builder.getDoubleTy(), builder.CreateStructGEP(jit.getNamedType("struct.Complex"), matchResult.first, 0));
                Value *imagValue2 = builder.CreateLoad(builder.getDoubleTy(), builder.CreateStructGEP(jit.getNamedType("struct.Complex"), matchResult.first, 1));
                isMatch = builder.CreateAnd(builder.CreateFCmpOEQ(realValue1, realValue2), builder.CreateFCmpOEQ(imagValue1, imagValue2));
                break;
            }
            case AbaciValue::String:
                isMatch = builder.CreateCall(module.getFunction("compareString"), { whenResult.first, matchResult.first });
                break;
            default:
                UnexpectedError0(BadType);
        }
        builder.CreateCondBr(isMatch, caseBlocks.at(blockNumber * 2 + 1), caseBlocks.at(blockNumber * 2 + 2));
        builder.SetInsertPoint(caseBlocks.at(blockNumber * 2 + 1));
        (*this)(*(when.second), caseBlocks.back());
        ++blockNumber;
    }
    if (!caseStmt.unmatched.statements.empty()) {
        builder.SetInsertPoint(caseBlocks.at(caseBlocks.size() - 2));
        (*this)(caseStmt.unmatched, caseBlocks.back());
    }
    builder.SetInsertPoint(caseBlocks.back());
}

template<>
void StmtCodeGen::codeGen([[maybe_unused]] const Function& function) const {
}

template<>
void StmtCodeGen::codeGen(const FunctionCall& functionCall) const {
    std::vector<Value*> arguments;
    std::vector<Type> types;
    for (const auto& arg : functionCall.args) {
        ExprCodeGen expr(jit, locals, temps);
        expr(arg);
        auto result = expr.get();
        arguments.push_back(result.first);
        types.push_back(result.second);
    }
    auto type = jit.getCache()->getFunctionInstantiationType(functionCall.name, types);
    builder.CreateCall(module.getFunction(mangled(functionCall.name, types)), arguments);
}

template<>
void StmtCodeGen::codeGen(const ReturnStmt& returnStmt) const {
    ExprCodeGen expr(jit, locals, temps);
    expr(returnStmt.expression);
    auto returnValue = expr.get();
    Value *value;
    if (temps->isTemporary(returnValue.first)) {
        temps->removeTemporary(returnValue.first);
        value = returnValue.first;
    }
    else {
        value = cloneValue(jit, returnValue.first, returnValue.second);
    }
    auto [ variables, index ] = locals->getIndex(RETURN_V);
    Assert(index != LocalSymbols::noVariable);
    storeMutableValue(jit, variables->getValue(index), value);
    Temporaries *tmps = temps;
    while (tmps != nullptr) {
        tmps->destroyTemporaries(jit);
        tmps = tmps->getEnclosing();
    }
    LocalSymbols *lcls = locals;
    while (lcls->getEnclosing() != nullptr) {
        lcls->destroyVariables(jit);
        lcls = lcls->getEnclosing();
    }
    builder.CreateBr(exitBlock);
}

template<>
void StmtCodeGen::codeGen([[maybe_unused]] const ExprFunction& expressionFunction) const {
}

template<>
void StmtCodeGen::codeGen([[maybe_unused]] const Class& classTemplate) const {
}

template<>
void StmtCodeGen::codeGen(const DataAssignStmt& dataAssign) const {
    const std::string& name = dataAssign.name.name;
    Type type;
    ExprCodeGen expr(jit, locals, temps);
    expr(dataAssign.value);
    auto result = expr.get();
    auto [ variables, index ] = locals ? locals->getIndex(name) : std::pair{ nullptr, LocalSymbols::noVariable };
    if (index != LocalSymbols::noVariable) {
        type = variables->getType(index);
        if (isConstant(type)) {
            UnexpectedError1(NoConstantAssign, name);
        }
        Value *value = loadMutableValue(jit, variables->getValue(index), type);
        Value *valuePtr = nullptr;
        for (const auto& member : dataAssign.memberList) {
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
            UnexpectedError1(VariableNotExist, name);
        }
        type = globals->getType(globalIndex);
        if (isConstant(type)) {
            UnexpectedError1(NoConstantAssign, name);
        }
        Value *value = loadGlobalValue(jit, globalIndex, globals->getType(globalIndex));
        Value *valuePtr = nullptr;
        for (const auto& member : dataAssign.memberList) {
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
void StmtCodeGen::codeGen(const MethodCall& methodCall) const {
    const std::string& name = methodCall.name.name;
    Type type;
    Value *thisPtr = nullptr;
    if (locals) {
        auto [ variables, index ] = locals->getIndex(name);
        if (index != LocalSymbols::noVariable) {
            type = variables->getType(index);
            Value *value = loadMutableValue(jit, variables->getValue(index), type);
            for (const auto& member : methodCall.memberList) {
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
        Value *value = loadGlobalValue(jit, globalIndex, globals->getType(globalIndex));
        type = globals->getType(globalIndex);
        for (const auto& member : methodCall.memberList) {
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
        std::string methodName = instanceType->className + '.' + methodCall.method;
        std::vector<Value*> arguments;
        std::vector<Type> types;
        for (const auto& arg : methodCall.args) {
            ExprCodeGen expr(jit, locals, temps);
            expr(arg);
            auto result = expr.get();
            arguments.push_back(result.first);
            types.push_back(result.second);
        }
        types.insert(types.begin(), instanceType);
        arguments.insert(arguments.begin(), thisPtr);
        auto type = jit.getCache()->getFunctionInstantiationType(methodName, types);
        builder.CreateCall(module.getFunction(mangled(methodName, types)), arguments);
    }
    else {
        UnexpectedError0(BadObject);
    }
}

template<>
void StmtCodeGen::codeGen([[maybe_unused]] const ExpressionStmt& expressionStmt) const {
}

template<>
void StmtCodeGen::codeGen(const ListAssignStmt& listAssign) const {
    ExprCodeGen expr(jit, locals, temps);
    expr(listAssign.value);
    auto result = expr.get();
    auto [ variables, index ] = locals ? locals->getIndex(listAssign.name.name) : std::pair{ nullptr, LocalSymbols::noVariable };
    if (index != LocalSymbols::noVariable) {
        Type type = variables->getType(index);
        if (isConstant(type)) {
            UnexpectedError1(NoConstantAssign, listAssign.name.name);
        }
        if (std::holds_alternative<std::shared_ptr<TypeBase>>(type)) {
            if (auto typePtr = std::dynamic_pointer_cast<TypeList>(std::get<std::shared_ptr<TypeBase>>(type))) {
                Value *value = loadMutableValue(jit, variables->getValue(index), type), *elementPtr = nullptr, *index = nullptr, *list = nullptr;;
                for (const auto& indexExpression : listAssign.indexes) {
                    if (typePtr == nullptr) {
                        UnexpectedError1(TooManyIndexes, listAssign.name.name);
                    }
                    ExprCodeGen expr(jit, locals, temps);
                    expr(indexExpression);
                    if (typeToScalar(expr.get().second) != AbaciValue::Integer) {
                        UnexpectedError0(IndexNotInt);
                    }
                    index = expr.get().first;
                    list = value;
                    Value *listSize = builder.CreateLoad(builder.getInt64Ty(), builder.CreateStructGEP(jit.getNamedType("struct.List"), value, 0));
                    ArrayType *array = ArrayType::get(builder.getInt64Ty(), 0);
                    Value *listElements = builder.CreateLoad(PointerType::get(array, 0), builder.CreateStructGEP(jit.getNamedType("struct.List"), value, 1));
                    elementPtr = builder.CreateGEP(array, listElements, { builder.getInt32(0), index });
                    type = typePtr->elementType;
                    value = builder.CreateLoad(typeToLLVMType(jit, type), elementPtr);
                    if (std::holds_alternative<std::shared_ptr<TypeBase>>(type)) {
                        typePtr = std::dynamic_pointer_cast<TypeList>(std::get<std::shared_ptr<TypeBase>>(type));
                    }
                    else {
                        typePtr = nullptr;
                    }
                }
                if (result.second != AbaciValue::None) {
                    Assert(type == result.second);
                    destroyValue(jit, value, type);
                    if (temps->isTemporary(result.first)) {
                        temps->removeTemporary(result.first);
                        builder.CreateStore(result.first, elementPtr);
                    }
                    else {
                        builder.CreateStore(cloneValue(jit, result.first, result.second), elementPtr);
                    }
                }
                else {
                    destroyValue(jit, value, type);
                    builder.CreateCall(module.getFunction("deleteElement"), { list, index });
                }
                return;
            }
        }
        UnexpectedError1(VariableNotList, listAssign.name.name);
    }
    else {
        auto globals = jit.getRuntimeContext().globals;
        auto globalIndex = globals->getIndex(listAssign.name.name);
        if (globalIndex == GlobalSymbols::noVariable) {
            UnexpectedError1(VariableNotExist, listAssign.name.name);
        }
        Type type = globals->getType(globalIndex);
        if (isConstant(type)) {
            UnexpectedError1(NoConstantAssign, listAssign.name.name);
        }
        if (std::holds_alternative<std::shared_ptr<TypeBase>>(type)) {
            if (auto typePtr = std::dynamic_pointer_cast<TypeList>(std::get<std::shared_ptr<TypeBase>>(type))) {
                Value *value = loadGlobalValue(jit, globalIndex, type), *elementPtr = nullptr, *index = nullptr, *list = nullptr;
                for (const auto& indexExpression : listAssign.indexes) {
                    if (typePtr == nullptr) {
                        UnexpectedError1(TooManyIndexes, listAssign.name.name);
                    }
                    ExprCodeGen expr(jit, locals, temps);
                    expr(indexExpression);
                    if (typeToScalar(expr.get().second) != AbaciValue::Integer) {
                        UnexpectedError0(IndexNotInt);
                    }
                    index = expr.get().first;
                    list = value;
                    Value *listSize = builder.CreateLoad(builder.getInt64Ty(), builder.CreateStructGEP(jit.getNamedType("struct.List"), value, 0));
                    ArrayType *array = ArrayType::get(builder.getInt64Ty(), 0);
                    Value *listElements = builder.CreateLoad(PointerType::get(array, 0), builder.CreateStructGEP(jit.getNamedType("struct.List"), value, 1));
                    elementPtr = builder.CreateGEP(array, listElements, { builder.getInt32(0), index });
                    type = typePtr->elementType;
                    value = builder.CreateLoad(typeToLLVMType(jit, type), elementPtr);
                    if (std::holds_alternative<std::shared_ptr<TypeBase>>(type)) {
                        typePtr = std::dynamic_pointer_cast<TypeList>(std::get<std::shared_ptr<TypeBase>>(type));
                    }
                    else {
                        typePtr = nullptr;
                    }
                }
                if (result.second != AbaciValue::None) {
                    Assert(type == result.second);
                    destroyValue(jit, value, type);
                    if (temps->isTemporary(result.first)) {
                        temps->removeTemporary(result.first);
                        builder.CreateStore(result.first, elementPtr);
                    }
                    else {
                        builder.CreateStore(cloneValue(jit, result.first, result.second), elementPtr);
                    }
                }
                else {
                    destroyValue(jit, value, type);
                    builder.CreateCall(module.getFunction("deleteElement"), { list, index });
                }
                return;
            }
        }
        UnexpectedError1(VariableNotList, listAssign.name.name);
    }
}

template<>
void StmtCodeGen::codeGen(const DataListAssignStmt& dataListAssign) const {
    const std::string& name = dataListAssign.name.name;
    Type type;
    ExprCodeGen expr(jit, locals, temps);
    expr(dataListAssign.value);
    auto result = expr.get();
    auto [ variables, index ] = locals ? locals->getIndex(name) : std::pair{ nullptr, LocalSymbols::noVariable };
    if (index != LocalSymbols::noVariable) {
        type = variables->getType(index);
        if (isConstant(type)) {
            UnexpectedError1(NoConstantAssign, name);
        }
        Value *value = loadMutableValue(jit, variables->getValue(index), type);
        Value *valuePtr = nullptr;
        for (const auto& member : dataListAssign.memberList) {
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
        Assert(type == AbaciValue::None || type == result.second);
        if (std::holds_alternative<std::shared_ptr<TypeBase>>(type)) {
            if (auto typePtr = std::dynamic_pointer_cast<TypeList>(std::get<std::shared_ptr<TypeBase>>(type))) {
                Value *index = nullptr, *list = nullptr, *elementPtr = nullptr;
                for (const auto& indexExpression : dataListAssign.indexes) {
                    if (typePtr == nullptr) {
                        UnexpectedError1(TooManyIndexes, dataListAssign.name.name);
                    }
                    ExprCodeGen expr(jit, locals, temps);
                    expr(indexExpression);
                    if (typeToScalar(expr.get().second) != AbaciValue::Integer) {
                        UnexpectedError0(IndexNotInt);
                    }
                    index = expr.get().first;
                    list = value;
                    Value *listSize = builder.CreateLoad(builder.getInt64Ty(), builder.CreateStructGEP(jit.getNamedType("struct.List"), value, 0));
                    ArrayType *array = ArrayType::get(builder.getInt64Ty(), 0);
                    Value *listElements = builder.CreateLoad(PointerType::get(array, 0), builder.CreateStructGEP(jit.getNamedType("struct.List"), value, 1));
                    elementPtr = builder.CreateGEP(array, listElements, { builder.getInt32(0), index });
                    type = typePtr->elementType;
                    value = builder.CreateLoad(typeToLLVMType(jit, type), elementPtr);
                    if (std::holds_alternative<std::shared_ptr<TypeBase>>(type)) {
                        typePtr = std::dynamic_pointer_cast<TypeList>(std::get<std::shared_ptr<TypeBase>>(type));
                    }
                    else {
                        typePtr = nullptr;
                    }
                }
                if (result.second != AbaciValue::None) {
                    Assert(type == result.second);
                    destroyValue(jit, value, type);
                    if (temps->isTemporary(result.first)) {
                        temps->removeTemporary(result.first);
                        builder.CreateStore(result.first, elementPtr);
                    }
                    else {
                        builder.CreateStore(cloneValue(jit, result.first, result.second), elementPtr);
                    }
                }
                else {
                    destroyValue(jit, value, type);
                    builder.CreateCall(module.getFunction("deleteElement"), { list, index });
                }
                return;
            }
        }
        UnexpectedError1(VariableNotList, dataListAssign.name.name);
    }
    else {
        auto globals = jit.getRuntimeContext().globals;
        auto globalIndex = globals->getIndex(name);
        if (globalIndex == GlobalSymbols::noVariable) {
            UnexpectedError1(VariableNotExist, name);
        }
        type = globals->getType(globalIndex);
        if (isConstant(type)) {
            UnexpectedError1(NoConstantAssign, name);
        }
        Value *value = loadGlobalValue(jit, globalIndex, globals->getType(globalIndex));
        Value *valuePtr = nullptr;
        for (const auto& member : dataListAssign.memberList) {
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
        Assert(type == AbaciValue::None || type == result.second);
        if (std::holds_alternative<std::shared_ptr<TypeBase>>(type)) {
            if (auto typePtr = std::dynamic_pointer_cast<TypeList>(std::get<std::shared_ptr<TypeBase>>(type))) {
                Value *index = nullptr, *list = nullptr, *elementPtr = nullptr;
                for (const auto& indexExpression : dataListAssign.indexes) {
                    if (typePtr == nullptr) {
                        UnexpectedError1(TooManyIndexes, name);
                    }
                    ExprCodeGen expr(jit, locals, temps);
                    expr(indexExpression);
                    if (typeToScalar(expr.get().second) != AbaciValue::Integer) {
                        UnexpectedError0(IndexNotInt);
                    }
                    index = expr.get().first;
                    list = value;
                    Value *listSize = builder.CreateLoad(builder.getInt64Ty(), builder.CreateStructGEP(jit.getNamedType("struct.List"), value, 0));
                    ArrayType *array = ArrayType::get(builder.getInt64Ty(), 0);
                    Value *listElements = builder.CreateLoad(PointerType::get(array, 0), builder.CreateStructGEP(jit.getNamedType("struct.List"), value, 1));
                    elementPtr = builder.CreateGEP(array, listElements, { builder.getInt32(0), index });
                    type = typePtr->elementType;
                    value = builder.CreateLoad(typeToLLVMType(jit, type), elementPtr);
                    if (std::holds_alternative<std::shared_ptr<TypeBase>>(type)) {
                        typePtr = std::dynamic_pointer_cast<TypeList>(std::get<std::shared_ptr<TypeBase>>(type));
                    }
                    else {
                        typePtr = nullptr;
                    }
                }
                if (result.second != AbaciValue::None) {
                    Assert(type == result.second);
                    destroyValue(jit, value, type);
                    if (temps->isTemporary(result.first)) {
                        temps->removeTemporary(result.first);
                        builder.CreateStore(result.first, elementPtr);
                    }
                    else {
                        builder.CreateStore(cloneValue(jit, result.first, result.second), elementPtr);
                    }
                }
                else {
                    destroyValue(jit, value, type);
                    builder.CreateCall(module.getFunction("deleteElement"), { list, index });
                }
                return;
            }
        }
        UnexpectedError1(VariableNotList, dataListAssign.name.name);
    }
}

void StmtCodeGen::operator()(const StmtNode& stmt) const {
    const auto *stmtData = stmt.get();
    if (dynamic_cast<const CommentStmt*>(stmtData)) {
        codeGen(dynamic_cast<const CommentStmt&>(*stmtData));
    }
    else if (dynamic_cast<const PrintStmt*>(stmtData)) {
        codeGen(dynamic_cast<const PrintStmt&>(*stmtData));
    }
    else if (dynamic_cast<const InitStmt*>(stmtData)) {
        codeGen(dynamic_cast<const InitStmt&>(*stmtData));
    }
    else if (dynamic_cast<const AssignStmt*>(stmtData)) {
        codeGen(dynamic_cast<const AssignStmt&>(*stmtData));
    }
    else if (dynamic_cast<const IfStmt*>(stmtData)) {
        codeGen(dynamic_cast<const IfStmt&>(*stmtData));
    }
    else if (dynamic_cast<const WhileStmt*>(stmtData)) {
        codeGen(dynamic_cast<const WhileStmt&>(*stmtData));
    }
    else if (dynamic_cast<const RepeatStmt*>(stmtData)) {
        codeGen(dynamic_cast<const RepeatStmt&>(*stmtData));
    }
    else if (dynamic_cast<const CaseStmt*>(stmtData)) {
        codeGen(dynamic_cast<const CaseStmt&>(*stmtData));
    }
    else if (dynamic_cast<const Function*>(stmtData)) {
        codeGen(dynamic_cast<const Function&>(*stmtData));
    }
    else if (dynamic_cast<const FunctionCall*>(stmtData)) {
        codeGen(dynamic_cast<const FunctionCall&>(*stmtData));
    }
    else if (dynamic_cast<const ReturnStmt*>(stmtData)) {
        codeGen(dynamic_cast<const ReturnStmt&>(*stmtData));
    }
    else if (dynamic_cast<const ExprFunction*>(stmtData)) {
        codeGen(dynamic_cast<const ExprFunction&>(*stmtData));
    }
    else if (dynamic_cast<const Class*>(stmtData)) {
        codeGen(dynamic_cast<const Class&>(*stmtData));
    }
    else if (dynamic_cast<const DataAssignStmt*>(stmtData)) {
        codeGen(dynamic_cast<const DataAssignStmt&>(*stmtData));
    }
    else if (dynamic_cast<const MethodCall*>(stmtData)) {
        codeGen(dynamic_cast<const MethodCall&>(*stmtData));
    }
    else if (dynamic_cast<const ExpressionStmt*>(stmtData)) {
        codeGen(dynamic_cast<const ExpressionStmt&>(*stmtData));
    }
    else if (dynamic_cast<const ListAssignStmt*>(stmtData)) {
        codeGen(dynamic_cast<const ListAssignStmt&>(*stmtData));
    }
    else if (dynamic_cast<const DataListAssignStmt*>(stmtData)) {
        codeGen(dynamic_cast<const DataListAssignStmt&>(*stmtData));
    }
    else {
        UnexpectedError0(BadStmtNode);
    }
}

} // namespace abaci::codegen
