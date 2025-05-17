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
using abaci::ast::ExprPair;
using abaci::ast::FunctionValueCall;
using abaci::ast::StmtNode;
using abaci::ast::StmtList;
using abaci::ast::CommentStmt;
using abaci::ast::PrintStmt;
using abaci::ast::PrintList;
using abaci::ast::InitStmt;
using abaci::ast::AssignStmt;
using abaci::ast::CallList;
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
using abaci::ast::MethodCall;
using abaci::ast::ExpressionStmt;
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
        if (!std::holds_alternative<ReturnStmt>(stmts.statements.back().data)) {
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
    PrintList printData{};
    if (!std::holds_alternative<std::monostate>(print.expression.data)) {
        printData.push_back(print.expression);
    }
#ifdef ABACI_USE_OLDER_BOOST
    if (print.separator != Operator::None) {
        printData.push_back(print.separator);
    }
#endif
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
    std::string name = assign.name.name;
    Type type = AbaciValue::Unset;
    TypedValue value = { nullptr, type }, parent;
    Value *valuePtr = nullptr, *index = nullptr, *sliceBegin = nullptr, *sliceEnd = nullptr;
    if (locals) {
        auto [ variables, index ] = locals->getIndex(name);
        if (index != LocalSymbols::noVariable) {
            type = variables->getType(index);
            if (!assign.calls.empty()) {
                value = { loadMutableValue(jit, variables->getValue(index), type), removeConstFromType(type) };
            }
        }
    }
    if (type == AbaciValue::Unset) {
        auto globals = jit.getRuntimeContext().globals;
        auto globalIndex = globals->getIndex(name);
        if (globalIndex != GlobalSymbols::noVariable) {
            type = globals->getType(globalIndex);
            if (!assign.calls.empty()) {
                value = { loadGlobalValue(jit, globalIndex, type), removeConstFromType(type) };
            }
        }
    }
    if (type == AbaciValue::Unset) {
        UnexpectedError1(VariableNotExist, name);
    }
    else if (isConstant(type)) {
        UnexpectedError1(NoConstantAssign, assign.name.name);
    }
    for (const auto& callElement : assign.calls) {
        switch (callElement.call.index()) {
            case CallList::TypeVariable: {
                const auto& callVariable = std::get<Variable>(callElement.call);
                if (typeToScalar(removeConstFromType(type)) != AbaciValue::Instance) {
                    UnexpectedError1(BadObject, name);
                }
                auto instanceType = std::dynamic_pointer_cast<TypeInstance>(std::get<std::shared_ptr<TypeBase>>(type));
                Assert(instanceType != nullptr);
                auto index = jit.getCache()->getMemberIndex(jit.getCache()->getClass(instanceType->className), callVariable.name);
                type = instanceType->variableTypes.at(index);
                ArrayType *array = ArrayType::get(builder.getInt64Ty(), instanceType->variableTypes.size());
                Value *arrayPtr = builder.CreateLoad(PointerType::get(array, 0), builder.CreateStructGEP(jit.getNamedType("struct.Instance"), value.first, 2));
                valuePtr = builder.CreateGEP(array, arrayPtr, { builder.getInt32(0), builder.getInt32(index) });
                parent = value;
                value = { builder.CreateLoad(typeToLLVMType(jit, type), valuePtr), type };
                name += '.' + callVariable.name;
                break;
            }
            case CallList::TypeIndexes: {
                const auto& callIndexes = std::get<ExprList>(callElement.call);
                std::shared_ptr<TypeList> typePtr;
                if (typeToScalar(removeConstFromType(type)) == AbaciValue::List) {
                    typePtr = std::dynamic_pointer_cast<TypeList>(std::get<std::shared_ptr<TypeBase>>(type));
                    Assert(typePtr != nullptr);
                }
                else if (typeToScalar(removeConstFromType(type)) != AbaciValue::String) {
                    UnexpectedError1(VariableNotList, name);
                }
                for (const auto& indexExpression : callIndexes) {
                    if (typePtr == nullptr && typeToScalar(removeConstFromType(type)) != AbaciValue::String) {
                        UnexpectedError1(TooManyIndexes, name);
                    }
                    name += "[]";
                    ExprCodeGen expr(jit, locals, temps);
                    expr(indexExpression);
                    if (typeToScalar(expr.get().second) != AbaciValue::Integer) {
                        UnexpectedError0(IndexNotInt);
                    }
                    if (typeToScalar(removeConstFromType(type)) == AbaciValue::List) {
                        index = expr.get().first;
                        Value *listSize = builder.CreateLoad(builder.getInt64Ty(), builder.CreateStructGEP(jit.getNamedType("struct.List"), value.first, 0));
                        index = builder.CreateCall(module.getFunction("validIndex"), { index, listSize, ConstantInt::get(builder.getInt1Ty(), 0) });
                        ArrayType *array = ArrayType::get(builder.getInt64Ty(), 0);
                        Value *listElements = builder.CreateLoad(PointerType::get(array, 0), builder.CreateStructGEP(jit.getNamedType("struct.List"), value.first, 1));
                        valuePtr = builder.CreateGEP(array, listElements, { builder.getInt32(0), index });
                        type = typePtr->elementType;
                        parent = value;
                        value = { builder.CreateLoad(typeToLLVMType(jit, type), valuePtr), type };
                        if (std::holds_alternative<std::shared_ptr<TypeBase>>(type)) {
                            typePtr = std::dynamic_pointer_cast<TypeList>(std::get<std::shared_ptr<TypeBase>>(type));
                        }
                        else {
                            typePtr = nullptr;
                        }
                    }
                    else if (typeToScalar(removeConstFromType(type)) == AbaciValue::String) {
                        if (&indexExpression != &callIndexes.back()) {
                            UnexpectedError1(TooManyIndexes, name);
                        }
                        index = expr.get().first;
                        parent = value;
                        value = { builder.CreateCall(module.getFunction("indexString"), { value.first, index }), AbaciValue::String };
                        temps->addTemporary(value);
                    }
                }
                break;
            }
            case CallList::TypeSlice: {
                const auto& callSlice = std::get<ExprPair>(callElement.call);
                Assert(callSlice.size() == 2);
                std::shared_ptr<TypeList> typePtr;
                if (typeToScalar(removeConstFromType(type)) == AbaciValue::List) {
                    typePtr = std::dynamic_pointer_cast<TypeList>(std::get<std::shared_ptr<TypeBase>>(type));
                    Assert(typePtr != nullptr);
                    type = typePtr->elementType;
                }
                else if (typeToScalar(removeConstFromType(type)) != AbaciValue::String) {
                    UnexpectedError1(VariableNotList, name);
                }
                name += "[:]";
                if (&callElement != &assign.calls.back()) {
                    UnexpectedError1(SliceNotAtEnd, name);
                }
                ExprCodeGen exprBegin(jit, locals, temps);
                exprBegin(callSlice.at(0));
                if (typeToScalar(exprBegin.get().second) != AbaciValue::Integer) {
                    UnexpectedError1(IndexNotInt, name);
                }
                ExprCodeGen exprEnd(jit, locals, temps);
                exprEnd(callSlice.at(1));
                if (typeToScalar(exprEnd.get().second) != AbaciValue::Integer) {
                    UnexpectedError1(IndexNotInt, name);
                }
                if (typeToScalar(removeConstFromType(type)) == AbaciValue::List) {
                    sliceBegin = exprBegin.get().first;
                    sliceEnd = exprEnd.get().first;
                    parent = value;
                }
                else if (typeToScalar(removeConstFromType(type)) == AbaciValue::String) {
                    sliceBegin = exprBegin.get().first;
                    sliceEnd = exprEnd.get().first;
                    parent = value;
                }
                break;
            }
            default:
                UnexpectedError0(BadCall);
        }
    }
    ExprCodeGen expr(jit, locals, temps);
    expr(assign.value);
    auto result = expr.get();
    if (!assign.calls.empty() && (assign.calls.back().call.index() == CallList::TypeIndexes || assign.calls.back().call.index() == CallList::TypeSlice)) {
        if (parent.second != result.second && result.second != AbaciValue::None) {
            UnexpectedError2(AssignMismatch, typeToString(parent.second), typeToString(result.second));
        }
    }
    else if (type != result.second) {
        UnexpectedError1(VariableType, name);
    }
    if (assign.calls.empty()) {
        if (locals) {
            auto [ variables, index ] = locals->getIndex(name);
            if (index != LocalSymbols::noVariable) {
                Assert(variables->getType(index) == result.second);
                destroyValue(jit, loadMutableValue(jit, variables->getValue(index), result.second), result.second);
                if (temps->isTemporary(result.first)) {
                    temps->removeTemporary(result.first);
                    storeMutableValue(jit, variables->getValue(index), result.first);
                }
                else {
                    storeMutableValue(jit, variables->getValue(index), cloneValue(jit, result.first, result.second));
                }
                return;
            }
        }
        auto globals = jit.getRuntimeContext().globals;
        auto globalIndex = globals->getIndex(assign.name.name);
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
    else if (assign.calls.back().call.index() == CallList::TypeVariable) {
        Assert(valuePtr != nullptr);
        Assert(type == result.second);
        destroyValue(jit, value.first, type);
        if (temps->isTemporary(result.first)) {
            temps->removeTemporary(result.first);
            builder.CreateStore(result.first, valuePtr);
        }
        else {
            builder.CreateStore(cloneValue(jit, result.first, type), valuePtr);
        }
    }
    else if (assign.calls.back().call.index() == CallList::TypeIndexes) {
        if (typeToScalar(removeConstFromType(parent.second)) == AbaciValue::List) {
            if (result.second != AbaciValue::None) {
                Assert(valuePtr != nullptr);
                Assert(type == result.second);
                destroyValue(jit, value.first, type);
                if (temps->isTemporary(result.first)) {
                    temps->removeTemporary(result.first);
                    builder.CreateStore(result.first, valuePtr);
                }
                else {
                    builder.CreateStore(cloneValue(jit, result.first, result.second), valuePtr);
                }
            }
            else {
                Assert(parent.first != nullptr);
                destroyValue(jit, value.first, type);
                builder.CreateCall(module.getFunction("deleteElement"), { parent.first, index });
            }
        }
        else if (typeToScalar(removeConstFromType(parent.second)) == AbaciValue::String) {
            if (result.second != AbaciValue::None) {
                Assert(typeToScalar(removeConstFromType(result.second)) == AbaciValue::String);
                Value *endIndex = builder.CreateAdd(index, builder.getInt64(1));
                builder.CreateCall(module.getFunction("spliceString"), { parent.first, index, endIndex, result.first });
            }
            else {
                Value *endIndex = builder.CreateAdd(index, builder.getInt64(1));
                builder.CreateCall(module.getFunction("spliceString"), { parent.first, index, endIndex, ConstantPointerNull::get(PointerType::get(jit.getNamedType("struct.String"), 0)) });
            }
        }
        else {
            UnexpectedError0(BadType);
        }
    }
    else if (assign.calls.back().call.index() == CallList::TypeSlice) {
        if (typeToScalar(removeConstFromType(parent.second)) == AbaciValue::List) {
            Value *deletedElements;
            if (result.second != AbaciValue::None) {
                Assert(parent.second == result.second);
                Value *clonedValue = cloneValue(jit, result.first, result.second);
                deletedElements = builder.CreateCall(module.getFunction("spliceList"), { parent.first, sliceBegin, sliceEnd, clonedValue,
                    ConstantInt::get(builder.getInt1Ty(), typeToScalar(removeConstFromType(type)) > AbaciValue::Floating) });
                builder.CreateCall(module.getFunction("destroyList"), { clonedValue });
            }
            else {
                deletedElements = builder.CreateCall(module.getFunction("spliceList"), { parent.first, sliceBegin, sliceEnd, ConstantPointerNull::get(PointerType::get(jit.getNamedType("struct.List"), 0)),
                    ConstantInt::get(builder.getInt1Ty(), typeToScalar(removeConstFromType(type)) > AbaciValue::Floating) });
            }
            Value *needDestroy = builder.CreateICmpNE(deletedElements, ConstantPointerNull::get(PointerType::get(jit.getNamedType("struct.List"), 0)));
            BasicBlock *destroyBlock = BasicBlock::Create(jit.getContext(), "", jit.getFunction());
            BasicBlock *noDestroyBlock = BasicBlock::Create(jit.getContext(), "", jit.getFunction());
            BasicBlock *mergeBlock = BasicBlock::Create(jit.getContext(), "", jit.getFunction());
            builder.CreateCondBr(needDestroy, destroyBlock, noDestroyBlock);
            builder.SetInsertPoint(destroyBlock);
            destroyValue(jit, deletedElements, parent.second);
            builder.CreateBr(mergeBlock);
            builder.SetInsertPoint(noDestroyBlock);
            builder.CreateBr(mergeBlock);
            builder.SetInsertPoint(mergeBlock);
        }
        else if (typeToScalar(removeConstFromType(parent.second)) == AbaciValue::String) {
            if (result.second != AbaciValue::None) {
                Assert(parent.second == result.second);
                builder.CreateCall(module.getFunction("spliceString"), { parent.first, sliceBegin, sliceEnd, result.first });
            }
            else {
                builder.CreateCall(module.getFunction("spliceString"), { parent.first, sliceBegin, sliceEnd, ConstantPointerNull::get(PointerType::get(jit.getNamedType("struct.String"), 0)) });
            }
        }
    }
    else {
        UnexpectedError0(BadCall);
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
        matches.emplace_back(&match.expression, &match.block);
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
                    UnexpectedError1(BadObject, name);
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
                UnexpectedError1(BadObject, name);
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
        UnexpectedError1(BadObject, name);
    }
}

template<>
void StmtCodeGen::codeGen([[maybe_unused]] const ExpressionStmt& expressionStmt) const {
}

void StmtCodeGen::operator()(const StmtNode& stmt) const {
    std::visit([this](const auto& nodeType){
        this->codeGen(nodeType);
    }, stmt.data);
}

} // namespace abaci::codegen
