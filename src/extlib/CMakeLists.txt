target_sources(${TARGET_NAME} PRIVATE
    "lib_net.cpp"
    "lib_main.cpp"
    "lib_message_queue.cpp"
    "console_input.cpp"
    "util/util.cpp"
)

if(CMAKE_C_COMPILER_TARGET MATCHES "windows" OR WIN32)
    message(STATUS "Building for Windows (Linking ws2_32)")
    
    target_compile_definitions(${TARGET_NAME} PRIVATE 
        _WIN32 
        WIN32
        _CRT_SECURE_NO_WARNINGS
    )
    
    target_link_libraries(${TARGET_NAME} ws2_32 winmm)
    
else()
    message(STATUS "Building for Non-Windows (Unix/Mac Sockets)")
    
    target_compile_definitions(${TARGET_NAME} PRIVATE 
        _CRT_SECURE_NO_WARNINGS
    )
endif()
