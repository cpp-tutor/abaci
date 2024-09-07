#ifndef Expr_hpp
#define Expr_hpp

#include "utility/Type.hpp"
#include "utility/Operator.hpp"
#include "utility/Constant.hpp"
#include "utility/Variable.hpp"
#include <boost/spirit/home/x3/support/ast/position_tagged.hpp>
#include <boost/fusion/adapted/struct.hpp>
#include <vector>
#include <variant>
#include <utility>
#include <memory>

namespace abaci::ast {

using boost::spirit::x3::position_tagged;

class ExprNode;

struct ExprList : std::vector<ExprNode>, position_tagged {};

using abaci::utility::AbaciValue;
using abaci::utility::Constants;
using abaci::utility::Operator;
using abaci::utility::Variable;

struct FunctionValueCall : position_tagged {
    std::string name;
    ExprList args;
};

struct DataMember : position_tagged {
    Variable name;
    std::vector<Variable> member_list;
};

struct MethodValueCall : position_tagged {
    Variable name;
    std::vector<Variable> member_list;
    std::string method;
    ExprList args;
};

struct UserInput {
    std::string dummy;
};

struct TypeConv {
    AbaciValue::Type to_type;
    std::shared_ptr<ExprNode> expression;
};

class ExprNode : position_tagged {
public:
    enum Association { Unset, Left, Right, Unary, Boolean };
    enum Type { UnsetNode, ValueNode, OperatorNode, ListNode, VariableNode, FunctionNode, DataMemberNode, MethodNode, InputNode, TypeConvNode };
    std::variant<std::monostate,std::size_t,Operator,std::pair<Association,ExprList>,Variable,FunctionValueCall,DataMember,MethodValueCall,UserInput,TypeConv> data;
};

struct TypeConvItems {
    std::string to_type;
    ExprNode expression;
};

} // namespace abaci::ast

BOOST_FUSION_ADAPT_STRUCT(abaci::ast::ExprNode, data)
BOOST_FUSION_ADAPT_STRUCT(abaci::ast::FunctionValueCall, name, args)
BOOST_FUSION_ADAPT_STRUCT(abaci::ast::DataMember, name, member_list)
BOOST_FUSION_ADAPT_STRUCT(abaci::ast::MethodValueCall, name, member_list, method, args)
BOOST_FUSION_ADAPT_STRUCT(abaci::ast::UserInput, dummy)
BOOST_FUSION_ADAPT_STRUCT(abaci::ast::TypeConvItems, to_type, expression)

#endif
