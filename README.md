# Abaci scripting/programming language

## About

Abaci is a JIT-compiled interactive scripting language based on the LLVM toolchain. It is intended to be easy to learn by novice programmers and able to be translated into other natural (written) languages if desired. The interactive environment started by default compiles user input in real-time and executes it quickly. When called with a textual script file as an argument, this file is compiled and executed as a batch process.

## Pre-requisites

To build this project requires:

* A C++ compiler, tested only with `g++` version 12.2.0, Visual Studio 2022 C++ and `clang++` version 14.0.6
* The LLVM development libraries, built against version 14.0
* The Boost headers, specifically Boost Spirit X3 in Boost 1.74
* Headers and link-library of fmt built against version 10.2.1

This project was developed using Debian current stable (Bookworm).

## Building

To create the executable `abaci` in a `build` sub-directory of the project source files under Linux use:

```bash
mkdir build && cd build
cmake ..
make
```

To build with Clang it may be necessary to use C++20 (instead of C++23) in the CMakeLists.txt:

```cmake
set(CMAKE_CXX_STANDARD 20)
```

To build against C++23's `<format>` and `<print>` headers (if available) instead of fmt, use `cmake .. -DABACI_USE_STD_FORMAT=1"`.

To build with older Boost headers, in particular version 1.74, use `cmake .. -DABACI_USE_OLDER_BOOST=1` to avoid compilation errors for `parser/Parse.cpp`.

Run `./abaci` without arguments to enter an interactive session, or with a source filename as the single argument to execute a script.

To create the executable `abaci.exe` under Windows, using CMake and VS 2022, first build LLVM from source. Then make a `build` directory at the same level as `src` and issue:

```bash
cmake.exe .. -A x64 -Thost=x64 -DLLVM_DIR=C:\Users\<path_to>\llvm-19.1.1\lib\cmake\llvm -DBoost_INCLUDE_DIR=C:\Users\<path_to>\boost_1_86_0 -DABACI_USE_STD_FORMAT=1 -DCMAKE_CXX_FLAGS="/MP /EHsc"
```

Then load `Abaci.sln` into VS 2022 and select build type "Release" before building.

(Replace `<path_to>` in each case with the correct path to LLVM and Boost.)

## Status

This project is currently under heavy development and should be considered pre-alpha. Current plans for features to be added include:

* Broken parsing for method calls to be investigated *(completed)*
* Homogeneous lists *(completed)*
* String indexing *(completed)*
* Closures (major changes are likely needed)
* Error reporting tidied up *(completed)* with more accurate location reporting *(investigated and considered not possible)* 
* Memory leaks closed *(completed)*
* Unit tests of Abaci code *(completed)*

Please raise feature requests (and bugs found) as issues.

## Language overview

### Assignments

Constant assignment

```
let a = 3.3
```

Variable assignment

```
let b <- 1
```

Variable reassginment

```
b <- 2
```

Tuple assignment

```
class mytuple(x, y, z) endclass
let c <- mytuple(1, 2.2, "hi")
print c.z
```

Array and string assignment

```
let d = [ 2, 3, 5, 7, 11 ]
print !d
print d[3]
d[3] <- nil
print !d
print d[3]
let s <- "123456"
s[1] <- nil
print s
s[0] <- "88"
print s
```

Splice and slice

```
let l <- [[1,2],[3,4]]
l[1:1] <- [[7,7]]
print !l
let l2 <- l[0:2]
print !l2
```

Math operators

`+`, `-`, `*`, `/` (always creates floating point number), `%` (integer only), `//` (floor division), `**` (exponentiation)

### Comparison operators

`=` (same as assign constant, but in different context), `!=`, `<`, `<=`, `>=`, `>`, `<`/`<=` value `<`/`<=`

### Decisions

```
if *condition*
    statements...
endif
```

```
if *condition*
    statements...
else
    statements...
endif
```

```
case *expression>
  when *match1* statements...
  when *match2*, *match3*,... statements...
  else  # this is optional
  statements...
endcase
```

### Loops

```
while *condition*
  statements...
endwhile
```

```
repeat
  statements...
until *condition*
```

### Type conversions

```
let a <- 2.2
print str(a) + " floating"
print int(a)
print real(3 + 4j)
print imag(3 + 4j)
print float("3.14") * 2.0
```

### Function definition

```
let f(x) -> x + 1
print f(2)
```

```
fn g() 
  let x <- 0
  while 0 <= x < 10
    print x
    x <- x + 1
  endwhile
endfn

g()
```

### Function call

```
b <- f(b)
g()
```

### Native function call

For C or C++ functions linked to the executable the `native` keyword makes them available in the interpreter.

```
native fn user.sine(f64) -> f64
native fn user.putstr(i8*) -> none
```

Note that the types are different from regular Abaci types, they are:

`i1 i8 i16 i32 i64 f32 f64 i8* none`

and convert from `int`, `float` and `str` automatically.

Call syntax is the same as for regular functions:

```
print sine(3.14 / 2)
putstr("Abaci")
```

### Class definition

```
class c(x,y)
  fn i(d)
    print this.x * d
    print this.y * d
  endfn
endclass

let f <- c(2.2,3j)
f.i(2.5)
```

### Comments

```
\# This is a comment
```

## Unit tests

There is an additional target `abacitests` which creates an executable in directory `testing`. To build this on Linux use:

```bash
make test
```

On Windows, build sub-project `abacitests` to create `abacitests.exe`, or use:

```bash
msbuild Abaci.sln -t:abacitests -p:Configuration=Release -m
```

## Version history

* **0.0.1** (2024-Sep-07): First release of code for this project, many known bugs and problems

* **0.0.2** (2024-Sep-16): Second release with many changes and fixes, no known bugs

* **0.0.3** (2024-Sep-19): Third release with Clang compatibility and more fixes

* **0.0.4** (2024-Sep-21): Fourth release with stability and error message improvements

* **0.0.5** (2024-Sep-28): Fifth release with list support and many other improvements

* **0.0.6** (2024-Oct-04): Sixth release with code improvements and support for different builds

* **0.1.1** (2024-Oct-22): Seventh release with small syntax changes and unit testing capability

* **0.1.2** (2024-Nov-12): Eighth release with integrated unit testing

* **0.1.3** (2024-Nov-25): Ninth release with code improvements and some support for string indexing

* **0.1.4** (2024-Dec-21): Tenth release with full support for string indexing

* **0.2.0** (2025-May-18): Strings assumed to use UTF-8 encoding, splicing and slicing support for lists and strings, library can throw exceptions for Windows build

* **0.2.1** (2025-Jun-02): Added support for native functions, libraries must be already linked to executable to use functions from them.

* **0.2.2** (2025-Jun-09): Multiple native libraries can be dynamically loaded at runtime.

* **0.2.5** (2025-Aug-05): Allow trailing type specifiers for free function and class parameters, empty lists produce error message.

## License

All source code and documentation released under the MIT License, &copy;2024-25 Richard Spencer
