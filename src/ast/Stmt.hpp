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
using boost::spirit::x3::position_tagged;

class StmtNode;

struct StmtList : std::vector<StmtNode>, LocalSymbols, Temporaries {};

using abaci::utility::Operator;

struct StmtData : position_tagged {
    virtual ~StmtData() {}
};

class StmtNode {
public:
    StmtNode() = default;
    StmtNode(const StmtNode&) = default;
    StmtNode& operator=(const StmtNode&) = default;
    StmtNode(StmtData *nodeImpl) { data.reset(nodeImpl); }
    const StmtData *get() const { return data.get(); }
private:
    std::shared_ptr<StmtData> data;
};

struct CommentStmt : StmtData {
    std::string commentString;
};

using PrintList = std::vector<std::variant<ExprNode,Operator>>;

struct PrintStmt : StmtData {
    ExprNode expression;
    PrintList format;
};

struct InitStmt : StmtData {
    Variable name;
    Operator assign;
    ExprNode value;
};

struct AssignStmt : StmtData {
    Variable name;
    ExprNode value;
};

struct IfStmt : StmtData {
    ExprNode condition;
    StmtList trueBlock;
    StmtList falseBlock;
};

struct WhileStmt : StmtData {
    ExprNode condition;
    StmtList loopBlock;
};

struct RepeatStmt : StmtData {
    StmtList loopBlock;
    ExprNode condition;
};

struct WhenStmt {
    ExprNode expression;
    StmtList block;
};

struct WhenList : std::vector<WhenStmt>, position_tagged {};

struct CaseStmt : StmtData {
    ExprNode caseValue;
    WhenList matches;
    StmtList unmatched;
};

struct Function : StmtData {
    std::string name;
    std::vector<Variable> parameters;
    StmtList functionBody;
};

struct FunctionCall : StmtData {
    std::string name;
    ExprList args;
};

struct ReturnStmt : StmtData {
    ReturnStmt() = default;
    explicit ReturnStmt(const ExprNode& expression) : expression{ expression } {}
    ExprNode expression;
};

struct ExprFunction : StmtData {
    std::string name;
    std::vector<Variable> parameters;
    ExprNode expression;
};

using FunctionList = std::vector<Function>;

struct Class : StmtData {
    std::string name;
    std::vector<Variable> variables;
    FunctionList methods;    
};

struct DataAssignStmt : StmtData {
    Variable name;
    std::vector<Variable> memberList;
    ExprNode value;
};

struct MethodCall : StmtData {
    Variable name;
    std::vector<Variable> memberList;
    std::string method;
    ExprList args;
};

struct ExpressionStmt : StmtData {
    ExprNode expression;
};

struct ListInitStmt : StmtData {
    Variable name;
    Operator assign;
    std::string type;
    ExprNode firstElement;
    ExprList otherElements;
};

struct ListAssignStmt : StmtData {
    Variable name;
    ExprList indexes;
    ExprNode value;
};

struct DataListAssignStmt : StmtData {
    Variable name;
    std::vector<Variable> memberList;
    ExprList indexes;
    ExprNode value;
};

} // namespace abaci::ast

BOOST_FUSION_ADAPT_STRUCT(abaci::ast::CommentStmt, commentString)
BOOST_FUSION_ADAPT_STRUCT(abaci::ast::PrintStmt, expression, format)
BOOST_FUSION_ADAPT_STRUCT(abaci::ast::InitStmt, name, assign, value)
BOOST_FUSION_ADAPT_STRUCT(abaci::ast::AssignStmt, name, value)
BOOST_FUSION_ADAPT_STRUCT(abaci::ast::IfStmt, condition, trueBlock, falseBlock)
BOOST_FUSION_ADAPT_STRUCT(abaci::ast::WhileStmt, condition, loopBlock)
BOOST_FUSION_ADAPT_STRUCT(abaci::ast::RepeatStmt, loopBlock, condition)
BOOST_FUSION_ADAPT_STRUCT(abaci::ast::WhenStmt, expression, block)
BOOST_FUSION_ADAPT_STRUCT(abaci::ast::CaseStmt, caseValue, matches, unmatched)
BOOST_FUSION_ADAPT_STRUCT(abaci::ast::Function, name, parameters, functionBody)
BOOST_FUSION_ADAPT_STRUCT(abaci::ast::FunctionCall, name, args)
BOOST_FUSION_ADAPT_STRUCT(abaci::ast::ReturnStmt, expression)
BOOST_FUSION_ADAPT_STRUCT(abaci::ast::ExprFunction, name, parameters, expression)
BOOST_FUSION_ADAPT_STRUCT(abaci::ast::Class, name, variables, methods)
BOOST_FUSION_ADAPT_STRUCT(abaci::ast::DataAssignStmt, name, memberList, value)
BOOST_FUSION_ADAPT_STRUCT(abaci::ast::MethodCall, name, memberList, method, args)
BOOST_FUSION_ADAPT_STRUCT(abaci::ast::ExpressionStmt, expression)
BOOST_FUSION_ADAPT_STRUCT(abaci::ast::ListInitStmt, name, assign, type, firstElement, otherElements)
BOOST_FUSION_ADAPT_STRUCT(abaci::ast::ListAssignStmt, name, indexes, value)
BOOST_FUSION_ADAPT_STRUCT(abaci::ast::DataListAssignStmt, name, memberList, indexes, value)

#endif
