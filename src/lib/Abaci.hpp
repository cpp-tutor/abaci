#ifndef Abaci_hpp
#define Abaci_hpp

#include "utility/Type.hpp"
#include "utility/Operator.hpp"
#include "utility/Context.hpp"
#include <cstdint>

namespace abaci::lib {

using abaci::utility::Complex;
using abaci::utility::String;
using abaci::utility::Instance;
using abaci::utility::List;
using abaci::utility::Operator;
using abaci::utility::Context;
using abaci::utility::AbaciValue;

Complex *makeComplex(double real, double imag);
String *makeString(char8_t *str, std::size_t len);
Instance *makeInstance(char8_t *className, std::size_t size);
List *makeList(std::size_t size);

Complex *cloneComplex(Complex *existing);
String *cloneString(String *existing);
Instance *cloneInstance(Instance *existing);
List *cloneList(List *existing);

void destroyComplex(Complex *object);
void destroyString(String *object);
void destroyInstance(Instance *object);
void destroyList(List *object);

bool compareString(String *str1, String *str2);
String *concatString(String *str1, String *str2);
String *indexString(String *object, std::size_t index);
Complex *opComplex(Operator op, Complex *operand1, Complex *operand2);
void deleteElement(List *object, std::size_t element);

String *userInput(Context *ctx);
AbaciValue toType(int toType, AbaciValue value, int fromType);

template<typename T>
void printValue(Context *ctx, T value);

void printComma(Context *ctx);
void printLn(Context *ctx);

} // namespace abaci::lib

#endif