#include "ExprCodeGen.hpp"
#include "utility/Constant.hpp"
#include "utility/Symbol.hpp"
#include "utility/Report.hpp"
#include "localize/Messages.hpp"
#include "engine/Utility.hpp"
#include <llvm/IR/Constants.h>

namespace abaci::codegen {

using namespace llvm;

using abaci::ast::ExprNode;
using abaci::ast::ExprList;
using abaci::utility::AbaciValue;
using abaci::utility::Operator;
using abaci::utility::TypeBase;
using abaci::utility::LocalSymbols;
using abaci::utility::GlobalSymbols;
using abaci::utility::LocalSymbols;
using abaci::utility::mangled;
using abaci::utility::removeConstFromType;
using abaci::utility::TypeInstance;
using abaci::engine::loadMutableValue;
using abaci::engine::loadGlobalValue;
using abaci::engine::Cache;

template<>
void ExprCodeGen::codeGen<ExprNode::ValueNode>(const ExprNode& node) const {
    auto index = std::get<ExprNode::ValueNode>(node.data);
    const auto& constantValue = constants.get(index);
    switch (typeToScalar(constantValue.second)) {
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
void ExprCodeGen::codeGen<ExprNode::ListNode>(const ExprNode& node) const {
    switch (std::get<ExprNode::ListNode>(node.data).first) {
        case ExprNode::Left: {
            const auto& expr = std::get<ExprNode::ListNode>(node.data).second;
            (*this)(expr.front());
            auto result = pop();
            for (auto iter = ++expr.begin(); iter != expr.end();) {
                auto op = std::get<Operator>(iter++->data);
                (*this)(*iter++);
                auto operand = pop();
                auto type = (operand.second != result.second) ? promote(result, operand) : typeToScalar(result.second);
                switch (type) {
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
                                LogicError0(BadOperator);
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
                                LogicError0(BadOperator);
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
                                LogicError0(BadOperator);
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
                                LogicError0(BadOperator);
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
                                LogicError0(BadOperator);
                        }
                        break;
                    default:
                        LogicError0(BadType);
                        break;
                }
            }
            push(result);
            break;
        }
        case ExprNode::Right: {
            const auto& expr = std::get<ExprNode::ListNode>(node.data).second;
            (*this)(expr.back());
            auto result = pop();
            for (auto iter = ++expr.rbegin(); iter != expr.rend();) {
                auto op = std::get<Operator>(iter++->data);
                (*this)(*iter++);
                auto operand = pop();
                auto type = (operand.second != result.second) ? promote(result, operand) : typeToScalar(result.second);
                switch (type) {
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
                                LogicError0(BadOperator);
                        }
                        break;
                    case AbaciValue::Floating:
                        switch (op) {
                            case Operator::Exponent:
                                result.first = builder.CreateCall(module.getFunction("pow"), { operand.first, result.first });
                                break;
                            default:
                                LogicError0(BadOperator);
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
                                LogicError0(BadOperator);
                        }
                        break;
                default:
                    LogicError0(BadType);
                    break;
                }
            }
            push(result);
            break;
        }
        case ExprNode::Unary: {
            const auto& expr = std::get<ExprNode::ListNode>(node.data).second;
            (*this)(expr.back());
            auto result = pop();
            auto type = typeToScalar(result.second);
            for (auto iter = ++expr.rbegin(); iter != expr.rend();) {
                auto op = std::get<Operator>(iter++->data);
                switch (type) {
                    case AbaciValue::Boolean:
                        switch (op) {
                            case Operator::Not:
                            case Operator::Compl:
                                result.first = builder.CreateNot(result.first);
                                break;
                            default:
                                LogicError0(BadOperator);
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
                            default:
                                LogicError0(BadOperator);
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
                            default:
                                LogicError0(BadOperator);
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
                            default:
                                LogicError0(BadOperator);
                        }
                        break;
                default:
                    LogicError0(BadType);
                    break;
                }
            }
            push(result);
            break;
        }
        case ExprNode::Boolean: {
            const auto& expr = std::get<ExprNode::ListNode>(node.data).second;
            (*this)(expr.front());
            auto result = pop();
            if (expr.size() == 1) {
                push(result);
            }
            else {
                Value *bool_result = ConstantInt::get(builder.getInt1Ty(), true);
                for (auto iter = ++expr.begin(); iter != expr.end();) {
                    auto op = std::get<Operator>(iter++->data);
                    (*this)(*iter++);
                    auto operand = pop();
                    auto type = (operand.second != result.second) ? promote(result, operand) : typeToScalar(result.second);
                    switch (type) {
                        case AbaciValue::Boolean:
                            switch (op) {
                                case Operator::Equal:
                                    bool_result = builder.CreateAnd(bool_result, builder.CreateICmpEQ(result.first, operand.first));
                                    break;
                                case Operator::NotEqual:
                                    bool_result = builder.CreateAnd(bool_result, builder.CreateICmpNE(result.first, operand.first));
                                    break;
                                case Operator::Less:
                                    bool_result = builder.CreateAnd(bool_result, builder.CreateICmpULT(result.first, operand.first));
                                    break;
                                case Operator::LessEqual:
                                    bool_result = builder.CreateAnd(bool_result, builder.CreateICmpULE(result.first, operand.first));
                                    break;
                                case Operator::GreaterEqual:
                                    bool_result = builder.CreateAnd(bool_result, builder.CreateICmpUGE(result.first, operand.first));
                                    break;
                                case Operator::Greater:
                                    bool_result = builder.CreateAnd(bool_result, builder.CreateICmpUGT(result.first, operand.first));
                                    break;
                                case Operator::And:
                                    bool_result = builder.CreateAnd(bool_result, builder.CreateAnd(result.first, operand.first));
                                    break;
                                case Operator::Or:
                                    bool_result = builder.CreateAnd(bool_result, builder.CreateOr(result.first, operand.first));
                                    break;
                                default:
                                    LogicError0(BadOperator);
                            }
                            break;
                        case AbaciValue::Integer:
                            switch (op) {
                                case Operator::Equal:
                                    bool_result = builder.CreateAnd(bool_result, builder.CreateICmpEQ(result.first, operand.first));
                                    break;
                                case Operator::NotEqual:
                                    bool_result = builder.CreateAnd(bool_result, builder.CreateICmpNE(result.first, operand.first));
                                    break;
                                case Operator::Less:
                                    bool_result = builder.CreateAnd(bool_result, builder.CreateICmpSLT(result.first, operand.first));
                                    break;
                                case Operator::LessEqual:
                                    bool_result = builder.CreateAnd(bool_result, builder.CreateICmpSLE(result.first, operand.first));
                                    break;
                                case Operator::GreaterEqual:
                                    bool_result = builder.CreateAnd(bool_result, builder.CreateICmpSGE(result.first, operand.first));
                                    break;
                                case Operator::Greater:
                                    bool_result = builder.CreateAnd(bool_result, builder.CreateICmpSGT(result.first, operand.first));
                                    break;
                                case Operator::And:
                                    bool_result = builder.CreateAnd(bool_result, builder.CreateAnd(toBoolean(result), toBoolean(operand)));
                                    break;
                                case Operator::Or:
                                    bool_result = builder.CreateAnd(bool_result, builder.CreateOr(toBoolean(result), toBoolean(operand)));
                                    break;
                                default:
                                    LogicError0(BadOperator);
                            }
                            break;
                        case AbaciValue::Floating:
                            switch (op) {
                                case Operator::Equal:
                                    bool_result = builder.CreateAnd(bool_result, builder.CreateFCmpOEQ(result.first, operand.first));
                                    break;
                                case Operator::NotEqual:
                                    bool_result = builder.CreateAnd(bool_result, builder.CreateFCmpONE(result.first, operand.first));
                                    break;
                                case Operator::Less:
                                    bool_result = builder.CreateAnd(bool_result, builder.CreateFCmpOLT(result.first, operand.first));
                                    break;
                                case Operator::LessEqual:
                                    bool_result = builder.CreateAnd(bool_result, builder.CreateFCmpOLE(result.first, operand.first));
                                    break;
                                case Operator::GreaterEqual:
                                    bool_result = builder.CreateAnd(bool_result, builder.CreateFCmpOGE(result.first, operand.first));
                                    break;
                                case Operator::Greater:
                                    bool_result = builder.CreateAnd(bool_result, builder.CreateFCmpOGT(result.first, operand.first));
                                    break;
                                case Operator::And:
                                    bool_result = builder.CreateAnd(bool_result, builder.CreateAnd(toBoolean(result), toBoolean(operand)));
                                    break;
                                case Operator::Or:
                                    bool_result = builder.CreateAnd(bool_result, builder.CreateOr(toBoolean(result), toBoolean(operand)));
                                    break;
                                default:
                                    LogicError0(BadOperator);
                            }
                            break;
                        case AbaciValue::Complex: {
                            Value *real_value1 = builder.CreateLoad(builder.getDoubleTy(), builder.CreateStructGEP(jit.getNamedType("struct.Complex"), result.first, 0));
                            Value *imag_value1 = builder.CreateLoad(builder.getDoubleTy(), builder.CreateStructGEP(jit.getNamedType("struct.Complex"), result.first, 1));
                            Value *real_value2 = builder.CreateLoad(builder.getDoubleTy(), builder.CreateStructGEP(jit.getNamedType("struct.Complex"), operand.first, 0));
                            Value *imag_value2 = builder.CreateLoad(builder.getDoubleTy(), builder.CreateStructGEP(jit.getNamedType("struct.Complex"), operand.first, 1));
                            switch (op) {
                                case Operator::Equal:
                                    bool_result = builder.CreateAnd(bool_result,
                                        builder.CreateAnd(builder.CreateFCmpOEQ(real_value1, real_value2), builder.CreateFCmpOEQ(imag_value1, imag_value2)));
                                    break;
                                case Operator::NotEqual:
                                    bool_result = builder.CreateAnd(bool_result,
                                        builder.CreateOr(builder.CreateFCmpONE(real_value1, real_value2), builder.CreateFCmpONE(imag_value1, imag_value2)));
                                    break;
                                default:
                                    LogicError0(BadOperator);
                            }
                            break;
                        }
                        case AbaciValue::String: {
                            Value *str1_ptr = builder.CreateLoad(builder.getInt8PtrTy(), builder.CreateStructGEP(jit.getNamedType("struct.String"), result.first, 0));
                            Value *str2_ptr = builder.CreateLoad(builder.getInt8PtrTy(), builder.CreateStructGEP(jit.getNamedType("struct.String"), operand.first, 0));
                            switch (op) {
                                case Operator::Equal:
                                    bool_result = builder.CreateAnd(bool_result,
                                        builder.CreateICmpEQ(ConstantInt::get(builder.getInt32Ty(), 0),
                                        builder.CreateCall(module.getFunction("compareString"), { str1_ptr, str2_ptr })));
                                    break;
                                case Operator::NotEqual:
                                    bool_result = builder.CreateAnd(bool_result,
                                        builder.CreateICmpNE(ConstantInt::get(builder.getInt32Ty(), 0),
                                        builder.CreateCall(module.getFunction("compareString"), { str1_ptr, str2_ptr })));
                                    break;
                                default:
                                    LogicError0(BadOperator);
                            }
                            break;
                        }
                        default:
                            LogicError0(BadType);
                            break;
                    }
                    result.first = operand.first;
                    result.second = type;
                }
                result.first = bool_result;
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
void ExprCodeGen::codeGen<ExprNode::VariableNode>(const ExprNode& node) const {
    const std::string& name = std::get<ExprNode::VariableNode>(node.data).get();
    if (locals) {
        auto [ vars, index ] = locals->getIndex(name);
        if (index != LocalSymbols::noVariable) {
            auto *value = loadMutableValue(jit, vars->getValue(index), vars->getType(index));
            push({ value, removeConstFromType(vars->getType(index)) });
            return;
        }
    }
    auto globals = jit.getRuntimeContext().globals;
    auto globalIndex = globals->getIndex(name);
    if (globalIndex != GlobalSymbols::noVariable) {
        auto *value = loadGlobalValue(jit, globalIndex, globals->getType(globalIndex));
        push({ value, removeConstFromType(globals->getType(globalIndex)) });
    }
    else {
        UnexpectedError1(VarNotExist, name);
    }
}

template<>
void ExprCodeGen::codeGen<ExprNode::FunctionNode>(const ExprNode& node) const {
    const auto& call = std::get<ExprNode::FunctionNode>(node.data);
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
            for (int index = 0; const auto& value : call.args) {
                ExprCodeGen expr(jit, locals, temps);
                expr(value);
                auto result = expr.get();
                Value *ptr = builder.CreateGEP(array, arrayPtr, { builder.getInt32(0), builder.getInt32(index) });
                builder.CreateStore(builder.CreateBitCast(result.first, builder.getInt64Ty()), ptr);
                temps->removeTemporary(result.first);
                ++index;
            }
            auto instanceType = std::make_shared<TypeInstance>();
            instanceType->className = call.name;
            temps->addTemporary({ instance, instanceType });
            push({ instance, instanceType });
            break;
        }
        default:
            UnexpectedError1(CallableNotExist, call.name);
    }
}

template<>
void ExprCodeGen::codeGen<ExprNode::DataMemberNode>(const ExprNode& node) const {
    const auto& data = std::get<ExprNode::DataMemberNode>(node.data);
    const std::string& name = data.name.get();
    Type type;
    if (locals) {
        auto [ vars, index ] = locals->getIndex(name);
        if (index != LocalSymbols::noVariable) {
            type = vars->getType(index);
            auto *value = loadMutableValue(jit, vars->getValue(index), type);
            for (const auto& member : data.member_list) {
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
            push({ value, removeConstFromType(type) });
            return;
        }
    }
    auto globals = jit.getRuntimeContext().globals;
    auto globalIndex = globals->getIndex(name);
    if (globalIndex != GlobalSymbols::noVariable) {
        auto *value = loadGlobalValue(jit, globalIndex, globals->getType(globalIndex));
        type = globals->getType(globalIndex);
        for (const auto& member : data.member_list) {
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
        push({ value, removeConstFromType(type) });
    }
    else {
        UnexpectedError1(VarNotExist, name);
    }
}

template<>
void ExprCodeGen::codeGen<ExprNode::MethodNode>(const ExprNode& node) const {
    const auto& method_call = std::get<ExprNode::MethodNode>(node.data);
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
        arguments.insert(arguments.begin(), thisPtr);
        auto return_type = jit.getCache()->getFunctionInstantiationType(method_name, types);
        auto result = builder.CreateCall(module.getFunction(mangled(method_name, types)), arguments);
        temps->addTemporary({ result, return_type });
        push({ result, return_type });
    }
    else {
        UnexpectedError0(BadObject);
    }
}

template<>
void ExprCodeGen::codeGen<ExprNode::InputNode>([[maybe_unused]] const ExprNode& node) const {
    Value *userInput = builder.CreateCall(module.getFunction("userInput"), { getContextValue(jit) });
    temps->addTemporary({ userInput, AbaciValue::String });
    push({ userInput, AbaciValue::String });
}

template<>
void ExprCodeGen::codeGen<ExprNode::TypeConvNode>(const ExprNode& node) const {
    const auto& type_conversion = std::get<ExprNode::TypeConvNode>(node.data);
    ExprCodeGen expr(jit, locals, temps);
    expr(*(type_conversion.expression));
    auto result = expr.get();
    auto target_type = type_conversion.to_type;
    if (typeToScalar(target_type) == AbaciValue::Real || typeToScalar(target_type) == AbaciValue::Imag) {
        target_type = AbaciValue::Floating;
    }
    Value *conversion = builder.CreateBitCast(
        builder.CreateCall(module.getFunction("toType"), {
            builder.getInt32(static_cast<int>(typeToScalar(type_conversion.to_type))),
            builder.CreateBitCast(result.first, builder.getInt64Ty()),
            builder.getInt32(static_cast<int>(typeToScalar(result.second)))
        }),
        typeToLLVMType(jit, target_type));
    temps->addTemporary({ conversion, target_type });
    push({ conversion, target_type });
}

void ExprCodeGen::operator()(const ExprNode& node) const {
    switch (node.data.index()) {
        case ExprNode::ValueNode:
            codeGen<ExprNode::ValueNode>(node);
            break;
        case ExprNode::ListNode:
            codeGen<ExprNode::ListNode>(node);
            break;
        case ExprNode::VariableNode:
            codeGen<ExprNode::VariableNode>(node);
            break;
        case ExprNode::FunctionNode:
            codeGen<ExprNode::FunctionNode>(node);
            break;
        case ExprNode::DataMemberNode:
            codeGen<ExprNode::DataMemberNode>(node);
            break;
        case ExprNode::MethodNode:
            codeGen<ExprNode::MethodNode>(node);
            break;
        case ExprNode::InputNode:
            codeGen<ExprNode::InputNode>(node);
            break;
        case ExprNode::TypeConvNode:
            codeGen<ExprNode::TypeConvNode>(node);
            break;
        default:
            UnexpectedError0(BadNode);
    }
}

AbaciValue::Type ExprCodeGen::promote(TypedValue& operand1, TypedValue& operand2) const {
    if (std::holds_alternative<std::shared_ptr<TypeBase>>(operand1.second)
        || std::holds_alternative<std::shared_ptr<TypeBase>>(operand2.second)) {
        UnexpectedError0(BadCoerceTypes);
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