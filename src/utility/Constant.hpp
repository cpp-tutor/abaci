#ifndef Constant_hpp
#define Constant_hpp

#include "Type.hpp"
#include <vector>
#include <variant>
#include <utility>
#include <algorithm>

namespace abaci::utility {

class Constants {
    std::vector<std::pair<AbaciValue,Type>> constants;
public:
    std::size_t add(AbaciValue value, const Type& type) {
        if (auto iter = std::find_if(constants.begin(), constants.end(),
                [&value,type = typeToScalar(type)](const auto& match){
                    return type < AbaciValue::Complex && match.first.integer == value.integer;
                }
            ); 1 || constants.empty() || iter != constants.end()) {
            constants.emplace_back(std::pair{ value, type });
            return constants.size() - 1;
        }
        else {
            return iter - constants.begin();
        }
    }
    template<typename T>
    std::size_t add(const T& value);
    const auto& get(std::size_t index) const {
        return constants.at(index);
    }
    AbaciValue getValue(std::size_t index) const {
        return constants.at(index).first;
    }
    const auto& getType(std::size_t index) const {
        return constants.at(index).second;
    }
    ~Constants();
};

} // namespace abaci::utility

#endif
