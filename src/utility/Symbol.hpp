#ifndef Symbol_hpp
#define Symbol_hpp

#include <llvm/ADT/APFloat.h>
#include "Type.hpp"
#include "engine/JITfwd.hpp"
#include "Report.hpp"
#include "localize/Messages.hpp"
#include <vector>
#include <string>
#include <unordered_map>
#include <utility>
#include <cstdint>

namespace abaci::utility {

class LocalSymbols;

class LocalSymbols {
    std::vector<TypedValue> symbols;
    std::unordered_map<std::string,std::size_t> identifiers;
    LocalSymbols *enclosing;
public:
    LocalSymbols(LocalSymbols *enclosing = nullptr) : enclosing{ enclosing } {}
    void setEnclosing(LocalSymbols *encl) {
        enclosing = encl;
    }
    LocalSymbols *getEnclosing() const {
        return enclosing;
    }
    void setValue(std::size_t index, llvm::Value *value) {
        symbols.at(index).first = value;
    }
    std::size_t add(const std::string& name, llvm::Value *value, const Type& type) {
        identifiers.insert({ name, symbols.size() });
        symbols.push_back({ value, type });
        return symbols.size() - 1;
    }
    std::pair<LocalSymbols*,std::size_t> getIndex(const std::string& name, bool noEnclosing = false) {
        auto iter = identifiers.find(name);
        if (iter != identifiers.end()) {
            return { this, iter->second };
        }
        else if (!noEnclosing && enclosing != nullptr) {
            return enclosing->getIndex(name);
        }
        else {
            return { nullptr, noVariable };
        }
    }
    auto *getValue(std::size_t index) const {
        if (index < symbols.size()) {
            return symbols.at(index).first;
        }
        else {
            UnexpectedError0(BadVarIndex);
        }
    }
    const auto& getType(std::size_t index) const {
        if (index < symbols.size()) {
            return symbols.at(index).second;
        }
        else {
            UnexpectedError0(BadVarIndex);
        }
    }
    std::size_t size() const {
        return symbols.size();
    }
    void clear() {
        symbols.clear();
    }
    void makeVariables(abaci::engine::JIT& jit);
    void destroyVariables(abaci::engine::JIT& jit);
    static const std::size_t noVariable;
};

class GlobalSymbols {
    std::vector<Type> symbols;
    std::unordered_map<std::string,std::size_t> identifiers;
public:
    GlobalSymbols() = default;
    std::size_t add(const std::string& name, const Type& type) {
        identifiers.insert({ name, symbols.size() });
        symbols.push_back(type);
        return symbols.size() - 1;
    }
    std::size_t getIndex(const std::string& name) const {
        if (auto iter = identifiers.find(name); iter != identifiers.end()) {
            return iter->second;
        }
        else {
            return noVariable;
        }
    }
    const auto& getType(std::size_t index) const {
        if (index < symbols.size()) {
            return symbols.at(index);
        }
        else {
            UnexpectedError0(BadVarIndex);
        }
    }
    std::size_t size() const {
        return symbols.size();
    }
    void deleteAll(AbaciValue* rawArray) const;
    static const std::size_t noVariable;
};

} // namespace abaci::utility

#endif