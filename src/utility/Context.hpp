#ifndef Context_hpp
#define Context_hpp

#include "Constant.hpp"
#include "Symbol.hpp"
#include "RawArray.hpp"
#include <istream>
#include <ostream>
#include <cstring>

namespace abaci::utility {

class Context {
public:
    AbaciValue *rawGlobals{ nullptr };
    std::istream *input;
    std::ostream *output;
    Constants *constants;
    GlobalSymbols *globals;
    RawArray<AbaciValue> rawArray;
    Context(std::istream& input, std::ostream& output, Constants& constants)
        : input{ &input }, output{ &output }, constants{ &constants },
        globals{ new GlobalSymbols }, rawArray{ &rawGlobals } {}
    ~Context() {
        globals->deleteAll(rawGlobals);
        delete globals;
        globals = nullptr;
    }
};

} // namespace abaci::utility

#endif