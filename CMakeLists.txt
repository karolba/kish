cmake_minimum_required(VERSION 3.5)

project(kish LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(kish
    main.cpp
    Parser.cpp
    Tokenizer.cpp
    WordExpander.cpp
    CommandExpander.cpp
    utils.cpp
    Global.cpp
)
