cmake_minimum_required(VERSION 3.10)

# ================================
# Название проекта
# ================================
project(RPG_Editor LANGUAGES CXX)

# ================================
# Стандарт C++
# ================================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ================================
# Исходные файлы проекта
# ================================
set(SOURCES
    main.cpp
    functions.cpp
)

set(HEADERS
    functions.h
)

# ================================
# Создание исполняемого файла
# ================================
add_executable(rpg_editor
    ${SOURCES}
    ${HEADERS}
)

# ================================
# Предупреждения компилятора (по ГОСТ/методичке приветствуется)
# ================================
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(rpg_editor PRIVATE -Wall -Wextra -Wpedantic)
elseif (MSVC)
    target_compile_options(rpg_editor PRIVATE /W4)
endif()
