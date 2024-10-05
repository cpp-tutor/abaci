#include "Type.hpp"
#include "Report.hpp"
#include "localize/Messages.hpp"
#include "localize/Keywords.hpp"
#include <charconv>

namespace abaci::utility {

AbaciValue::Type typeToScalar(const Type& type) {
    if (std::holds_alternative<AbaciValue::Type>(type)) {
        return std::get<AbaciValue::Type>(type);
    }
    else if (std::holds_alternative<std::shared_ptr<TypeBase>>(type)) {
        auto typePtr = std::get<std::shared_ptr<TypeBase>>(type);
        AbaciValue::Type scalarConstant = typePtr->isConstant ? AbaciValue::Constant : AbaciValue::None;
        if (std::dynamic_pointer_cast<TypeInstance>(typePtr)) {
            return static_cast<AbaciValue::Type>(AbaciValue::Instance | scalarConstant);
        }
        else if (std::dynamic_pointer_cast<TypeList>(typePtr)) {
            return static_cast<AbaciValue::Type>(AbaciValue::List | scalarConstant);
        }
        else {
            UnexpectedError0(BadType);
        }
    }
    else {
        UnexpectedError0(BadType);
    }
}

std::string typeToString(const Type& type) {
    if (std::holds_alternative<std::shared_ptr<TypeBase>>(type)) {
        if (auto instance = std::dynamic_pointer_cast<TypeInstance>(std::get<std::shared_ptr<TypeBase>>(type))) {
            std::string instanceType;
            instanceType = instance->className;
            auto separator = "";
            instanceType.append("(");
            for (const auto& type : instance->variableTypes) {
                instanceType.append(separator);
                instanceType.append(typeToString(type));
                separator = ",";
            }
            instanceType.append(")");
            return instanceType;
        }
        else if (auto list = std::dynamic_pointer_cast<TypeList>(std::get<std::shared_ptr<TypeBase>>(type))) {
            return "[" + typeToString(list->elementType) + "]";
        }
        UnexpectedError0(NotImpl);
    }
    auto scalarType = typeToScalar(removeConstFromType(type));
    for (const auto& item : TypeConversions) {
        if (item.second == scalarType) {
            return item.first;
        }
    }
    return BadType;
}

bool isConstant(const Type& type) {
    if (std::holds_alternative<AbaciValue::Type>(type)) {
        return std::get<AbaciValue::Type>(type) & AbaciValue::Constant;
    }
    else if (std::holds_alternative<std::shared_ptr<TypeBase>>(type)) {
        return std::get<std::shared_ptr<TypeBase>>(type)->isConstant;
    }
    else {
        UnexpectedError0(BadType);
    }
}

Type addConstToType(const Type& type) {
    if (std::holds_alternative<AbaciValue::Type>(type)) {
        return static_cast<AbaciValue::Type>(std::get<AbaciValue::Type>(type) | AbaciValue::Constant);
    }
    else if (std::holds_alternative<std::shared_ptr<TypeBase>>(type)) {
        if (auto instance = std::dynamic_pointer_cast<TypeInstance>(std::get<std::shared_ptr<TypeBase>>(type))) {
            auto constType = std::make_shared<TypeInstance>(*instance);
            constType->isConstant = true;
            return constType;
        }
        else if (auto list = std::dynamic_pointer_cast<TypeList>(std::get<std::shared_ptr<TypeBase>>(type))) {
            auto constType = std::make_shared<TypeList>(*list);
            constType->isConstant = true;
            return constType;
        }
        UnexpectedError0(NotImpl);
    }
    else {
        UnexpectedError0(BadType);
    }
}

Type removeConstFromType(const Type& type) {
    if (std::holds_alternative<AbaciValue::Type>(type)) {
        return static_cast<AbaciValue::Type>(std::get<AbaciValue::Type>(type) & ~AbaciValue::Constant);
    }
    else if (std::holds_alternative<std::shared_ptr<TypeBase>>(type)) {
        if (auto instance = std::dynamic_pointer_cast<TypeInstance>(std::get<std::shared_ptr<TypeBase>>(type))) {
            auto nonConstType = std::make_shared<TypeInstance>(*instance);
            nonConstType->isConstant = false;
            return nonConstType;
        }
        else if (auto list = std::dynamic_pointer_cast<TypeList>(std::get<std::shared_ptr<TypeBase>>(type))) {
            auto constType = std::make_shared<TypeList>(*list);
            constType->isConstant = false;
            return constType;
        }
        UnexpectedError0(NotImpl);
    }
    else {
        UnexpectedError0(BadType);
    }
}

std::string mangled(const std::string& name, const std::vector<Type>& types) {
    std::string functionName;
    for (unsigned char ch : name) {
        if (ch >= 0x80 || ch == '\'') {
            functionName.push_back('.');
            char buffer[16];
            auto [ptr, ec] = std::to_chars(buffer, buffer + sizeof(buffer), static_cast<int>(ch), 16);
            if (ec != std::errc()) {
                UnexpectedError0(BadNumericConv);
            }
            *ptr = '\0';
            functionName.append(buffer);
        }
        else if (isalnum(ch) || ch == '_' || ch == '.') {
            functionName.push_back(ch);
        }
        else {
            UnexpectedError0(BadChar);
        }
    }
    for (const auto& parameterType : types) {
        functionName.push_back('.');
        if (std::holds_alternative<AbaciValue::Type>(parameterType)) {
            char buffer[16];
            auto [ptr, ec] = std::to_chars(buffer, buffer + sizeof(buffer),
                static_cast<int>(std::get<AbaciValue::Type>(parameterType) & AbaciValue::TypeMask), 10);
            if (ec != std::errc()) {
                UnexpectedError0(BadNumericConv);
            }
            *ptr = '\0';
            functionName.append(buffer);
        }
        else if (std::holds_alternative<std::shared_ptr<TypeBase>>(parameterType)) {
            if (auto instance = std::dynamic_pointer_cast<TypeInstance>(std::get<std::shared_ptr<TypeBase>>(parameterType))) {
                for (unsigned char ch : instance->className) {
                    if (ch >= 0x80 || ch == '\'') {
                        functionName.push_back('.');
                        char buffer[16];
                        auto [ptr, ec] = std::to_chars(buffer, buffer + sizeof(buffer), static_cast<int>(ch), 16);
                        if (ec != std::errc()) {
                            UnexpectedError0(BadNumericConv);
                        }
                        *ptr = '\0';
                        functionName.append(buffer);
                    }
                    else if (isalnum(ch) || ch == '_' || ch == '.') {
                        functionName.push_back(ch);
                    }
                    else {
                        UnexpectedError0(BadChar);
                    }
                }
            }
            else {
                UnexpectedError0(NotImpl);
            }
        }
    }
    return functionName;
}


bool operator==(const Type& lhs, const Type& rhs) {
    if (lhs.index() != rhs.index()) {
        return false;
    }
    else if (std::holds_alternative<AbaciValue::Type>(lhs)) {
        return (std::get<AbaciValue::Type>(lhs) & ~AbaciValue::Constant)
            == (std::get<AbaciValue::Type>(rhs) & ~AbaciValue::Constant);
    }
    else if (std::holds_alternative<std::shared_ptr<TypeBase>>(lhs)) {
        if (auto lhsPtr = std::dynamic_pointer_cast<TypeInstance>(std::get<std::shared_ptr<TypeBase>>(lhs)),
            rhsPtr = std::dynamic_pointer_cast<TypeInstance>(std::get<std::shared_ptr<TypeBase>>(rhs)); lhsPtr && rhsPtr) {
            if (lhsPtr->className != rhsPtr->className) {
                return false;
            }
            else {
                for (std::size_t index = 0; const auto& type : lhsPtr->variableTypes) {
                    if (type != rhsPtr->variableTypes.at(index++)) {
                        return false;
                    }
                }
                return true;
            }
        }
        else if (auto lhsPtr = std::dynamic_pointer_cast<TypeList>(std::get<std::shared_ptr<TypeBase>>(lhs)),
            rhsPtr = std::dynamic_pointer_cast<TypeList>(std::get<std::shared_ptr<TypeBase>>(rhs)); lhsPtr && rhsPtr) {
            if (lhsPtr->elementType != rhsPtr->elementType) {
                return false;
            }
            else {
                return true;
            }
        }
        return false;
    }
    else {
        UnexpectedError0(BadType);
    }
}
    
const std::unordered_map<std::string,AbaciValue::Type> TypeConversions{
    { NIL, AbaciValue::None },
    { BOOL, AbaciValue::Boolean },
    { INT, AbaciValue::Integer },
    { FLOAT, AbaciValue::Floating },
    { COMPLEX, AbaciValue::Complex },
    { STR, AbaciValue::String },
    { REAL, AbaciValue::Real },
    { IMAG, AbaciValue::Imag }
};

const std::unordered_map<AbaciValue::Type,std::vector<AbaciValue::Type>> ValidConversions{
    { AbaciValue::Integer, { AbaciValue::Boolean, AbaciValue::Integer, AbaciValue::Floating, AbaciValue::String } },
    { AbaciValue::Floating, { AbaciValue::Boolean, AbaciValue::Integer, AbaciValue::Floating, AbaciValue::String } },
    { AbaciValue::Complex, { AbaciValue::Integer, AbaciValue::Floating, AbaciValue::Complex, AbaciValue::String } },
    { AbaciValue::String, { AbaciValue::Boolean, AbaciValue::Integer, AbaciValue::Floating, AbaciValue::Complex, AbaciValue::String } },
    { AbaciValue::Real, { AbaciValue::Complex } },
    { AbaciValue::Imag, { AbaciValue::Complex } }
};

} // namespace abaci::utility