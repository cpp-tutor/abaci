#include "Utility.hpp"
#include "engine/JIT.hpp"
#include "Report.hpp"
#include "localize/Messages.hpp"

namespace abaci::utility {

using llvm::AllocaInst;
using llvm::PointerType;
using llvm::ArrayType;
using llvm::Value;
using LLVMType = llvm::Type*;

void destroyValue(JIT& jit, Value *value, const Type& type) {
    switch (typeToScalar(removeConstFromType(type))) {
        case AbaciValue::Boolean:
        case AbaciValue::Integer:
        case AbaciValue::Floating:
            break;
        case AbaciValue::Complex:
            jit.getBuilder().CreateCall(jit.getModule().getFunction("destroyComplex"), { value });
            break;
        case AbaciValue::String:
            jit.getBuilder().CreateCall(jit.getModule().getFunction("destroyString"), { value });
            break;
        case AbaciValue::Instance: {
            auto instanceType = std::dynamic_pointer_cast<TypeInstance>(std::get<std::shared_ptr<TypeBase>>(type));
            ArrayType *array = ArrayType::get(jit.getBuilder().getInt64Ty(), instanceType->variableTypes.size());
            Value *arrayPtr = jit.getBuilder().CreateLoad(PointerType::get(array, 0), jit.getBuilder().CreateStructGEP(jit.getNamedType("struct.Instance"), value, 2));
            for (std::size_t index = 0; index != instanceType->variableTypes.size(); ++index) {
                Value *variable = jit.getBuilder().CreateGEP(array, arrayPtr, { jit.getBuilder().getInt32(0), jit.getBuilder().getInt32(index) });
                destroyValue(jit, jit.getBuilder().CreateLoad(typeToLLVMType(jit, type), variable), instanceType->variableTypes.at(index));
            }
            jit.getBuilder().CreateCall(jit.getModule().getFunction("destroyInstance"), { value });
            break;
        }
        default:
            UnexpectedError0(BadType);
    }
}

Value *getContextValue(JIT& jit) {
    return jit.getBuilder().CreateLoad(
        PointerType::get(jit.getNamedType("struct.Context"), 0),
        jit.getModule().getGlobalVariable("Context")
    );
}

LLVMType typeToLLVMType(JIT& jit, const Type& type) {
    switch (typeToScalar(removeConstFromType(type))) {
        case AbaciValue::None:
            return jit.getBuilder().getVoidTy();
        case AbaciValue::Boolean:
            return jit.getBuilder().getInt1Ty();
        case AbaciValue::Integer:
            return jit.getBuilder().getInt64Ty();
        case AbaciValue::Floating:
            return jit.getBuilder().getDoubleTy();
        case AbaciValue::Complex:
            return PointerType::get(jit.getNamedType("struct.Complex"), 0);
        case AbaciValue::String:
            return PointerType::get(jit.getNamedType("struct.String"), 0);
        case AbaciValue::Instance:
            return PointerType::get(jit.getNamedType("struct.Instance"), 0);
        default:
            UnexpectedError0(BadType);
    }
}

Value *cloneValue(JIT& jit, Value *value, const Type& type) {
    switch (typeToScalar(removeConstFromType(type))) {
        case AbaciValue::Boolean:
        case AbaciValue::Integer:
        case AbaciValue::Floating:
            return value;
        case AbaciValue::Complex:
            return jit.getBuilder().CreateCall(jit.getModule().getFunction("cloneComplex"), { value });
        case AbaciValue::String:
            return jit.getBuilder().CreateCall(jit.getModule().getFunction("cloneString"), { value });
        case AbaciValue::Instance: {
            auto instance = jit.getBuilder().CreateCall(jit.getModule().getFunction("cloneInstance"), { value });
            auto instanceType = std::dynamic_pointer_cast<TypeInstance>(std::get<std::shared_ptr<TypeBase>>(type));
            ArrayType *array = ArrayType::get(jit.getBuilder().getInt64Ty(), instanceType->variableTypes.size());
            Value *readArrayPtr = jit.getBuilder().CreateLoad(PointerType::get(array, 0), jit.getBuilder().CreateStructGEP(jit.getNamedType("struct.Instance"), value, 2));
            Value *writeArrayPtr = jit.getBuilder().CreateLoad(PointerType::get(array, 0), jit.getBuilder().CreateStructGEP(jit.getNamedType("struct.Instance"), instance, 2));
            for (std::size_t index = 0; index != instanceType->variableTypes.size(); ++index) {
                Value *variableRead = jit.getBuilder().CreateGEP(array, readArrayPtr, { jit.getBuilder().getInt32(0), jit.getBuilder().getInt32(index) });
                Value *variable = cloneValue(jit, jit.getBuilder().CreateLoad(typeToLLVMType(jit, type), variableRead), instanceType->variableTypes.at(index));
                Value *variableWrite = jit.getBuilder().CreateGEP(array, writeArrayPtr, { jit.getBuilder().getInt32(0), jit.getBuilder().getInt32(index) });
                jit.getBuilder().CreateStore(jit.getBuilder().CreateBitCast(variable, jit.getBuilder().getInt64Ty()), variableWrite);
            }
            return instance;
        }
        default:
            UnexpectedError0(BadType);
    }
}

AllocaInst *makeMutableValue(JIT& jit, const Type& type) {
    return jit.getBuilder().CreateAlloca(typeToLLVMType(jit, type), nullptr);
}

void storeMutableValue(JIT& jit, Value *allocValue, Value *value) {
    jit.getBuilder().CreateStore(value, allocValue);
}

Value *loadMutableValue(JIT& jit, Value *allocValue, const Type& type) {
    return jit.getBuilder().CreateLoad(typeToLLVMType(jit, type), allocValue);
}

void storeGlobalValue(JIT& jit, std::size_t index, Value *value) {
    Value *context = getContextValue(jit);
    ArrayType *arrayType = ArrayType::get(jit.getBuilder().getInt64Ty(), jit.getRuntimeContext().globals->size());
    Value *globalsPtr = jit.getBuilder().CreateLoad(PointerType::get(arrayType, 0), jit.getBuilder().CreateStructGEP(jit.getNamedType("struct.Context"), context, 0));
    Value *globalPtr = jit.getBuilder().CreateGEP(arrayType, globalsPtr, { jit.getBuilder().getInt32(0), jit.getBuilder().getInt32(index) });
    jit.getBuilder().CreateStore(jit.getBuilder().CreateBitCast(value, jit.getBuilder().getInt64Ty()), globalPtr);
}

Value *loadGlobalValue(JIT& jit, std::size_t index, const Type& type) {
    Value *context = getContextValue(jit);
    ArrayType *arrayType = ArrayType::get(jit.getBuilder().getInt64Ty(), jit.getRuntimeContext().globals->size());
    Value *globalsPtr = jit.getBuilder().CreateLoad(PointerType::get(arrayType, 0), jit.getBuilder().CreateStructGEP(jit.getNamedType("struct.Context"), context, 0));
    Value *globalPtr = jit.getBuilder().CreateGEP(arrayType, globalsPtr, { jit.getBuilder().getInt32(0), jit.getBuilder().getInt32(index) });
    return jit.getBuilder().CreateLoad(typeToLLVMType(jit, type), globalPtr);
}

} // namespace abaci::utility