# Makefile for abaci project

CPP = g++
#CPP = clang++
CPPFLAGS = -Wall -Wextra -std=c++23 -I. #-fsanitize=address
LDFLAGS = -lfmt `llvm-config --ldflags --libs core executionengine interpreter analysis native bitwriter orcjit` #-fsanitize=address
OBJS = lib/Abaci.o utility/Type.o utility/Constant.o utility/Operator.o utility/Symbol.o parser/Parse.o \
	codegen/TypeCodeGen.o codegen/ExprCodeGen.o codegen/StmtCodeGen.o \
	engine/Utility.o engine/Cache.o engine/JIT.o main.o

BASE = abaci

all: $(BASE)

%.o: %.cpp
	$(CPP) $(CPPFLAGS) -c -o $@ $<

$(BASE): $(OBJS)
	$(CPP) -o $(BASE) $(OBJS) $(LDFLAGS)

clean:
	rm -f $(BASE) $(OBJS)
