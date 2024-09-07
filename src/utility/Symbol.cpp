#include "Symbol.hpp"
#include "engine/JIT.hpp"
#include "engine/Utility.hpp"
#include "lib/Abaci.hpp"
#include "localize/Keywords.hpp"
#include <limits>
#include <iostream>

namespace abaci::utility {

using abaci::engine::JIT;
using abaci::engine::makeMutableValue;
using abaci::engine::destroyValue;
using abaci::engine::loadMutableValue;
using abaci::lib::destroyComplex;
using abaci::lib::destroyString;
using abaci::lib::destroyInstance;

void LocalSymbols::makeVariables(JIT& jit) {
    for (auto& symbol : symbols) {
        symbol.first = makeMutableValue(jit, symbol.second);
    }
}

void LocalSymbols::destroyVariables(JIT& jit) {
    if (auto [ vars, index ] = getIndex(THIS_V, true); index != noVariable) {
        symbols.erase(symbols.begin() + index);
        identifiers.erase(THIS_V);
    }
    for (auto iter = symbols.crbegin(); iter != symbols.crend(); ++iter) {
        switch (typeToScalar(removeConstFromType(iter->second))) {
            case AbaciValue::Boolean:
            case AbaciValue::Integer:
            case AbaciValue::Floating:
                break;
            case AbaciValue::Complex:
            case AbaciValue::String:
            case AbaciValue::Instance:
                destroyValue(jit, loadMutableValue(jit, iter->first, iter->second), iter->second);
                break;
            default:
                UnexpectedError0(BadType);
        }
    }
}

const std::size_t LocalSymbols::noVariable = std::numeric_limits<std::size_t>::max();

const std::size_t GlobalSymbols::noVariable = std::numeric_limits<std::size_t>::max();

static void deleteValue(AbaciValue value, const Type& type) {
    switch (typeToScalar(removeConstFromType(type))) {
        case AbaciValue::Boolean:
        case AbaciValue::Integer:
        case AbaciValue::Floating:
            break;
        case AbaciValue::Complex:
            destroyComplex(reinterpret_cast<Complex*>(value.object));
            break;
        case AbaciValue::String:
            destroyString(reinterpret_cast<String*>(value.object));
            break;
        case AbaciValue::Instance: {
            auto *instance = reinterpret_cast<Instance*>(value.object);
            auto instanceType = std::dynamic_pointer_cast<TypeInstance>(std::get<std::shared_ptr<TypeBase>>(type));
            for (std::size_t index = 0; index != instance->variablesCount; ++index) {
                deleteValue(instance->variables[index], instanceType->variableTypes.at(index));
            }
            destroyInstance(instance);
            break;
        }
        default:
            try {
                UnexpectedError0(BadType);
            }
            catch (...) {}
            break;
    }
}

void GlobalSymbols::deleteAll(AbaciValue* rawArray) const {
    for (int index = 0; const auto& type : symbols) {
        deleteValue(rawArray[index++], type);
    }
}

} // namespace abaci::utility
