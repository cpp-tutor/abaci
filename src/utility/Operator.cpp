#include "Operator.hpp"
#include "localize/Keywords.hpp"

namespace abaci::utility {

const std::unordered_map<std::string,Operator> Operators{
    { PLUS, Operator::Plus },
    { MINUS, Operator::Minus },
    { TIMES, Operator::Times },
    { DIVIDE, Operator::Divide },
    { MODULO, Operator::Modulo },
    { FLOOR_DIVIDE, Operator::FloorDivide },
    { EXPONENT, Operator::Exponent },
    { EQUAL, Operator::Equal },
    { NOT_EQUAL, Operator::NotEqual },
    { LESS, Operator::Less },
    { LESS_EQUAL, Operator::LessEqual },
    { GREATER_EQUAL, Operator::GreaterEqual },
    { GREATER, Operator::Greater },
    { NOT, Operator::Not },
    { AND, Operator::And },
    { OR, Operator::Or },
    { BITWISE_COMPL, Operator::Compl },
    { BITWISE_AND, Operator::BitAnd },
    { BITWISE_OR, Operator::BitOr },
    { BITWISE_XOR, Operator::BitXor },
    { COMMA, Operator::Comma },
    { SEMICOLON, Operator::SemiColon },
    { FROM, Operator::From },
    { TO, Operator::To }
};

} // namespace abaci::utility