#include "ExprCodeGen.hpp"
#include "utility/Constant.hpp"
#include "utility/Symbol.hpp"
#include "utility/Report.hpp"
#include "localize/Messages.hpp"
#include "utility/Utility.hpp"
#include "lib/Abaci.hpp"
#include <llvm/IR/Constants.h>

namespace abaci::codegen {

using namespace llvm;

using abaci::ast::ExprNode;
using abaci::ast::ExprList;
using abaci::ast::Variable;
using abaci::ast::FunctionValueCall;
using abaci::ast::UserInput;
using abaci::ast::TypeConv;
using abaci::ast::List;
using abaci::ast::CallList;
using abaci::ast::MultiCall;
using abaci::lib::makeString;
using abaci::utility::AbaciValue;
using abaci::utility::Operator;
using abaci::utility::TypeBase;
using abaci::utility::LocalSymbols;
using abaci::utility::GlobalSymbols;
using abaci::utility::LocalSymbols;
using abaci::utility::mangled;
using abaci::utility::removeConstFromType;
using abaci::utility::typeToScalar;
using abaci::utility::TypeList;
using abaci::utility::TypeInstance;
using abaci::utility::TypeConversions;
using abaci::utility::cloneValue;
using abaci::utility::loadMutableValue;
using abaci::utility::loadGlobalValue;
using abaci::utility::getContextValue;
using abaci::engine::Cache;

template<>
void ExprCodeGen::codeGen([[maybe_unused]] const std::monostate& none) const {
}

template<>
void ExprCodeGen::codeGen(const std::size_t& index) const {
    const auto& constantValue = constants.get(index);
    switch (typeToScalar(constantValue.second)) {
        case AbaciValue::None:
            push({ ConstantPointerNull::get(PointerType::get(jit.getNamedType("struct.Instance"), 0)), AbaciValue::None });
            break;
        case AbaciValue::Boolean:
            push({ ConstantInt::get(builder.getInt1Ty(), constantValue.first.integer), AbaciValue::Boolean });
            break;
        case AbaciValue::Integer:
            push({ ConstantInt::get(builder.getInt64Ty(), constantValue.first.integer), AbaciValue::Integer });
            break;
        case AbaciValue::Floating:
            push({ ConstantFP::get(builder.getDoubleTy(), constantValue.first.floating), AbaciValue::Floating });
            break;
        case AbaciValue::Complex:
            push({ ConstantExpr::getIntToPtr(ConstantInt::get(builder.getInt64Ty(), constantValue.first.integer), PointerType::get(jit.getNamedType("struct.Complex"), 0)), AbaciValue::Complex });
            break;
        case AbaciValue::String:
            push({ ConstantExpr::getIntToPtr(ConstantInt::get(builder.getInt64Ty(), constantValue.first.integer), PointerType::get(jit.getNamedType("struct.String"), 0)), AbaciValue::String });
            break;
        default:
            UnexpectedError0(BadType);
    }
}

template<>
void ExprCodeGen::codeGen([[maybe_unused]] const Operator& op) const {
}

template<>
void ExprCodeGen::codeGen(const std::pair<ExprNode::Association,ExprList>& listNode) const {
    switch (listNode.first) {
        case ExprNode::Left: {
            const auto& expr = listNode.second;
            (*this)(expr.front());
            auto result = pop();
            for (auto iter = ++expr.begin(); iter != expr.end();) {
                auto op = std::get<Operator>(iter++->data);
                (*this)(*iter++);
                auto operand = pop();
                auto type = (operand.second != result.second) ? promote(result, operand) : result.second;
                switch (typeToScalar(type)) {
                    case AbaciValue::Boolean:
                        switch (op) {
                            case Operator::BitAnd:
                                result.first = builder.CreateAnd(result.first, operand.first);
                                break;
                            case Operator::BitXor:
                                result.first = builder.CreateXor(result.first, operand.first);
                                break;
                            case Operator::BitOr:
                                result.first = builder.CreateOr(result.first, operand.first);
                                break;
                            default:
                                UnexpectedError0(BadOperator);
                        }
                        break;
                    case AbaciValue::Integer:
                        switch (op) {
                            case Operator::Plus:
                                result.first = builder.CreateAdd(result.first, operand.first);
                                break;
                            case Operator::Minus:
                                result.first = builder.CreateSub(result.first, operand.first);
                                break;
                            case Operator::Times:
                                result.first = builder.CreateMul(result.first, operand.first);
                                break;
                            case Operator::Modulo:
                                result.first = builder.CreateSRem(result.first, operand.first);
                                break;
                            case Operator::FloorDivide:
                                result.first = builder.CreateSDiv(result.first, operand.first);
                                break;
                            case Operator::Divide: {
                                auto a = builder.CreateSIToFP(result.first, builder.getDoubleTy());
                                auto b = builder.CreateSIToFP(operand.first, builder.getDoubleTy());
                                result.first = builder.CreateFDiv(a, b);
                                result.second = AbaciValue::Floating;
                                break;
                            }
                            case Operator::BitAnd:
                                result.first = builder.CreateAnd(result.first, operand.first);
                                break;
                            case Operator::BitXor:
                                result.first = builder.CreateXor(result.first, operand.first);
                                break;
                            case Operator::BitOr:
                                result.first = builder.CreateOr(result.first, operand.first);
                                break;
                            default:
                                UnexpectedError0(BadOperator);
                        }
                        break;
                    case AbaciValue::Floating:
                        switch (op) {
                            case Operator::Plus:
                                result.first = builder.CreateFAdd(result.first, operand.first);
                                break;
                            case Operator::Minus:
                                result.first = builder.CreateFSub(result.first, operand.first);
                                break;
                            case Operator::Times:
                                result.first = builder.CreateFMul(result.first, operand.first);
                                break;
                            case Operator::Divide:
                                result.first = builder.CreateFDiv(result.first, operand.first);
                                break;
                            default:
                                UnexpectedError0(BadOperator);
                        }
                        break;
                    case AbaciValue::Complex:
                        switch (op) {
                            case Operator::Plus:
                            case Operator::Minus:
                            case Operator::Times:
                            case Operator::Divide:
                                result.first = builder.CreateCall(module.getFunction("opComplex"),
                                    { ConstantInt::get(builder.getInt32Ty(), static_cast<int>(op)),
                                        result.first, operand.first });
                                temps->addTemporary({ result.first, type });
                                break;
                            default:
                                UnexpectedError0(BadOperator);
                        }
                        break;
                    case AbaciValue::String:
                        switch (op) {
                            case Operator::Plus:
                                result.first = builder.CreateCall(module.getFunction("concatString"),
                                    { result.first, operand.first });
                                temps->addTemporary({ result.first, type });
                                break;
                            default:
                                UnexpectedError0(BadOperator);
                        }
                        break;
                    case AbaciValue::List:
                        switch (op) {
                            case Operator::Plus: {
                                Assert(operand.second == result.second);
                                auto typeBase = std::get<std::shared_ptr<TypeBase>>(result.second);
                                Type elementType = std::dynamic_pointer_cast<TypeList>(typeBase)->elementType;
                                Value *listSize1 = builder.CreateLoad(builder.getInt64Ty(), builder.CreateStructGEP(jit.getNamedType("struct.List"), result.first, 0));
                                Value *listSize2 = builder.CreateLoad(builder.getInt64Ty(), builder.CreateStructGEP(jit.getNamedType("struct.List"), operand.first, 0));
                                Value *listSize = builder.CreateAdd(listSize1, listSize2);
                                Value *list = builder.CreateCall(module.getFunction("makeList"), { listSize });
                                ArrayType *array = ArrayType::get(builder.getInt64Ty(), 0);
                                Value *listRead1 = builder.CreateLoad(PointerType::get(array, 0), builder.CreateStructGEP(jit.getNamedType("struct.List"), result.first, 1));
                                Value *listRead2 = builder.CreateLoad(PointerType::get(array, 0), builder.CreateStructGEP(jit.getNamedType("struct.List"), operand.first, 1));
                                Value *listWrite = builder.CreateLoad(PointerType::get(array, 0), builder.CreateStructGEP(jit.getNamedType("struct.List"), list, 1));
                                AllocaInst *readIndex = builder.CreateAlloca(builder.getInt64Ty(), nullptr);
                                AllocaInst *writeIndex = builder.CreateAlloca(builder.getInt64Ty(), nullptr);
                                builder.CreateStore(builder.getInt64(0), readIndex);
                                builder.CreateStore(builder.getInt64(0), writeIndex);
                                BasicBlock *preBlock1 = BasicBlock::Create(jit.getContext(), "", jit.getFunction());
                                BasicBlock *loopBlock1 = BasicBlock::Create(jit.getContext(), "", jit.getFunction());
                                BasicBlock *postBlock1 = BasicBlock::Create(jit.getContext(), "", jit.getFunction());
                                BasicBlock *preBlock2 = BasicBlock::Create(jit.getContext(), "", jit.getFunction());
                                BasicBlock *loopBlock2 = BasicBlock::Create(jit.getContext(), "", jit.getFunction());
                                BasicBlock *postBlock2 = BasicBlock::Create(jit.getContext(), "", jit.getFunction());
                                builder.CreateBr(preBlock1);
                                builder.SetInsertPoint(preBlock1);
                                Value *condition = builder.CreateICmpNE(builder.CreateLoad(builder.getInt64Ty(), readIndex), listSize1);
                                builder.CreateCondBr(condition, loopBlock1, postBlock1);
                                builder.SetInsertPoint(loopBlock1);
                                Value *readIndexValue = builder.CreateLoad(builder.getInt64Ty(), readIndex);
                                Value *writeIndexValue = builder.CreateLoad(builder.getInt64Ty(), writeIndex);
                                Value *elementRead = builder.CreateGEP(array, listRead1, { builder.getInt32(0), readIndexValue });
                                Value *element = cloneValue(jit, builder.CreateLoad(typeToLLVMType(jit, elementType), elementRead), elementType);
                                Value *elementWrite = builder.CreateGEP(array, listWrite, { builder.getInt32(0), writeIndexValue });
                                builder.CreateStore(builder.CreateBitCast(element, builder.getInt64Ty()), elementWrite);
                                builder.CreateStore(builder.CreateAdd(readIndexValue, builder.getInt64(1)), readIndex);
                                builder.CreateStore(builder.CreateAdd(writeIndexValue, builder.getInt64(1)), writeIndex);
                                builder.CreateBr(preBlock1);
                                builder.SetInsertPoint(postBlock1);
                                builder.CreateStore(builder.getInt64(0), readIndex);
                                builder.CreateBr(preBlock2);
                                builder.SetInsertPoint(preBlock2);
                                condition = builder.CreateICmpNE(builder.CreateLoad(builder.getInt64Ty(), readIndex), listSize2);
                                builder.CreateCondBr(condition, loopBlock2, postBlock2);
                                builder.SetInsertPoint(loopBlock2);
                                readIndexValue = builder.CreateLoad(builder.getInt64Ty(), readIndex);
                                writeIndexValue = builder.CreateLoad(builder.getInt64Ty(), writeIndex);
                                elementRead = builder.CreateGEP(array, listRead2, { builder.getInt32(0), readIndexValue });
                                element = cloneValue(jit, builder.CreateLoad(typeToLLVMType(jit, elementType), elementRead), elementType);
                                elementWrite = builder.CreateGEP(array, listWrite, { builder.getInt32(0), writeIndexValue });
                                builder.CreateStore(builder.CreateBitCast(element, builder.getInt64Ty()), elementWrite);
                                builder.CreateStore(builder.CreateAdd(readIndexValue, builder.getInt64(1)), readIndex);
                                builder.CreateStore(builder.CreateAdd(writeIndexValue, builder.getInt64(1)), writeIndex);
                                builder.CreateBr(preBlock2);
                                builder.SetInsertPoint(postBlock2);
                                temps->addTemporary({ list, result.second });
                                result.first = list;
                                break;
                            }
                            default:
                                UnexpectedError0(BadOperator);
                        }
                        break;
                    default:
                        UnexpectedError0(BadType);
                        break;
                }
            }
            push(result);
            break;
        }
        case ExprNode::Right: {
            const auto& expr = listNode.second;
            (*this)(expr.back());
            auto result = pop();
            for (auto iter = ++expr.rbegin(); iter != expr.rend();) {
                auto op = std::get<Operator>(iter++->data);
                (*this)(*iter++);
                auto operand = pop();
                auto type = (operand.second != result.second) ? promote(result, operand) : result.second;
                switch (typeToScalar(type)) {
                    case AbaciValue::Integer:
                        switch (op) {
                            case Operator::Exponent: {
                                auto a = builder.CreateSIToFP(result.first, builder.getDoubleTy());
                                auto b = builder.CreateSIToFP(operand.first, builder.getDoubleTy());
                                result.first = builder.CreateCall(module.getFunction("pow"), { b, a });
                                result.second = AbaciValue::Floating;
                                break;
                            }
                            default:
                                UnexpectedError0(BadOperator);
                        }
                        break;
                    case AbaciValue::Floating:
                        switch (op) {
                            case Operator::Exponent:
                                result.first = builder.CreateCall(module.getFunction("pow"), { operand.first, result.first });
                                break;
                            default:
                                UnexpectedError0(BadOperator);
                        }
                        break;
                    case AbaciValue::Complex:
                        switch (op) {
                            case Operator::Exponent:
                                result.first = builder.CreateCall(module.getFunction("opComplex"),
                                    { ConstantInt::get(builder.getInt32Ty(), static_cast<int>(op)),
                                        operand.first, result.first });
                                temps->addTemporary({ result.first, type });
                                break;
                            default:
                                UnexpectedError0(BadOperator);
                        }
                        break;
                default:
                    UnexpectedError0(BadType);
                    break;
                }
            }
            push(result);
            break;
        }
        case ExprNode::Unary: {
            const auto& expr = listNode.second;
            (*this)(expr.back());
            auto result = pop();
            auto type = result.second;
            for (auto iter = ++expr.rbegin(); iter != expr.rend();) {
                auto op = std::get<Operator>(iter++->data);
                switch (typeToScalar(type)) {
                    case AbaciValue::None:
                        switch (op) {
                            case Operator::Question: {
                                auto str = typeToString(type);
                                auto *obj = makeString(reinterpret_cast<char8_t*>(str.data()), str.size());
                                result.first = ConstantExpr::getIntToPtr(ConstantInt::get(builder.getInt64Ty(), reinterpret_cast<intptr_t>(obj)), PointerType::get(jit.getNamedType("struct.String"), 0));
                                result.second = AbaciValue::String;
                                temps->addTemporary({ result.first, result.second });
                                break;
                            }
                            default:
                                UnexpectedError0(BadOperator);
                        }
                        break;
                    case AbaciValue::Boolean:
                        switch (op) {
                            case Operator::Not:
                            case Operator::Compl:
                                result.first = builder.CreateNot(result.first);
                                break;
                            case Operator::Question: {
                                auto str = typeToString(type);
                                auto *obj = makeString(reinterpret_cast<char8_t*>(str.data()), str.size());
                                result.first = ConstantExpr::getIntToPtr(ConstantInt::get(builder.getInt64Ty(), reinterpret_cast<intptr_t>(obj)), PointerType::get(jit.getNamedType("struct.String"), 0));
                                result.second = AbaciValue::String;
                                temps->addTemporary({ result.first, result.second });
                                break;
                            }
                            default:
                                UnexpectedError0(BadOperator);
                        }
                        break;
                    case AbaciValue::Integer:
                        switch (op) {
                            case Operator::Minus:
                                result.first = builder.CreateNeg(result.first);
                                break;
                            case Operator::Not:
                                result.first = builder.CreateNot(toBoolean(result));
                                result.second = AbaciValue::Boolean;
                                break;
                            case Operator::Compl:
                                result.first = builder.CreateNot(result.first);
                                break;
                            case Operator::Question: {
                                auto str = typeToString(type);
                                auto *obj = makeString(reinterpret_cast<char8_t*>(str.data()), str.size());
                                result.first = ConstantExpr::getIntToPtr(ConstantInt::get(builder.getInt64Ty(), reinterpret_cast<intptr_t>(obj)), PointerType::get(jit.getNamedType("struct.String"), 0));
                                result.second = AbaciValue::String;
                                temps->addTemporary({ result.first, result.second });
                                break;
                            }
                            default:
                                UnexpectedError0(BadOperator);
                        }
                        break;
                    case AbaciValue::Floating:
                        switch (op) {
                            case Operator::Minus:
                                result.first = builder.CreateFNeg(result.first);
                                break;
                            case Operator::Not:
                                result.first = builder.CreateNot(toBoolean(result));
                                result.second = AbaciValue::Boolean;
                                break;
                            case Operator::Question: {
                                auto str = typeToString(type);
                                auto *obj = makeString(reinterpret_cast<char8_t*>(str.data()), str.size());
                                result.first = ConstantExpr::getIntToPtr(ConstantInt::get(builder.getInt64Ty(), reinterpret_cast<intptr_t>(obj)), PointerType::get(jit.getNamedType("struct.String"), 0));
                                result.second = AbaciValue::String;
                                temps->addTemporary({ result.first, result.second });
                                break;
                            }
                            default:
                                UnexpectedError0(BadOperator);
                        }
                        break;
                    case AbaciValue::Complex:
                        switch (op) {
                            case Operator::Minus: {
                                result.first = builder.CreateCall(module.getFunction("opComplex"),
                                    { ConstantInt::get(builder.getInt32Ty(), static_cast<int>(op)),
                                        result.first, ConstantPointerNull::get(PointerType::get(jit.getNamedType("struct.Complex"), 0)) });
                                temps->addTemporary({ result.first, type });
                                break;
                            }
                            case Operator::Question: {
                                auto str = typeToString(type);
                                auto *obj = makeString(reinterpret_cast<char8_t*>(str.data()), str.size());
                                result.first = ConstantExpr::getIntToPtr(ConstantInt::get(builder.getInt64Ty(), reinterpret_cast<intptr_t>(obj)), PointerType::get(jit.getNamedType("struct.String"), 0));
                                result.second = AbaciValue::String;
                                temps->addTemporary({ result.first, result.second });
                                break;
                            }
                            default:
                                UnexpectedError0(BadOperator);
                        }
                        break;
                    case AbaciValue::String:
                        switch (op) {
                            case Operator::Bang:
                                result.first = builder.CreateLoad(builder.getInt64Ty(), builder.CreateStructGEP(jit.getNamedType("struct.String"), result.first, 1));
                                result.second = AbaciValue::Integer;
                                break;
                            case Operator::Question: {
                                auto str = typeToString(type);
                                auto *obj = makeString(reinterpret_cast<char8_t*>(str.data()), str.size());
                                result.first = ConstantExpr::getIntToPtr(ConstantInt::get(builder.getInt64Ty(), reinterpret_cast<intptr_t>(obj)), PointerType::get(jit.getNamedType("struct.String"), 0));
                                result.second = AbaciValue::String;
                                temps->addTemporary({ result.first, result.second });
                                break;
                            }
                            default:
                                UnexpectedError0(BadOperator);
                        }
                        break;
                    case AbaciValue::Instance:
                        switch (op) {
                            case Operator::Question: {
                                auto str = typeToString(type);
                                auto *obj = makeString(reinterpret_cast<char8_t*>(str.data()), str.size());
                                result.first = ConstantExpr::getIntToPtr(ConstantInt::get(builder.getInt64Ty(), reinterpret_cast<intptr_t>(obj)), PointerType::get(jit.getNamedType("struct.String"), 0));
                                result.second = AbaciValue::String;
                                temps->addTemporary({ result.first, result.second });
                                break;
                            }
                            default:
                                UnexpectedError0(BadOperator);
                        }
                        break;
                    case AbaciValue::List:
                        switch (op) {
                            case Operator::Bang:
                                result.first = builder.CreateLoad(builder.getInt64Ty(), builder.CreateStructGEP(jit.getNamedType("struct.List"), result.first, 0));
                                result.second = AbaciValue::Integer;
                                break;
                            case Operator::Question: {
                                auto str = typeToString(type);
                                auto *obj = makeString(reinterpret_cast<char8_t*>(str.data()), str.size());
                                result.first = ConstantExpr::getIntToPtr(ConstantInt::get(builder.getInt64Ty(), reinterpret_cast<intptr_t>(obj)), PointerType::get(jit.getNamedType("struct.String"), 0));
                                result.second = AbaciValue::String;
                                temps->addTemporary({ result.first, result.second });
                                break;
                            }
                            default:
                                UnexpectedError0(BadOperator);
                        }
                        break;
                default:
                    UnexpectedError0(BadType);
                    break;
                }
            }
            push(result);
            break;
        }
        case ExprNode::Boolean: {
            const auto& expr = listNode.second;
            (*this)(expr.front());
            auto result = pop();
            if (expr.size() == 1) {
                push(result);
            }
            else {
                Value *boolResult = ConstantInt::get(builder.getInt1Ty(), true);
                for (auto iter = ++expr.begin(); iter != expr.end();) {
                    auto op = std::get<Operator>(iter++->data);
                    (*this)(*iter++);
                    auto operand = pop();
                    auto type = (operand.second != result.second) ? promote(result, operand) : result.second;
                    switch (typeToScalar(type)) {
                        case AbaciValue::Boolean:
                            switch (op) {
                                case Operator::Equal:
                                    boolResult = builder.CreateAnd(boolResult, builder.CreateICmpEQ(result.first, operand.first));
                                    break;
                                case Operator::NotEqual:
                                    boolResult = builder.CreateAnd(boolResult, builder.CreateICmpNE(result.first, operand.first));
                                    break;
                                case Operator::Less:
                                    boolResult = builder.CreateAnd(boolResult, builder.CreateICmpULT(result.first, operand.first));
                                    break;
                                case Operator::LessEqual:
                                    boolResult = builder.CreateAnd(boolResult, builder.CreateICmpULE(result.first, operand.first));
                                    break;
                                case Operator::GreaterEqual:
                                    boolResult = builder.CreateAnd(boolResult, builder.CreateICmpUGE(result.first, operand.first));
                                    break;
                                case Operator::Greater:
                                    boolResult = builder.CreateAnd(boolResult, builder.CreateICmpUGT(result.first, operand.first));
                                    break;
                                case Operator::And:
                                    boolResult = builder.CreateAnd(boolResult, builder.CreateAnd(result.first, operand.first));
                                    break;
                                case Operator::Or:
                                    boolResult = builder.CreateAnd(boolResult, builder.CreateOr(result.first, operand.first));
                                    break;
                                default:
                                    UnexpectedError0(BadOperator);
                            }
                            break;
                        case AbaciValue::Integer:
                            switch (op) {
                                case Operator::Equal:
                                    boolResult = builder.CreateAnd(boolResult, builder.CreateICmpEQ(result.first, operand.first));
                                    break;
                                case Operator::NotEqual:
                                    boolResult = builder.CreateAnd(boolResult, builder.CreateICmpNE(result.first, operand.first));
                                    break;
                                case Operator::Less:
                                    boolResult = builder.CreateAnd(boolResult, builder.CreateICmpSLT(result.first, operand.first));
                                    break;
                                case Operator::LessEqual:
                                    boolResult = builder.CreateAnd(boolResult, builder.CreateICmpSLE(result.first, operand.first));
                                    break;
                                case Operator::GreaterEqual:
                                    boolResult = builder.CreateAnd(boolResult, builder.CreateICmpSGE(result.first, operand.first));
                                    break;
                                case Operator::Greater:
                                    boolResult = builder.CreateAnd(boolResult, builder.CreateICmpSGT(result.first, operand.first));
                                    break;
                                case Operator::And:
                                    boolResult = builder.CreateAnd(boolResult, builder.CreateAnd(toBoolean(result), toBoolean(operand)));
                                    break;
                                case Operator::Or:
                                    boolResult = builder.CreateAnd(boolResult, builder.CreateOr(toBoolean(result), toBoolean(operand)));
                                    break;
                                default:
                                    UnexpectedError0(BadOperator);
                            }
                            break;
                        case AbaciValue::Floating:
                            switch (op) {
                                case Operator::Equal:
                                    boolResult = builder.CreateAnd(boolResult, builder.CreateFCmpOEQ(result.first, operand.first));
                                    break;
                                case Operator::NotEqual:
                                    boolResult = builder.CreateAnd(boolResult, builder.CreateFCmpONE(result.first, operand.first));
                                    break;
                                case Operator::Less:
                                    boolResult = builder.CreateAnd(boolResult, builder.CreateFCmpOLT(result.first, operand.first));
                                    break;
                                case Operator::LessEqual:
                                    boolResult = builder.CreateAnd(boolResult, builder.CreateFCmpOLE(result.first, operand.first));
                                    break;
                                case Operator::GreaterEqual:
                                    boolResult = builder.CreateAnd(boolResult, builder.CreateFCmpOGE(result.first, operand.first));
                                    break;
                                case Operator::Greater:
                                    boolResult = builder.CreateAnd(boolResult, builder.CreateFCmpOGT(result.first, operand.first));
                                    break;
                                case Operator::And:
                                    boolResult = builder.CreateAnd(boolResult, builder.CreateAnd(toBoolean(result), toBoolean(operand)));
                                    break;
                                case Operator::Or:
                                    boolResult = builder.CreateAnd(boolResult, builder.CreateOr(toBoolean(result), toBoolean(operand)));
                                    break;
                                default:
                                    UnexpectedError0(BadOperator);
                            }
                            break;
                        case AbaciValue::Complex: {
                            Value *realValue1 = builder.CreateLoad(builder.getDoubleTy(), builder.CreateStructGEP(jit.getNamedType("struct.Complex"), result.first, 0));
                            Value *imagValue1 = builder.CreateLoad(builder.getDoubleTy(), builder.CreateStructGEP(jit.getNamedType("struct.Complex"), result.first, 1));
                            Value *realValue2 = builder.CreateLoad(builder.getDoubleTy(), builder.CreateStructGEP(jit.getNamedType("struct.Complex"), operand.first, 0));
                            Value *imagValue2 = builder.CreateLoad(builder.getDoubleTy(), builder.CreateStructGEP(jit.getNamedType("struct.Complex"), operand.first, 1));
                            switch (op) {
                                case Operator::Equal:
                                    boolResult = builder.CreateAnd(boolResult,
                                        builder.CreateAnd(builder.CreateFCmpOEQ(realValue1, realValue2), builder.CreateFCmpOEQ(imagValue1, imagValue2)));
                                    break;
                                case Operator::NotEqual:
                                    boolResult = builder.CreateAnd(boolResult,
                                        builder.CreateOr(builder.CreateFCmpONE(realValue1, realValue2), builder.CreateFCmpONE(imagValue1, imagValue2)));
                                    break;
                                default:
                                    UnexpectedError0(BadOperator);
                            }
                            break;
                        }
                        case AbaciValue::String: {
                            switch (op) {
                                case Operator::Equal:
                                    boolResult = builder.CreateAnd(boolResult,
                                        builder.CreateICmpEQ(ConstantInt::get(builder.getInt1Ty(), true),
                                        builder.CreateCall(module.getFunction("compareString"), { result.first, operand.first })));
                                    break;
                                case Operator::NotEqual:
                                    boolResult = builder.CreateAnd(boolResult,
                                        builder.CreateICmpEQ(ConstantInt::get(builder.getInt1Ty(), false),
                                        builder.CreateCall(module.getFunction("compareString"), { result.first, operand.first })));
                                    break;
                                default:
                                    UnexpectedError0(BadOperator);
                            }
                            break;
                        }
                        default:
                            UnexpectedError0(BadType);
                            break;
                    }
                    result.first = operand.first;
                    result.second = type;
                }
                result.first = boolResult;
                result.second = AbaciValue::Boolean;
                push(result);
            }
            break;
        }
        default:
            UnexpectedError0(BadAssociation);
    }
}

template<>
void ExprCodeGen::codeGen(const FunctionValueCall& call) const {
    switch (jit.getCache()->getCacheType(call.name)) {
        case Cache::CacheFunction: {
            std::vector<Value*> arguments;
            std::vector<Type> types;
            for (const auto& arg : call.args) {
                ExprCodeGen expr(jit, locals, temps);
                expr(arg);
                auto result = expr.get();
                arguments.push_back(result.first);
                types.push_back(result.second);
            }
            auto type = jit.getCache()->getFunctionInstantiationType(call.name, types);
            auto result = builder.CreateCall(module.getFunction(mangled(call.name, types)), arguments);
            temps->addTemporary({ result, type });
            push({ result, type });
            break;
        }
        case Cache::CacheClass: {
            Constant *name = ConstantDataArray::getString(module.getContext(), call.name.c_str());
            AllocaInst *className = builder.CreateAlloca(name->getType(), nullptr);
            builder.CreateStore(name, className);
            Value *variablesCount = ConstantInt::get(builder.getInt64Ty(), call.args.size());
            Value *instance = builder.CreateCall(module.getFunction("makeInstance"), { className, variablesCount });
            ArrayType *array = ArrayType::get(builder.getInt64Ty(), call.args.size());
            Value *arrayPtr = builder.CreateLoad(PointerType::get(array, 0), builder.CreateStructGEP(jit.getNamedType("struct.Instance"), instance, 2));
            auto instanceType = std::make_shared<TypeInstance>();
            instanceType->className = call.name;
            for (std::size_t index = 0; const auto& value : call.args) {
                ExprCodeGen expr(jit, locals, temps);
                expr(value);
                auto result = expr.get();
                Value *makeTemporary;
                if (!temps->isTemporary(result.first)) {
                    makeTemporary = cloneValue(jit, result.first, result.second);
                }
                else {
                    makeTemporary = result.first;
                    temps->removeTemporary(result.first);
                }
                Value *ptr = builder.CreateGEP(array, arrayPtr, { builder.getInt32(0), builder.getInt32(index) });
                builder.CreateStore(builder.CreateBitCast(makeTemporary, builder.getInt64Ty()), ptr);
                instanceType->variableTypes.push_back(result.second);
                ++index;
            }
            temps->addTemporary({ instance, instanceType });
            push({ instance, instanceType });
            break;
        }
        default:
            UnexpectedError1(CallableNotExist, call.name);
    }
}

template<>
void ExprCodeGen::codeGen([[maybe_unused]] const UserInput& input) const {
    Value *userInput = builder.CreateCall(module.getFunction("userInput"), { getContextValue(jit) });
    temps->addTemporary({ userInput, AbaciValue::String });
    push({ userInput, AbaciValue::String });
}

template<>
void ExprCodeGen::codeGen(const TypeConv& typeConversion) const {
    ExprCodeGen expr(jit, locals, temps);
    expr(*(typeConversion.expression));
    auto result = expr.get();
    auto targetType = typeConversion.toType;
    if (typeToScalar(targetType) == AbaciValue::Real || typeToScalar(targetType) == AbaciValue::Imag) {
        targetType = AbaciValue::Floating;
    }
    Value *conversion = builder.CreateBitCast(
        builder.CreateCall(module.getFunction("toType"), {
            builder.getInt32(static_cast<int>(typeToScalar(typeConversion.toType))),
            builder.CreateBitCast(result.first, builder.getInt64Ty()),
            builder.getInt32(static_cast<int>(typeToScalar(result.second)))
        }),
        typeToLLVMType(jit, targetType));
    temps->addTemporary({ conversion, targetType });
    push({ conversion, targetType });
}

template<>
void ExprCodeGen::codeGen(const List& list) const {
    Assert(!list.elementType.empty() || !list.elements->empty());
    Type elementType = AbaciValue::None;
    if (!list.elementType.empty()) {
        if (auto iter = TypeConversions.find(list.elementType); iter != TypeConversions.end()) {
            elementType = iter->second;
        }
    }
    std::size_t listSize = list.elements->size();
    Value *listObject = builder.CreateCall(module.getFunction("makeList"), { builder.getInt64(listSize) });
    if (!list.elements->empty()) {
        ArrayType *array = ArrayType::get(builder.getInt64Ty(), list.elements->size());
        Value *arrayPtr = builder.CreateLoad(PointerType::get(array, 0), builder.CreateStructGEP(jit.getNamedType("struct.List"), listObject, 1));
        Value *valuePtr = builder.CreateGEP(array, arrayPtr, { builder.getInt32(0), builder.getInt32(0) });
        for (std::size_t index = 0; const auto& element : *(list.elements)) {
            ExprCodeGen expr(jit, locals, temps);
            expr(element);
            auto result = expr.get();
            if (list.elementType.empty() && elementType == AbaciValue::None) {
                elementType = result.second;
            }
            Assert(elementType == result.second);
            Value *value;
            if (temps->isTemporary(result.first)) {
                temps->removeTemporary(result.first);
                value = result.first;
            }
            else {
                value = cloneValue(jit, result.first, result.second);
            }
            ArrayType *array = ArrayType::get(builder.getInt64Ty(), list.elements->size());
            Value *arrayPtr = builder.CreateLoad(PointerType::get(array, 0), builder.CreateStructGEP(jit.getNamedType("struct.List"), listObject, 1));
            Value *valuePtr = builder.CreateGEP(array, arrayPtr, { builder.getInt32(0), builder.getInt32(index) });
            builder.CreateStore(value, valuePtr);
            ++index;
        }
    }
    auto type = std::make_shared<TypeList>();
    type->elementType = elementType;
    temps->addTemporary({ listObject, type });
    push({ listObject, type });
}

template<>
void ExprCodeGen::codeGen(const MultiCall& call) const {
    std::string name = call.name.name;
    Type type = AbaciValue::Unset;
    TypedValue value = { nullptr, type };
    if (locals) {
        auto [ variables, index ] = locals->getIndex(name);
        if (index != LocalSymbols::noVariable) {
            type = removeConstFromType(variables->getType(index));
            value = { loadMutableValue(jit, variables->getValue(index), type), type };
        }
    }
    if (type == AbaciValue::Unset) {
        auto globals = jit.getRuntimeContext().globals;
        auto globalIndex = globals->getIndex(name);
        if (globalIndex != GlobalSymbols::noVariable) {
            type = removeConstFromType(globals->getType(globalIndex));
            value = { loadGlobalValue(jit, globalIndex, type), type };
        }
    }
    if (type == AbaciValue::Unset) {
        UnexpectedError1(VariableNotExist, name);
    }
    for (const auto& callElement : call.calls) {
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
                Value *valuePtr = builder.CreateGEP(array, arrayPtr, { builder.getInt32(0), builder.getInt32(index) });
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
                    name += "[]";
                    if (typePtr == nullptr && typeToScalar(removeConstFromType(type)) != AbaciValue::String) {
                        UnexpectedError1(TooManyIndexes, name);
                    }
                    ExprCodeGen expr(jit, locals, temps);
                    expr(indexExpression);
                    if (typeToScalar(expr.get().second) != AbaciValue::Integer) {
                        UnexpectedError1(IndexNotInt, name);
                    }
                    if (typeToScalar(removeConstFromType(type)) == AbaciValue::List) {
                        Value *index = expr.get().first;
                        Value *listSize = builder.CreateLoad(builder.getInt64Ty(), builder.CreateStructGEP(jit.getNamedType("struct.List"), value.first, 0));
                        index = builder.CreateCall(module.getFunction("validIndex"), { index, listSize });
                        ArrayType *array = ArrayType::get(builder.getInt64Ty(), 0);
                        Value *listElements = builder.CreateLoad(PointerType::get(array, 0), builder.CreateStructGEP(jit.getNamedType("struct.List"), value.first, 1));
                        Value *elementPtr = builder.CreateGEP(array, listElements, { builder.getInt32(0), index });
                        type = typePtr->elementType;
                        value = { builder.CreateLoad(typeToLLVMType(jit, type), elementPtr), type };
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
                        Value *index = expr.get().first;
                        value = { builder.CreateCall(module.getFunction("indexString"), { value.first, index }), AbaciValue::String };
                        temps->addTemporary(value);
                    }
                }
                break;
            }
            case CallList::TypeFunction: {
                const auto& callMethod = std::get<FunctionValueCall>(callElement.call);
                if (typeToScalar(removeConstFromType(type)) != AbaciValue::Instance) {
                    UnexpectedError1(BadObject, name);
                }
                auto instanceType = std::dynamic_pointer_cast<TypeInstance>(std::get<std::shared_ptr<TypeBase>>(type));
                Assert(instanceType != nullptr);
                std::string methodName = instanceType->className + '.' + callMethod.name;
                std::vector<Value*> arguments;
                std::vector<Type> types;
                for (const auto& arg : callMethod.args) {
                    ExprCodeGen expr(jit, locals, temps);
                    expr(arg);
                    auto result = expr.get();
                    arguments.push_back(result.first);
                    types.push_back(result.second);
                }
                types.insert(types.begin(), instanceType);
                arguments.insert(arguments.begin(), value.first);
                type = jit.getCache()->getFunctionInstantiationType(methodName, types);
                auto result = builder.CreateCall(module.getFunction(mangled(methodName, types)), arguments);
                temps->addTemporary({ result, type });
                value = { result, type };
                break;
            }
            default:
                UnexpectedError0(BadCall);
        }
    }
    push(value);
}

void ExprCodeGen::operator()(const ExprNode& node) const {
    std::visit([this](const auto& nodeType){
        this->codeGen(nodeType);
    }, node.data);
}

Type ExprCodeGen::promote(TypedValue& operand1, TypedValue& operand2) const {
    if (std::holds_alternative<std::shared_ptr<TypeBase>>(operand1.second)
        || std::holds_alternative<std::shared_ptr<TypeBase>>(operand2.second)) {
            if (operand1.second == operand2.second) {
                return operand1.second;
            }
            else {
                UnexpectedError0(BadCoerceTypes);
            }
    }
    if ((typeToScalar(operand1.second) == typeToScalar(operand2.second))) {
        return typeToScalar(operand1.second);
    }
    auto [ lo, hi ] = (typeToScalar(operand1.second) < typeToScalar(operand2.second)) ? std::pair{ &operand1, &operand2 } : std::pair{ &operand2, &operand1 };
    switch (typeToScalar(hi->second)) {
        case AbaciValue::Boolean:
        case AbaciValue::Integer:
            break;
        case AbaciValue::Floating:
            switch (typeToScalar(lo->second)) {
                case AbaciValue::Integer:
                    lo->first = builder.CreateSIToFP(lo->first, builder.getDoubleTy());
                    break;
                default:
                    break;
            }
            break;
        case AbaciValue::Complex:
            switch (typeToScalar(lo->second)) {
                case AbaciValue::Integer: {
                    auto real = builder.CreateSIToFP(lo->first, builder.getDoubleTy());
                    auto imag = ConstantFP::get(builder.getDoubleTy(), 0.0);
                    lo->first = builder.CreateCall(module.getFunction("makeComplex"), { real, imag });
                    temps->addTemporary({ lo->first, hi->second });
                    break;
                }
                case AbaciValue::Floating: {
                    auto real = lo->first;
                    auto imag = ConstantFP::get(builder.getDoubleTy(), 0.0);
                    lo->first = builder.CreateCall(module.getFunction("makeComplex"), { real, imag });
                    temps->addTemporary({ lo->first, hi->second });
                    break;
                }
                default:
                    break;
            }
            break;
        default:
            UnexpectedError0(BadCoerceTypes);
    }
    lo->second = hi->second;
    return typeToScalar(hi->second);
}

Value *ExprCodeGen::toBoolean(TypedValue& operand) const {
    Value *boolean;
    switch (typeToScalar(operand.second)) {
        case AbaciValue::Boolean:
            boolean = operand.first;
            break;
        case AbaciValue::Integer:
            boolean = builder.CreateICmpNE(operand.first, ConstantInt::get(builder.getInt64Ty(), 0));
            break;
        case AbaciValue::Floating:
            boolean = builder.CreateFCmpONE(operand.first, ConstantFP::get(builder.getDoubleTy(), 0.0));
            break;
        case AbaciValue::String: {
            Value *length = builder.CreateLoad(builder.getInt64Ty(), builder.CreateStructGEP(jit.getNamedType("struct.String"), operand.first, 1));
            boolean = builder.CreateICmpNE(length, ConstantInt::get(builder.getInt64Ty(), 0));
            break;
        }
        default:
            UnexpectedError0(NoBoolean);
    }
    return boolean;
}

}