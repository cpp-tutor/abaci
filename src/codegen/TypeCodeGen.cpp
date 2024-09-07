#include "TypeCodeGen.hpp"
#include "localize/Messages.hpp"
#include "localize/Keywords.hpp"
#include <llvm/IR/Constants.h>

using namespace llvm;

namespace abaci::codegen {

using abaci::ast::FunctionValueCall;
using abaci::ast::DataMember;
using abaci::ast::MethodValueCall;
using abaci::ast::TypeConv;
using abaci::ast::ExprNode;
using abaci::ast::ExprList;
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
using abaci::utility::isConstant;
using abaci::utility::removeConstFromType;
using abaci::utility::addConstToType;
using abaci::utility::TypeInstance;
using abaci::utility::TypeBase;
using abaci::utility::ValidConversions;

void TypeEvalGen::operator()(const ExprNode& node) const {
    switch (node.data.index()) {
        case ExprNode::ValueNode:
            push(context->constants->getType(std::get<ExprNode::ValueNode>(node.data)));
            break;
        case ExprNode::ListNode: {
            switch (std::get<ExprNode::ListNode>(node.data).first) {
                case ExprNode::Left: {
                    const auto& expr = std::get<ExprNode::ListNode>(node.data).second;
                    (*this)(expr.front());
                    auto type = pop();
                    for (auto iter = ++expr.begin(); iter != expr.end();) {
                        auto op = std::get<Operator>(iter++->data);
                        (*this)(*iter++);
                        auto new_type = promote(type, pop());
                        if (new_type == AbaciValue::Integer && op == Operator::Divide) {
                            new_type = AbaciValue::Floating;
                        }
                        type = new_type;
                    }
                    push(type);
                    break;
                }
                case ExprNode::Right: {
                    const auto& expr = std::get<ExprNode::ListNode>(node.data).second;
                    (*this)(expr.back());
                    auto type = pop();
                    for (auto iter = ++expr.rbegin(); iter != expr.rend();) {
                        ++iter;
                        (*this)(*iter++);
                        type = promote(type, AbaciValue::Floating);
                        type = promote(type, pop());
                    }
                    push(type);
                    break;
                }
                case ExprNode::Unary: {
                    const auto& expr = std::get<ExprNode::ListNode>(node.data).second;
                    (*this)(expr.back());
                    auto type = pop();
                    for (auto iter = ++expr.rbegin(); iter != expr.rend();) {
                        auto op = std::get<Operator>(iter++->data);
                        if (op == Operator::Not) {
                            type = AbaciValue::Boolean;
                        }
                    }
                    push(type);
                    break;
                }
                case ExprNode::Boolean: {
                    const auto& expr = std::get<ExprNode::ListNode>(node.data).second;
                    (*this)(expr.front());
                    auto type = pop();
                    if (expr.size() > 1) {
                        type = AbaciValue::Boolean;
                        for (auto iter = ++expr.begin(); iter != expr.end();) {
                            ++iter;
                            (*this)(*iter++);
                        }
                    }
                    push(type);
                    break;
                }
                default:
                    UnexpectedError0(BadAssociation);
            }
            break;
        }
        case ExprNode::VariableNode: {
            const std::string& name = std::get<ExprNode::VariableNode>(node.data).get();
            if (locals) {
                if (auto [ vars, index ] = locals->getIndex(name); index != LocalSymbols::noVariable) {
                    push(removeConstFromType(vars->getType(index)));
                    return;
                }
            }
            if (auto globalIndex = context->globals->getIndex(name); globalIndex != GlobalSymbols::noVariable) {
                push(removeConstFromType(context->globals->getType(globalIndex)));
            }
            else {
                LogicError1(VarNotExist, name);
            }
            break;
        }
        case ExprNode::FunctionNode: {
            const auto& call = std::get<ExprNode::FunctionNode>(node.data);
            switch (cache->getCacheType(call.name)) {
                case Cache::CacheFunction: {
                    const auto& cache_function = cache->getFunction(call.name);
                    std::vector<Type> types;
                    for (const auto& arg : call.args) {
                        TypeEvalGen expr(context, cache, locals);
                        expr(arg);
                        types.push_back(expr.get());
                    }
                    LocalSymbols params;
                    for (auto arg_type = types.begin(); const auto& parameter : cache_function.parameters) {
                        auto type = *arg_type++;
                        params.add(parameter.get(), nullptr, addConstToType(type));
                    }
                    cache->addFunctionInstantiation(call.name, types, &params, context);
                    auto return_type = cache->getFunctionInstantiationType(call.name, types);
                    push(return_type);
                    break;
                }
                case Cache::CacheClass: {
                    auto object = std::make_shared<TypeInstance>();
                    object->className = call.name;
                    for (const auto& arg : call.args) {
                        TypeEvalGen expr(context, cache, locals);
                        expr(arg);
                        object->variableTypes.push_back(expr.get());
                    }
                    push(object);
                    break;
                }
                default:
                    LogicError1(CallableNotExist, call.name);
            }
            break;
        }
        case ExprNode::DataMemberNode: {
            const auto& data = std::get<ExprNode::DataMemberNode>(node.data);
            const std::string& name = data.name.get();
            Type type;
            if (locals) {
                if (auto [ vars, index ] = locals->getIndex(name); index != LocalSymbols::noVariable) {
                    type = vars->getType(index);
                    for (const auto& member : data.member_list) {
                        if (typeToScalar(removeConstFromType(type)) != AbaciValue::Instance) {
                            LogicError0(BadObject);
                        }
                        auto instanceType = std::dynamic_pointer_cast<TypeInstance>(std::get<std::shared_ptr<TypeBase>>(type));
                        Assert(instanceType != nullptr);
                        auto index = cache->getMemberIndex(cache->getClass(instanceType->className), member);
                        type = instanceType->variableTypes.at(index);
                    }
                    push(removeConstFromType(type));
                    return;
                }
            }
            if (auto globalIndex = context->globals->getIndex(name); globalIndex != GlobalSymbols::noVariable) {
                type = context->globals->getType(globalIndex);
                for (const auto& member : data.member_list) {
                    if (typeToScalar(removeConstFromType(type)) != AbaciValue::Instance) {
                        LogicError0(BadObject);
                    }
                    auto instanceType = std::dynamic_pointer_cast<TypeInstance>(std::get<std::shared_ptr<TypeBase>>(type));
                    Assert(instanceType != nullptr);
                    auto index = cache->getMemberIndex(cache->getClass(instanceType->className), member);
                    type = instanceType->variableTypes.at(index);
                }
                push(removeConstFromType(type));
            }
            else {
                if (name == THIS_V) {
                    if (0 /*functionType != Method*/) {
                        LogicError1(VarNotExist, THIS);        
                    }
                }
                else {
                    LogicError1(VarNotExist, name);
                }
            }
            break;
        }
        case ExprNode::MethodNode: {
            const auto& method_call = std::get<ExprNode::MethodNode>(node.data);
            const std::string& name = method_call.name.get();
            auto [ vars, index ] = locals ? locals->getIndex(name) : std::pair{ nullptr, LocalSymbols::noVariable };
            Type type;
            if (index != LocalSymbols::noVariable) {
                type = vars->getType(index);
                if (isConstant(type)) {
                    LogicError1(NoConstantAssign, name);
                }
                for (const auto& member : method_call.member_list) {
                    if (typeToScalar(removeConstFromType(type)) != AbaciValue::Instance) {
                        LogicError0(BadObject);
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
                    LogicError1(VarNotExist, (name == THIS_V) ? THIS : name);
                }
                type = context->globals->getType(globalIndex);
                if (isConstant(type)) {
                    LogicError1(NoConstantAssign, name);
                }
                for (const auto& member : method_call.member_list) {
                    if (typeToScalar(removeConstFromType(type)) != AbaciValue::Instance) {
                        LogicError0(BadObject);
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
                std::string method_name = instanceType->className + '.' + method_call.method;
                const auto& cache_function = cache->getFunction(method_name);
                std::vector<Type> types;
                for (const auto& arg : method_call.args) {
                    TypeEvalGen expr(context, cache, locals);
                    expr(arg);
                    types.push_back(expr.get());
                }
                LocalSymbols params;
                for (auto arg_type = types.begin(); const auto& parameter : cache_function.parameters) {
                    auto type = *arg_type++;
                    params.add(parameter.get(), nullptr, addConstToType(type));
                }
                cache->addFunctionInstantiation(method_name, types, &params, context, true, type);
                auto return_type = cache->getFunctionInstantiationType(method_name, types);
                push(return_type);
            }
            else {
                LogicError0(BadObject);
            }
            break;
        }
        case ExprNode::InputNode:
            push(AbaciValue::String);
            break;
        case ExprNode::TypeConvNode: {
            auto target_type = std::get<ExprNode::TypeConvNode>(node.data).to_type;
            auto expression = std::get<ExprNode::TypeConvNode>(node.data).expression;
            TypeEvalGen expr(context, cache, locals);
            expr(*expression);
            if (auto iter = ValidConversions.find(target_type); iter != ValidConversions.end()) {
                if (std::find(iter->second.begin(), iter->second.end(), expr.get()) == iter->second.end()) {
                    LogicError0(BadConvType);
                }
            }
            else {
                LogicError1(BadConvTarget, static_cast<int>(typeToScalar(target_type)));
            }
            if (typeToScalar(target_type) == AbaciValue::Real || typeToScalar(target_type) == AbaciValue::Imag) {
                target_type = AbaciValue::Floating;
            }
            push(target_type);
            break;
        }
        default:
            UnexpectedError0(BadNode);
    }
}

AbaciValue::Type TypeEvalGen::promote(const Type& type_operand1, const Type& type_operand2) const {
    if (!std::holds_alternative<AbaciValue::Type>(type_operand1)
        || !std::holds_alternative<AbaciValue::Type>(type_operand2)) {
        LogicError0(NoObject);
    }
    auto operand1 = std::get<AbaciValue::Type>(type_operand1), operand2 = std::get<AbaciValue::Type>(type_operand2);
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

void TypeCodeGen::operator()(const abaci::ast::StmtList& stmts) const {
    LocalSymbols *enclosing = locals;
    Temporaries *enclosingTemps = temps;
    if (!stmts.empty()) {
        locals = const_cast<LocalSymbols*>(static_cast<const LocalSymbols*>(&stmts));
        temps = const_cast<Temporaries*>(static_cast<const Temporaries*>(&stmts));
        Assert(locals->size() == 0);
        Assert(temps->size() == 0);
        locals->setEnclosing(enclosing);
        temps->setEnclosing(enclosingTemps);
        for (const auto& stmt : stmts) {
            if (dynamic_cast<const ReturnStmt*>(stmt.get()) && &stmt != &stmts.back()) {
                LogicError0(ReturnAtEnd);
            }
            (*this)(stmt);
        }
    }
    locals = enclosing;
    temps = enclosingTemps;
}

template<>
void TypeCodeGen::codeGen([[maybe_unused]] const CommentStmt& comment) const {
}

template<>
void TypeCodeGen::codeGen(const PrintStmt& print) const {
    PrintList print_data{ print.expression };
    print_data.insert(print_data.end(), print.format.begin(), print.format.end());
    for (auto field : print_data) {
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
        if (locals->getIndex(define.name.get(), true).second != LocalSymbols::noVariable) {
            LogicError1(VarExists, define.name.get());
        }
        else if (define.assign == Operator::Equal) {
            locals->add(define.name.get(), nullptr, addConstToType(expr.get()));
        }
        else {
            locals->add(define.name.get(), nullptr, expr.get());
        }
    }
    else {
        if (context->globals->getIndex(define.name.get()) != GlobalSymbols::noVariable) {
            LogicError1(VarExists, define.name.get());
        }
        else if (define.assign == Operator::Equal) {
            context->globals->add(define.name.get(), addConstToType(expr.get()));
        }
        else {
            context->globals->add(define.name.get(), expr.get());
        }
    }
}

template<>
void TypeCodeGen::codeGen(const AssignStmt& assign) const {
    TypeEvalGen expr(context, cache, locals);
    expr(assign.value);
    auto [ vars, index ] = locals ? locals->getIndex(assign.name.get()) : std::pair{ nullptr, LocalSymbols::noVariable };
    if (index != LocalSymbols::noVariable) {
        if (isConstant(vars->getType(index))) {
            LogicError1(NoConstantAssign, assign.name.get());
        }
        else if (vars->getType(index) != expr.get()) {
            LogicError1(VarType, assign.name.get());
        }
    }
    else {
        auto globalIndex = context->globals->getIndex(assign.name.get());
        if (globalIndex == GlobalSymbols::noVariable) {
            LogicError1(VarNotExist, assign.name.get());
        }
        else if (isConstant(context->globals->getType(globalIndex))) {
            LogicError1(NoConstantAssign, assign.name.get());
        }
        else if (context->globals->getType(globalIndex) != expr.get()) {
            LogicError1(VarType, assign.name.get());
        }
    }
}

template<>
void TypeCodeGen::codeGen(const IfStmt& if_stmt) const {
    TypeEvalGen expr(context, cache, locals);
    expr(if_stmt.condition);
    (*this)(if_stmt.true_test);
    (*this)(if_stmt.false_test);
}

template<>
void TypeCodeGen::codeGen(const WhileStmt& while_stmt) const {
    TypeEvalGen expr(context, cache, locals);
    expr(while_stmt.condition);
    (*this)(while_stmt.loop_block);
}

template<>
void TypeCodeGen::codeGen(const RepeatStmt& repeat_stmt) const {
    (*this)(repeat_stmt.loop_block);
    TypeEvalGen expr(context, cache, locals);
    expr(repeat_stmt.condition);
}

template<>
void TypeCodeGen::codeGen(const CaseStmt& case_stmt) const {
    TypeEvalGen expr(context, cache, locals);
    expr(case_stmt.case_value);
    for (const auto& when : case_stmt.matches) {
        TypeEvalGen expr(context, cache, locals);
        expr(when.expression);
        (*this)(when.block);
    }
    if (!case_stmt.unmatched.empty()) {
        (*this)(case_stmt.unmatched);
    }
}

template<>
void TypeCodeGen::codeGen(const Function& function) const {
    if (functionType != NotFunction) {
        LogicError0(FuncTopLevel);
    }
    cache->addFunctionTemplate(function.name, function.parameters, function.function_body);
}

template<>
void TypeCodeGen::codeGen(const FunctionCall& function_call) const {
    const auto& cache_function = cache->getFunction(function_call.name);
    std::vector<Type> types;
    for (const auto& arg : function_call.args) {
        TypeEvalGen expr(context, cache, locals);
        expr(arg);
        types.push_back(expr.get());
    }
    LocalSymbols params;
    for (auto arg_type = types.begin(); const auto& parameter : cache_function.parameters) {
        auto type = *arg_type++;
        params.add(parameter.get(), nullptr, addConstToType(type));
    }
    cache->addFunctionInstantiation(function_call.name, types, &params, context);
}

template<>
void TypeCodeGen::codeGen(const ReturnStmt& return_stmt) const {
    if (functionType == NotFunction) {
        LogicError0(ReturnOnlyInFunc);
    }
    TypeEvalGen expr(context, cache, locals);
    expr(return_stmt.expression);
    auto result = expr.get();
    if (typeToScalar(result) != AbaciValue::None) {
        if (return_type.has_value() && return_type.value() != result) {
            LogicError0(FuncTypeSet);
        }
        return_type = result;
    }
}

template<>
void TypeCodeGen::codeGen(const ExprFunction& expression_function) const {
    StmtList function_body;
    function_body.emplace_back(new ReturnStmt{ expression_function.expression });
    cache->addFunctionTemplate(expression_function.name, expression_function.parameters, function_body);
}

template<>
void TypeCodeGen::codeGen(const Class& class_template) const {
    std::vector<std::string> method_names;
    for (const auto& method : class_template.methods) {
        method_names.push_back(method.name);
        cache->addFunctionTemplate(class_template.name + '.' + method.name, method.parameters, method.function_body);
    }
    cache->addClassTemplate(class_template.name, class_template.variables, method_names);
}

template<>
void TypeCodeGen::codeGen(const DataAssignStmt& data_assign) const {
    const std::string& name = data_assign.name.get();
    TypeEvalGen expr(context, cache, locals);
    expr(data_assign.value);
    auto [ vars, index ] = locals ? locals->getIndex(name) : std::pair{ nullptr, LocalSymbols::noVariable };
    Type type;
    if (index != LocalSymbols::noVariable) {
        type = vars->getType(index);
        if (isConstant(type)) {
            LogicError1(NoConstantAssign, name);
        }
        for (const auto& member : data_assign.member_list) {
            if (typeToScalar(removeConstFromType(type)) != AbaciValue::Instance) {
                LogicError0(BadObject);
            }
            auto instanceType = std::dynamic_pointer_cast<TypeInstance>(std::get<std::shared_ptr<TypeBase>>(type));
            Assert(instanceType != nullptr);
            auto index = cache->getMemberIndex(cache->getClass(instanceType->className), member);
            type = instanceType->variableTypes.at(index);
        }
        if (type != expr.get()) {
            LogicError0(DataType);
        }
    }
    else {
        auto globalIndex = context->globals->getIndex(name);
        if (globalIndex == GlobalSymbols::noVariable) {
            LogicError1(VarNotExist, (name == THIS_V) ? THIS : name);
        }
        else if (isConstant(context->globals->getType(globalIndex))) {
            LogicError1(NoConstantAssign, name);
        }
        type = context->globals->getType(globalIndex);
        for (const auto& member : data_assign.member_list) {
            if (typeToScalar(removeConstFromType(type)) != AbaciValue::Instance) {
                LogicError0(BadObject);
            }
            auto instanceType = std::dynamic_pointer_cast<TypeInstance>(std::get<std::shared_ptr<TypeBase>>(type));
            Assert(instanceType != nullptr);
            auto index = cache->getMemberIndex(cache->getClass(instanceType->className), member);
            type = instanceType->variableTypes.at(index);
        }
        if (type != expr.get()) {
            LogicError0(DataType);
        }
    }
}

template<>
void TypeCodeGen::codeGen(const MethodCall& method_call) const {
    const std::string& name = method_call.name.get();
    auto [ vars, index ] = locals ? locals->getIndex(name) : std::pair{ nullptr, LocalSymbols::noVariable };
    Type type;
    if (index != LocalSymbols::noVariable) {
        type = vars->getType(index);
        if (isConstant(type)) {
            LogicError1(NoConstantAssign, name);
        }
        for (const auto& member : method_call.member_list) {
            if (typeToScalar(removeConstFromType(type)) != AbaciValue::Instance) {
                LogicError0(BadObject);
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
            LogicError1(VarNotExist, (name == THIS_V) ? THIS : name);
        }
        type = context->globals->getType(globalIndex);
        if (isConstant(type)) {
            LogicError1(NoConstantAssign, name);
        }
        for (const auto& member : method_call.member_list) {
            if (typeToScalar(removeConstFromType(type)) != AbaciValue::Instance) {
                LogicError0(BadObject);
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
        std::string method_name = instanceType->className + '.' + method_call.method;
        const auto& cache_function = cache->getFunction(method_name);
        std::vector<Type> types;
        for (const auto& arg : method_call.args) {
            TypeEvalGen expr(context, cache, locals);
            expr(arg);
            types.push_back(expr.get());
        }
        LocalSymbols params;
        for (auto arg_type = types.begin(); const auto& parameter : cache_function.parameters) {
            auto type = *arg_type++;
            params.add(parameter.get(), nullptr, addConstToType(type));
        }
        cache->addFunctionInstantiation(method_name, types, &params, context, true, type);
    }
    else {
        LogicError0(BadObject);
    }
}

template<>
void TypeCodeGen::codeGen([[maybe_unused]] const ExpressionStmt& expression_stmt) const {
    LogicError0(NoExpression);
}

void TypeCodeGen::operator()(const StmtNode& stmt) const {
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
