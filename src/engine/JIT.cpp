#include "JIT.hpp"
#include "codegen/TypeCodeGen.hpp"
#include "codegen/StmtCodeGen.hpp"
#include "lib/Abaci.hpp"
#include "utility/Report.hpp"
#include "utility/Temporary.hpp"
#include "localize/Keywords.hpp"
#include <llvm/IR/Verifier.h>
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/ExecutionEngine/Orc/ThreadSafeModule.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/IR/Type.h>
#include <stdexcept>
#include <memory>
#include <cmath>
#include <cstring>
#include <iostream>

namespace abaci::engine {

using abaci::codegen::TypeCodeGen;
using abaci::codegen::StmtCodeGen;
using abaci::ast::ReturnStmt;
using abaci::utility::Temporaries;
using LLVMType = llvm::Type*;

using namespace llvm;
using namespace llvm::orc;
using namespace abaci::lib;

void JIT::initialize() {
    FunctionType *pow_func_type = FunctionType::get(builder.getDoubleTy(), { builder.getDoubleTy(), builder.getDoubleTy() }, false);
    Function::Create(pow_func_type, Function::ExternalLinkage, "pow", module.get());
    StructType *abaci_value_type = StructType::create(*context, "struct.AbaciValue");
    abaci_value_type->setBody({ builder.getInt64Ty() });
    StructType *complex_type = StructType::create(*context, "struct.Complex");
    complex_type->setBody({ builder.getDoubleTy(), builder.getDoubleTy() });
    StructType *string_type = StructType::create(*context, "struct.String");
    string_type->setBody({ builder.getInt8PtrTy(), builder.getInt64Ty() });
    StructType *instance_type = StructType::create(*context, "struct.Instance");
    instance_type->setBody({ builder.getInt8PtrTy(), builder.getInt64Ty(), PointerType::get(abaci_value_type, 0) });
    StructType *context_type = StructType::create(*context, "struct.Context");
    context_type->setBody({ PointerType::get(abaci_value_type, 0), builder.getInt8PtrTy(), builder.getInt8PtrTy(), builder.getInt8PtrTy(), builder.getInt8PtrTy(), builder.getInt8PtrTy() });
    global_context = new llvm::GlobalVariable(*module, PointerType::get(context_type, 0), false, llvm::GlobalValue::ExternalLinkage, ConstantPointerNull::get(PointerType::get(context_type, 0)), "Context");
    FunctionType *print_value_boolean_type = FunctionType::get(builder.getVoidTy(), { PointerType::get(context_type, 0), builder.getInt1Ty() }, false);
    Function::Create(print_value_boolean_type, Function::ExternalLinkage, "printValueBoolean", module.get());
    FunctionType *print_value_integer_type = FunctionType::get(builder.getVoidTy(), { PointerType::get(context_type, 0), builder.getInt64Ty() }, false);
    Function::Create(print_value_integer_type, Function::ExternalLinkage, "printValueInteger", module.get());
    FunctionType *print_value_floating_type = FunctionType::get(builder.getVoidTy(), { PointerType::get(context_type, 0), builder.getDoubleTy() }, false);
    Function::Create(print_value_floating_type, Function::ExternalLinkage, "printValueFloating", module.get());
    FunctionType *print_value_complex_type = FunctionType::get(builder.getVoidTy(), { PointerType::get(context_type, 0), PointerType::get(complex_type, 0) }, false);
    Function::Create(print_value_complex_type, Function::ExternalLinkage, "printValueComplex", module.get());
    FunctionType *print_value_string_type = FunctionType::get(builder.getVoidTy(), { PointerType::get(context_type, 0), PointerType::get(string_type, 0) }, false);
    Function::Create(print_value_string_type, Function::ExternalLinkage, "printValueString", module.get());
    FunctionType *print_value_instance_type = FunctionType::get(builder.getVoidTy(), { PointerType::get(context_type, 0), PointerType::get(instance_type, 0) }, false);
    Function::Create(print_value_instance_type, Function::ExternalLinkage, "printValueInstance", module.get());
    FunctionType *print_comma_type = FunctionType::get(builder.getVoidTy(), { PointerType::get(context_type, 0) }, false);
    Function::Create(print_comma_type, Function::ExternalLinkage, "printComma", module.get());
    FunctionType *print_ln_type = FunctionType::get(builder.getVoidTy(), { PointerType::get(context_type, 0) }, false);
    Function::Create(print_ln_type, Function::ExternalLinkage, "printLn", module.get());
    FunctionType *make_complex_type = FunctionType::get(PointerType::get(complex_type, 0), { builder.getDoubleTy(), builder.getDoubleTy() }, false);
    Function::Create(make_complex_type, Function::ExternalLinkage, "makeComplex", module.get());
    FunctionType *make_string_type = FunctionType::get(PointerType::get(string_type, 0), { builder.getInt8PtrTy(), builder.getInt64Ty() }, false);
    Function::Create(make_string_type, Function::ExternalLinkage, "makeString", module.get());
    FunctionType *make_instance_type = FunctionType::get(PointerType::get(instance_type, 0), { builder.getInt8PtrTy(), builder.getInt64Ty() }, false);
    Function::Create(make_instance_type, Function::ExternalLinkage, "makeInstance", module.get());
    FunctionType *clone_complex_type = FunctionType::get(PointerType::get(complex_type, 0), { PointerType::get(complex_type, 0) }, false);
    Function::Create(clone_complex_type, Function::ExternalLinkage, "cloneComplex", module.get());
    FunctionType *clone_string_type = FunctionType::get(PointerType::get(string_type, 0), { PointerType::get(string_type, 0) }, false);
    Function::Create(clone_string_type, Function::ExternalLinkage, "cloneString", module.get());
    FunctionType *clone_instance_type = FunctionType::get(PointerType::get(instance_type, 0), { PointerType::get(instance_type, 0) }, false);
    Function::Create(clone_instance_type, Function::ExternalLinkage, "cloneInstance", module.get());
    FunctionType *complex_math_type = FunctionType::get(PointerType::get(complex_type, 0), { builder.getInt32Ty(), PointerType::get(complex_type, 0), PointerType::get(complex_type, 0) }, false);
    Function::Create(complex_math_type, Function::ExternalLinkage, "opComplex", module.get());
    FunctionType *compare_string_type = FunctionType::get(builder.getInt1Ty(), { PointerType::get(string_type, 0),  PointerType::get(string_type, 0) }, false);
    Function::Create(compare_string_type, Function::ExternalLinkage, "compareString", module.get());
    FunctionType *concat_string_type = FunctionType::get(PointerType::get(string_type, 0), { PointerType::get(string_type, 0),  PointerType::get(string_type, 0) }, false);
    Function::Create(concat_string_type, Function::ExternalLinkage, "concatString", module.get());
    FunctionType *destroy_complex_type = FunctionType::get(builder.getVoidTy(), { PointerType::get(complex_type, 0) }, false);
    Function::Create(destroy_complex_type, Function::ExternalLinkage, "destroyComplex", module.get());
    FunctionType *destroy_string_type = FunctionType::get(builder.getVoidTy(), { PointerType::get(string_type, 0) }, false);
    Function::Create(destroy_string_type, Function::ExternalLinkage, "destroyString", module.get());
    FunctionType *destroy_instance_type = FunctionType::get(builder.getVoidTy(), { PointerType::get(instance_type, 0) }, false);
    Function::Create(destroy_instance_type, Function::ExternalLinkage, "destroyInstance", module.get());
    FunctionType *user_input_type = FunctionType::get(PointerType::get(string_type, 0), { PointerType::get(context_type, 0) }, false);
    Function::Create(user_input_type, Function::ExternalLinkage, "userInput", module.get());
    FunctionType *to_type_type = FunctionType::get(builder.getInt64Ty(), { builder.getInt32Ty(), builder.getInt64Ty(), builder.getInt32Ty() }, false);
    Function::Create(to_type_type, Function::ExternalLinkage, "toType", module.get());
    for (const auto& instantiation : cache->getInstantiations()) {
        std::string function_name{ mangled(instantiation.name, instantiation.parameter_types) };
        std::vector<LLVMType> param_types;
        if (instantiation.is_method) {
            param_types.push_back(typeToLLVMType(*this, instantiation.this_type));
        }
        for (const auto& type : instantiation.parameter_types) {
            param_types.push_back(typeToLLVMType(*this, type));
        }
        auto return_type = typeToLLVMType(*this, instantiation.return_type);
        FunctionType *inst_func_type = FunctionType::get(return_type, { param_types }, false);
        current_function = Function::Create(inst_func_type, Function::ExternalLinkage, function_name, *module);
        BasicBlock *entry_block = BasicBlock::Create(*context, "entry", current_function);
        BasicBlock *exit_block = BasicBlock::Create(*context, "exit");
        builder.SetInsertPoint(entry_block);
        const auto& cache_function = cache->getFunction(instantiation.name);
        LocalSymbols parameters;
        if (instantiation.is_method) {
            auto *thisValue = makeMutableValue(*this, instantiation.parameter_types.at(0));
            parameters.add(THIS_V, thisValue, instantiation.this_type);
            storeMutableValue(*this, thisValue, current_function->getArg(0));
        }
        for (int index = instantiation.is_method; const auto& param : cache_function.parameters) {
            auto *paramValue = makeMutableValue(*this, instantiation.parameter_types.at(index));
            parameters.add(param.get(), paramValue, instantiation.parameter_types.at(index));
            storeMutableValue(*this, paramValue, current_function->getArg(index));
            ++index;
        }
        if (instantiation.return_type != AbaciValue::None) {
            auto *returnValue = makeMutableValue(*this, instantiation.return_type);
            parameters.add(RETURN_V, returnValue, instantiation.return_type);
        }
        TypeCodeGen typeGen(runtimeContext, cache, &parameters, (instantiation.is_method) ? TypeCodeGen::Method : TypeCodeGen::FreeFunction);
        typeGen(cache_function.body);
        StmtCodeGen stmt(*this, nullptr, &parameters, exit_block);
        stmt(cache_function.body);
        if (!dynamic_cast<const ReturnStmt*>(cache_function.body.back().get())) {
            builder.CreateBr(exit_block);
        }
        exit_block->insertInto(current_function);
        builder.SetInsertPoint(exit_block);
        if (instantiation.return_type == AbaciValue::None) {
            builder.CreateRetVoid();
        }
        else {
            builder.CreateRet(loadMutableValue(*this, parameters.getValue(parameters.getIndex(RETURN_V).second), instantiation.return_type));
        }
        parameters.clear();
    }
    FunctionType *main_func_type = FunctionType::get(builder.getVoidTy(), {}, false);
    current_function = Function::Create(main_func_type, Function::ExternalLinkage, function_name, module.get());
    BasicBlock *entry_block = BasicBlock::Create(*context, "entry", current_function);
    builder.SetInsertPoint(entry_block);
}

ExecFunctionType JIT::getExecFunction(Context *ctx) {
    builder.CreateRetVoid();
    cache->clearInstantiations();
    //verifyModule(*module, &errs());
    InitializeNativeTarget();
    InitializeNativeTargetAsmPrinter();
    jit = jit_builder.create();
    if (!jit) {
        UnexpectedError0(NoLLJIT);
    }
    if (auto err = (*jit)->addIRModule(ThreadSafeModule(std::move(module), std::move(context)))) {
        handleAllErrors(std::move(err), [&](ErrorInfoBase& eib) {
            errs() << "Error: " << eib.message() << '\n';
        });
        UnexpectedError0(NoModule);
    }
    if (auto err = (*jit)->getMainJITDylib().define(absoluteSymbols(SymbolMap{
            {(*jit)->getExecutionSession().intern("printValueBoolean"), { reinterpret_cast<uintptr_t>(&printValue<bool>), JITSymbolFlags::Exported }},
            {(*jit)->getExecutionSession().intern("printValueInteger"), { reinterpret_cast<uintptr_t>(&printValue<uint64_t>), JITSymbolFlags::Exported }},
            {(*jit)->getExecutionSession().intern("printValueFloating"), { reinterpret_cast<uintptr_t>(&printValue<double>), JITSymbolFlags::Exported }},
            {(*jit)->getExecutionSession().intern("printValueComplex"), { reinterpret_cast<uintptr_t>(&printValue<Complex*>), JITSymbolFlags::Exported }},
            {(*jit)->getExecutionSession().intern("printValueString"), { reinterpret_cast<uintptr_t>(&printValue<String*>), JITSymbolFlags::Exported }},
            {(*jit)->getExecutionSession().intern("printValueInstance"), { reinterpret_cast<uintptr_t>(&printValue<Instance*>), JITSymbolFlags::Exported }},
            {(*jit)->getExecutionSession().intern("printComma"), { reinterpret_cast<uintptr_t>(&printComma), JITSymbolFlags::Exported }},
            {(*jit)->getExecutionSession().intern("printLn"), { reinterpret_cast<uintptr_t>(&printLn), JITSymbolFlags::Exported }},
            {(*jit)->getExecutionSession().intern("userInput"), { reinterpret_cast<uintptr_t>(&userInput), JITSymbolFlags::Exported }},
            {(*jit)->getExecutionSession().intern("toType"), { reinterpret_cast<uintptr_t>(&toType), JITSymbolFlags::Exported }},
            {(*jit)->getExecutionSession().intern("makeComplex"), { reinterpret_cast<uintptr_t>(&makeComplex), JITSymbolFlags::Exported }},
            {(*jit)->getExecutionSession().intern("makeString"), { reinterpret_cast<uintptr_t>(&makeString), JITSymbolFlags::Exported }},
            {(*jit)->getExecutionSession().intern("makeInstance"), { reinterpret_cast<uintptr_t>(&makeInstance), JITSymbolFlags::Exported }},
            {(*jit)->getExecutionSession().intern("opComplex"), { reinterpret_cast<uintptr_t>(&opComplex), JITSymbolFlags::Exported }},
            {(*jit)->getExecutionSession().intern("compareString"), { reinterpret_cast<uintptr_t>(&concatString), JITSymbolFlags::Exported }},
            {(*jit)->getExecutionSession().intern("concatString"), { reinterpret_cast<uintptr_t>(&concatString), JITSymbolFlags::Exported }},
            {(*jit)->getExecutionSession().intern("cloneComplex"), { reinterpret_cast<uintptr_t>(&cloneComplex), JITSymbolFlags::Exported }},
            {(*jit)->getExecutionSession().intern("cloneString"), { reinterpret_cast<uintptr_t>(&cloneString), JITSymbolFlags::Exported }},
            {(*jit)->getExecutionSession().intern("cloneInstance"), { reinterpret_cast<uintptr_t>(&cloneInstance), JITSymbolFlags::Exported }},
            {(*jit)->getExecutionSession().intern("destroyComplex"), { reinterpret_cast<uintptr_t>(&destroyComplex), JITSymbolFlags::Exported }},
            {(*jit)->getExecutionSession().intern("destroyString"), { reinterpret_cast<uintptr_t>(&destroyString), JITSymbolFlags::Exported }},
            {(*jit)->getExecutionSession().intern("destroyInstance"), { reinterpret_cast<uintptr_t>(&destroyInstance), JITSymbolFlags::Exported }},
            {(*jit)->getExecutionSession().intern("pow"), { reinterpret_cast<uintptr_t>(static_cast<double(*)(double,double)>(&pow)), JITSymbolFlags::Exported }},
            {(*jit)->getExecutionSession().intern("memcpy"), { reinterpret_cast<uintptr_t>(&memcpy), JITSymbolFlags::Exported }}
            }))) {
        handleAllErrors(std::move(err), [&](ErrorInfoBase& eib) {
            errs() << "Error: " << eib.message() << '\n';
        });
        UnexpectedError0(NoSymbol);
    }
    auto ctx_symbol = (*jit)->lookup("Context");
    auto func_symbol = (*jit)->lookup(function_name);
    if (!ctx_symbol || !func_symbol) {
        UnexpectedError0(NoJITFunc);
    }
    Context **ctx_addr = reinterpret_cast<Context**>(ctx_symbol->getAddress());
    *ctx_addr = ctx;
    return reinterpret_cast<ExecFunctionType>(func_symbol->getAddress());
}

} // namespace abaci::engine
