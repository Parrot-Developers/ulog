set(LIB_SOURCES
        ulog_read.c 
        ulog_write.c
        ulog_write_android.c
        ulog.cpp
)

add_library(ulog ${LIB_SOURCES})

option(BUILD_SHARED_LIBS "Build using shared libraries" ON)

target_include_directories(ulog PUBLIC
                            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                            $<INSTALL_INTERFACE:include>
)

target_compile_definitions(ulog PRIVATE "_FILE_OFFSET_BITS=64")
target_compile_definitions(ulog PRIVATE "LIBULOG_HEADERS=${CMAKE_CURRENT_SOURCE_DIR}/include/ulog.h:${CMAKE_CURRENT_SOURCE_DIR}/include/ulograw.h;")
target_compile_options(ulog PRIVATE "-fvisibility=hidden")


target_link_libraries(ulog
                      pthread
)

