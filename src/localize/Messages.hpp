#ifndef Messages_hpp
#define Messages_hpp

#define MESSAGE(MSG, STR) inline const char *MSG = reinterpret_cast<const char*>(u8##STR)

MESSAGE(InstanceOf, "<Instance of {}>");
MESSAGE(UnknownType, "Unknown type ({}).");
MESSAGE(BadOperator, "Unknown operator in this context.");
MESSAGE(BadConvType, "Bad type for conversion to this type.");
MESSAGE(NeedType, "Must be \'{}\' type.");
MESSAGE(BadConvTarget, "Bad target conversion type ({}).");

MESSAGE(VarExists, "Variable \'{}\' already exists.");
MESSAGE(VarNotExist, "Variable \'{}\' does not exist.");
MESSAGE(VarType, "Existing variable \'{}\' has different type.");
MESSAGE(BadNumericConv, "Bad numeric conversion when generating mangled name.");
MESSAGE(BadChar, "Bad character in function name.");
MESSAGE(BadType, "Bad type.");

MESSAGE(ClassExists, "Class \'{}\' already exists.");
MESSAGE(FunctionExists, "Function \'{}\' already exists.");
MESSAGE(WrongArgs, "Wrong number of arguments (have {}, need {}).");
MESSAGE(FunctionNotExist, "Function \'{}\' does not exist.");
MESSAGE(NoInst, "No such instantiation for function \'{}\'.");
MESSAGE(ClassNotExist, "Class \'{}\' does not exist.");
MESSAGE(DataNotExist, "Object does not have data member \'{}\'.");

MESSAGE(NoType, "Type \'{}\' not found.");

MESSAGE(NoLLJIT, "Failed to create LLJIT instance.");
MESSAGE(NoModule, "Failed to add IR module.");
MESSAGE(NoSymbol, "Failed to add symbols to module.");
MESSAGE(NoJITFunction, "JIT function not found.");

MESSAGE(NoAssignObject, "Cannot assign objects.");
MESSAGE(CallableNotExist, "No function or class called \'{}\'.");
MESSAGE(BadObject, "Not an object.");
MESSAGE(BadConvSource, "Bad source conversion type.");
MESSAGE(BadAssociation, "Unknown association type.");
MESSAGE(BadNode, "Bad node type.");
MESSAGE(BadCoerceTypes, "Incompatible types.");
MESSAGE(NoObject, "Operation is incompatible with object type.");
MESSAGE(NoBoolean, "Cannot convert this type to Boolean.");

MESSAGE(BadPrint, "Bad print entity.");
MESSAGE(NoConstantAssign, "Cannot reassign to constant \'{}\'.");
MESSAGE(DataType, "Data member already has different type.");
MESSAGE(BadStmtNode, "Bad StmtNode type.");

MESSAGE(ReturnAtEnd, "Return statement must be at end of block.");
MESSAGE(ObjectType, "Existing object \'{}\' has different type(s).");
MESSAGE(FuncTopLevel, "Functions must be defined at top-level.");
MESSAGE(ReturnOnlyInFunction, "Return statement can only appear inside a function.");
MESSAGE(FuncTypeSet, "Function return type already set to different type.");
MESSAGE(NoExpression, "Expression not permitted in this context.");

MESSAGE(BadParse, "Could not parse file.");
MESSAGE(InitialPrompt, "Abaci version {}\nEnter code, or \"{}\" to end:\n> ");
MESSAGE(InputPrompt, "> ");
MESSAGE(ContinuationPrompt, ". ");
MESSAGE(SyntaxError, "Unrecognized input.");
MESSAGE(Version, "0.0.3 (2024-Sep-19)");

MESSAGE(BadVarIndex, "Bad index of variable.");
MESSAGE(BadTemp, "Bad temporary.");
MESSAGE(NotImpl, "Not yet implemented.");
MESSAGE(CompilerInconsistency, "Compiler inconsistency detected: {}");
MESSAGE(AssertionFailed, "Assertion failed ({})");
MESSAGE(SourceFile, "\nSource filename: {}");
MESSAGE(LineNumber, ", Line number: {}");
MESSAGE(NotRefThis, "Variable \'this\' cannot be used in this way.");

#endif
