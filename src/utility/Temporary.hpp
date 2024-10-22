#ifndef Temporary_hpp
#define Temporary_hpp

#include "Type.hpp"
#include "Report.hpp"
#include "localize/Messages.hpp"
#include "Utility.hpp"
#include <algorithm>

namespace abaci::utility {

using abaci::engine::JIT;

class Temporaries;

class Temporaries {
    std::vector<TypedValue> temps;
    Temporaries *enclosing;
public:
    Temporaries() = default;
    Temporaries(Temporaries *enclosing) : enclosing { enclosing } {}
    void setEnclosing(Temporaries *encl) {
        enclosing = encl;
    }
    Temporaries *getEnclosing() const {
        return enclosing;
    }
    void addTemporary(const TypedValue& temp) {
        temps.push_back(temp);
    }
    bool isTemporary(llvm::Value *value) {
        return std::find_if(temps.begin(), temps.end(), [&value](const auto& elem){
                return elem.first == value;
            }) != temps.end();
    }
    void removeTemporary(llvm::Value *value) {
        if (auto iter = std::find_if(temps.begin(), temps.end(), [&value](const auto& elem){
                return elem.first == value;
            }); iter != temps.end()) {
            temps.erase(iter);
        }
        else {
            UnexpectedError0(BadTemporary);
        }
    }
    void destroyTemporaries(JIT &jit) {
        for (auto iter = temps.rbegin(); iter != temps.rend(); ++iter) {
            destroyValue(jit, iter->first, iter->second);
        }
    }
    std::size_t size() const {
        return temps.size();
    }
    void clear() {
        temps.clear();
    }
};

} // namespace abaci::utility

#endif