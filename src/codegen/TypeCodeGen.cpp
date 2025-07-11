#include "TypeCodeGen.hpp"
#include "localize/Messages.hpp"
#include "localize/Keywords.hpp"
#include <llvm/IR/Constants.h>

using namespace llvm;

namespace abaci::codegen {

using abaci::ast::FunctionValueCall;
using abaci::ast::TypeConv;
using abaci::ast::ListItems;
using abaci::ast::UserInput;
using abaci::ast::List;
using abaci::ast::CallList;
using abaci::ast::MultiCall;
using abaci::ast::ExprNode;
using abaci::ast::ExprList;
using abaci::ast::ExprPair;
using abaci::ast::PrintList;
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
using abaci::ast::Parameter;
using abaci::ast::Function;
using abaci::ast::FunctionCall;
using abaci::ast::ReturnStmt;
using abaci::ast::ExprFunction;
using abaci::ast::Class;
using abaci::ast::MethodCall;
using abaci::ast::NativeFunction;
using abaci::ast::ExpressionStmt;
using abaci::utility::Operator;
using abaci::utility::GlobalSymbols;
using abaci::utility::isConstant;
using abaci::utility::removeConstFromType;
using abaci::utility::addConstToType;
using abaci::utility::typeToScalar;
using abaci::utility::typeToString;
using abaci::utility::nativeTypeToType;
using abaci::utility::operatorToString;
using abaci::utility::TypeInstance;
using abaci::utility::TypeList;
using abaci::utility::TypeBase;
using abaci::utility::NativeType;
using abaci::utility::ValidConversions;
using abaci::utility::TypeConversions;

template<>
void TypeEvalGen::codeGen([[maybe_unused]] const std::monostate& none) const {
}

template<>
void TypeEvalGen::codeGen(const std::size_t& index) const {
    push(context->constants->getType(index));
}

template<>
void TypeEvalGen::codeGen([[maybe_unused]] const Operator& op) const {
}

template<>
void TypeEvalGen::codeGen(const std::pair<ExprNode::Association,ExprList>& listNode) const {
    switch (listNode.first) {
        case ExprNode::Left: {
            const auto& expr = listNode.second;
            (*this)(expr.front());
            auto type = pop();
            for (auto iter = ++expr.begin(); iter != expr.end();) {
                auto op = std::get<Operator>(iter++->data);
                (*this)(*iter++);
                type = promote(type, pop());
                switch (typeToScalar(type)) {
                    case AbaciValue::None:
                        break;
                    case AbaciValue::Boolean:
                        switch (op) {
                            case Operator::BitAnd:
                            case Operator::BitXor:
                            case Operator::BitOr:
                                break;
                            default:
                                LogicError2(BadOperatorForType, operatorToString(op), typeToString(type));
                        }
                        break;
                    case AbaciValue::Integer:
                        switch (op) {
                            case Operator::Plus:
                            case Operator::Minus:
                            case Operator::Times:
                            case Operator::Modulo:
                            case Operator::FloorDivide:
                            case Operator::BitAnd:
                            case Operator::BitXor:
                            case Operator::BitOr:
                                break;
                            case Operator::Divide:
                                type = AbaciValue::Floating;
                                break;
                            default:
                                LogicError2(BadOperatorForType, operatorToString(op), typeToString(type));
                        }
                        break;
                    case AbaciValue::Floating:
                    case AbaciValue::Complex:
                        switch (op) {
                            case Operator::Plus:
                            case Operator::Minus:
                            case Operator::Times:
                            case Operator::Divide:
                                break;
                            default:
                                LogicError2(BadOperatorForType, operatorToString(op), typeToString(type));
                        }
                        break;
                    case AbaciValue::String:
                        switch (op) {
                            case Operator::Plus:
                                break;
                            default:
                                LogicError2(BadOperatorForType, operatorToString(op), typeToString(type));
                        }
                        break;
                    case AbaciValue::List:
                        switch (op) {
                            case Operator::Plus:
                                break;
                            default:
                                LogicError2(BadOperatorForType, operatorToString(op), typeToString(type));
                        }
                        break;
                    default:
                        LogicError2(BadOperatorForType, operatorToString(op), typeToString(type));
                        break;
                }
            }
            push(type);
            break;
        }
        case ExprNode::Right: {
            const auto& expr = listNode.second;
            (*this)(expr.back());
            auto type = pop();
            for (auto iter = ++expr.rbegin(); iter != expr.rend();) {
                auto op = std::get<Operator>(iter++->data);
                (*this)(*iter++);
                type = promote(type, pop());
                switch (typeToScalar(type)) {
                    case AbaciValue::None:
                        break;
                    case AbaciValue::Integer:
                        switch (op) {
                            case Operator::Exponent:
                                type = AbaciValue::Floating;
                                break;
                            default:
                                LogicError2(BadOperatorForType, operatorToString(op), typeToString(type));
                        }
                        break;
                    case AbaciValue::Floating:
                    case AbaciValue::Complex:
                        switch (op) {
                            case Operator::Exponent:
                                break;
                            default:
                                LogicError2(BadOperatorForType, operatorToString(op), typeToString(type));
                        }
                        break;
                    default:
                        LogicError2(BadOperatorForType, operatorToString(op), typeToString(type));
                        break;
                }
            }
            push(type);
            break;
        }
        case ExprNode::Unary: {
            const auto& expr = listNode.second;
            (*this)(expr.back());
            auto type = pop();
            for (auto iter = ++expr.rbegin(); iter != expr.rend();) {
                auto op = std::get<Operator>(iter++->data);
                switch (typeToScalar(type)) {
                    case AbaciValue::None:
                        switch (op) {
                            case Operator::Question:
                                type = AbaciValue::String;
                                break;
                            default:
                                LogicError2(BadOperatorForType, operatorToString(op), typeToString(type));
                        }
                        break;
                    case AbaciValue::Boolean:
                        switch (op) {
                            case Operator::Not:
                            case Operator::Compl:
                                break;
                            case Operator::Question:
                                type = AbaciValue::String;
                                break;
                            default:
                                LogicError2(BadOperatorForType, operatorToString(op), typeToString(type));
                        }
                        break;
                    case AbaciValue::Integer:
                        switch (op) {
                            case Operator::Not:
                                type = AbaciValue::Boolean;
                                break;
                            case Operator::Minus:
                            case Operator::Compl:
                                break;
                            case Operator::Question:
                                type = AbaciValue::String;
                                break;
                            default:
                                LogicError2(BadOperatorForType, operatorToString(op), typeToString(type));
                        }
                        break;
                    case AbaciValue::Floating:
                        switch (op) {
                            case Operator::Not:
                                type = AbaciValue::Boolean;
                                break;
                            case Operator::Minus:
                                break;
                            case Operator::Question:
                                type = AbaciValue::String;
                                break;
                            default:
                                LogicError2(BadOperatorForType, operatorToString(op), typeToString(type));
                        }
                        break;
                    case AbaciValue::Complex:
                        switch (op) {
                            case Operator::Minus:
                                break;
                            case Operator::Question:
                                type = AbaciValue::String;
                                break;
                            default:
                                LogicError2(BadOperatorForType, operatorToString(op), typeToString(type));
                        }
                        break;
                    case AbaciValue::String:
                    case AbaciValue::List:
                        switch (op) {
                            case Operator::Bang:
                                type = AbaciValue::Integer;
                                break;
                            case Operator::Question:
                                type = AbaciValue::String;
                                break;
                            default:
                                LogicError2(BadOperatorForType, operatorToString(op), typeToString(type));
                        }
                        break;
                    case AbaciValue::Instance:
                        switch (op) {
                            case Operator::Question:
                                type = AbaciValue::String;
                                break;
                            default:
                                LogicError2(BadOperatorForType, operatorToString(op), typeToString(type));
                        }
                        break;
                    default:
                        LogicError2(BadOperatorForType, operatorToString(op), typeToString(type));
                        break;
                }
            }
            push(type);
            break;
        }
        case ExprNode::Boolean: {
            const auto& expr = listNode.second;
            (*this)(expr.front());
            auto type = pop();
            if (expr.size() == 1) {
                push(type);
            }
            else {
                for (auto iter = ++expr.begin(); iter != expr.end();) {
                    auto op = std::get<Operator>(iter++->data);
                    (*this)(*iter++);
                    type = promote(type, pop());
                    switch (typeToScalar(type)) {
                        case AbaciValue::None:
                            break;
                        case AbaciValue::Boolean:
                        case AbaciValue::Integer:
                        case AbaciValue::Floating:
                            switch (op) {
                                case Operator::Equal:
                                case Operator::NotEqual:
                                case Operator::Less:
                                case Operator::LessEqual:
                                case Operator::GreaterEqual:
                                case Operator::Greater:
                                case Operator::And:
                                case Operator::Or:
                                    break;
                                default:
                                    LogicError2(BadOperatorForType, operatorToString(op), typeToString(type));
                            }
                            break;
                        case AbaciValue::Complex:
                        case AbaciValue::String:
                            switch (op) {
                                case Operator::Equal:
                                case Operator::NotEqual:
                                    break;
                                default:
                                    LogicError2(BadOperatorForType, operatorToString(op), typeToString(type));
                            }
                            break;
                        default:
                            LogicError2(BadOperatorForType, operatorToString(op), typeToString(type));
                            break;
                    }
                }
                type = AbaciValue::Boolean;
                push(type);
            }
            break;
        }
        default:
            UnexpectedError0(BadAssociation);
    }
}

template<>
void TypeEvalGen::codeGen(const FunctionValueCall& call) const {
    switch (cache->getCacheType(call.name)) {
        case Cache::CacheFunction: {
            const auto& cacheFunction = cache->getFunction(call.name);
            std::vector<Type> types;
            for (const auto& arg : call.args) {
                TypeEvalGen expr(context, cache, locals);
                expr(arg);
                types.push_back(expr.get());
            }
            LocalSymbols params;
            for (auto argType = types.begin(); const auto& parameter : cacheFunction.parameters) {
                auto type = *argType++;
                if (typeToScalar(parameter.optionalType) != AbaciValue::None && type != parameter.optionalType) {
                    LogicError2(TypeMismatch, static_cast<int>(argType - types.begin()), call.name);
                }
                params.add(parameter.name.name, nullptr, addConstToType(type));
            }
            cache->addFunctionInstantiation(call.name, types, &params, context);
            auto returnType = cache->getFunctionInstantiationType(call.name, types);
            if (typeToScalar(cacheFunction.returnType) != AbaciValue::None && returnType != cacheFunction.returnType) {
                LogicError2(ResultTypeMismatch, typeToString(returnType), typeToString(cacheFunction.returnType));
            }
            push(returnType);
            break;
        }
        case Cache::CacheClass: {
            const Cache::Class& cacheClass = cache->getClass(call.name);
            auto object = std::make_shared<TypeInstance>();
            object->className = call.name;
            for (auto member = cacheClass.variables.begin(); const auto& arg : call.args) {
                TypeEvalGen expr(context, cache, locals);
                expr(arg);
                auto type = member->optionalType;;
                if (typeToScalar(member->optionalType) != AbaciValue::None && expr.get() != member->optionalType) {
                    LogicError2(TypeMismatchClass, static_cast<int>(member - cacheClass.variables.begin() + 1), call.name);
                }
                object->variableTypes.push_back(expr.get());
                ++member;
            }
            push(object);
            break;
        }
        case Cache::CacheNativeFunction: {
            const auto& nativeFunction = cache->getNativeFunction(call.name);
            auto iter = nativeFunction.parameterTypes.cbegin();
            for (const auto& arg : call.args) {
                if (*iter == NativeType::none) {
                    LogicError2(ArgumentType, static_cast<int>(iter - nativeFunction.parameterTypes.cbegin()), call.name);
                }
                TypeEvalGen expr(context, cache, locals);
                expr(arg);
                if (typeToScalar(expr.get()) != nativeTypeToType(*iter)) {
                    LogicError2(ArgumentType, static_cast<int>(iter - nativeFunction.parameterTypes.cbegin()), call.name);
                }
                if (*iter == NativeType::i64star) {
                    auto list = std::dynamic_pointer_cast<TypeList>(std::get<std::shared_ptr<TypeBase>>(expr.get()));
                    if (list == nullptr) {
                        LogicError0(BadType);
                    }
                    else if (typeToScalar(list->elementType) != AbaciValue::Integer) {
                        LogicError1(BadListType, typeToString(expr.get()));
                    }
                }
                else if (*iter == NativeType::f64star) {
                    auto list = std::dynamic_pointer_cast<TypeList>(std::get<std::shared_ptr<TypeBase>>(expr.get()));
                    if (list == nullptr) {
                        LogicError0(BadType);
                    }
                    else if (typeToScalar(list->elementType) != AbaciValue::Integer) {
                        LogicError1(BadListType, typeToString(expr.get()));
                    }
                }
                if ((*iter == NativeType::i64star || *iter == NativeType::f64star) && isConstant(expr.get())) {
                    LogicError1(ListIsConst, typeToString(expr.get()));
                }
                ++iter;
            }
            auto nativeReturnType = nativeTypeToType(nativeFunction.returnType);
            push(nativeReturnType);
            break;
        }
        default:
            LogicError1(CallableNotExist, call.name);
    }
}

template<>
void TypeEvalGen::codeGen([[maybe_unused]] const UserInput& input) const {
    push(AbaciValue::String);
}


template<>
void TypeEvalGen::codeGen(const TypeConv& typeConversion) const {
    auto targetType = typeConversion.toType;
    auto expression = typeConversion.expression;
    TypeEvalGen expr(context, cache, locals);
    expr(*expression);
    if (auto iter = ValidConversions.find(targetType); iter != ValidConversions.end()) {
        if (std::find(iter->second.begin(), iter->second.end(), expr.get()) == iter->second.end()) {
            LogicError0(BadConvType);
        }
    }
    else {
        LogicError1(BadConvTarget, static_cast<int>(typeToScalar(targetType)));
    }
    if (typeToScalar(targetType) == AbaciValue::Real || typeToScalar(targetType) == AbaciValue::Imag) {
        targetType = AbaciValue::Floating;
    }
    push(targetType);
}

template<>
void TypeEvalGen::codeGen(const List& list) const {
    if (list.elementType.empty() && list.elements->empty()) {
        LogicError0(EmptyListNeedsType);
    }
    Type elementType = AbaciValue::None;
    if (!list.elementType.empty()) {
        if (auto iter = TypeConversions.find(list.elementType); iter != TypeConversions.end()) {
            elementType = iter->second;
        }
    }
    if (!list.elements->empty()) {
        for (const auto& element : *(list.elements)) {
            TypeEvalGen expr(context, cache, locals);
            expr(element);
            if (list.elementType.empty() && elementType == AbaciValue::None) {
                elementType = expr.get();
            }
            if (elementType != expr.get()) {
                LogicError2(ListTypeMismatch, typeToString(elementType), typeToString(expr.get()));
            }
        }
    }
    auto type = std::make_shared<TypeList>();
    type->elementType = elementType;
    push(type);
}

template<>
void TypeEvalGen::codeGen(const MultiCall& call) const {
    std::string name = call.name.name;
    Type type = AbaciValue::Unset;
    if (locals) {
        if (auto [ variables, index ] = locals->getIndex(name); index != LocalSymbols::noVariable) {
            type = removeConstFromType(variables->getType(index));
        }
    }
    if (type == AbaciValue::Unset) {
        if (auto globalIndex = context->globals->getIndex(name); globalIndex != GlobalSymbols::noVariable) {
            type = removeConstFromType(context->globals->getType(globalIndex));
        }
    }
    if (type == AbaciValue::Unset) {
        LogicError1(VariableNotExist, name);
    }
    for (const auto& callElement : call.calls) {
        switch (callElement.call.index()) {
            case CallList::TypeVariable: {
                const auto& callVariable = std::get<Variable>(callElement.call);
                if (typeToScalar(removeConstFromType(type)) != AbaciValue::Instance) {
                    LogicError1(BadObject, name);
                }
                auto instanceType = std::dynamic_pointer_cast<TypeInstance>(std::get<std::shared_ptr<TypeBase>>(type));
                Assert(instanceType != nullptr);
                auto index = cache->getMemberIndex(cache->getClass(instanceType->className), callVariable.name);
                type = instanceType->variableTypes.at(index);
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
                    LogicError1(VariableNotList, name);
                }
                for (const auto& indexExpression : callIndexes) {
                    if (typePtr == nullptr && typeToScalar(removeConstFromType(type)) != AbaciValue::String) {
                        LogicError1(TooManyIndexes, name);
                    }
                    name += "[]";
                    TypeEvalGen expr(context, cache, locals);
                    expr(indexExpression);
                    if (typeToScalar(expr.get()) != AbaciValue::Integer) {
                        LogicError1(IndexNotInt, name);
                    }
                    if (typeToScalar(removeConstFromType(type)) == AbaciValue::List) {
                        type = typePtr->elementType;
                        if (std::holds_alternative<std::shared_ptr<TypeBase>>(type)) {
                            typePtr = std::dynamic_pointer_cast<TypeList>(std::get<std::shared_ptr<TypeBase>>(type));
                        }
                        else {
                            typePtr = nullptr;
                        }
                    }
                    else if (typeToScalar(removeConstFromType(type)) == AbaciValue::String) {
                        if (&indexExpression != &callIndexes.back()) {
                            LogicError1(TooManyIndexes, name);
                        }
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
                }
                else if (typeToScalar(removeConstFromType(type)) != AbaciValue::String) {
                    LogicError1(VariableNotList, name);
                }
                name += "[:]";
                TypeEvalGen exprBegin(context, cache, locals);
                exprBegin(callSlice.at(0));
                if (typeToScalar(exprBegin.get()) != AbaciValue::Integer) {
                    LogicError1(SliceNotInt, name);
                }
                TypeEvalGen exprEnd(context, cache, locals);
                exprEnd(callSlice.at(1));
                if (typeToScalar(exprEnd.get()) != AbaciValue::Integer) {
                    LogicError1(SliceNotInt, name);
                }
                break;
            }
            case CallList::TypeFunction: {
                const auto& callMethod = std::get<FunctionValueCall>(callElement.call);
                if (typeToScalar(removeConstFromType(type)) != AbaciValue::Instance) {
                    LogicError1(BadObject, name);
                }
                auto instanceType = std::dynamic_pointer_cast<TypeInstance>(std::get<std::shared_ptr<TypeBase>>(type));
                Assert(instanceType != nullptr);
                std::string methodName = instanceType->className + '.' + callMethod.name;
                const auto& cacheFunction = cache->getFunction(methodName);
                std::vector<Type> types;
                types.push_back(type);
                for (const auto& arg : callMethod.args) {
                    TypeEvalGen expr(context, cache, locals);
                    expr(arg);
                    types.push_back(expr.get());
                }
                LocalSymbols params;
                params.add(THIS_V, nullptr, type);
                for (auto argType = types.begin(); const auto& parameter : cacheFunction.parameters) {
                    auto type = *argType++;
                    params.add(parameter.name.name, nullptr, addConstToType(type));
                }
                cache->addFunctionInstantiation(methodName, types, &params, context);
                type = cache->getFunctionInstantiationType(methodName, types);
                break;
            }
            default:
                LogicError0(BadCall);
                break;
        }
    }
    push(type);
}

Type TypeEvalGen::promote(const Type& typeOperand1, const Type& typeOperand2) const {
    if (std::holds_alternative<std::shared_ptr<TypeBase>>(typeOperand1)
        || std::holds_alternative<std::shared_ptr<TypeBase>>(typeOperand2)) {
            if (typeOperand1 == typeOperand2) {
                return typeOperand1;
            }
            else {
                LogicError2(IncompatibleTypes, typeToString(typeOperand1), typeToString(typeOperand2));
            }
    }
    auto operand1 = std::get<AbaciValue::Type>(typeOperand1), operand2 = std::get<AbaciValue::Type>(typeOperand2);
    if (operand1 == operand2) {
        return operand1;
    }
    else if (operand1 == AbaciValue::None || operand2 == AbaciValue::None) {
        return AbaciValue::None;
    }
    else if (operand1 < AbaciValue::String && operand2 < AbaciValue::String) {
        return std::max(operand1, operand2);
    }
    else {
        LogicError0(BadType);
    }
}

void TypeEvalGen::operator()(const ExprNode& node) const {
    std::visit([this](const auto& nodeType){
        this->codeGen(nodeType);
    }, node.data);
}

void TypeCodeGen::operator()(const abaci::ast::StmtList& stmts) const {
    if (!stmts.statements.empty()) {
        LocalSymbols *enclosing = locals;
        locals = const_cast<LocalSymbols*>(static_cast<const LocalSymbols*>(&stmts));
        Assert(locals->size() == 0);
        locals->setEnclosing(enclosing);
        for (const auto& stmt : stmts.statements) {
            if (std::holds_alternative<ReturnStmt>(stmt.data) && &stmt != &stmts.statements.back()) {
                LogicError0(ReturnAtEnd);
            }
            (*this)(stmt);
        }
        if (functionType == GetReturnType) {
            locals->clear();
        }
        locals = locals->getEnclosing();
    }
}

template<>
void TypeCodeGen::codeGen([[maybe_unused]] const CommentStmt& comment) const {
}

template<>
void TypeCodeGen::codeGen(const PrintStmt& print) const {
    PrintList printData;
    if (!std::holds_alternative<std::monostate>(print.expression.data)) {
        printData.push_back(print.expression);
    }
    for (const auto& format : print.format) {
        if (std::holds_alternative<Operator>(format.data)) {
            printData.push_back(std::get<Operator>(format.data));
        }
        else {
            printData.push_back(format);
        }
    }
    if (print.trailing != Operator::None && print.trailing != Operator::Colon) {
        printData.push_back(print.trailing);
    }
    for (auto field : printData) {
        switch (field.index()) {
            case 0: {
                TypeEvalGen expr(context, cache, locals);
                expr(std::get<ExprNode>(field));
                break;
            }
            case 1:
                break;
            default:
                UnexpectedError0(BadPrint);
        }
    }
}

template<>
void TypeCodeGen::codeGen(const InitStmt& define) const {
    TypeEvalGen expr(context, cache, locals);
    expr(define.value);
    if (locals) {
        if (locals->getIndex(define.name.name, true).second != LocalSymbols::noVariable) {
            LogicError1(VariableExists, define.name.name);
        }
        else if (define.assign == Operator::Equal) {
            locals->add(define.name.name, nullptr, addConstToType(expr.get()));
        }
        else {
            locals->add(define.name.name, nullptr, expr.get());
        }
    }
    else {
        if (context->globals->getIndex(define.name.name) != GlobalSymbols::noVariable) {
            LogicError1(VariableExists, define.name.name);
        }
        else if (define.assign == Operator::Equal) {
            Assert(context->rawArray.add() == context->globals->add(define.name.name, addConstToType(expr.get())));
        }
        else {
            Assert(context->rawArray.add() == context->globals->add(define.name.name, expr.get()));
        }
    }
}

template<>
void TypeCodeGen::codeGen(const AssignStmt& assign) const {
    std::string name = assign.name.name;
    Type type = AbaciValue::Unset;
    if (locals) {
        if (auto [ variables, index ] = locals->getIndex(name); index != LocalSymbols::noVariable) {
            type = variables->getType(index);
        }
    }
    if (type == AbaciValue::Unset) {
        if (auto globalIndex = context->globals->getIndex(name); globalIndex != GlobalSymbols::noVariable) {
            type = context->globals->getType(globalIndex);
        }
    }
    if (type == AbaciValue::Unset) {
        LogicError1(VariableNotExist, name);
    }
    else if (isConstant(type)) {
        LogicError1(NoConstantAssign, assign.name.name);
    }
    for (const auto& callElement : assign.calls) {
        switch (callElement.call.index()) {
            case CallList::TypeVariable: {
                const auto& callVariable = std::get<Variable>(callElement.call);
                if (typeToScalar(removeConstFromType(type)) != AbaciValue::Instance) {
                    LogicError1(BadObject, name);
                }
                auto instanceType = std::dynamic_pointer_cast<TypeInstance>(std::get<std::shared_ptr<TypeBase>>(type));
                Assert(instanceType != nullptr);
                auto index = cache->getMemberIndex(cache->getClass(instanceType->className), callVariable.name);
                type = instanceType->variableTypes.at(index);
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
                    LogicError1(VariableNotList, name);
                }
                for (const auto& indexExpression : callIndexes) {
                    if (typePtr == nullptr && typeToScalar(removeConstFromType(type)) != AbaciValue::String) {
                        LogicError1(TooManyIndexes, name);
                    }
                    name += "[]";
                    TypeEvalGen expr(context, cache, locals);
                    expr(indexExpression);
                    if (typeToScalar(expr.get()) != AbaciValue::Integer) {
                        LogicError1(IndexNotInt, name);
                    }
                    if (typeToScalar(removeConstFromType(type)) == AbaciValue::List) {
                        type = typePtr->elementType;
                        if (std::holds_alternative<std::shared_ptr<TypeBase>>(type)) {
                            typePtr = std::dynamic_pointer_cast<TypeList>(std::get<std::shared_ptr<TypeBase>>(type));
                        }
                        else {
                            typePtr = nullptr;
                        }
                    }
                    else if (typeToScalar(removeConstFromType(type)) == AbaciValue::String) {
                        if (&indexExpression != &callIndexes.back()) {
                            LogicError1(TooManyIndexes, name);
                        }
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
                }
                else if (typeToScalar(removeConstFromType(type)) != AbaciValue::String) {
                    LogicError1(VariableNotList, name);
                }
                name += "[:]";
                if (&callElement != &assign.calls.back()) {
                    LogicError1(SliceNotAtEnd, name);
                }
                TypeEvalGen exprBegin(context, cache, locals);
                exprBegin(callSlice.at(0));
                if (typeToScalar(exprBegin.get()) != AbaciValue::Integer) {
                    LogicError1(SliceNotInt, name);
                }
                TypeEvalGen exprEnd(context, cache, locals);
                exprEnd(callSlice.at(1));
                if (typeToScalar(exprEnd.get()) != AbaciValue::Integer) {
                    LogicError1(SliceNotInt, name);
                }
                break;
            }
            default:
                LogicError0(BadCall);
                break;
        }
    }
    TypeEvalGen expr(context, cache, locals);
    expr(assign.value);
    if (!assign.calls.empty() && (assign.calls.back().call.index() == CallList::TypeIndexes || assign.calls.back().call.index() == CallList::TypeSlice)) {
        if (type != expr.get() && expr.get() != AbaciValue::None) {
            LogicError2(AssignMismatch, typeToString(type), typeToString(expr.get()));
        }
    }
    else if (type != expr.get()) {
        LogicError1(VariableType, name);
    }
}

template<>
void TypeCodeGen::codeGen(const IfStmt& ifStmt) const {
    TypeEvalGen expr(context, cache, locals);
    expr(ifStmt.condition);
    (*this)(ifStmt.trueBlock);
    (*this)(ifStmt.falseBlock);
}

template<>
void TypeCodeGen::codeGen(const WhileStmt& whileStmt) const {
    TypeEvalGen expr(context, cache, locals);
    expr(whileStmt.condition);
    (*this)(whileStmt.loopBlock);
}

template<>
void TypeCodeGen::codeGen(const RepeatStmt& repeatStmt) const {
    (*this)(repeatStmt.loopBlock);
    TypeEvalGen expr(context, cache, locals);
    expr(repeatStmt.condition);
}

template<>
void TypeCodeGen::codeGen(const CaseStmt& caseStmt) const {
    TypeEvalGen expr(context, cache, locals);
    expr(caseStmt.caseValue);
    switch (typeToScalar(removeConstFromType(expr.get()))) {
        case AbaciValue::Boolean:
        case AbaciValue::Integer:
        case AbaciValue::Floating:
        case AbaciValue::Complex:
        case AbaciValue::String:
            break;
        default:
            LogicError0(BadType);
    }
    for (const auto& when : caseStmt.matches) {
        TypeEvalGen expr(context, cache, locals);
        expr(when.expression);
        for (const auto& expression : when.expressions) {
            expr(expression);
        }
        (*this)(when.block);
    }
    if (!caseStmt.unmatched.statements.empty()) {
        (*this)(caseStmt.unmatched);
    }
}

template<>
void TypeCodeGen::codeGen(const Function& function) const {
    if (functionType != NotAFunction) {
        LogicError0(FunctionTopLevel);
    }
    cache->addFunctionTemplate(function.name, function.parameters, function.optionalReturnType, function.functionBody);
}

template<>
void TypeCodeGen::codeGen(const FunctionCall& functionCall) const {
    const auto functionType = cache->getCacheType(functionCall.name);
    switch (functionType) {
        case Cache::CacheFunction: {
            const auto& cacheFunction = cache->getFunction(functionCall.name);
            std::vector<Type> types;
            for (const auto& arg : functionCall.args) {
                TypeEvalGen expr(context, cache, locals);
                expr(arg);
                types.push_back(expr.get());
            }
            LocalSymbols params;
            for (auto argType = types.begin(); const auto& parameter : cacheFunction.parameters) {
                auto type = *argType++;
                params.add(parameter.name.name, nullptr, addConstToType(type));
            }
            cache->addFunctionInstantiation(functionCall.name, types, &params, context);
            break;
        }
        case Cache::CacheNativeFunction: {
            const auto& cacheNativeFunction = cache->getNativeFunction(functionCall.name);
            auto iter = cacheNativeFunction.parameterTypes.cbegin();
            for (const auto& arg : functionCall.args) {
                if (*iter == NativeType::none) {
                    LogicError2(ArgumentType, static_cast<int>(iter - cacheNativeFunction.parameterTypes.cbegin()), functionCall.name);
                }
                TypeEvalGen expr(context, cache, locals);
                expr(arg);
                if (typeToScalar(expr.get()) != nativeTypeToType(*iter)) {
                    LogicError2(ArgumentType, static_cast<int>(iter - cacheNativeFunction.parameterTypes.cbegin()), functionCall.name);
                }
                if (*iter == NativeType::i64star) {
                    auto list = std::dynamic_pointer_cast<TypeList>(std::get<std::shared_ptr<TypeBase>>(expr.get()));
                    if (list == nullptr) {
                        LogicError0(BadType);
                    }
                    else if (typeToScalar(list->elementType) != AbaciValue::Integer) {
                        LogicError1(BadListType, typeToString(expr.get()));
                    }
                }
                else if (*iter == NativeType::f64star) {
                    auto list = std::dynamic_pointer_cast<TypeList>(std::get<std::shared_ptr<TypeBase>>(expr.get()));
                    if (list == nullptr) {
                        LogicError0(BadType);
                    }
                    else if (typeToScalar(list->elementType) != AbaciValue::Integer) {
                        LogicError1(BadListType, typeToString(expr.get()));
                    }
                }
                if ((*iter == NativeType::i64star || *iter == NativeType::f64star) && isConstant(expr.get())) {
                    LogicError1(ListIsConst, typeToString(expr.get()));
                }
                ++iter;
            }
            break;
        }
        case Cache::CacheClass:
        case Cache::CacheNone:
            LogicError1(FunctionNotExist, functionCall.name);
    }
}

template<>
void TypeCodeGen::codeGen(const ReturnStmt& returnStmt) const {
    if (functionType == NotAFunction) {
        LogicError0(ReturnOnlyInFunction);
    }
    TypeEvalGen expr(context, cache, locals);
    expr(returnStmt.expression);
    auto result = expr.get();
    if (typeToScalar(result) != AbaciValue::None) {
        if (returnType.has_value() && returnType.value() != result) {
            LogicError0(FunctionTypeSet);
        }
        returnType = result;
    }
}

template<>
void TypeCodeGen::codeGen(const ExprFunction& expressionFunction) const {
    StmtList functionBody;
    functionBody.statements.emplace_back(ReturnStmt{ expressionFunction.expression });
    cache->addFunctionTemplate(expressionFunction.name, expressionFunction.parameters, expressionFunction.optionalReturnType, functionBody);
}

template<>
void TypeCodeGen::codeGen(const Class& classTemplate) const {
    std::vector<std::string> methodNames;
    for (const auto& method : classTemplate.methods) {
        methodNames.push_back(method.name);
        auto methodParameters = method.parameters;
        methodParameters.emplace(methodParameters.begin(), Variable{ THIS_V }, AbaciValue::None);
        cache->addFunctionTemplate(classTemplate.name + '.' + method.name, methodParameters, AbaciValue::None, method.functionBody);
    }
    cache->addClassTemplate(classTemplate.name, classTemplate.variables, methodNames);
}

template<>
void TypeCodeGen::codeGen(const MethodCall& methodCall) const {
    const std::string& name = methodCall.name.name;
    auto [ variables, index ] = locals ? locals->getIndex(name) : std::pair{ nullptr, LocalSymbols::noVariable };
    Type type;
    if (index != LocalSymbols::noVariable) {
        type = variables->getType(index);
        if (isConstant(type)) {
            LogicError1(NoConstantAssign, name);
        }
        for (const auto& member : methodCall.memberList) {
            if (typeToScalar(removeConstFromType(type)) != AbaciValue::Instance) {
                LogicError1(BadObject, name);
            }
            auto instanceType = std::dynamic_pointer_cast<TypeInstance>(std::get<std::shared_ptr<TypeBase>>(type));
            Assert(instanceType != nullptr);
            auto index = cache->getMemberIndex(cache->getClass(instanceType->className), member);
            type = instanceType->variableTypes.at(index);
        }
    }
    else {
        auto globalIndex = context->globals->getIndex(name);
        if (globalIndex == GlobalSymbols::noVariable) {
            LogicError1(VariableNotExist, (name == THIS_V) ? THIS : name);
        }
        type = context->globals->getType(globalIndex);
        if (isConstant(type)) {
            LogicError1(NoConstantAssign, name);
        }
        for (const auto& member : methodCall.memberList) {
            if (typeToScalar(removeConstFromType(type)) != AbaciValue::Instance) {
                LogicError1(BadObject, name);
            }
            auto instanceType = std::dynamic_pointer_cast<TypeInstance>(std::get<std::shared_ptr<TypeBase>>(type));
            Assert(instanceType != nullptr);
            auto index = cache->getMemberIndex(cache->getClass(instanceType->className), member);
            type = instanceType->variableTypes.at(index);
        }
    }
    if (typeToScalar(removeConstFromType(type)) == AbaciValue::Instance) {
        auto instanceType = std::dynamic_pointer_cast<TypeInstance>(std::get<std::shared_ptr<TypeBase>>(type));
        Assert(instanceType != nullptr);
        std::string methodName = instanceType->className + '.' + methodCall.method;
        const auto& cacheFunction = cache->getFunction(methodName);
        std::vector<Type> types;
        types.push_back(type);
        for (const auto& arg : methodCall.args) {
            TypeEvalGen expr(context, cache, locals);
            expr(arg);
            types.push_back(expr.get());
        }
        LocalSymbols params;
        params.add(THIS_V, nullptr, instanceType);
        for (auto argType = types.begin(); const auto& parameter : cacheFunction.parameters) {
            auto type = *argType++;
            params.add(parameter.name.name, nullptr, addConstToType(type));
        }
        cache->addFunctionInstantiation(methodName, types, &params, context);
    }
    else {
        LogicError1(BadObject, name);
    }
}

template<>
void TypeCodeGen::codeGen(const NativeFunction& nativeFn) const {
    if (auto sep = nativeFn.libraryFunction.find(DOT); sep == std::string::npos) {
        cache->addNativeFunction("", nativeFn.libraryFunction, nativeFn.params, nativeFn.result);
    }
    else {
        cache->addNativeFunction(nativeFn.libraryFunction.substr(0, sep), nativeFn.libraryFunction.substr(sep + std::string(DOT).size()), nativeFn.params, nativeFn.result);
    }
}

template<>
void TypeCodeGen::codeGen([[maybe_unused]] const ExpressionStmt& expressionStmt) const {
    LogicError0(NoExpression);
}

void TypeCodeGen::operator()(const StmtNode& stmt) const {
    std::visit([this](const auto& nodeType){
        this->codeGen(nodeType);
    }, stmt.data);
}

} // namespace abaci::codegen
