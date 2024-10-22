#include "TestJIT.hpp"
#include "utility/Context.hpp"
#include "utility/Constant.hpp"
#include "ast/Stmt.hpp"
#include "parser/Parse.hpp"
#include "codegen/TypeCodeGen.hpp"
#include "codegen/StmtCodeGen.hpp"
#include "engine/JIT.hpp"
#include "localize/Keywords.hpp"
#include "localize/Messages.hpp"
#include "utility/Temporary.hpp"
#include <string>
#include <sstream>

using abaci::ast::StmtList;
using abaci::engine::Cache;
using abaci::utility::Constants;
using abaci::utility::Context;
using abaci::utility::Temporaries;

class TestJITImpl {
    std::istringstream input;
    std::ostringstream output;
    StmtList ast;
    Constants constants;
    Context context;
    Cache functions;
    Temporaries temps;
public:
    TestJITImpl() : context(input, output, constants) {}
    TestJIT::Result test(const std::string& programText, const std::string& inputText);
    const std::string getOutput() const {
        return output.str();
    }
    void clearFunctions() {
        functions.clearInstantiations();
    }
};