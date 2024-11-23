#include "Abaci.hpp"
#include "utility/Report.hpp"
#include "localize/Messages.hpp"
#include "localize/Keywords.hpp"
#include <complex>
#include <charconv>
#include <cstring>

#ifdef ABACI_USE_STD_FORMAT
#include <format>
#include <print>
#else
#include <fmt/core.h>
#include <fmt/format.h>
#include <fmt/ostream.h>
#endif

namespace abaci::lib {

using abaci::utility::AbaciValue;

Complex *makeComplex(double real, double imag) {
    auto *object = new Complex{};
    object->real = real;
    object->imag = imag;
    return object;
}

String *makeString(char8_t *str, std::size_t length) {
    auto *object = new String{};
    object->ptr = new char8_t[length];
    memcpy(object->ptr, str, length);
    object->length = length;
    return object;
}

Instance *makeInstance(char8_t *className, std::size_t size) {
    auto *object = new Instance{};
    auto length = strlen(reinterpret_cast<char*>(className));
    object->className = new char8_t[length + 1];
    strcpy(reinterpret_cast<char*>(object->className), reinterpret_cast<char*>(className));
    object->variablesCount = size;
    object->variables = new AbaciValue[size];
    memset(reinterpret_cast<void*>(object->variables), 0, sizeof(AbaciValue) * size);
    return object;
}

List *makeList(std::size_t size) {
    auto *object = new List{};
    object->length = size;
    object->elements = new AbaciValue[size];
    memset(reinterpret_cast<void*>(object->elements), 0, sizeof(AbaciValue) * size);
    return object;
}

Complex *cloneComplex(Complex *existing) {
    auto *object = new Complex{};
    object->real = existing->real;
    object->imag = existing->imag;
    return object;
}

String *cloneString(String *existing) {
    auto *object = new String{};
    object->ptr = new char8_t[existing->length];
    memcpy(object->ptr, existing->ptr, existing->length);
    object->length = existing->length;
    return object;
}

Instance *cloneInstance(Instance *existing) {
    if (existing != nullptr) {
        auto *object = new Instance{};
        auto length = strlen(reinterpret_cast<char*>(existing->className));
        object->className = new char8_t[length + 1];
        strcpy(reinterpret_cast<char*>(object->className), reinterpret_cast<char*>(existing->className));
        object->variablesCount = existing->variablesCount;
        object->variables = new AbaciValue[existing->variablesCount];
        memset(reinterpret_cast<void*>(object->variables), 0, sizeof(AbaciValue) * existing->variablesCount);
        return object;
    }
    else {
        return nullptr;
    }
}

List *cloneList(List *existing) {
    auto *object = new List{};
    object->length = existing->length;
    object->elements = new AbaciValue[existing->length];
    memset(reinterpret_cast<void*>(object->elements), 0, sizeof(AbaciValue) * existing->length);
    return object;
}

void destroyComplex(Complex *object) {
    delete object;
}

void destroyString(String *object) {
    delete[] object->ptr;
    delete object;
}

void destroyInstance(Instance *object) {
    if (object != nullptr) {
        delete[] object->className;
        delete[] object->variables;
        delete object;
    }
}

void destroyList(List *object) {
    delete[] object->elements;
    delete object;
}

bool compareString(String *str1, String *str2) {
    return str1->length == str2->length
        && !strncmp(reinterpret_cast<char*>(str1->ptr), reinterpret_cast<char*>(str2->ptr), str1->length);
}

String *concatString(String *str1, String *str2) {
    auto *object = new String{};
    object->length = str1->length + str2->length;
    object->ptr = new char8_t[object->length];
    memcpy(object->ptr, str1->ptr, str1->length);
    memcpy(object->ptr + static_cast<ptrdiff_t>(str1->length), str2->ptr, str2->length);
    return object;
}

String *indexString(String *object, std::size_t index) {
    return makeString(object->ptr + index, 1);
}

Complex *opComplex(Operator op, Complex *operand1, Complex *operand2) {
    std::complex<double> a(operand1->real, operand1->imag), b, r;
    if (operand2 != nullptr) {
        b = std::complex<double>(operand2->real, operand2->imag);
    }
    switch (op) {
        case Operator::Plus:
            r = a + b;
            break;
        case Operator::Minus:
            if (operand2) {
                r = a - b;
            }
            else {
                r = -a;
            }
            break;
        case Operator::Times:
            r = a * b;
            break;
        case Operator::Divide:
            r = a / b;
            break;
        case Operator::Exponent:
            r = std::pow(a, b);
            break;
        default:
            UnexpectedError0(BadOperator);
    }
    auto *object = new Complex{};
    object->real = r.real();
    object->imag = r.imag();
    return object;
}

String *userInput(Context *ctx) {
    std::string str;
    std::getline(*(ctx->input), str);
    return makeString(reinterpret_cast<char8_t*>(str.data()), str.size());
}

void deleteElement(List *object, std::size_t element) {
    --object->length;
    memmove(object->elements + element, object->elements + element + 1, (object->length - element) * sizeof(AbaciValue));
}

AbaciValue toType(int toType, AbaciValue value, int fromType) {
    AbaciValue result;
    result.integer = 0;
    switch (toType) {
        case AbaciValue::Integer:
            switch(fromType) {
                case AbaciValue::Boolean:
                    result.integer = value.integer ? 1 : 0;
                    break;
                case AbaciValue::Integer:
                    result.integer = value.integer;
                    break;
                case AbaciValue::Floating:
                    result.integer = static_cast<long long>(value.floating);
                    break;
                case AbaciValue::String: {
                    auto *object = reinterpret_cast<String*>(value.object);
                    auto *str = reinterpret_cast<const char*>(object->ptr);
                    if (strncmp(HEX_PREFIX, str, strlen(HEX_PREFIX)) == 0) {
                        std::from_chars(str + strlen(HEX_PREFIX), str + object->length, result.integer, 16);
                    }
                    else if (strncmp(BIN_PREFIX, str, strlen(BIN_PREFIX)) == 0) {
                        std::from_chars(str + strlen(BIN_PREFIX), str + object->length, result.integer, 2);
                    }
                    else if (strncmp(OCT_PREFIX, str, strlen(OCT_PREFIX)) == 0) {
                        std::from_chars(str + strlen(OCT_PREFIX), str + object->length, result.integer, 8);
                    }
                    else {
                        std::from_chars(str, str + object->length, result.integer, 10);
                    }
                    break;
                }
                default:
                    break;
            }
            break;
        case AbaciValue::Floating:
            switch(fromType) {
                case AbaciValue::Boolean:
                    result.floating = value.integer ? 1.0 : 0.0;
                    break;
                case AbaciValue::Integer:
                    result.floating = static_cast<double>(value.integer);
                    break;
                case AbaciValue::Floating:
                    result.floating = value.floating;
                    break;
                case AbaciValue::String: {
                    auto *object = reinterpret_cast<String*>(value.object);
                    auto *str = reinterpret_cast<const char*>(object->ptr);
                    std::from_chars(str, str + object->length, result.floating);
                    break;
                }
                default:
                    break;
            }
            break;
        case AbaciValue::Complex: {
            Complex *object = new Complex{};
            switch(fromType) {
                case AbaciValue::Integer:
                    object->real = static_cast<double>(value.integer);
                    object->imag = 0.0;
                    break;
                case AbaciValue::Floating:
                    object->real = value.floating;
                    object->imag = 0.0;
                    break;
                case AbaciValue::Complex:
                    object->real = static_cast<Complex*>(value.object)->real;
                    object->imag = static_cast<Complex*>(value.object)->imag;
                    break;
                case AbaciValue::String: {
                    auto *strObject = reinterpret_cast<String*>(value.object);
                    auto *str = reinterpret_cast<const char*>(strObject->ptr);
                    double d;
                    auto [ ptr, ec ] = std::from_chars(str, str + strObject->length, d);
                    if (strncmp(ptr, IMAGINARY, strlen(IMAGINARY)) == 0) {
                        object->real = 0;
                        object->imag = d;
                    }
                    else if (*ptr) {
                        object->real = d;
                        if (*ptr == '+') {
                            ++ptr;
                        }
                        std::from_chars(ptr, str + strObject->length, d);
                        object->imag = d;
                    }
                    else {
                        object->real = d;
                        object->imag = 0;
                    }
                    break;
                }
                default:
                    break;
            }
            result.object = static_cast<void*>(object);
            break;
        }
        case AbaciValue::String: {
            std::string str;
            switch (fromType) {
                case AbaciValue::Boolean:
                    str =
#ifdef ABACI_USE_STD_FORMAT
                    std::format
#else
                    fmt::format
#endif
                    ("{}", value.integer ? TRUE : FALSE);
                    break;
                case AbaciValue::Integer:
                    str =
#ifdef ABACI_USE_STD_FORMAT
                    std::format
#else
                    fmt::format
#endif
                    ("{}", static_cast<long long>(value.integer));
                    break;
                case AbaciValue::Floating:
                    str =
#ifdef ABACI_USE_STD_FORMAT
                    std::format
#else
                    fmt::format
#endif
                    ("{:.10g}", value.floating);
                    break;
                case AbaciValue::Complex:
                    str =
#ifdef ABACI_USE_STD_FORMAT
                    std::format
#else
                    fmt::format
#endif
                    ("{:.10g}", static_cast<Complex*>(value.object)->real);
                    if (static_cast<Complex*>(value.object)->imag != 0) {
                        str.append(
#ifdef ABACI_USE_STD_FORMAT
                            std::format
#else
                            fmt::format
#endif
                            ("{:+.10g}{}", static_cast<Complex*>(value.object)->imag, IMAGINARY));
                    }
                    break;
                case AbaciValue::String: {
                    auto *object = static_cast<String*>(value.object);
                    str.assign(reinterpret_cast<const char*>(object->ptr), object->length);
                    break;
                }
                default:
                    break;
            }
            result.object = static_cast<void*>(makeString(reinterpret_cast<char8_t*>(str.data()), str.size()));
            break;
        }
        case AbaciValue::Real:
            switch(fromType) {
                case AbaciValue::Complex:
                    result.floating = static_cast<Complex*>(value.object)->real;
                    break;
                default:
                    break;
            }
            break;
        case AbaciValue::Imag:
            switch(fromType) {
                case AbaciValue::Complex:
                    result.floating = static_cast<Complex*>(value.object)->imag;
                    break;
                default:
                    break;
            }
            break;
        default:
            break;
    }
    return result;
}


template<>
void printValue<bool>(Context *ctx, bool value) {
#ifdef ABACI_USE_STD_FORMAT
    std::print
#else
    fmt::print
#endif
    (*(ctx->output), "{}", (value ? TRUE : FALSE));
}

template<>
void printValue<uint64_t>(Context *ctx, uint64_t value) {
#ifdef ABACI_USE_STD_FORMAT
    std::print
#else
    fmt::print
#endif
    (*(ctx->output), "{}", static_cast<long long>(value));
}

template<>
void printValue<double>(Context *ctx, double value) {
#ifdef ABACI_USE_STD_FORMAT
    std::print
#else
    fmt::print
#endif
    (*(ctx->output), "{:.10g}", value);
}

template<>
void printValue<Complex*>(Context *ctx, Complex *value) {
#ifdef ABACI_USE_STD_FORMAT
    std::print
#else
    fmt::print
#endif
    (*(ctx->output), "{:.10g}", value->real);
    if (value->imag != 0.0) {
#ifdef ABACI_USE_STD_FORMAT
        std::print
#else
        fmt::print
#endif
        (*(ctx->output), "{:+.10g}{}", value->imag, IMAGINARY);
    }
}

template<>
void printValue<String*>(Context *ctx, String *value) {
    ctx->output->write(reinterpret_cast<const char*>(value->ptr), value->length);
}

template<>
void printValue<Instance*>(Context *ctx, Instance *value) {
    if (value != nullptr) {
#ifdef ABACI_USE_STD_FORMAT
        const auto* className = reinterpret_cast<const char*>(value->className);
        *(ctx->output) << std::vformat(InstanceOf, std::make_format_args(className));
#else
        fmt::print(*(ctx->output), fmt::runtime(InstanceOf), reinterpret_cast<const char*>(value->className));
#endif
    }
    else {
        *(ctx->output) << NIL;
    }
}

template<>
void printValue<List*>(Context *ctx, List *value) {
#ifdef ABACI_USE_STD_FORMAT
    const auto length = value->length;
    *(ctx->output) << std::vformat(ListOf, std::make_format_args(length));
#else
    fmt::print(*(ctx->output), fmt::runtime(ListOf), value->length);
#endif
}

void printComma(Context *ctx) {
#ifdef ABACI_USE_STD_FORMAT
    std::print
#else
    fmt::print
#endif
    (*(ctx->output), "{}", ' ');
}

void printLn(Context *ctx) {
#ifdef ABACI_USE_STD_FORMAT
    std::print
#else
    fmt::print
#endif
    (*(ctx->output), "{}", '\n');
}

} // namespace abaci::lib
