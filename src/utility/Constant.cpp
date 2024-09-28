#include "Constant.hpp"
#include "Report.hpp"
#include "localize/Messages.hpp"
#include "lib/Abaci.hpp"
#include <cstring>

namespace abaci::utility {

using abaci::lib::makeComplex;
using abaci::lib::makeString;
using abaci::lib::destroyComplex;
using abaci::lib::destroyString;

std::size_t Constants::add(AbaciValue value, const Type& type) {
    if (auto iter = std::find_if(constants.begin(), constants.end(),
            [&value,type = typeToScalar(type)](const auto& match){
                if (typeToScalar(match.second) != type) {
                    return false;
                }
                switch (type) {
                    case AbaciValue::None:
                        return match.first.object == value.object;
                    case AbaciValue::Boolean:
                    case AbaciValue::Integer:
                        return match.first.integer == value.integer;
                    case AbaciValue::Floating:
                        return match.first.floating == value.floating;
                    case AbaciValue::Complex: {
                        auto *matchComplex = reinterpret_cast<Complex*>(match.first.object);
                        auto *valueComplex = reinterpret_cast<Complex*>(value.object);
                        if (matchComplex->real == valueComplex->real && matchComplex->imag == valueComplex->imag) {
                            destroyComplex(valueComplex);
                            return true;
                        }
                        else {
                            return false;
                        }
                    }
                    case AbaciValue::String: {
                        auto *matchString = reinterpret_cast<String*>(match.first.object);
                        auto *valueString = reinterpret_cast<String*>(value.object);
                        if (matchString->length == valueString->length
                            && !strncmp(reinterpret_cast<const char*>(matchString->ptr),
                                reinterpret_cast<const char*>(valueString->ptr),
                                matchString->length)) {
                                    destroyString(valueString);
                                    return true;
                                }
                                else {
                                    return false;
                                }
                    }
                    default:
                        return false;
                }
            }
        ); iter == constants.end()) {
        constants.emplace_back(std::pair{ value, type });
        return constants.size() - 1;
    }
    else {
        return iter - constants.begin();
    }
}

template<>
std::size_t Constants::add(const std::nullptr_t&) {
    AbaciValue value{};
    value.object = nullptr;
    return add(value, AbaciValue::None);
}

template<>
std::size_t Constants::add(const bool& boolean) {
    AbaciValue value{};
    value.integer = boolean;
    return add(value, AbaciValue::Boolean);
}

template<>
std::size_t Constants::add(const uint64_t& integer) {
    AbaciValue value{};
    value.integer = integer;
    return add(value, AbaciValue::Integer);
}

template<>
std::size_t Constants::add(const double& floating) {
    AbaciValue value{};
    value.floating = floating;
    return add(value, AbaciValue::Floating);
}

template<>
std::size_t Constants::add(const Complex& cplx) {
    AbaciValue value{};
    value.object = makeComplex(cplx.real, cplx.imag);
    return add(value, AbaciValue::Complex);
}

template<>
std::size_t Constants::add(const std::string& str) {
    AbaciValue value{};
    value.object = makeString(reinterpret_cast<char8_t*>(const_cast<char*>(str.data())), str.size());
    return add(value, AbaciValue::String);
}

Constants::~Constants() {
    for (auto& c : constants) {
        auto type = typeToScalar(c.second);
        switch (type) {
            case AbaciValue::Boolean:
            case AbaciValue::Integer:
            case AbaciValue::Floating:
                break;
            case AbaciValue::Complex:
                destroyComplex(reinterpret_cast<Complex*>(c.first.object));
                break;
            case AbaciValue::String:
                destroyString(reinterpret_cast<String*>(c.first.object));
                break;
            default:
                try {
                    UnexpectedError0(BadType);
                }
                catch (...) {}
                break;
        }
    }
}

} // namespace abaci::utility
