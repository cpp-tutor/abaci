#include "JIT.hpp"
#include "codegen/TypeCodeGen.hpp"
#include "codegen/StmtCodeGen.hpp"
#include "lib/Abaci.hpp"
#include "utility/Report.hpp"
#include "utility/Temporary.hpp"
#include "localize/Keywords.hpp"
#include <llvm/IR/Verifier.h>
#include <llvm/ExecutionEngine/Orc/Core.h>
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/ExecutionEngine/Orc/ThreadSafeModule.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/IR/Type.h>
#include <stdexcept>
#include <memory>
#include <cmath>

#if LLVM_VERSION_MAJOR < 17
#define RUNTIME_FUNCTION(NAME, ADDRESS) Symbols[(*jit)->getExecutionSession().intern(NAME)] = { reinterpret_cast<uintptr_t>(ADDRESS), JITSymbolFlags::Callable | JITSymbolFlags::Exported };
#else
#define RUNTIME_FUNCTION(NAME, ADDRESS) Symbols[(*jit)->getExecutionSession().intern(NAME)] = ExecutorSymbolDef(ExecutorAddr(JITEvaluatedSymbol::fromPointer(ADDRESS).getAddress()), JITSymbolFlags::Callable | JITSymbolFlags::Exported);
#endif

namespace abaci::engine {

using abaci::codegen::TypeCodeGen;
using abaci::codegen::StmtCodeGen;
using abaci::ast::ReturnStmt;
using abaci::utility::Temporaries;
using abaci::utility::makeMutableValue;
using abaci::utility::storeMutableValue;
using abaci::utility::loadMutableValue;

using LLVMType = llvm::Type*;

using namespace llvm;
using namespace llvm::orc;
using namespace abaci::lib;

void JIT::initialize() {
    StructType *abaciValueType = StructType::create(*context, "struct.AbaciValue");
    abaciValueType->setBody({ builder.getInt64Ty() });
    StructType *complexType = StructType::create(*context, "struct.Complex");
    complexType->setBody({ builder.getDoubleTy(), builder.getDoubleTy() });
    StructType *stringType = StructType::create(*context, "struct.String");
    stringType->setBody({ PointerType::get(builder.getInt8Ty(), 0), builder.getInt64Ty(), builder.getInt64Ty() });
    StructType *instanceType = StructType::create(*context, "struct.Instance");
    instanceType->setBody({ PointerType::get(builder.getInt8Ty(), 0), builder.getInt64Ty(), PointerType::get(abaciValueType, 0) });
    StructType *listType = StructType::create(*context, "struct.List");
    listType->setBody({ builder.getInt64Ty(), PointerType::get(abaciValueType, 0) });
    StructType *contextType = StructType::create(*context, "struct.Context");
    contextType->setBody({ PointerType::get(abaciValueType, 0), PointerType::get(builder.getInt8Ty(), 0), PointerType::get(builder.getInt8Ty(), 0), PointerType::get(builder.getInt8Ty(), 0), PointerType::get(builder.getInt8Ty(), 0) });
    globalContext = new llvm::GlobalVariable(*module, PointerType::get(contextType, 0), false, llvm::GlobalValue::ExternalLinkage, ConstantPointerNull::get(PointerType::get(contextType, 0)), "Context");
    FunctionType *printValueNilType = FunctionType::get(builder.getVoidTy(), { PointerType::get(contextType, 0), PointerType::get(instanceType, 0) }, false);
    Function::Create(printValueNilType, Function::ExternalLinkage, "printValueNil", module.get());
    FunctionType *printValueBooleanType = FunctionType::get(builder.getVoidTy(), { PointerType::get(contextType, 0), builder.getInt1Ty() }, false);
    Function::Create(printValueBooleanType, Function::ExternalLinkage, "printValueBoolean", module.get());
    FunctionType *printValueIntegerType = FunctionType::get(builder.getVoidTy(), { PointerType::get(contextType, 0), builder.getInt64Ty() }, false);
    Function::Create(printValueIntegerType, Function::ExternalLinkage, "printValueInteger", module.get());
    FunctionType *printValueFloatingType = FunctionType::get(builder.getVoidTy(), { PointerType::get(contextType, 0), builder.getDoubleTy() }, false);
    Function::Create(printValueFloatingType, Function::ExternalLinkage, "printValueFloating", module.get());
    FunctionType *printValueComplexType = FunctionType::get(builder.getVoidTy(), { PointerType::get(contextType, 0), PointerType::get(complexType, 0) }, false);
    Function::Create(printValueComplexType, Function::ExternalLinkage, "printValueComplex", module.get());
    FunctionType *printValueStringType = FunctionType::get(builder.getVoidTy(), { PointerType::get(contextType, 0), PointerType::get(stringType, 0) }, false);
    Function::Create(printValueStringType, Function::ExternalLinkage, "printValueString", module.get());
    FunctionType *printValueInstanceType = FunctionType::get(builder.getVoidTy(), { PointerType::get(contextType, 0), PointerType::get(instanceType, 0) }, false);
    Function::Create(printValueInstanceType, Function::ExternalLinkage, "printValueInstance", module.get());
    FunctionType *printValueListType = FunctionType::get(builder.getVoidTy(), { PointerType::get(contextType, 0), PointerType::get(listType, 0) }, false);
    Function::Create(printValueListType, Function::ExternalLinkage, "printValueList", module.get());
    FunctionType *printCommaType = FunctionType::get(builder.getVoidTy(), { PointerType::get(contextType, 0) }, false);
    Function::Create(printCommaType, Function::ExternalLinkage, "printComma", module.get());
    FunctionType *printLnType = FunctionType::get(builder.getVoidTy(), { PointerType::get(contextType, 0) }, false);
    Function::Create(printLnType, Function::ExternalLinkage, "printLn", module.get());
    FunctionType *makeComplexType = FunctionType::get(PointerType::get(complexType, 0), { builder.getDoubleTy(), builder.getDoubleTy() }, false);
    Function::Create(makeComplexType, Function::ExternalLinkage, "makeComplex", module.get());
    FunctionType *makeStringType = FunctionType::get(PointerType::get(stringType, 0), { PointerType::get(builder.getInt8Ty(), 0), builder.getInt64Ty() }, false);
    Function::Create(makeStringType, Function::ExternalLinkage, "makeString", module.get());
    FunctionType *makeInstanceType = FunctionType::get(PointerType::get(instanceType, 0), { PointerType::get(builder.getInt8Ty(), 0), builder.getInt64Ty() }, false);
    Function::Create(makeInstanceType, Function::ExternalLinkage, "makeInstance", module.get());
    FunctionType *makeListType = FunctionType::get(PointerType::get(listType, 0), { builder.getInt64Ty() }, false);
    Function::Create(makeListType, Function::ExternalLinkage, "makeList", module.get());
    FunctionType *cloneComplexType = FunctionType::get(PointerType::get(complexType, 0), { PointerType::get(complexType, 0) }, false);
    Function::Create(cloneComplexType, Function::ExternalLinkage, "cloneComplex", module.get());
    FunctionType *cloneStringType = FunctionType::get(PointerType::get(stringType, 0), { PointerType::get(stringType, 0) }, false);
    Function::Create(cloneStringType, Function::ExternalLinkage, "cloneString", module.get());
    FunctionType *cloneInstanceType = FunctionType::get(PointerType::get(instanceType, 0), { PointerType::get(instanceType, 0) }, false);
    Function::Create(cloneInstanceType, Function::ExternalLinkage, "cloneInstance", module.get());
    FunctionType *cloneListType = FunctionType::get(PointerType::get(listType, 0), { PointerType::get(listType, 0) }, false);
    Function::Create(cloneListType, Function::ExternalLinkage, "cloneList", module.get());
    FunctionType *complexMathType = FunctionType::get(PointerType::get(complexType, 0), { builder.getInt32Ty(), PointerType::get(complexType, 0), PointerType::get(complexType, 0) }, false);
    Function::Create(complexMathType, Function::ExternalLinkage, "opComplex", module.get());
    FunctionType *compareStringType = FunctionType::get(builder.getInt1Ty(), { PointerType::get(stringType, 0), PointerType::get(stringType, 0) }, false);
    Function::Create(compareStringType, Function::ExternalLinkage, "compareString", module.get());
    FunctionType *concatStringType = FunctionType::get(PointerType::get(stringType, 0), { PointerType::get(stringType, 0), PointerType::get(stringType, 0) }, false);
    Function::Create(concatStringType, Function::ExternalLinkage, "concatString", module.get());
    FunctionType *validIndexType = FunctionType::get(builder.getInt64Ty(), { builder.getInt64Ty(), builder.getInt64Ty(), builder.getInt1Ty() }, false);
    Function::Create(validIndexType, Function::ExternalLinkage, "validIndex", module.get());
    FunctionType *indexStringType = FunctionType::get(PointerType::get(stringType, 0), { PointerType::get(stringType, 0), builder.getInt64Ty() }, false);
    Function::Create(indexStringType, Function::ExternalLinkage, "indexString", module.get());
    FunctionType *spliceStringType = FunctionType::get(builder.getVoidTy(), { PointerType::get(stringType, 0), builder.getInt64Ty(), builder.getInt64Ty(), PointerType::get(stringType, 0) }, false);
    Function::Create(spliceStringType, Function::ExternalLinkage, "spliceString", module.get());
    FunctionType *sliceStringType = FunctionType::get(PointerType::get(stringType, 0), { PointerType::get(stringType, 0), builder.getInt64Ty(), builder.getInt64Ty() }, false);
    Function::Create(sliceStringType, Function::ExternalLinkage, "sliceString", module.get());
    FunctionType *sliceListType = FunctionType::get(PointerType::get(listType, 0), { PointerType::get(listType, 0), builder.getInt64Ty(), builder.getInt64Ty() }, false);
    Function::Create(sliceListType, Function::ExternalLinkage, "sliceList", module.get());
    FunctionType *spliceListType = FunctionType::get(PointerType::get(listType, 0), { PointerType::get(listType, 0), builder.getInt64Ty(), builder.getInt64Ty(), PointerType::get(listType, 0), builder.getInt1Ty() }, false);
    Function::Create(spliceListType, Function::ExternalLinkage, "spliceList", module.get());
    FunctionType *deleteElementType = FunctionType::get(builder.getVoidTy(), { PointerType::get(listType, 0), builder.getInt64Ty() }, false);
    Function::Create(deleteElementType, Function::ExternalLinkage, "deleteElement", module.get());
    FunctionType *destroyComplexType = FunctionType::get(builder.getVoidTy(), { PointerType::get(complexType, 0) }, false);
    Function::Create(destroyComplexType, Function::ExternalLinkage, "destroyComplex", module.get());
    FunctionType *destroyStringType = FunctionType::get(builder.getVoidTy(), { PointerType::get(stringType, 0) }, false);
    Function::Create(destroyStringType, Function::ExternalLinkage, "destroyString", module.get());
    FunctionType *destroyInstanceType = FunctionType::get(builder.getVoidTy(), { PointerType::get(instanceType, 0) }, false);
    Function::Create(destroyInstanceType, Function::ExternalLinkage, "destroyInstance", module.get());
    FunctionType *destroyListType = FunctionType::get(builder.getVoidTy(), { PointerType::get(listType, 0) }, false);
    Function::Create(destroyListType, Function::ExternalLinkage, "destroyList", module.get());
    FunctionType *userInputType = FunctionType::get(PointerType::get(stringType, 0), { PointerType::get(contextType, 0) }, false);
    Function::Create(userInputType, Function::ExternalLinkage, "userInput", module.get());
    FunctionType *toTypeType = FunctionType::get(builder.getInt64Ty(), { builder.getInt32Ty(), builder.getInt64Ty(), builder.getInt32Ty() }, false);
    Function::Create(toTypeType, Function::ExternalLinkage, "toType", module.get());
    FunctionType *powFunctionType = FunctionType::get(builder.getDoubleTy(), { builder.getDoubleTy(), builder.getDoubleTy() }, false);
    Function::Create(powFunctionType, Function::ExternalLinkage, "pow", module.get());
    for (const auto& instantiation : cache->getInstantiations()) {
        std::string functionName{ mangled(instantiation.name, instantiation.parameterTypes) };
        std::vector<LLVMType> parameterTypes;
        for (const auto& type : instantiation.parameterTypes) {
            parameterTypes.push_back(typeToLLVMType(*this, type));
        }
        const auto& cacheFunction = cache->getFunction(instantiation.name);
        LocalSymbols parameters;
        for (std::size_t index = 0; const auto& param : cacheFunction.parameters) {
            parameters.add(param.name, nullptr, instantiation.parameterTypes.at(index));
            ++index;
        }
        TypeCodeGen typeGen(runtimeContext, cache, &parameters,
            (cacheFunction.parameters.size() >= 1 && cacheFunction.parameters.at(0).name == THIS_V)
            ? TypeCodeGen::Method : TypeCodeGen::FreeFunction);
        typeGen(cacheFunction.body);
        Assert(typeGen.get() == instantiation.returnType);
        auto returnType = typeToLLVMType(*this, instantiation.returnType);
        FunctionType *instantiationFunctionType = FunctionType::get(returnType, { parameterTypes }, false);
        currentFunction = Function::Create(instantiationFunctionType, Function::ExternalLinkage, functionName, *module);
        BasicBlock *entryBlock = BasicBlock::Create(*context, "entry", currentFunction);
        BasicBlock *exitBlock = BasicBlock::Create(*context, "exit");
        builder.SetInsertPoint(entryBlock);
        for (std::size_t index = 0; index != cacheFunction.parameters.size(); ++index) {
            Value *paramValue = makeMutableValue(*this, instantiation.parameterTypes.at(index));
            parameters.setValue(index, paramValue);
            storeMutableValue(*this, paramValue, currentFunction->getArg(index));
        }
        if (instantiation.returnType != AbaciValue::None) {
            Value *returnValue = makeMutableValue(*this, instantiation.returnType);
            parameters.add(RETURN_V, returnValue, instantiation.returnType);
        }
        StmtCodeGen stmt(*this, nullptr, &parameters, exitBlock);
        stmt(cacheFunction.body);
        if (cacheFunction.body.statements.empty() || !std::holds_alternative<ReturnStmt>(cacheFunction.body.statements.back().data)) {
            builder.CreateBr(exitBlock);
        }
        exitBlock->insertInto(currentFunction);
        builder.SetInsertPoint(exitBlock);
        if (instantiation.returnType == AbaciValue::None) {
            builder.CreateRet(ConstantPointerNull::get(PointerType::get(getNamedType("struct.Instance"), 0)));
        }
        else {
            builder.CreateRet(loadMutableValue(*this, parameters.getValue(parameters.getIndex(RETURN_V, true).second), instantiation.returnType));
        }
        parameters.clear();
    }
    FunctionType *main_function_type = FunctionType::get(builder.getVoidTy(), {}, false);
    currentFunction = Function::Create(main_function_type, Function::ExternalLinkage, functionName, module.get());
    BasicBlock *entryBlock = BasicBlock::Create(*context, "entry", currentFunction);
    builder.SetInsertPoint(entryBlock);
}

ExecFunctionType JIT::getExecFunction() {
    builder.CreateRetVoid();
    InitializeNativeTarget();
    InitializeNativeTargetAsmPrinter();
    auto JTMB = JITTargetMachineBuilder::detectHost();
    if (!JTMB) {
        errs() << "Error detecting host: " << toString(JTMB.takeError()) << '\n';
    }
    auto opts = JTMB->getOptions();
    opts.ExceptionModel = ExceptionHandling::WinEH; // Enable exception handling
#ifdef _WIN64
    JTMB->setOptions(opts);
#endif

    jit = jitBuilder.setJITTargetMachineBuilder(std::move(*JTMB)).create();
    if (!jit) {
        UnexpectedError0(NoLLJIT);
    }
    SymbolMap Symbols;
    RUNTIME_FUNCTION("printValueNil", &printValue<Instance*>)
    RUNTIME_FUNCTION("printValueBoolean", &printValue<bool>)
    RUNTIME_FUNCTION("printValueInteger", &printValue<uint64_t>)
    RUNTIME_FUNCTION("printValueFloating", &printValue<double>)
    RUNTIME_FUNCTION("printValueComplex", &printValue<Complex*>)
    RUNTIME_FUNCTION("printValueString", &printValue<String*>)
    RUNTIME_FUNCTION("printValueInstance", &printValue<Instance*>)
    RUNTIME_FUNCTION("printValueList", &printValue<List*>)
    RUNTIME_FUNCTION("printComma", &printComma)
    RUNTIME_FUNCTION("printLn", &printLn)
    RUNTIME_FUNCTION("userInput", &userInput)
    RUNTIME_FUNCTION("toType", &toType)
    RUNTIME_FUNCTION("makeComplex", &makeComplex)
    RUNTIME_FUNCTION("makeString", &makeString)
    RUNTIME_FUNCTION("makeInstance", &makeInstance)
    RUNTIME_FUNCTION("makeList", &makeList)
    RUNTIME_FUNCTION("opComplex", &opComplex)
    RUNTIME_FUNCTION("compareString", &compareString)
    RUNTIME_FUNCTION("concatString", &concatString)
    RUNTIME_FUNCTION("validIndex", &validIndex)
    RUNTIME_FUNCTION("indexString", &indexString)
    RUNTIME_FUNCTION("sliceString", &sliceString)
    RUNTIME_FUNCTION("spliceString", &spliceString)
    RUNTIME_FUNCTION("sliceList", &sliceList)
    RUNTIME_FUNCTION("spliceList", &spliceList)
    RUNTIME_FUNCTION("deleteElement", &deleteElement)
    RUNTIME_FUNCTION("cloneComplex", &cloneComplex)
    RUNTIME_FUNCTION("cloneString", &cloneString)
    RUNTIME_FUNCTION("cloneInstance", &cloneInstance)
    RUNTIME_FUNCTION("cloneList", &cloneList)
    RUNTIME_FUNCTION("destroyComplex", &destroyComplex)
    RUNTIME_FUNCTION("destroyString", &destroyString)
    RUNTIME_FUNCTION("destroyInstance", &destroyInstance)
    RUNTIME_FUNCTION("destroyList", &destroyList)
    RUNTIME_FUNCTION("pow", static_cast<double(*)(double,double)>(&pow))
    RUNTIME_FUNCTION("memcpy", &memcpy)
    if (auto err = (*jit)->getMainJITDylib().define(std::make_unique<AbsoluteSymbolsMaterializationUnit>(std::move(Symbols)))) {
        handleAllErrors(std::move(err), [&](ErrorInfoBase& eib) {
            errs() << "Error: " << eib.message() << '\n';
        });
        UnexpectedError0(NoSymbol);
    }
    if (auto err = (*jit)->addIRModule(ThreadSafeModule(std::move(module), std::move(context)))) {
        handleAllErrors(std::move(err), [&](ErrorInfoBase& eib) {
            errs() << "Error: " << eib.message() << '\n';
            });
        UnexpectedError0(NoModule);
    }
    auto contextSymbol = (*jit)->lookup("Context");
    auto functionSymbol = (*jit)->lookup(functionName);
    if (!contextSymbol || !functionSymbol) {
        UnexpectedError0(NoJITFunction);
    }
#if LLVM_VERSION_MAJOR < 17
    Context **contextAddr = reinterpret_cast<Context**>(contextSymbol->getAddress());
    *contextAddr = runtimeContext;
    return reinterpret_cast<ExecFunctionType>(functionSymbol->getAddress());
#else
    Context **contextAddr = contextSymbol->toPtr<Context**>();
    *contextAddr = runtimeContext;
    return functionSymbol->toPtr<ExecFunctionType>();
#endif
}

} // namespace abaci::engine
