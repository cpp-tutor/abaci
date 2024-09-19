#include "Cache.hpp"
#include "codegen/TypeCodeGen.hpp"
#include "utility/Report.hpp"
#include "localize/Messages.hpp"
#include <algorithm>

namespace abaci::engine {

using abaci::codegen::TypeCodeGen;
using abaci::utility::mangled;

void Cache::addClassTemplate(const std::string& name, const std::vector<Variable>& variables, const std::vector<std::string>& methods) {
    auto iter = classes.find(name);
    if (iter == classes.end()) {
        classes.insert({ name, { variables, methods }});
    }
    else {
        LogicError1(ClassExists, name);
    }
}

void Cache::addFunctionTemplate(const std::string& name, const std::vector<Variable>& parameters, const StmtList& body) {
    auto iter = functions.find(name);
    if (iter == functions.end()) {
        functions.insert({ name, { parameters, body }});
    }
    else {
        LogicError1(FunctionExists, name);
    }
}

void Cache::addFunctionInstantiation(const std::string& name, const std::vector<Type>& types, LocalSymbols *params, Context *context) {
    auto iter = functions.find(name);
    if (iter != functions.end()) {
        if (types.size() == iter->second.parameters.size()) {
            bool create_instantiation = true;
            auto mangledName = mangled(name, types);
            for (const auto& instantiation : instantiations) {
                if (mangledName == mangled(instantiation.name, instantiation.parameterTypes)) {
                    create_instantiation = false;
                    break;
                }
            }
            if (create_instantiation) {
                instantiations.push_back({ name, types, AbaciValue::None });
                TypeCodeGen genReturnType(context, this, params, TypeCodeGen::GetReturnType);
                genReturnType(iter->second.body);
                auto iter = std::find_if(instantiations.begin(), instantiations.end(),
                        [&](const auto& elem){
                            return mangled(elem.name, elem.parameterTypes) == mangled(name, types);
                        });
                if (iter != instantiations.end()) {
                    instantiations.erase(iter);
                }
                instantiations.push_back({ name, types, genReturnType.get() });
            }
        }
        else {
            LogicError2(WrongArgs, types.size(), iter->second.parameters.size());
        }
    }
    else {
        LogicError1(FunctionNotExist, name);
    }
}

Type Cache::getFunctionInstantiationType(const std::string& name, const std::vector<Type>& types) const {
    auto mangledName = mangled(name, types);
    for (const auto& instantiation : instantiations) {
        if (mangledName == mangled(instantiation.name, instantiation.parameterTypes)) {
            return instantiation.returnType;
        }
    }
    UnexpectedError1(NoInst, name);
}

const Cache::Function& Cache::getFunction(const std::string& name) const {
    auto iter = functions.find(name);
    if (iter != functions.end()) {
        return iter->second;
    }
    UnexpectedError1(FunctionNotExist, name);
}

const Cache::Class& Cache::getClass(const std::string& name) const {
    auto iter = classes.find(name);
    if (iter != classes.end()) {
        return iter->second;
    }
    UnexpectedError1(ClassNotExist, name);
}

unsigned Cache::getMemberIndex(const Class& cacheClass, const Variable& member) const {
    auto iter = std::find(cacheClass.variables.begin(), cacheClass.variables.end(), member);
    if (iter != cacheClass.variables.end()) {
        return iter - cacheClass.variables.begin();
    }
    LogicError1(DataNotExist, member.get());
}

Cache::CacheType Cache::getCacheType(const std::string& name) const {
    auto iter1 = functions.find(name);
    if (iter1 != functions.end()) {
        return CacheFunction;
    }
    auto iter2 = classes.find(name);
    if (iter2 != classes.end()) {
        return CacheClass;
    }
    return CacheNone;
}

} // namespace abaci::engine
