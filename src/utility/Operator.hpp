#ifndef Operator_hpp
#define Operator_hpp

#include <string>
#include <unordered_map>

namespace abaci::utility {

enum class Operator { None, Plus, Minus, Times, Divide, Modulo, FloorDivide, Exponent,
    Equal, NotEqual, Less, LessEqual, GreaterEqual, Greater,
    Not, And, Or, Compl, BitAnd, BitOr, BitXor,
    Comma, SemiColon, From, To };

extern const std::unordered_map<std::string,Operator> Operators;

const std::string& operatorToString(Operator op);

} // namespace abaci::utility

#endif
