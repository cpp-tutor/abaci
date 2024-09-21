#include "ExprCodeGen.hpp"
#include "utility/Constant.hpp"
#include "utility/Symbol.hpp"
#include "utility/Report.hpp"
#include "localize/Messages.hpp"
#include "utility/Utility.hpp"
#include <llvm/IR/Constants.h>

namespace abaci::codegen {

using namespace llvm;

using abaci::ast::ExprNode;
using abaci::ast::ExprList;
using abaci::ast::Variable;
using abaci::utility::AbaciValue;
using abaci::utility::Operator;
using abaci::utility::TypeBase;
using abaci::utility::LocalSymbols;
using abaci::utility::GlobalSymbols;
using abaci::utility::LocalSymbols;
using abaci::utility::mangled;
using abaci::utility::removeConstFromType;
using abaci::utility::TypeInstance;
using abaci::utility::cloneValue;
using abaci::utility::loadMutableValue;
using abaci::utility::loadGlobalValue;
using abaci::utility::getContextValue;
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
        case AbaciValue::Complex: {
            Value *value = cloneValue(jit, ConstantExpr::getIntToPtr(ConstantInt::get(builder.getInt64Ty(), constantValue.first.integer), PointerType::get(jit.getNamedType("struct.Complex"), 0)), AbaciValue::Complex);
            temps->addTemporary({ value, AbaciValue::Complex });
            push({ value, AbaciValue::Complex });
            break;
        }
        case AbaciValue::String: {
            Value *value = cloneValue(jit, ConstantExpr::getIntToPtr(ConstantInt::get(builder.getInt64Ty(), constantValue.first.integer), PointerType::get(jit.getNamedType("struct.String"), 0)), AbaciValue::String);
            temps->addTemporary({ value, AbaciValue::String });
            push({ value, AbaciValue::String });
            break;
        }
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
                    default:
                        UnexpectedError0(BadType);
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
            const auto& expr = std::get<ExprNode::ListNode>(node.data).second;
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
                    auto type = (operand.second != result.second) ? promote(result, operand) : typeToScalar(result.second);
                    switch (type) {
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
void ExprCodeGen::codeGen<ExprNode::VariableNode>(const ExprNode& node) const {
    const Variable& variable = std::get<ExprNode::VariableNode>(node.data);
    if (locals) {
        auto [ vars, index ] = locals->getIndex(variable.name);
        if (index != LocalSymbols::noVariable) {
            auto type = removeConstFromType(vars->getType(index));
            Value *value = cloneValue(jit, loadMutableValue(jit, vars->getValue(index), type), type);
            temps->addTemporary({ value, type });
            push({ value, type });
            return;
        }
    }
    auto globals = jit.getRuntimeContext().globals;
    auto globalIndex = globals->getIndex(variable.name);
    if (globalIndex != GlobalSymbols::noVariable) {
        auto type = removeConstFromType(globals->getType(globalIndex));
        Value *value = cloneValue(jit, loadGlobalValue(jit, globalIndex, type), type);
        temps->addTemporary({ value, type });
        push({ value, type });
    }
    else {
        UnexpectedError1(VarNotExist, variable.name);
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
            for (std::size_t index = 0; const auto& value : call.args) {
                ExprCodeGen expr(jit, locals, temps);
                expr(value);
                auto result = expr.get();
                Value *ptr = builder.CreateGEP(array, arrayPtr, { builder.getInt32(0), builder.getInt32(index) });
                builder.CreateStore(builder.CreateBitCast(result.first, builder.getInt64Ty()), ptr);
                if (temps->isTemporary(result.first)) {
                    temps->removeTemporary(result.first);
                }
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
    const std::string& name = data.name.name;
    Type type;
    if (locals) {
        auto [ vars, index ] = locals->getIndex(name);
        if (index != LocalSymbols::noVariable) {
            type = vars->getType(index);
            Value *value = loadMutableValue(jit, vars->getValue(index), type);
            for (const auto& member : data.memberList) {
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
            auto stackType = removeConstFromType(type);
            Value *stackValue = cloneValue(jit, value, type);
            temps->addTemporary({ stackValue, stackType });
            push({ stackValue, stackType });
            return;
        }
    }
    auto globals = jit.getRuntimeContext().globals;
    auto globalIndex = globals->getIndex(name);
    if (globalIndex != GlobalSymbols::noVariable) {
        Value *value = loadGlobalValue(jit, globalIndex, globals->getType(globalIndex));
        type = globals->getType(globalIndex);
        for (const auto& member : data.memberList) {
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
        auto stackType = removeConstFromType(type);
        Value *stackValue = cloneValue(jit, value, type);
        temps->addTemporary({ stackValue, stackType });
        push({ stackValue, stackType });
    }
    else {
        UnexpectedError1(VarNotExist, name);
    }
}

template<>
void ExprCodeGen::codeGen<ExprNode::MethodNode>(const ExprNode& node) const {
    const auto& methodCall = std::get<ExprNode::MethodNode>(node.data);
    const std::string& name = methodCall.name.name;
    Type type;
    Value *thisPtr = nullptr;
    if (locals) {
        auto [ vars, index ] = locals->getIndex(name);
        if (index != LocalSymbols::noVariable) {
            type = vars->getType(index);
            Value *value = loadMutableValue(jit, vars->getValue(index), type);
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
        auto returnType = jit.getCache()->getFunctionInstantiationType(methodName, types);
        auto result = builder.CreateCall(module.getFunction(mangled(methodName, types)), arguments);
        temps->addTemporary({ result, returnType });
        push({ result, returnType });
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
    const auto& typeConversion = std::get<ExprNode::TypeConvNode>(node.data);
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