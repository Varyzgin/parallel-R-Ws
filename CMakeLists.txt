cmake_minimum_required(VERSION 3.10)
project(ParallelProgram)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Находим pthread
find_package(Threads REQUIRED)

# Добавляем исполняемый файл
add_executable(parallel_program src/ParallelProgram.cpp)

# Линкуем pthread
target_link_libraries(parallel_program PRIVATE Threads::Threads)

# Для Linux может потребоваться явно указать -pthread
if(UNIX AND NOT APPLE)
    target_link_options(parallel_program PRIVATE -pthread)
endif()