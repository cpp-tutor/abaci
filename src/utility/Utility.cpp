#include "Utility.hpp"
#include "engine/JIT.hpp"
#include "Report.hpp"
#include "localize/Messages.hpp"

namespace abaci::utility {

using llvm::AllocaInst;
using llvm::PointerType;
using llvm::ArrayType;
using llvm::Value;
using llvm::BasicBlock;
using LLVMType = llvm::Type*;

void destroyValue(JIT& jit, Value *value, const Type& type) {
    switch (typeToScalar(removeConstFromType(type))) {
        case AbaciValue::None:
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
            Assert(instanceType != nullptr);
            ArrayType *array = ArrayType::get(jit.getBuilder().getInt64Ty(), instanceType->variableTypes.size());
            Value *arrayPtr = jit.getBuilder().CreateLoad(PointerType::get(array, 0), jit.getBuilder().CreateStructGEP(jit.getNamedType("struct.Instance"), value, 2));
            for (std::size_t index = 0; index != instanceType->variableTypes.size(); ++index) {
                Value *variable = jit.getBuilder().CreateGEP(array, arrayPtr, { jit.getBuilder().getInt32(0), jit.getBuilder().getInt32(index) });
                destroyValue(jit, jit.getBuilder().CreateLoad(typeToLLVMType(jit, instanceType->variableTypes.at(index)), variable), instanceType->variableTypes.at(index));
            }
            jit.getBuilder().CreateCall(jit.getModule().getFunction("destroyInstance"), { value });
            break;
        }
        case AbaciValue::List: {
            auto listType = std::dynamic_pointer_cast<TypeList>(std::get<std::shared_ptr<TypeBase>>(type));
            Assert(listType != nullptr);
            Value *listSize = jit.getBuilder().CreateLoad(jit.getBuilder().getInt64Ty(), jit.getBuilder().CreateStructGEP(jit.getNamedType("struct.List"), value, 0));
            ArrayType *array = ArrayType::get(jit.getBuilder().getInt64Ty(), 0);
            Value *listElements = jit.getBuilder().CreateLoad(PointerType::get(array, 0), jit.getBuilder().CreateStructGEP(jit.getNamedType("struct.List"), value, 1));
            AllocaInst *index = jit.getBuilder().CreateAlloca(jit.getBuilder().getInt64Ty(), nullptr);
            jit.getBuilder().CreateStore(jit.getBuilder().getInt64(0), index);
            BasicBlock *preBlock = BasicBlock::Create(jit.getContext(), "", jit.getFunction());
            BasicBlock *loopBlock = BasicBlock::Create(jit.getContext(), "", jit.getFunction());
            BasicBlock *postBlock = BasicBlock::Create(jit.getContext(), "", jit.getFunction());
            jit.getBuilder().CreateBr(preBlock);
            jit.getBuilder().SetInsertPoint(preBlock);
            Value *condition = jit.getBuilder().CreateICmpNE(jit.getBuilder().CreateLoad(jit.getBuilder().getInt64Ty(), index), listSize);
            jit.getBuilder().CreateCondBr(condition, loopBlock, postBlock);
            jit.getBuilder().SetInsertPoint(loopBlock);
            Value *indexValue = jit.getBuilder().CreateLoad(jit.getBuilder().getInt64Ty(), index);
            Value *element = jit.getBuilder().CreateGEP(array, listElements, { jit.getBuilder().getInt32(0), indexValue });
            destroyValue(jit, jit.getBuilder().CreateLoad(typeToLLVMType(jit, listType->elementType), element), listType->elementType);
            jit.getBuilder().CreateStore(jit.getBuilder().CreateAdd(indexValue, jit.getBuilder().getInt64(1)), index);
            jit.getBuilder().CreateBr(preBlock);
            jit.getBuilder().SetInsertPoint(postBlock);
            jit.getBuilder().CreateCall(jit.getModule().getFunction("destroyList"), { value });
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
            return PointerType::get(jit.getNamedType("struct.Instance"), 0);
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
        case AbaciValue::List:
            return PointerType::get(jit.getNamedType("struct.List"), 0);
        default:
            UnexpectedError0(BadType);
    }
}

Value *cloneValue(JIT& jit, Value *value, const Type& type) {
    switch (typeToScalar(removeConstFromType(type))) {
        case AbaciValue::None:
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
            Assert(instanceType != nullptr);
            ArrayType *array = ArrayType::get(jit.getBuilder().getInt64Ty(), instanceType->variableTypes.size());
            Value *readArrayPtr = jit.getBuilder().CreateLoad(PointerType::get(array, 0), jit.getBuilder().CreateStructGEP(jit.getNamedType("struct.Instance"), value, 2));
            Value *writeArrayPtr = jit.getBuilder().CreateLoad(PointerType::get(array, 0), jit.getBuilder().CreateStructGEP(jit.getNamedType("struct.Instance"), instance, 2));
            for (std::size_t index = 0; index != instanceType->variableTypes.size(); ++index) {
                Value *variableRead = jit.getBuilder().CreateGEP(array, readArrayPtr, { jit.getBuilder().getInt32(0), jit.getBuilder().getInt32(index) });
                Value *variable = cloneValue(jit, jit.getBuilder().CreateLoad(typeToLLVMType(jit, instanceType->variableTypes.at(index)), variableRead), instanceType->variableTypes.at(index));
                Value *variableWrite = jit.getBuilder().CreateGEP(array, writeArrayPtr, { jit.getBuilder().getInt32(0), jit.getBuilder().getInt32(index) });
                jit.getBuilder().CreateStore(jit.getBuilder().CreateBitCast(variable, jit.getBuilder().getInt64Ty()), variableWrite);
            }
            return instance;
        }
        case AbaciValue::List: {
            auto listType = std::dynamic_pointer_cast<TypeList>(std::get<std::shared_ptr<TypeBase>>(type));
            Assert(listType != nullptr);
            auto list = jit.getBuilder().CreateCall(jit.getModule().getFunction("cloneList"), { value });
            Value *listSize = jit.getBuilder().CreateLoad(jit.getBuilder().getInt64Ty(), jit.getBuilder().CreateStructGEP(jit.getNamedType("struct.List"), value, 0));
            ArrayType *array = ArrayType::get(jit.getBuilder().getInt64Ty(), 0);
            Value *listRead = jit.getBuilder().CreateLoad(PointerType::get(array, 0), jit.getBuilder().CreateStructGEP(jit.getNamedType("struct.List"), value, 1));
            Value *listWrite = jit.getBuilder().CreateLoad(PointerType::get(array, 0), jit.getBuilder().CreateStructGEP(jit.getNamedType("struct.List"), list, 1));
            AllocaInst *index = jit.getBuilder().CreateAlloca(jit.getBuilder().getInt64Ty(), nullptr);
            jit.getBuilder().CreateStore(jit.getBuilder().getInt64(0), index);
            BasicBlock *preBlock = BasicBlock::Create(jit.getContext(), "", jit.getFunction());
            BasicBlock *loopBlock = BasicBlock::Create(jit.getContext(), "", jit.getFunction());
            BasicBlock *postBlock = BasicBlock::Create(jit.getContext(), "", jit.getFunction());
            jit.getBuilder().CreateBr(preBlock);
            jit.getBuilder().SetInsertPoint(preBlock);
            Value *condition = jit.getBuilder().CreateICmpNE(jit.getBuilder().CreateLoad(jit.getBuilder().getInt64Ty(), index), listSize);
            jit.getBuilder().CreateCondBr(condition, loopBlock, postBlock);
            jit.getBuilder().SetInsertPoint(loopBlock);
            Value *indexValue = jit.getBuilder().CreateLoad(jit.getBuilder().getInt64Ty(), index);
            Value *elementRead = jit.getBuilder().CreateGEP(array, listRead, { jit.getBuilder().getInt32(0), indexValue });
            Value *element = cloneValue(jit, jit.getBuilder().CreateLoad(typeToLLVMType(jit, listType->elementType), elementRead), listType->elementType);
            Value *elementWrite = jit.getBuilder().CreateGEP(array, listWrite, { jit.getBuilder().getInt32(0), indexValue });
            jit.getBuilder().CreateStore(jit.getBuilder().CreateBitCast(element, jit.getBuilder().getInt64Ty()), elementWrite);
            jit.getBuilder().CreateStore(jit.getBuilder().CreateAdd(indexValue, jit.getBuilder().getInt64(1)), index);
            jit.getBuilder().CreateBr(preBlock);
            jit.getBuilder().SetInsertPoint(postBlock);
            return list;
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