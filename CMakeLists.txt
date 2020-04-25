cmake_minimum_required(VERSION 3.5)

project(kish LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(
    tcbrindle-span/include
)

add_compile_options(
    -W
    -Wall
    -Wextra
    -pedantic
)

add_executable(kish
    main.cpp
    Parser.cpp
    Tokenizer.cpp
    WordExpander.cpp
    CommandExpander.cpp
    utils.cpp
    Global.cpp
    builtins.cpp
    executor.cpp
    builtins/true.cpp
    builtins/false.cpp
    builtins/help.cpp
    builtins/cd.cpp
    builtins/colon.cpp

    # Just so those files show up in the Qt Creator project view
    test/tests.sh
    README.md
)
