add_library(network
    src/network.c
)

target_include_directories(network PUBLIC include)

target_link_libraries(network PUBLIC
    common
)
