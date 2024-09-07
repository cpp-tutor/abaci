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
using llvm::GlobalVariable;
using llvm::orc::LLJITBuilder;
using llvm::orc::LLJIT;
using abaci::utility::AbaciValue;
using abaci::utility::Context;
using ExecFunctionType = void(*)();

class JIT {
    std::unique_ptr<LLVMContext> context;
    std::unique_ptr<Module> module;
    IRBuilder<> builder;
    std::string module_name, function_name;
    Function *current_function{ nullptr };
    Cache *cache;
    Context *runtimeContext;
    GlobalVariable *global_context;
    LLJITBuilder jit_builder;
    llvm::Expected<std::unique_ptr<LLJIT>> jit{ nullptr };
    void initialize();
public:
    JIT(const std::string& module_name, const std::string& function_name, Context *runtimeContext, Cache *cache)
        : context{ std::make_unique<LLVMContext>() },
        module{ std::make_unique<Module>(module_name, *context) }, builder{ *context },
        module_name{ module_name }, function_name{ function_name }, cache{ cache },
        runtimeContext{ runtimeContext } {
        initialize();
    }
    auto& getContext() { return *context; }
    auto& getModule() { return *module; }
    auto& getBuilder() { return builder; }
    auto getFunction() { return current_function; }
    auto getCache() { return cache; }
    auto& getConstants() { return *(runtimeContext->constants); }
    auto& getRuntimeContext() { return *runtimeContext; }
    StructType *getNamedType(const std::string& name) const {
        for (auto& type : module->getIdentifiedStructTypes()) {
            if (type->getName().data() == name) {
                return type;
            }
        }
        UnexpectedError1(NoType, name);
    }
    ExecFunctionType getExecFunction(Context *ctx);
};

} // namespace abaci::engine

#endif
