#include "Operator.hpp"
#include "Report.hpp"
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
    { LEFT_BRACKET, Operator::LeftBracket },
    { RIGHT_BRACKET, Operator::RightBracket },
    { COMMA, Operator::Comma },
    { SEMICOLON, Operator::SemiColon },
    { QUESTION, Operator::Question },
    { BANG, Operator::Bang },
    { FROM, Operator::From },
    { TO, Operator::To }
};

const std::string& operatorToString(Operator op) {
    for (const auto& item : Operators) {
        if (item.second == op) {
            return item.first;
        }
    }
    UnexpectedError0(BadOperator);
}

} // namespace abaci::utility