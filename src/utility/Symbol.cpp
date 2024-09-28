#include "Symbol.hpp"
#include "engine/JIT.hpp"
#include "Utility.hpp"
#include "lib/Abaci.hpp"
#include "localize/Keywords.hpp"
#include <limits>

namespace abaci::utility {

using abaci::engine::JIT;
using abaci::lib::destroyComplex;
using abaci::lib::destroyString;
using abaci::lib::destroyInstance;
using abaci::lib::destroyList;

void LocalSymbols::makeVariables(JIT& jit) {
    for (auto& symbol : symbols) {
        symbol.first = makeMutableValue(jit, symbol.second);
    }
}

void LocalSymbols::destroyVariables(JIT& jit) {
    if (auto [ variables, index ] = getIndex(THIS_V, true); index != noVariable) {
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
            case AbaciValue::List:
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
            if (reinterpret_cast<void*>(value.object) != nullptr) {
                destroyComplex(reinterpret_cast<Complex*>(value.object));
            }
            break;
        case AbaciValue::String:
            if (reinterpret_cast<void*>(value.object) != nullptr) {
                destroyString(reinterpret_cast<String*>(value.object));
            }
            break;
        case AbaciValue::Instance:
            if (reinterpret_cast<void*>(value.object) != nullptr) {
                auto *instance = reinterpret_cast<Instance*>(value.object);
                auto instanceType = std::dynamic_pointer_cast<TypeInstance>(std::get<std::shared_ptr<TypeBase>>(type));
                for (std::size_t index = 0; index != instance->variablesCount; ++index) {
                    deleteValue(instance->variables[index], instanceType->variableTypes.at(index));
                }
                destroyInstance(instance);
            }
            break;
        case AbaciValue::List:
            if (reinterpret_cast<void*>(value.object) != nullptr) {
                auto *list = reinterpret_cast<List*>(value.object);
                auto listType = std::dynamic_pointer_cast<TypeList>(std::get<std::shared_ptr<TypeBase>>(type));
                for (std::size_t index = 0; index != list->length; ++index) {
                    deleteValue(list->elements[index], listType->elementType);
                }
                destroyList(list);
            }
            break;
        default:
            try {
                UnexpectedError0(BadType);
            }
            catch (...) {}
            break;
    }
}

void GlobalSymbols::deleteAll(AbaciValue* rawArray) const {
    for (std::size_t index = 0; const auto& type : symbols) {
        deleteValue(rawArray[index++], type);
    }
}

} // namespace abaci::utility
