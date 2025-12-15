cmake_minimum_required(VERSION 3.25)
project(POS_Battleship C)

set(CMAKE_C_STANDARD 11)

add_executable( server
        server/server.c
)

# ncurses library
target_include_directories(server PRIVATE ${CURSES_INCLUDE_DIRS})
target_link_libraries(server ${CURSES_LIBRARIES})