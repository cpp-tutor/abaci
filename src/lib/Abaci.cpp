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

static std::size_t utf8StrLen(const char8_t *str, std::size_t sz);
static const char8_t *utf8StrPos(const char8_t *str, std::size_t index);

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
    object->utf8Length = utf8StrLen(str, length);
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
    object->utf8Length = existing->utf8Length;
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
    object->utf8Length = str1->utf8Length + str2->utf8Length;
    object->length = str1->length + str2->length;
    object->ptr = new char8_t[object->length];
    memcpy(object->ptr, str1->ptr, str1->length);
    memcpy(object->ptr + static_cast<ptrdiff_t>(str1->length), str2->ptr, str2->length);
    return object;
}

std::size_t validIndex(int64_t index, std::size_t limit, bool isSlice) {
    std::size_t positiveIndex = (index >= 0) ? index : limit + index - isSlice;
    if (positiveIndex >= (limit + isSlice)) {
        LogicError2(IndexOutOfRange, index, limit);
    }
    return positiveIndex;
}

String *indexString(String *object, std::size_t index) {
    index = validIndex(index, object->utf8Length, false);
    const char8_t *posBegin = utf8StrPos(object->ptr, index), *posEnd = utf8StrPos(posBegin, 1);
    return makeString(const_cast<char8_t*>(posBegin), static_cast<std::size_t>(posEnd - posBegin));
}

String *sliceString(String *object, std::size_t indexBegin, std::size_t indexEnd) {
    indexBegin = validIndex(indexBegin, object->utf8Length, true);
    indexEnd = validIndex(indexEnd, object->utf8Length, true);
    if (indexBegin > indexEnd) {
        indexBegin = indexEnd;
    }
    const char8_t *posBegin = utf8StrPos(object->ptr, indexBegin), *posEnd = utf8StrPos(posBegin, indexEnd - indexBegin);
    return makeString(const_cast<char8_t*>(posBegin), static_cast<std::size_t>(posEnd - posBegin));
}

void spliceString(String *object, std::size_t indexBegin, std::size_t indexEnd, String *splice) {
    indexBegin = validIndex(indexBegin, object->length, true);
    indexEnd = validIndex(indexEnd, object->length, true);
    if (indexBegin > indexEnd) {
        indexBegin = indexEnd;
    }
    const char8_t *posBegin = utf8StrPos(object->ptr, indexBegin), *posEnd = utf8StrPos(posBegin, indexEnd - indexBegin);
    std::size_t rawBegin = posBegin - object->ptr, rawEnd = posEnd - object->ptr;
    if (splice == nullptr) {
        memmove(const_cast<char8_t*>(posBegin), posEnd, object->length - static_cast<std::size_t>(posEnd - object->ptr));
        object->utf8Length -= indexEnd - indexBegin;
        object->length -= rawEnd - rawBegin;
    }
    else if ((rawEnd - rawBegin) >= splice->length) {
        memcpy(const_cast<char8_t*>(posBegin), splice->ptr, splice->length);
        memmove(const_cast<char8_t*>(posBegin) + splice->length, posEnd, object->length - rawEnd);
        object->utf8Length += -(indexEnd - indexBegin) + splice->utf8Length;
        object->length += -(rawEnd - rawBegin) + splice->length;
    }
    else {
        auto *str = new char8_t[object->length - (rawEnd - rawBegin) + splice->length];
        memcpy(str, object->ptr, rawBegin);
        memcpy(str + rawBegin, splice->ptr, splice->length);
        memcpy(str + rawBegin + splice->length, posEnd, object->length - rawEnd);
        delete[] object->ptr;
        object->ptr = str;
        object->utf8Length += -(indexEnd - indexBegin) + splice->utf8Length;
        object->length += -(rawEnd - rawBegin) + splice->length;
    }
}

List *sliceList(List *existing, std::size_t indexBegin, std::size_t indexEnd) {
    indexBegin = validIndex(indexBegin, existing->length, true);
    indexEnd = validIndex(indexEnd, existing->length, true);
    if (indexBegin > indexEnd) {
        indexBegin = indexEnd;
    }
    auto *object = new List{};
    object->elements = existing->elements + indexBegin;
    object->length = indexEnd - indexBegin;
    return object;
}

List *spliceList(List *object, std::size_t indexBegin, std::size_t indexEnd, List *splice, bool needDeleted) {
    List *deleted = nullptr;
    indexBegin = validIndex(indexBegin, object->length, true);
    indexEnd = validIndex(indexEnd, object->length, true);
    if (indexBegin > indexEnd) {
        indexBegin = indexEnd;
    }
    if (needDeleted && (indexEnd - indexBegin) != 0) {
        deleted = makeList(indexEnd - indexBegin);
        memcpy(deleted->elements, object->elements + indexBegin, (indexEnd - indexBegin) * sizeof(AbaciValue));
    }
    if (splice == nullptr) {
        memmove(object->elements + indexBegin, object->elements + indexEnd, (object->length - indexEnd) * sizeof(AbaciValue));
        object->length -= indexEnd - indexBegin;
    }
    else if ((indexEnd - indexBegin) >= splice->length) {
        memcpy(object->elements + indexBegin, splice->elements, splice->length * sizeof(AbaciValue));
        memmove(object->elements + indexBegin + splice->length, object->elements + indexEnd, (object->length - indexEnd) * sizeof(AbaciValue));
        object->length += -(indexEnd - indexBegin) + splice->length;
    }
    else {
        auto *elems = new AbaciValue[object->length - (indexEnd - indexBegin) + splice->length];
        memcpy(elems, object->elements, indexBegin * sizeof(AbaciValue));
        memcpy(elems + indexBegin, splice->elements, splice->length * sizeof(AbaciValue));
        memcpy(elems + indexBegin + splice->length, object->elements + indexEnd, (object->length - indexEnd) * sizeof(AbaciValue));
        delete[] object->elements;
        object->elements = elems;
        object->length += -(indexEnd - indexBegin) + splice->length;
    }
    return deleted;
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

static std::size_t utf8StrLen(const char8_t *str, std::size_t sz) {
    std::size_t length{};
    const char8_t *ptr = str;
    while (ptr < (str + static_cast<ptrdiff_t>(sz))) {
        if (*ptr < 0b10000000) {
            ++length;
            ++ptr;
        }
        else if (*ptr < 0b11000000) {
            UnexpectedError1(BadString, ptr - str);
        }
        else if (*ptr < 0b11100000) {
            ++length;
            ptr += 2;
        }
        else if (*ptr < 0b11110000) {
            ++length;
            ptr += 3;
        }
        else if (*ptr < 0b11111000) {
            ++length;
            ptr += 4;
        }
        else {
            UnexpectedError1(BadString, ptr - str);
        }
    }
    return length;
}

static const char8_t *utf8StrPos(const char8_t *str, std::size_t index) {
    const char8_t *pos = str;
    std::size_t ch{};
    while (ch < index) {
        if (*pos < 0b10000000) {
            ++ch;
            ++pos;
        }
        else if (*pos < 0b11000000) {
            UnexpectedError1(BadString, pos - str);
        }
        else if (*pos < 0b11100000) {
            ++ch;
            pos += 2;
        }
        else if (*pos < 0b11110000) {
            ++ch;
            pos += 3;
        }
        else if (*pos < 0b11111000) {
            ++ch;
            pos += 4;
        }
        else {
            UnexpectedError1(BadString, pos - str);
        }
    }
    return pos;
}


} // namespace abaci::lib
