set(LIB_SOURCES
        ulog_read.c 
        ulog_write.c
        ulog.cpp
)
if(UNIX)
  set(LIB_SOURCES
        ${LIB_SOURCES}
        ulog_write_android.c 
        ulog_write_bin.c 
        ulog_write_raw.c
  )
endif()


add_library(ulog ${LIB_SOURCES})

#option(BUILD_SHARED_LIBS "Build using shared libraries" ON)
set_target_properties(ulog PROPERTIES
                    POSITION_INDEPENDENT_CODE ${BUILD_SHARED_LIBS}
)

target_include_directories(ulog PUBLIC
                            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                            $<INSTALL_INTERFACE:include>
)

target_compile_definitions(ulog PRIVATE "LIBULOG_HEADERS=${CMAKE_CURRENT_SOURCE_DIR}/include/ulog.h:${CMAKE_CURRENT_SOURCE_DIR}/include/ulograw.h;")
target_compile_options(ulog PRIVATE "-fvisibility=hidden")

if(WIN32)
        target_link_libraries(ulog
                        pthread
        )
endif()
