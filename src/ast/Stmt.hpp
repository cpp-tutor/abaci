#ifndef Stmt_hpp
#define Stmt_hpp

#include "Expr.hpp"
#include "utility/Symbol.hpp"
#include "utility/Temporary.hpp"
#include <boost/fusion/adapted/struct.hpp>
#include <boost/spirit/home/x3/support/ast/position_tagged.hpp>
#include <memory>
#include <vector>
#include <variant>

namespace abaci::ast {

using abaci::utility::LocalSymbols;
using abaci::utility::Temporaries;
using abaci::utility::NativeType;
using boost::spirit::x3::position_tagged;

struct StmtNode;

struct StmtList : LocalSymbols, Temporaries {
    std::vector<StmtNode> statements;
};

using abaci::utility::Operator;

struct CommentStmt {
    std::string commentString;
};

using PrintList = std::vector<std::variant<ExprNode,Operator>>;

struct PrintStmt {
    ExprNode expression;
    ExprList format;
    Operator trailing;
};

struct InitStmt {
    Variable name;
    Operator assign;
    ExprNode value;
};

struct AssignStmt {
    Variable name;
    std::vector<CallList> calls;
    ExprNode value;
};

struct IfStmt {
    ExprNode condition;
    StmtList trueBlock;
    StmtList falseBlock;
};

struct WhileStmt {
    ExprNode condition;
    StmtList loopBlock;
};

struct RepeatStmt {
    StmtList loopBlock;
    ExprNode condition;
};

struct WhenStmt {
    ExprNode expression;
    std::vector<ExprNode> expressions;
    StmtList block;
};

using WhenList = std::vector<WhenStmt>;

struct CaseStmt {
    ExprNode caseValue;
    WhenList matches;
    StmtList unmatched;
};

struct Function {
    std::string name;
    std::vector<Variable> parameters;
    StmtList functionBody;
};

struct FunctionCall {
    std::string name;
    ExprList args;
};

struct ReturnStmt {
    ReturnStmt() = default;
    explicit ReturnStmt(const ExprNode& expression) : expression{ expression } {}
    ExprNode expression;
};

struct ExprFunction {
    std::string name;
    std::vector<Variable> parameters;
    ExprNode expression;
};

using FunctionList = std::vector<Function>;

struct Class {
    std::string name;
    std::vector<Variable> variables;
    FunctionList methods;    
};

struct MethodCall {
    Variable name;
    std::vector<Variable> memberList;
    std::string method;
    ExprList args;
};

struct NativeFunction {
    std::string libraryFunction;
    std::vector<NativeType> params;
    NativeType result;
};

struct ExpressionStmt {
    ExprNode expression;
};

struct StmtNode {
    StmtNode() = default;
    template<typename T>
    explicit StmtNode(const T& data) : data{ data } {} 
    std::variant<CommentStmt,PrintStmt,InitStmt,AssignStmt,IfStmt,WhileStmt,RepeatStmt,CaseStmt,Function,FunctionCall,ReturnStmt,ExprFunction,Class,MethodCall,NativeFunction,ExpressionStmt> data;
};

} // namespace abaci::ast

BOOST_FUSION_ADAPT_STRUCT(abaci::ast::CommentStmt, commentString)
BOOST_FUSION_ADAPT_STRUCT(abaci::ast::PrintStmt, expression, format, trailing)
BOOST_FUSION_ADAPT_STRUCT(abaci::ast::InitStmt, name, assign, value)
BOOST_FUSION_ADAPT_STRUCT(abaci::ast::AssignStmt, name, calls, value)
BOOST_FUSION_ADAPT_STRUCT(abaci::ast::IfStmt, condition, trueBlock, falseBlock)
BOOST_FUSION_ADAPT_STRUCT(abaci::ast::WhileStmt, condition, loopBlock)
BOOST_FUSION_ADAPT_STRUCT(abaci::ast::RepeatStmt, loopBlock, condition)
BOOST_FUSION_ADAPT_STRUCT(abaci::ast::WhenStmt, expression, expressions, block)
BOOST_FUSION_ADAPT_STRUCT(abaci::ast::CaseStmt, caseValue, matches, unmatched)
BOOST_FUSION_ADAPT_STRUCT(abaci::ast::Function, name, parameters, functionBody)
BOOST_FUSION_ADAPT_STRUCT(abaci::ast::FunctionCall, name, args)
BOOST_FUSION_ADAPT_STRUCT(abaci::ast::ReturnStmt, expression)
BOOST_FUSION_ADAPT_STRUCT(abaci::ast::ExprFunction, name, parameters, expression)
BOOST_FUSION_ADAPT_STRUCT(abaci::ast::Class, name, variables, methods)
BOOST_FUSION_ADAPT_STRUCT(abaci::ast::MethodCall, name, memberList, method, args)
BOOST_FUSION_ADAPT_STRUCT(abaci::ast::NativeFunction, libraryFunction, params, result)
BOOST_FUSION_ADAPT_STRUCT(abaci::ast::ExpressionStmt, expression)
BOOST_FUSION_ADAPT_STRUCT(abaci::ast::StmtList, statements)

#endif
