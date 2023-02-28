cmake_minimum_required(VERSION 3.15)
project(ulog VERSION 1.0)

add_subdirectory(libulog)


install(TARGETS ${PROJECT_NAME}
    EXPORT ${PROJECT_NAME}-targets
    PUBLIC_HEADER DESTINATION include
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)
