add_executable(app main.cpp)

if (OPTIMIZE STREQUAL "ON")
    target_compile_options(app PRIVATE -O2 -fstack-usage)
elseif (OPTIMIZE STREQUAL "OFF")
    target_compile_options(app PRIVATE -O0 -fstack-usage)
endif()

target_link_libraries(app PRIVATE cv)