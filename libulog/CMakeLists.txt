cmake_minimum_required(VERSION 3.15)
project(ulog VERSION 1.0)

set(LIB_SOURCES
        ulog_read.c 
        ulog_write.c
        ulog_write_android.c
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


add_library(${PROJECT_NAME} SHARED ${LIB_SOURCES})

# checks if set up rpath exists for install
if(COMMAND set_up_rpath)
    set_up_rpath()
else()
    message("Set up rpath not defined!")
endif()

#option(BUILD_SHARED_LIBS "Build using shared libraries" ON)
set_target_properties(${PROJECT_NAME} PROPERTIES
                    POSITION_INDEPENDENT_CODE ${BUILD_SHARED_LIBS}
)

target_include_directories(${PROJECT_NAME} PUBLIC
                            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                            $<INSTALL_INTERFACE:include>
)

target_compile_definitions(${PROJECT_NAME} PRIVATE "LIBULOG_HEADERS=${CMAKE_CURRENT_SOURCE_DIR}/include/ulog.h:${CMAKE_CURRENT_SOURCE_DIR}/include/ulograw.h;")
target_compile_options(${PROJECT_NAME} PRIVATE "-fvisibility=hidden")


target_link_libraries(${PROJECT_NAME}
                    PRIVATE
                      pthread
)


install(TARGETS ${PROJECT_NAME}
        EXPORT ${PROJECT_NAME}-targets
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
        RUNTIME DESTINATION bin
        )
