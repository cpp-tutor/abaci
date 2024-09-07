#ifndef Utility_hpp
#define Utility_hpp

#include "utility/Type.hpp"
#include "JITfwd.hpp"
#include <llvm/IR/Value.h>
#include <llvm/IR/Instructions.h>

namespace abaci::engine {

using abaci::utility::Type;

void destroyValue(JIT& jit, llvm::Value *value, const Type& type);
llvm::Value *getContextValue(JIT& jit);
llvm::Type *typeToLLVMType(JIT& jit, const Type& type);
llvm::Value *cloneValue(JIT& jit, llvm::Value *value, const Type& type);
llvm::AllocaInst *makeMutableValue(JIT& jit, const Type& type);
void storeMutableValue(JIT& jit, llvm::Value *allocValue, llvm::Value *value);
llvm::Value *loadMutableValue(JIT& jit, llvm::Value *allocValue, const Type& type);
void storeGlobalValue(JIT& jit, std::size_t index, llvm::Value *value);
llvm::Value *loadGlobalValue(JIT& jit, std::size_t index, const Type& type);

} // namespace abaci::engine

#endif