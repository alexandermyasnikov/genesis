
project(genesis_console)

set(CMAKE_BUILD_TYPE Release)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "-Wall -Wextra -Werror -pedantic -fconcepts")
set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DPRODUCTION")

add_executable(genesis_console src/genesis_console.cpp)

add_executable(genesis_gui src/genesis_gui.cpp)
target_link_libraries(genesis_gui sfml-graphics sfml-window sfml-system)
target_link_libraries(genesis_gui pthread)


