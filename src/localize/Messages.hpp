#ifndef Messages_hpp
#define Messages_hpp

#define MESSAGE(MSG, STR) inline const char *MSG = reinterpret_cast<const char*>(u8##STR)

MESSAGE(InstanceOf, "<Instance of {}>");
MESSAGE(ListOf, "<List of size {}>");
MESSAGE(BadOperator, "Unknown operator.");
MESSAGE(BadConvType, "Bad type for conversion to this type.");
MESSAGE(BadConvTarget, "Bad target conversion type ({}).");

MESSAGE(VariableExists, "Variable \'{}\' already exists.");
MESSAGE(VariableNotExist, "Variable \'{}\' does not exist.");
MESSAGE(VariableType, "Existing variable \'{}\' has different type.");
MESSAGE(BadNumericConv, "Bad numeric conversion when generating mangled name.");
MESSAGE(BadChar, "Bad character in function name.");
MESSAGE(BadType, "Bad type.");
MESSAGE(BadOperatorForType, "Bad operator \'{}\' for type \'{}\'.");

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
MESSAGE(BadObject, "Variable \'{}\' is not an object.");
MESSAGE(BadConvSource, "Bad source conversion type.");
MESSAGE(BadAssociation, "Unknown association type.");
MESSAGE(BadCoerceTypes, "Incompatible types.");
MESSAGE(NoBoolean, "Cannot convert this type to Boolean.");

MESSAGE(BadPrint, "Bad print entity.");
MESSAGE(BadCall, "Bad call entity.");
MESSAGE(NoConstantAssign, "Cannot reassign to constant \'{}\'.");
MESSAGE(DataType, "Data member already has different type.");
MESSAGE(BadStmtNode, "Bad StmtNode type.");

MESSAGE(ReturnAtEnd, "Return statement must be at end of block.");
MESSAGE(FunctionTopLevel, "Functions must be defined at top-level.");
MESSAGE(ReturnOnlyInFunction, "Return statement can only appear inside a function.");
MESSAGE(FunctionTypeSet, "Function return type already set to different type.");
MESSAGE(NoExpression, "Expression not permitted in this context.");

MESSAGE(InitialPrompt, "Abaci version {}\nEnter code, or \"{}\" to end:\n> ");
MESSAGE(InputPrompt, "> ");
MESSAGE(ContinuationPrompt, ". ");
MESSAGE(SyntaxError, "Unrecognized input.");
MESSAGE(Version, "0.1.4 (2024-Dec-21)");

MESSAGE(BadVariableIndex, "Bad index of variable.");
MESSAGE(BadTemporary, "Bad temporary.");
MESSAGE(NotImplemented, "Not yet implemented.");
MESSAGE(CompilerInconsistency, "Compiler inconsistency detected: {}");
MESSAGE(AssertionFailed, "Assertion failed ({})");
MESSAGE(SourceFile, "\nSource filename: {}");
MESSAGE(LineNumber, ", Line number: {}");
MESSAGE(ErrorAtLine, "Error at line {}: ");

MESSAGE(EmptyListNeedsType, "An empty list must be given an element type.");
MESSAGE(ListTypeMismatch, "Mismatching types for list elements (\'{}\' and \'{}\').");
MESSAGE(VariableNotList, "Variable \'{}\' is not a list or a string.");
MESSAGE(IndexNotInt, "Index to \'{}\' must be of integer type.");
MESSAGE(TooManyIndexes, "Cannot get subscript of \'{}\'.");
MESSAGE(AssignMismatch, "Mismatching types for element assignment (\'{}\' and \'{}\').");
MESSAGE(IncompatibleTypes, "Types \'{}\' and \'{}\' are incompatible.");

#endif
