#ifndef Keywords_hpp
#define Keywords_hpp

#define SYMBOL(TOKEN, VALUE) inline const char *TOKEN = reinterpret_cast<const char *>(u8##VALUE)

SYMBOL(AND, "and");
SYMBOL(BOOL, "bool");
SYMBOL(CASE, "case");
SYMBOL(CLASS, "class");
SYMBOL(COMPLEX, "complex");
SYMBOL(ELSE, "else");
SYMBOL(ENDCASE, "endcase");
SYMBOL(ENDCLASS, "endclass");
SYMBOL(ENDFN, "endfn");
SYMBOL(ENDIF, "endif");
SYMBOL(ENDWHILE, "endwhile");
SYMBOL(EXIT, "exit");
SYMBOL(FALSE, "false");
SYMBOL(FLOAT, "float");
SYMBOL(FN, "fn");
SYMBOL(IF, "if");
SYMBOL(IMAG, "imag");
SYMBOL(INPUT, "input");
SYMBOL(INT, "int");
SYMBOL(LET, "let");
SYMBOL(NATIVE, "native");
SYMBOL(NIL, "nil");
SYMBOL(NOT, "not");
SYMBOL(OR, "or");
SYMBOL(PRINT, "print");
SYMBOL(REAL, "real");
SYMBOL(REM, "#");
SYMBOL(REPEAT, "repeat");
SYMBOL(RETURN, "return");
SYMBOL(STR, "str");
SYMBOL(THIS, "this");
SYMBOL(TRUE, "true");
SYMBOL(UNTIL, "until");
SYMBOL(WHEN, "when");
SYMBOL(WHILE, "while");

SYMBOL(PLUS, "+");
SYMBOL(MINUS, "-");
SYMBOL(TIMES, "*");
SYMBOL(DIVIDE, "/");
SYMBOL(MODULO, "%");
SYMBOL(FLOOR_DIVIDE, "//");
SYMBOL(EXPONENT, "**");

SYMBOL(EQUAL, "=");
SYMBOL(NOT_EQUAL, "!=");
SYMBOL(LESS, "<");
SYMBOL(LESS_EQUAL, "<=");
SYMBOL(GREATER_EQUAL, ">=");
SYMBOL(GREATER, ">");

SYMBOL(BITWISE_AND, "&");
SYMBOL(BITWISE_OR, "|");
SYMBOL(BITWISE_XOR, "^");
SYMBOL(BITWISE_COMPL, "~");

SYMBOL(COMMA, ",");
SYMBOL(DOT, ".");
SYMBOL(SEMICOLON, ";");
SYMBOL(COLON, ":");
SYMBOL(QUESTION, "?");
SYMBOL(BANG, "!");
SYMBOL(LEFT_PAREN, "(");
SYMBOL(RIGHT_PAREN, ")");
SYMBOL(LEFT_BRACKET, "[");
SYMBOL(RIGHT_BRACKET, "]");
SYMBOL(FROM, "<-");
SYMBOL(TO, "->");
SYMBOL(IMAGINARY, "j");
SYMBOL(HEX_PREFIX, "0x");
SYMBOL(OCT_PREFIX, "0");
SYMBOL(BIN_PREFIX, "0b");

SYMBOL(NONE, "none");
SYMBOL(I1, "i1");
SYMBOL(I8, "i8");
SYMBOL(I16, "i16");
SYMBOL(I32, "i32");
SYMBOL(I64, "i64");
SYMBOL(F32, "f32");
SYMBOL(F64, "f64");
SYMBOL(I8_STAR, "i8*");

SYMBOL(THIS_V, "_this");
SYMBOL(RETURN_V, "_return");

#endif
