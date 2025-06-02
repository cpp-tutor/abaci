#ifndef JIT_hpp
#define JIT_hpp

#include "utility/Type.hpp"
#include "utility/Report.hpp"
#include "utility/Context.hpp"
#include "localize/Messages.hpp"
#include "Cache.hpp"
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <string>

namespace abaci::engine {

using llvm::LLVMContext;
using llvm::Module;
using llvm::IRBuilder;
using llvm::StructType;
using llvm::Function;
using llvm::FunctionType;
using llvm::GlobalVariable;
using llvm::orc::LLJITBuilder;
using llvm::orc::LLJIT;
using abaci::utility::AbaciValue;
using abaci::utility::Context;
using LLVMType = llvm::Type*;
using ExecFunctionType = void(*)();

class JIT {
    std::unique_ptr<LLVMContext> context;
    std::unique_ptr<Module> module;
    IRBuilder<> builder;
    std::string moduleName, functionName;
    Function *currentFunction{ nullptr };
    Cache *cache;
    Context *runtimeContext;
    GlobalVariable *globalContext;
    LLJITBuilder jitBuilder;
    llvm::Expected<std::unique_ptr<LLJIT>> jit{ nullptr };
    //std::unordered_map<std::string,void*> symbols;
    void initialize();
public:
    JIT(const std::string& moduleName, const std::string& functionName, Context *runtimeContext, Cache *cache)
        : context{ std::make_unique<LLVMContext>() },
        module{ std::make_unique<Module>(moduleName, *context) }, builder{ *context },
        moduleName{ moduleName }, functionName{ functionName }, cache{ cache },
        runtimeContext{ runtimeContext } {
            try {
                initialize();
            }
            catch (std::exception&) {
                cache->clearInstantiations();
                throw;
            }
    }
    auto& getContext() { return *context; }
    auto& getModule() { return *module; }
    auto& getBuilder() { return builder; }
    auto getFunction() { return currentFunction; }
    auto getCache() { return cache; }
    auto& getConstants() { return *(runtimeContext->constants); }
    auto& getRuntimeContext() { return *runtimeContext; }
    StructType *getNamedType(const std::string& name) const {
        StructType *type = StructType::getTypeByName(*context, name);
        if (!type) {
            UnexpectedError1(NoType, name);
        }
        return type;
    }
    void addJITFunction(const std::string& name, const std::vector<LLVMType> parameterTypes, LLVMType returnType) {
        auto *functionType = FunctionType::get(returnType, parameterTypes, false);
        Function::Create(functionType, Function::ExternalLinkage, name, module.get());
    }
    ExecFunctionType getExecFunction();
    ~JIT() {
        cache->clearInstantiations();
    }
};

} // namespace abaci::engine

#endif
