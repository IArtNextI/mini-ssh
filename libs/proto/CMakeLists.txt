add_library(proto
    src/proto.c
)

target_include_directories(proto PUBLIC include)

target_link_libraries(proto
    PUBLIC common
    PRIVATE network
)
