#include "Cache.hpp"
#include "codegen/TypeCodeGen.hpp"
#include "utility/Report.hpp"
#include "localize/Messages.hpp"
#include <algorithm>

using namespace std::literals;
#ifdef _WIN64
#include <windows.h>
typedef HMODULE LibraryHandle;
#define LIBRARY_SUFFIX ".dll"s
#define LIBRARY_PREFIX1 ""s
#define LIBRARY_PREFIX2 "lib"s
#else
#include <dlfcn.h>
typedef void *LibraryHandle;
#define LIBRARY_SUFFIX ".so"s
#define LIBRARY_PREFIX1 "./lib"s
#define LIBRARY_PREFIX2 "lib"s
#endif

namespace abaci::engine {

using abaci::codegen::TypeCodeGen;
using abaci::utility::mangled;
using LibrariesMap = std::unordered_map<std::string,LibraryHandle>;

void Cache::addClassTemplate(const std::string& name, const std::vector<Parameter>& variables, const std::vector<std::string>& methods) {
    auto iter = classes.find(name);
    if (iter == classes.end()) {
        classes.insert({ name, { variables, methods }});
    }
    else {
        LogicError1(ClassExists, name);
    }
}

void Cache::addFunctionTemplate(const std::string& name, const std::vector<Parameter>& parameters, const Type& returnType, const StmtList& body) {
    auto iter = functions.find(name);
    if (iter == functions.end()) {
        functions.insert({ name, { parameters, returnType, body }});
    }
    else {
        LogicError1(FunctionExists, name);
    }
}

#ifdef _WIN64
static std::wstring ConvertStringToWString(std::string&& str) {
    std::wstring wstr;
    int len = static_cast<int>(str.size());
    int wideLen = 0;
    int flags = MB_ERR_INVALID_CHARS;

    if (MultiByteToWideChar(CP_UTF8, flags, reinterpret_cast<const char*>(str.c_str()), len, nullptr, 0) == 0) {
        return wstr;
    }
    wideLen = MultiByteToWideChar(CP_UTF8, flags, reinterpret_cast<const char*>(str.c_str()), len, nullptr, 0);
    if (wideLen <= 0) {
        return wstr;
    }

    wstr.resize(wideLen);
    if (MultiByteToWideChar(CP_UTF8, flags, reinterpret_cast<const char*>(str.c_str()), len, &wstr[0], wideLen) == 0) {
        return std::wstring();
    }
    return wstr;
}
#endif

void Cache::addNativeFunction(const std::string& libraryName, const std::string& name, const std::vector<NativeType>& parameters, NativeType result) {
    auto iter = std::find_if(nativeFunctions.cbegin(), nativeFunctions.cend(), [&name](const NativeFunction& elem){ return elem.name == name; });
    if (iter == nativeFunctions.end()) {
        if (!libraries) {
            libraries = reinterpret_cast<void*>(new LibrariesMap);
        }
        LibraryHandle libraryHandle;
#ifdef _WIN64
        if (libraryName.empty()) {
            libraryHandle = GetModuleHandle(NULL);
        }
        else if (auto iter = reinterpret_cast<LibrariesMap*>(libraries)->find(libraryName); iter != reinterpret_cast<LibrariesMap*>(libraries)->end()) {
            libraryHandle = iter->second;
        }
        else {
            std::wstring libraryName1 = ConvertStringToWString(LIBRARY_PREFIX1 + libraryName + LIBRARY_SUFFIX),
                libraryName2 = ConvertStringToWString(LIBRARY_PREFIX2 + libraryName + LIBRARY_SUFFIX);
            libraryHandle = LoadLibrary(libraryName1.c_str());
            if (!libraryHandle) {
                libraryHandle = LoadLibrary(libraryName2.c_str());
                if (!libraryHandle) {
                    LogicError2(BadLibrary, libraryName, name);
                }
            }
            reinterpret_cast<LibrariesMap*>(libraries)->insert({ libraryName, libraryHandle });
        }
#else
        if (libraryName.empty()) {
            libraryHandle =  nullptr;
        }
        else if (auto iter = reinterpret_cast<LibrariesMap*>(libraries)->find(libraryName); iter != reinterpret_cast<LibrariesMap*>(libraries)->end()) {
            libraryHandle = iter->second;
        }
        else {
            std::string libraryName1 = LIBRARY_PREFIX1 + libraryName + LIBRARY_SUFFIX,
                libraryName2 = LIBRARY_PREFIX2 + libraryName + LIBRARY_SUFFIX;
            libraryHandle = dlopen(libraryName1.c_str(), RTLD_LAZY);
            if (!libraryName.empty() && !libraryHandle) {
                libraryHandle = dlopen(libraryName1.c_str(), RTLD_LAZY);
                if (!libraryHandle) {
                    LogicError2(BadLibrary, libraryName, name);
                }
            }
            reinterpret_cast<LibrariesMap*>(libraries)->insert({ libraryName, libraryHandle });
        }
#endif
#ifdef _WIN64
        FARPROC functionPtr = GetProcAddress(libraryHandle, name.c_str());
#else
        auto functionPtr = dlsym(libraryHandle, name.c_str());
#endif
        if (!functionPtr) {
            LogicError1(BadNativeFn, name);
        }
        nativeFunctions.push_back({ name, parameters, result, reinterpret_cast<void*>(functionPtr) });
    }
    else {
        LogicError1(NativeDefined, name);
    }
}

Cache::~Cache() {
    if (libraries) {
        for (const auto& library : *reinterpret_cast<LibrariesMap*>(libraries)) {
#ifdef _WIN64
            FreeLibrary(library.second);
#else
            dlclose(library.second);
#endif
        }
        delete reinterpret_cast<LibrariesMap*>(libraries);
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

const Cache::NativeFunction& Cache::getNativeFunction(const std::string& name) const {
    auto iter = std::find_if(nativeFunctions.cbegin(), nativeFunctions.cend(), [&name](const NativeFunction& elem){ return elem.name == name; });
    if (iter != nativeFunctions.cend()) {
        return *iter;
    }
    UnexpectedError1(BadNativeFn, name);
}

unsigned Cache::getMemberIndex(const Class& cacheClass, const Variable& member) const {
    auto iter = std::find_if(cacheClass.variables.begin(), cacheClass.variables.end(), [&member](const Parameter& param){ return param.name.name == member.name; });
    if (iter != cacheClass.variables.end()) {
        return iter - cacheClass.variables.begin();
    }
    LogicError1(DataNotExist, member.name);
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
    auto iter3 = std::find_if(nativeFunctions.cbegin(), nativeFunctions.cend(), [&name](const NativeFunction& elem){ return elem.name == name; });
    if (iter3 != nativeFunctions.cend()) {
        return CacheNativeFunction;
    }
    return CacheNone;
}

} // namespace abaci::engine
