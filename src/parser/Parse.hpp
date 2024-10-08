#ifndef Parse_hpp
#define Parse_hpp

#include "ast/Expr.hpp"
#include "ast/Stmt.hpp"
#include "utility/Constant.hpp"
#include <string>
#include <ostream>

namespace abaci::parser {

bool parseBlock(const std::string& block_str, abaci::ast::StmtList& ast, std::ostream& error, std::string::const_iterator& iter, abaci::utility::Constants *constants);

bool parseStatement(const std::string& stmt_str, abaci::ast::StmtNode& ast, std::ostream& error, std::string::const_iterator& iter, abaci::utility::Constants *constants);

bool testStatement(const std::string& stmt_str);

} // namespace abaci::parser

#endif
