cmake_minimum_required(VERSION 3.5)

project(kish LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(
    tcbrindle-span/include
)

add_subdirectory(replxx)
link_libraries(replxx)

add_compile_options(
    -W
    -Wall
    -Wextra
    -pedantic
)

# This list contains headers and tests so that Qt Creator shows them nicely in the Projects view
add_executable(kish
    main.cpp
    Token.h
    Parser.cpp
    Parser.h
    Tokenizer.cpp
    Tokenizer.h
    WordExpander.cpp
    WordExpander.h
    CommandExpander.cpp
    CommandExpander.h
    utils.cpp
    utils.h
    Global.cpp
    Global.h
    builtins.cpp
    builtins.h
    executor.cpp
    executor.h
    highlight.cpp
    highlight.h
    builtins/true.cpp
    builtins/true.h
    builtins/false.cpp
    builtins/false.h
    builtins/help.cpp
    builtins/help.h
    builtins/cd.cpp
    builtins/cd.h
    builtins/colon.cpp
    builtins/colon.h
    builtins/read.cpp
    builtins/read.h

    test/tests.sh
    README.md
)
