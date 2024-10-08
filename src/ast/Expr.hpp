#ifndef Expr_hpp
#define Expr_hpp

#include "utility/Type.hpp"
#include "utility/Operator.hpp"
#include "utility/Constant.hpp"
#include <boost/spirit/home/x3/support/ast/position_tagged.hpp>
#include <boost/fusion/adapted/struct.hpp>
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

struct DataMember : position_tagged {
    Variable name;
    std::vector<Variable> memberList;
};

struct MethodValueCall : position_tagged {
    Variable name;
    std::vector<Variable> memberList;
    std::string method;
    ExprList args;
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

struct ListIndex : position_tagged {
    Variable name;
    ExprList indexes;
};

struct DataListIndex : position_tagged {
    Variable name;
    std::vector<Variable> memberList;
    ExprList indexes;
};

struct ExprNode : position_tagged {
    enum Association { Unset, Left, Right, Unary, Boolean };
    std::variant<std::monostate,std::size_t,Operator,std::pair<Association,ExprList>,Variable,FunctionValueCall,DataMember,MethodValueCall,UserInput,TypeConv,List,ListIndex,DataListIndex> data;
};

struct TypeConvItems : position_tagged {
    std::string toType;
    ExprNode expression;
};

struct ListItems : position_tagged {
#ifdef ABACI_USE_OLDER_BOOST
    ExprNode firstElement;
    ExprList otherElements;
#else
    ExprList elements;
#endif
};

struct EmptyListItems : position_tagged {
    std::string elementType;
};

} // namespace abaci::ast

BOOST_FUSION_ADAPT_STRUCT(abaci::ast::ExprNode, data)
BOOST_FUSION_ADAPT_STRUCT(abaci::ast::Variable, name)
BOOST_FUSION_ADAPT_STRUCT(abaci::ast::FunctionValueCall, name, args)
BOOST_FUSION_ADAPT_STRUCT(abaci::ast::DataMember, name, memberList)
BOOST_FUSION_ADAPT_STRUCT(abaci::ast::MethodValueCall, name, memberList, method, args)
BOOST_FUSION_ADAPT_STRUCT(abaci::ast::UserInput, dummy)
BOOST_FUSION_ADAPT_STRUCT(abaci::ast::TypeConvItems, toType, expression)
#ifdef ABACI_USE_OLDER_BOOST
BOOST_FUSION_ADAPT_STRUCT(abaci::ast::ListItems, firstElement, otherElements)
#else
BOOST_FUSION_ADAPT_STRUCT(abaci::ast::ListItems, elements)
#endif
BOOST_FUSION_ADAPT_STRUCT(abaci::ast::EmptyListItems, elementType)
BOOST_FUSION_ADAPT_STRUCT(abaci::ast::ListIndex, name, indexes)
BOOST_FUSION_ADAPT_STRUCT(abaci::ast::DataListIndex, name, memberList, indexes)

#endif
