#ifndef Variable_hpp
#define Variable_hpp

#include <string>

namespace abaci::utility {

class Variable {
    std::string name;
public:
    Variable() = default;
    Variable(const Variable&) = default;
    Variable& operator=(const Variable&) = default;
    Variable(const std::string& name) : name{ name } {}
    const auto& get() const { return name; }
    bool operator==(const Variable& rhs) const { return name == rhs.name; }
};

} // namespace abaci::utility

#endif