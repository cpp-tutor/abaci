#ifndef Expr_hpp
#define Expr_hpp

#include "utility/Type.hpp"
#include "utility/Operator.hpp"
#include "utility/Constant.hpp"
#include <boost/spirit/home/x3/support/ast/position_tagged.hpp>
#include <boost/fusion/adapted/struct.hpp>
#include <boost/container/vector.hpp>
#include <vector>
#include <variant>
#include <utility>
#include <memory>

namespace abaci::ast {

using boost::spirit::x3::position_tagged;
using abaci::utility::AbaciValue;
using abaci::utility::Constants;
using abaci::utility::Operator;

struct ExprNode;

using ExprList = std::vector<ExprNode>;
using ExprPair = boost::container::vector<ExprNode>;

struct Variable : position_tagged {
    Variable() = default;
    Variable(const std::string& name) : name{ name } {}
    bool operator==(const Variable& other) const {
        return name == other.name;
    }
    std::string name;
};

struct FunctionValueCall : position_tagged {
    std::string name;
    ExprList args;
};

struct CallList {
    enum Type { TypeVariable, TypeIndexes, TypeSlice, TypeFunction };
    std::variant<Variable,ExprList,ExprPair,FunctionValueCall> call;
};

struct MultiCall : position_tagged {
    Variable name;
    std::vector<CallList> calls;
};

struct UserInput : position_tagged {
    std::string dummy;
};

struct TypeConv {
    AbaciValue::Type toType;
    std::shared_ptr<ExprNode> expression;
};

struct List {
    std::shared_ptr<ExprList> elements;
    std::string elementType;
};

struct ExprNode : position_tagged {
    enum Association { Unset, Left, Right, Unary, Boolean };
    std::variant<std::monostate,std::size_t,Operator,std::pair<Association,ExprList>,FunctionValueCall,MultiCall,UserInput,TypeConv,List> data;
};

struct TypeConvItems : position_tagged {
    std::string toType;
    ExprNode expression;
};

struct ListItems : position_tagged {
    ExprNode firstElement;
    ExprList otherElements;
};

struct EmptyListItems : position_tagged {
    std::string elementType;
};

} // namespace abaci::ast

BOOST_FUSION_ADAPT_STRUCT(abaci::ast::ExprNode, data)
BOOST_FUSION_ADAPT_STRUCT(abaci::ast::Variable, name)
BOOST_FUSION_ADAPT_STRUCT(abaci::ast::FunctionValueCall, name, args)
BOOST_FUSION_ADAPT_STRUCT(abaci::ast::UserInput, dummy)
BOOST_FUSION_ADAPT_STRUCT(abaci::ast::TypeConvItems, toType, expression)
BOOST_FUSION_ADAPT_STRUCT(abaci::ast::ListItems, firstElement, otherElements)
BOOST_FUSION_ADAPT_STRUCT(abaci::ast::EmptyListItems, elementType)
BOOST_FUSION_ADAPT_STRUCT(abaci::ast::CallList, call)
BOOST_FUSION_ADAPT_STRUCT(abaci::ast::MultiCall, name, calls)

#endif
