#ifndef Type_hpp
#define Type_hpp

#include <llvm/IR/Value.h>
#include <vector>
#include <string>
#include <variant>
#include <unordered_map>
#include <memory>
#include <cstdint>
#include <cstring>

namespace abaci::utility {

union AbaciValue;

struct Complex {
    double real, imag;
};

struct String {
    char8_t *ptr;
    std::size_t length;
};

struct Instance {
    char8_t *className;
    std::size_t variablesCount{};
    AbaciValue *variables;
};

struct List {
    std::size_t length;
    AbaciValue *elements;
};

union AbaciValue {
    enum Type { None, Boolean, Integer, Floating, Complex, String, Instance, List, TypeMask = 15, Real, Imag, Constant = 64 };
    uint64_t integer{};
    double floating;
    void *object;
};

static_assert(sizeof(AbaciValue) == 8, "AbaciValue must be exactly 64 bits.");

struct TypeBase {
    bool isConstant{ false };
    virtual ~TypeBase() = default;
};

using Type = std::variant<AbaciValue::Type,std::shared_ptr<TypeBase>>;

struct TypeInstance : TypeBase {
    std::string className;
    std::vector<Type> variableTypes;
};

struct TypeList : TypeBase {
    Type elementType;
    };

AbaciValue::Type typeToScalar(const Type& type);
std::string typeToString(const Type& type);
bool isConstant(const Type& type);
Type addConstToType(const Type& type);
Type removeConstFromType(const Type& type);
std::string mangled(const std::string& name, const std::vector<Type>& types);
bool operator==(const Type& lhs, const Type& rhs);

using TypedValue = std::pair<llvm::Value*,Type>;
extern const std::unordered_map<std::string,AbaciValue::Type> TypeConversions;
extern const std::unordered_map<AbaciValue::Type,std::vector<AbaciValue::Type>> ValidConversions;

} // namespace abaci::utility

#endif
