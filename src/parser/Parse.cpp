#include "Parse.hpp"
#include "localize/Keywords.hpp"
#include "localize/Messages.hpp"
#include "utility/Type.hpp"
#include "utility/Operator.hpp"
#include "utility/Report.hpp"
#include "utility/Constant.hpp"
#include "ast/Expr.hpp"
#include "ast/Stmt.hpp"
#include <boost/spirit/home/x3.hpp>
#include <boost/spirit/home/x3/support/utility/error_reporting.hpp>
#include <boost/spirit/home/x3/support/utility/annotate_on_success.hpp>
#include <optional>
#include <charconv>
#include <sstream>
#include <cstring>

namespace abaci::parser {

namespace x3 = boost::spirit::x3;
using x3::char_;
using x3::string;
using x3::lit;
using x3::lexeme;
using x3::ascii::space;

using abaci::utility::AbaciValue;
using abaci::utility::Constants;
using abaci::utility::Operator;
using abaci::utility::Operators;
using abaci::utility::Complex;
using abaci::utility::String;
using abaci::utility::TypeConversions;
using abaci::utility::NativeType;
using abaci::utility::NativeTypes;

using abaci::ast::ExprNode;
using abaci::ast::ExprList;
using abaci::ast::ExprPair;
using abaci::ast::Variable;
using abaci::ast::FunctionValueCall;
using abaci::ast::UserInput;
using abaci::ast::TypeConvItems;
using abaci::ast::TypeConv;
using abaci::ast::ListItems;
using abaci::ast::EmptyListItems;
using abaci::ast::List;
using abaci::ast::CallList;
using abaci::ast::MultiCall;
using abaci::ast::StmtNode;
using abaci::ast::StmtList;
using abaci::ast::CommentStmt;
using abaci::ast::PrintStmt;
using abaci::ast::InitStmt;
using abaci::ast::AssignStmt;
using abaci::ast::IfStmt;
using abaci::ast::WhileStmt;
using abaci::ast::RepeatStmt;
using abaci::ast::CaseStmt;
using abaci::ast::WhenStmt;
using abaci::ast::WhenList;
using abaci::ast::Function;
using abaci::ast::FunctionCall;
using abaci::ast::ReturnStmt;
using abaci::ast::ExprFunction;
using abaci::ast::Class;
using abaci::ast::MethodCall;
using abaci::ast::NativeFunction;
using abaci::ast::ExpressionStmt;

struct error_handler {
    template <typename Iterator, typename Exception, typename Context>
    x3::error_handler_result on_error(
        [[maybe_unused]] Iterator& first, [[maybe_unused]] const Iterator& last,
        const Exception& x, const Context& context)
    {
        auto& error_handler = x3::get<x3::error_handler_tag>(context).get();
        std::string message;
        if (x.which().size() < 32) {
            message = "Expecting \'" + x.which().substr(1, x.which().size() - 2) + "\'.";
        }
        else if (x.which().find("ExprNode") != std::string::npos) {
            message = "Found an invalid expression.";
        }
        else {
            message = "Encountered a syntax error.";
        }
        error_handler(x.where(), message);
        return x3::error_handler_result::fail;
    }
};

struct number_str_class;
struct base_number_str_class;
struct boolean_str_class;
struct string_str_class;
struct value_class;
struct native_type_class;

x3::rule<struct number_str_class, std::string> number_str;
x3::rule<struct base_number_str_class, std::string> base_number_str;
x3::rule<struct boolean_str_class, std::string> boolean_str;
x3::rule<struct string_str_class, std::string> string_str;
x3::rule<struct value_class, std::size_t> value;
x3::rule<struct native_type_class, NativeType> const native_type;

struct number_str_class : x3::annotate_on_success {};
struct base_number_str_class : x3::annotate_on_success {};
struct boolean_str_class : x3::annotate_on_success {};
struct string_str_class : x3::annotate_on_success {};
struct value_class : x3::annotate_on_success {};
struct native_type_class : x3::annotate_on_success {};

x3::rule<class plus, Operator> const plus;
x3::rule<class minus, Operator> const minus;
x3::rule<class times, Operator> const times;
x3::rule<class divide, Operator> const divide;
x3::rule<class modulo, Operator> const modulo;
x3::rule<class floor_divide, Operator> const floor_divide;
x3::rule<class equal, Operator> const equal;
x3::rule<class not_equal, Operator> const not_equal;
x3::rule<class less, Operator> const less;
x3::rule<class less_equal, Operator> const less_equal;
x3::rule<class greater_equal, Operator> const greater_equal;
x3::rule<class greater, Operator> const greater;
x3::rule<class exponent, Operator> const exponent;
x3::rule<class logical_and, Operator> const logical_and;
x3::rule<class logical_or, Operator> const logical_or;
x3::rule<class logical_not, Operator> const logical_not;
x3::rule<class bitwise_and, Operator> const bitwise_and;
x3::rule<class bitwise_or, Operator> const bitwise_or;
x3::rule<class bitwise_xor, Operator> const bitwise_xor;
x3::rule<class bitwise_compl, Operator> const bitwise_compl;
x3::rule<class comma, Operator> const comma;
x3::rule<class semicolon, Operator> const semicolon;
x3::rule<class question, Operator> const question;
x3::rule<class bang, Operator> const bang;
x3::rule<class from, Operator> const from;
x3::rule<class to, Operator> const to;

struct expression_class;
struct logic_or_class;
struct logic_and_class;
struct bit_or_class;
struct bit_xor_class;
struct bit_and_class;
struct equality_class;
struct comparison_class;
struct term_class;
struct factor_class;
struct unary_class;
struct index_class;

x3::rule<struct expression_class, ExprNode> const expression;
x3::rule<struct logic_or_class, ExprList> const logic_or;
x3::rule<class logic_and_n, ExprNode> const logic_and_n;
x3::rule<struct logic_and_class, ExprList> const logic_and;
x3::rule<class bit_or_n, ExprNode> const bit_or_n;
x3::rule<struct bit_or_class, ExprList> const bit_or;
x3::rule<class bit_xor_n, ExprNode> const bit_xor_n;
x3::rule<struct bit_xor_class, ExprList> const bit_xor;
x3::rule<class bit_and_n, ExprNode> const bit_and_n;
x3::rule<struct bit_and_class, ExprList> const bit_and;
x3::rule<class equality_n, ExprNode> const equality_n;
x3::rule<struct equality_class, ExprList> const equality;
x3::rule<class comaprison_n, ExprNode> const comparison_n;
x3::rule<struct comparison_class, ExprList> const comparison;
x3::rule<class term_n, ExprNode> const term_n;
x3::rule<struct term_class, ExprList> const term;
x3::rule<class factor_n, ExprNode> const factor_n;
x3::rule<struct factor_class, ExprList> const factor;
x3::rule<class unary_n, ExprNode> const unary_n;
x3::rule<struct unary_class, ExprList> const unary;
x3::rule<class index_n, ExprNode> const index_n;
x3::rule<struct index_class, ExprList> const index;
x3::rule<class primary_n, ExprNode> const primary_n;

struct expression_class : error_handler, x3::annotate_on_success {};
struct logic_or_class : x3::annotate_on_success {};
struct logic_and_class : x3::annotate_on_success {};
struct bit_or_class : x3::annotate_on_success {};
struct bit_xor_class : x3::annotate_on_success {};
struct bit_and_class : x3::annotate_on_success {};
struct equality_class : x3::annotate_on_success {};
struct comparison_class : x3::annotate_on_success {};
struct term_class : x3::annotate_on_success {};
struct factor_class : x3::annotate_on_success {};
struct unary_class : x3::annotate_on_success {};
struct index_class : x3::annotate_on_success {};

struct identifier_class;
struct variable_class;
struct this_ptr_class;
struct list_items_class;
struct empty_list_items_class;
struct list_class;

x3::rule<struct identifier_class, std::string> const identifier;
x3::rule<struct variable_class, Variable> const variable;
x3::rule<struct this_ptr_class, Variable> const this_ptr;
x3::rule<class function_value_call, FunctionValueCall> const function_value_call;
x3::rule<class call_index, ExprList> const call_index;
x3::rule<class call_slice, ExprPair> const call_slice;
x3::rule<class call_function, FunctionValueCall> const call_function;
x3::rule<class call_list, CallList> const call_list;
x3::rule<class this_call, MultiCall> const this_call;
x3::rule<class multi_call, MultiCall> const multi_call;
x3::rule<class user_input, UserInput> const user_input;
x3::rule<class type_conversion_items, TypeConvItems> const type_conversion_items;
x3::rule<class type_conversion, TypeConv> const type_conversion;
x3::rule<struct list_items_class, ListItems> const list_items;
x3::rule<struct empty_list_items_class, EmptyListItems> const empty_list_items;
x3::rule<struct list_class, List> const list;

struct identitifer_class : x3::annotate_on_success {};
struct variable_class : x3::annotate_on_success {};
struct this_ptr_class : x3::annotate_on_success {};
struct list_items_class : x3::annotate_on_success {};
struct empty_list_items_class : x3::annotate_on_success {};
struct list_class : x3::annotate_on_success {};

struct comment_items_class;
struct comment_class;
struct print_items_class;
struct print_stmt_class;
struct let_items_class;
struct let_stmt_class;
struct assign_list_class;
struct assign_items_class;
struct this_assign_items_class;
struct assign_stmt_class;
struct if_items_class;
struct if_stmt_class;
struct while_items_class;
struct while_stmt_class;
struct repeat_items_class;
struct repeat_stmt_class;
struct when_items_class;
struct case_items_class;
struct case_stmt_class;
struct function_parameters_class;
struct function_items_class;
struct function_class;
struct expression_function_items_class;
struct expression_function_class;
struct call_args_class;
struct call_items_class;
struct function_call_class;
struct return_items_class;
struct return_stmt_class;
struct class_items_class;
struct class_template_class;
struct method_call_items_class;
struct this_call_items_class;
struct method_call_class;
struct expression_stmt_items_class;
struct expression_stmt_class;
struct native_call_args_class;
struct native_function_items_class;
struct native_function_class;
struct statement_class;
struct block_class;

x3::rule<struct comment_items_class, CommentStmt> const comment_items;
x3::rule<struct comment_class, StmtNode> const comment;
x3::rule<struct print_items_class, PrintStmt> const print_items;
x3::rule<struct print_stmt_class, StmtNode> const print_stmt;
x3::rule<struct let_items_class, InitStmt> const let_items;
x3::rule<struct let_stmt_class, StmtNode> const let_stmt;
x3::rule<struct assign_list_class, CallList> const assign_list;
x3::rule<struct assign_items_class, AssignStmt> const assign_items;
x3::rule<struct this_assign_items_class, AssignStmt> const this_assign_items;
x3::rule<struct assign_stmt_class, StmtNode> const assign_stmt;
x3::rule<struct if_items_class, IfStmt> const if_items;
x3::rule<struct if_stmt_class, StmtNode> const if_stmt;
x3::rule<struct while_items_class, WhileStmt> const while_items;
x3::rule<struct while_stmt_class, StmtNode> const while_stmt;
x3::rule<struct repeat_items_class, RepeatStmt> const repeat_items;
x3::rule<struct repeat_stmt_class, StmtNode> const repeat_stmt;
x3::rule<struct when_items_class, WhenStmt> const when_items;
x3::rule<struct case_items_class, CaseStmt> const case_items;
x3::rule<struct case_stmt_class, StmtNode> const case_stmt;
x3::rule<struct function_parameters_class, std::vector<Variable>> const function_parameters;
x3::rule<struct function_items_class, Function> const function_items;
x3::rule<struct function_class, StmtNode> const function;
x3::rule<struct expression_function_items_class, ExprFunction> const expression_function_items;
x3::rule<struct expression_function_class, StmtNode> const expression_function;
x3::rule<struct call_args_class, ExprList> const call_args;
x3::rule<struct call_items_class, FunctionCall> const call_items;
x3::rule<struct function_call_class, StmtNode> const function_call;
x3::rule<struct return_items_class, ReturnStmt> const return_items;
x3::rule<struct return_stmt_class, StmtNode> const return_stmt;
x3::rule<struct class_items_class, Class> const class_items;
x3::rule<struct class_template_class, StmtNode> const class_template;
x3::rule<struct method_call_items_class, MethodCall> const method_call_items;
x3::rule<struct this_call_items_class, MethodCall> const this_call_items;
x3::rule<struct method_call_class, StmtNode> const method_call;
x3::rule<struct expression_stmt_items_class, ExpressionStmt> const expression_stmt_items;
x3::rule<struct expression_stmt_class, StmtNode> const expression_stmt;
x3::rule<struct native_call_args_class, std::vector<NativeType>> const native_call_args;
x3::rule<struct native_function_items_class, NativeFunction> const native_function_items;
x3::rule<struct native_function_class, StmtNode> const native_function;
x3::rule<struct keywords, std::string> const keywords;
x3::rule<struct statment_class, StmtNode> const statement;
x3::rule<struct block_class, StmtList> const block;

struct comment_items_class : error_handler, x3::annotate_on_success {};
struct comment_class : error_handler, x3::annotate_on_success {};
struct print_items_class : error_handler, x3::annotate_on_success {};
struct print_stmt_class : error_handler, x3::annotate_on_success {};
struct let_items_class : error_handler, x3::annotate_on_success {};
struct let_stmt_class : error_handler, x3::annotate_on_success {};
struct assign_list_class : error_handler, x3::annotate_on_success {};
struct assign_items_class : error_handler, x3::annotate_on_success {};
struct this_assign_items_class : error_handler, x3::annotate_on_success {};
struct assign_stmt_class : error_handler, x3::annotate_on_success {};
struct if_items_class : error_handler, x3::annotate_on_success {};
struct if_stmt_class : error_handler, x3::annotate_on_success {};
struct while_items_class : error_handler, x3::annotate_on_success {};
struct while_stmt_class : error_handler, x3::annotate_on_success {};
struct repeat_items_class : error_handler, x3::annotate_on_success {};
struct repeat_stmt_class : error_handler, x3::annotate_on_success {};
struct when_items_class : error_handler, x3::annotate_on_success {};
struct case_items_class : error_handler, x3::annotate_on_success {};
struct case_stmt_class : error_handler, x3::annotate_on_success {};
struct function_parameters_class : error_handler, x3::annotate_on_success {};
struct function_items_class : error_handler, x3::annotate_on_success {};
struct function_class : error_handler, x3::annotate_on_success {};
struct expression_function_items_class : error_handler, x3::annotate_on_success {};
struct expression_function_class : error_handler, x3::annotate_on_success {};
struct call_args_class : error_handler, x3::annotate_on_success {};
struct call_items_class : error_handler, x3::annotate_on_success {};
struct function_call_class : error_handler, x3::annotate_on_success {};
struct return_items_class : error_handler, x3::annotate_on_success {};
struct return_stmt_class : error_handler, x3::annotate_on_success {};
struct class_items_class : error_handler, x3::annotate_on_success {};
struct class_template_class : error_handler, x3::annotate_on_success {};
struct this_call_items_class : error_handler, x3::annotate_on_success {};
struct method_call_items_class : error_handler, x3::annotate_on_success {};
struct method_call_class : error_handler, x3::annotate_on_success {};
struct expression_stmt_items_class : error_handler, x3::annotate_on_success {};
struct expression_stmt_class : error_handler, x3::annotate_on_success {};
struct native_function_items_class : error_handler, x3::annotate_on_success {};
struct native_function_class : error_handler, x3::annotate_on_success {};
struct statement_class : error_handler, x3::annotate_on_success {};
struct block_class : error_handler, x3::annotate_on_success {};

static Constants *ConstantsTable(std::optional<Constants*> constants = std::nullopt) {
    static Constants *table = nullptr;
    if (constants.has_value()) {
        table = constants.value();
    }
    return table;
}

auto makeNumber = [](auto& ctx) {
    if (!ConstantsTable()) {
        return;
    }
    const auto& str = _attr(ctx);
    if (str.find(IMAGINARY) != std::string::npos) {
        double imag;
        std::from_chars(str.data(), str.data() + str.size() - strlen(IMAGINARY), imag);
        _val(ctx) = ConstantsTable()->add(Complex{ 0, imag });
    }
    else if (str.find(DOT) != std::string::npos) {
        double d;
        std::from_chars(str.data(), str.data() + str.size(), d);
        _val(ctx) = ConstantsTable()->add(d);
    }
    else {
        uint64_t ull;
        std::from_chars(str.data(), str.data() + str.size(), ull);
        _val(ctx) = ConstantsTable()->add(ull);
    }
};

auto makeBaseNumber = [](auto& ctx) {
    if (!ConstantsTable()) {
        return;
    }
    const auto& str = _attr(ctx);
    uint64_t ull{ 0 };
    if (str.find(HEX_PREFIX) == 0) {
        std::from_chars(str.data() + std::string(HEX_PREFIX).size(), str.data() + str.size(), ull, 16);
    }
    else if (str.find(BIN_PREFIX) == 0) {
        std::from_chars(str.data() + std::string(BIN_PREFIX).size(), str.data() + str.size(), ull, 2);
    }
    else if (str.find(OCT_PREFIX) == 0) {
        std::from_chars(str.data() + std::string(OCT_PREFIX).size(), str.data() + str.size(), ull, 8);
    }
    _val(ctx) = ConstantsTable()->add(ull);
};

auto makeBoolean = [](auto& ctx) {
    if (!ConstantsTable()) {
        return;
    }
    const auto& str = _attr(ctx);
    _val(ctx) = ConstantsTable()->add((str == TRUE) ? true : false);
};

auto makeString = [](auto& ctx) {
    if (!ConstantsTable()) {
        return;
    }
    const auto& str = _attr(ctx);
    _val(ctx) = ConstantsTable()->add(str);
};

auto makeNil = [](auto& ctx) {
    if (!ConstantsTable()) {
        return;
    }
    _val(ctx) = ConstantsTable()->add(nullptr);
};

auto makeThisPtr = [](auto& ctx){
    _val(ctx) = Variable(THIS_V);
};

auto makeTypeConversion = [](auto& ctx){
    const TypeConvItems& items = _attr(ctx);
    auto iter = TypeConversions.find(items.toType);
    if (iter != TypeConversions.end()) {
        _val(ctx) = TypeConv{ iter->second, std::make_shared<ExprNode>(items.expression) };
    }
};

auto makeList = [](auto& ctx){
    const ListItems& items = _attr(ctx);
    ExprList elements{ items.firstElement };
    elements.insert(elements.end(), items.otherElements.begin(), items.otherElements.end());
    _val(ctx) = List{ std::make_shared<ExprList>(std::move(elements)), "" };
};

auto makeEmptyList = [](auto& ctx){
    const EmptyListItems& items = _attr(ctx);
    _val(ctx) = List{ std::make_shared<ExprList>(), items.elementType };
};

auto makeNative = [](auto& ctx){
    const std::string type = get<std::string>(_attr(ctx));
    auto iter = NativeTypes.find(type);;
    if (iter == NativeTypes.end()) {
        UnexpectedError0(BadType);
    }
    _val(ctx) = iter->second;
};

template<std::size_t Ty = ExprNode::Unset>
struct MakeNode {
    template<typename Context>
    void operator()(Context& ctx) {
        if constexpr (Ty != ExprNode::Unset) {
            _val(ctx).data = std::pair{ static_cast<ExprNode::Association>(Ty), _attr(ctx) };
        }
        else {
            _val(ctx).data = _attr(ctx);
        }
    }
};

template<typename T>
struct MakeStmt {
    template<typename Context>
    void operator()(Context& ctx) {
        _val(ctx).data = _attr(ctx);
    }
};

auto getOperator = [](auto& ctx){
    const std::string str = _attr(ctx);
    const auto iter = Operators.find(str);
    if (iter != Operators.end()) {
        _val(ctx) = iter->second;
    }
    else {
        UnexpectedError0(BadOperator);
    }
};

const auto number_str_def = lexeme[+char_('0', '9') >> -( string(DOT) >> +char_('0', '9') ) >> -string(IMAGINARY)];
const auto base_number_str_def = lexeme[string(HEX_PREFIX) >> +( char_('0', '9') | char_('A', 'F') | char_('a', 'f') )]
    | lexeme[string(BIN_PREFIX) >> +char_('0', '1')]
    | lexeme[string(OCT_PREFIX) >> +char_('0', '7')];
const auto boolean_str_def = string(FALSE) | string(TRUE);
const auto string_str_def = lexeme['"' > *(char_ - '"') > '"'];
const auto value_def = base_number_str[makeBaseNumber] | number_str[makeNumber] | boolean_str[makeBoolean] | string_str[makeString] | lit(NIL)[makeNil];

const auto plus_def = string(PLUS)[getOperator];
const auto minus_def = string(MINUS)[getOperator];
const auto times_def = string(TIMES)[getOperator];
const auto divide_def = string(DIVIDE)[getOperator];
const auto modulo_def = string(MODULO)[getOperator];
const auto floor_divide_def = string(FLOOR_DIVIDE)[getOperator];
const auto exponent_def = string(EXPONENT)[getOperator];

const auto equal_def = string(EQUAL)[getOperator];
const auto not_equal_def = string(NOT_EQUAL)[getOperator];
const auto less_def = string(LESS)[getOperator];
const auto less_equal_def = string(LESS_EQUAL)[getOperator];
const auto greater_equal_def = string(GREATER_EQUAL)[getOperator];
const auto greater_def = string(GREATER)[getOperator];

const auto logical_and_def = string(AND)[getOperator];
const auto logical_or_def = string(OR)[getOperator];
const auto logical_not_def = string(NOT)[getOperator];
const auto bitwise_and_def = string(BITWISE_AND)[getOperator];
const auto bitwise_or_def = string(BITWISE_OR)[getOperator];
const auto bitwise_xor_def = string(BITWISE_XOR)[getOperator];
const auto bitwise_compl_def = string(BITWISE_COMPL)[getOperator];

const auto comma_def = string(COMMA)[getOperator];
const auto semicolon_def = string(SEMICOLON)[getOperator];
const auto question_def = string(QUESTION)[getOperator];
const auto bang_def = string(BANG)[getOperator];
const auto from_def = string(FROM)[getOperator];
const auto to_def = string(TO)[getOperator];

const auto identifier_def = lexeme[( ( char_('A', 'Z') | char_('a', 'z') | char_('\'') | char_('\200', '\377') )
    >> *( char_('A', 'Z') | char_('a', 'z') | char_('0', '9') | char_('_') | char_('\'') | char_('\200', '\377') ) ) - keywords];
const auto variable_def = identifier;
const auto this_ptr_def = lit(THIS)[makeThisPtr];
const auto function_value_call_def = identifier >> call_args;
const auto data_value_call_def = variable >> +( DOT >> variable );
const auto this_value_call_def = this_ptr >> *( DOT >> variable );
const auto data_method_call_def = variable >> DOT >> *( variable >> DOT ) >> identifier >> call_args;
const auto this_method_call_def = this_ptr >> DOT >> *( variable >> DOT ) >> identifier >> call_args;
const auto call_index_def = +( LEFT_BRACKET >> expression >> RIGHT_BRACKET );
const auto call_slice_def = LEFT_BRACKET >> expression >> COLON >> expression >> RIGHT_BRACKET;
const auto call_function_def = identifier >> call_args;
const auto call_list_def = ( DOT >> call_function ) | ( DOT >> variable ) | call_slice | call_index;
const auto multi_call_def = variable >> *call_list;
const auto this_call_def = this_ptr >> *call_list;
const auto user_input_def = lit(INPUT);
const auto type_conversion_items_def = ( string(INT) | string(FLOAT) | string(COMPLEX) | string(STR)
    | string(REAL) | string(IMAG) ) >> LEFT_PAREN >> expression >> RIGHT_PAREN;
const auto type_conversion_def = type_conversion_items[makeTypeConversion];
const auto list_items_def = LEFT_BRACKET >> expression >> *( COMMA >> expression ) >> RIGHT_BRACKET;
const auto empty_list_items_def = LEFT_BRACKET >> ( string(BOOL) | string(INT) | string(FLOAT) | string(COMPLEX) | string(STR) ) >> RIGHT_BRACKET;
const auto list_def = list_items[makeList] | empty_list_items[makeEmptyList];

const auto expression_def = logic_or[MakeNode<ExprNode::Boolean>()];
const auto logic_or_def = logic_and_n >> *( logical_or > logic_and_n );
const auto logic_and_n_def = logic_and[MakeNode<ExprNode::Boolean>()];
const auto logic_and_def = bit_or_n >> *( logical_and > bit_or_n );
const auto bit_or_n_def = bit_or[MakeNode<ExprNode::Left>()];
const auto bit_or_def = bit_xor_n >> *( bitwise_or > bit_xor_n );
const auto bit_xor_n_def = bit_xor[MakeNode<ExprNode::Left>()];
const auto bit_xor_def = bit_and_n >> *( bitwise_xor > bit_and_n );
const auto bit_and_n_def = bit_and[MakeNode<ExprNode::Left>()];
const auto bit_and_def = equality_n >> *( bitwise_and > equality_n );
const auto equality_n_def = equality[MakeNode<ExprNode::Boolean>()];
const auto equality_def = comparison_n >> *( (equal | not_equal) > comparison_n );
const auto comparison_n_def = comparison[MakeNode<ExprNode::Boolean>()];
const auto comparison_def = term_n >> -( ( ( greater_equal | greater ) > term_n )
    | ( ( less_equal | less ) > term_n ) >> -( ( less_equal | less ) > term_n ) );
const auto term_n_def = term[MakeNode<ExprNode::Left>()];
const auto term_def = factor_n >> *( ( plus | minus ) > factor_n );
const auto factor_n_def = factor[MakeNode<ExprNode::Left>()];
const auto factor_def = unary_n >> *( ( times | floor_divide | divide | modulo ) > unary_n);
const auto unary_n_def = unary[MakeNode<ExprNode::Unary>()];
const auto unary_def = *( minus | logical_not | bitwise_compl | question | bang ) >> index_n;
const auto index_n_def = index[MakeNode<ExprNode::Right>()];
const auto index_def = primary_n >> *( exponent > unary_n );
const auto primary_n_def = value[MakeNode<>()] | ( LEFT_PAREN > logic_or[MakeNode<ExprNode::Boolean>()] > RIGHT_PAREN )
    | type_conversion[MakeNode<>()] | function_value_call[MakeNode<>()] | this_call[MakeNode<>()] | multi_call[MakeNode<>()]
    | list[MakeNode<>()] | user_input[MakeNode<>()];

const auto keywords_def = lit(AND) | CASE | CLASS | COMPLEX | ELSE | ENDCASE | ENDCLASS | ENDFN | ENDIF | ENDWHILE
    | FALSE | FLOAT | FN | IF | IMAG | INPUT | INT | LET | NOT | OR | PRINT | REAL | REPEAT | RETURN | STR | THIS | TRUE | UNTIL | WHEN | WHILE;

const auto comment_items_def = lexeme[*( char_ - '\n' )];
const auto comment_def = REM >> comment_items[MakeStmt<CommentStmt>()];
const auto print_items_def = -( expression >> -( ( comma | semicolon ) >> *( expression >> ( comma | semicolon ) ) ) );
const auto print_stmt_def = PRINT >> print_items[MakeStmt<PrintStmt>()];
const auto let_items_def = variable >> (equal | from) >> expression;
const auto let_stmt_def = LET >> let_items[MakeStmt<InitStmt>()];
const auto assign_list_def = ( DOT >> variable ) | call_slice | call_index;
const auto assign_items_def = variable >> *assign_list >> FROM >> expression;
const auto this_assign_items_def = this_ptr >> *assign_list >> FROM >> expression;
const auto assign_stmt_def = this_assign_items[MakeStmt<AssignStmt>()] | assign_items[MakeStmt<AssignStmt>()];

const auto if_items_def = expression > block > -( ELSE > block );
const auto if_stmt_def = IF > if_items[MakeStmt<IfStmt>()] > ENDIF;

const auto while_items_def = expression > block;
const auto while_stmt_def = WHILE > while_items[MakeStmt<WhileStmt>()] > ENDWHILE;
const auto repeat_items_def = block > UNTIL > expression;
const auto repeat_stmt_def = REPEAT > repeat_items[MakeStmt<RepeatStmt>()];

const auto when_items_def = WHEN > expression >> *( COMMA > expression ) > block;
const auto case_items_def = expression > *when_items > -( ELSE > block );
const auto case_stmt_def = CASE > case_items[MakeStmt<CaseStmt>()] > ENDCASE;

const auto function_parameters_def = LEFT_PAREN > -( variable >> *( COMMA > variable ) ) > RIGHT_PAREN;
const auto function_items_def = identifier > function_parameters > block;
const auto function_def = FN > function_items[MakeStmt<Function>()] > ENDFN;
const auto expression_function_items_def = identifier >> function_parameters > TO > expression;
const auto expression_function_def = LET >> expression_function_items[MakeStmt<ExprFunction>()];

const auto call_args_def = LEFT_PAREN >> -( expression >> *( COMMA >> expression) ) >> RIGHT_PAREN;
const auto call_items_def = identifier > call_args;
const auto function_call_def = call_items[MakeStmt<FunctionCall>()];
const auto return_items_def = expression;
const auto return_stmt_def = RETURN > return_items[MakeStmt<ReturnStmt>()];

const auto class_items_def = identifier > function_parameters >> *(FN > function_items > ENDFN);
const auto class_template_def = CLASS > class_items[MakeStmt<Class>()] > ENDCLASS;

const auto this_call_items_def = this_ptr >> DOT >> *( variable >> DOT ) >> identifier >> call_args;
const auto method_call_items_def = variable >> DOT >> *( variable >> DOT ) >> identifier >> call_args;
const auto method_call_def = this_call_items[MakeStmt<MethodCall>()] | method_call_items[MakeStmt<MethodCall>()];

const auto native_type_def = lexeme[string(NONE) | string(I8_STAR) | string(I1) | string(I8) | string(I16) | string(I32) | string(I64) | string(F32) | string(F64)][makeNative];
const auto native_call_args_def = LEFT_PAREN >> -( native_type >> *( COMMA >> native_type) ) >> RIGHT_PAREN;
const auto native_function_items_def = identifier >> native_call_args >> TO >> native_type;
const auto native_function_def = lit(NATIVE) >> FN >> native_function_items[MakeStmt<NativeFunction>()];

const auto expression_stmt_items_def = expression;
const auto expression_stmt_def = expression_stmt_items[MakeStmt<ExpressionStmt>()];

const auto statement_def = native_function | assign_stmt | print_stmt | expression_function | let_stmt | if_stmt | while_stmt | repeat_stmt | case_stmt | return_stmt | function | class_template | method_call | function_call | expression_stmt | comment;
const auto block_def = *statement;

BOOST_SPIRIT_DEFINE(number_str, base_number_str, boolean_str, string_str, value)
BOOST_SPIRIT_DEFINE(plus, minus, times, divide, modulo, floor_divide, exponent,
    equal, not_equal, less, less_equal, greater_equal, greater,
    logical_and, logical_or, logical_not, bitwise_and, bitwise_or, bitwise_xor, bitwise_compl,
    comma, semicolon, question, bang, from, to)
BOOST_SPIRIT_DEFINE(expression, logic_or, logic_and, logic_and_n,
    bit_or, bit_or_n, bit_xor, bit_xor_n, bit_and, bit_and_n,
    equality, equality_n, comparison, comparison_n,
    term, term_n, factor, factor_n, unary, unary_n, index, index_n, primary_n)
BOOST_SPIRIT_DEFINE(identifier, variable, function_value_call,
    call_index, call_slice, call_function, call_list, this_call, multi_call, user_input, type_conversion_items, type_conversion, keywords,
    list_items, empty_list_items, list,
    comment_items, comment, print_items, print_stmt,
    let_items, let_stmt, assign_list, assign_items, assign_stmt, if_items, if_stmt,
    when_items, while_items, while_stmt, repeat_items, repeat_stmt, case_items, case_stmt,
    native_type, native_call_args, native_function, native_function_items,
    function_parameters, function_items, function, call_args, call_items, function_call,
    expression_function_items, expression_function,
    return_items, return_stmt, class_items, class_template,
    this_ptr, this_assign_items, this_call_items,
    method_call_items, method_call, expression_stmt_items, expression_stmt, statement, block)

bool parseBlock(const std::string& block_str, StmtList& ast, std::ostream& error, std::string::const_iterator& iter, Constants *constants) {
    ConstantsTable(constants);
    auto end = block_str.cend();
    using error_handler_type = x3::error_handler<std::string::const_iterator>;
    error_handler_type error_handler(iter, end, error);
    const auto parser = x3::with<x3::error_handler_tag>(std::ref(error_handler))[block];
    bool result = phrase_parse(iter, end, parser, space, ast);
    ConstantsTable(nullptr);
    return result;
}

bool parseStatement(const std::string& stmt_str, StmtNode& ast, std::ostream& error, std::string::const_iterator& iter, Constants *constants) {
    ConstantsTable(constants);
    auto end = stmt_str.cend();
    using error_handler_type = x3::error_handler<std::string::const_iterator>;
    error_handler_type error_handler(iter, end, error);
    const auto parser = x3::with<x3::error_handler_tag>(std::ref(error_handler))[statement];
    bool result = phrase_parse(iter, end, parser, space, ast);
    ConstantsTable(nullptr);
    return result;
}

bool testStatement(const std::string& stmt_str) {
    auto iter = stmt_str.begin();
    auto end = stmt_str.end();
    std::ostringstream dummy;
    using error_handler_type = x3::error_handler<std::string::const_iterator>;
    error_handler_type error_handler(iter, end, dummy);
    const auto parser = x3::with<x3::error_handler_tag>(std::ref(error_handler))[statement];
    return phrase_parse(iter, end, parser, space);
}

} // namespace abaci::parser
