#include "Constant.hpp"
#include "Report.hpp"
#include "localize/Messages.hpp"
#include "lib/Abaci.hpp"

namespace abaci::utility {

using abaci::lib::makeComplex;
using abaci::lib::makeString;
using abaci::lib::destroyComplex;
using abaci::lib::destroyString;

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
