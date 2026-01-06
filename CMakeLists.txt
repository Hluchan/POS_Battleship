cmake_minimum_required(VERSION 3.25)
project(POS_Battleship C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pthread -Wall -Wextra")

find_package(Curses REQUIRED)

set(COMMON_SOURCES
        common/protocol.h
        common/game_constants.h
)

add_executable(server
        server/server.c
        server/game_logic.c
        server/game_logic.h
        server/server.h
        ${COMMON_SOURCES}
)

target_include_directories(server PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CURSES_INCLUDE_DIRS}
)

target_link_libraries(server
        pthread
        ${CURSES_LIBRARIES}
)

add_executable(client
        client/client.c
        client/ui.c
        client/ui.h
        client/client.h
        ${COMMON_SOURCES}
)

target_include_directories(client PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CURSES_INCLUDE_DIRS}
)

target_link_libraries(client
        pthread
        ${CURSES_LIBRARIES}
)
