cmake_minimum_required(VERSION 3.16)
project(MiniSSH)

add_subdirectory(libs)

add_executable(server server/main.c)

target_link_libraries(server PRIVATE
    network
    proto
)

add_executable(client client/main.c)

target_link_libraries(client PRIVATE
    network
    pthread
    proto
)
