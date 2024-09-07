#ifndef Parse_hpp
#define Parse_hpp

#include "ast/Expr.hpp"
#include "ast/Stmt.hpp"
#include "utility/Constant.hpp"
#include <string>
#include <ostream>

namespace abaci::parser {

bool parse_block(const std::string& block_str, abaci::ast::StmtList& ast, std::ostream& error, abaci::utility::Constants *constants);

bool parse_statement(std::string& stmt_str, abaci::ast::StmtNode& ast, std::ostream& error, abaci::utility::Constants *constants);

bool test_statement(const std::string& stmt_str);

} // namespace abaci::parser

#endif
