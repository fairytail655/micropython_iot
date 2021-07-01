# Create an INTERFACE library for our C module.
add_library(iot_light INTERFACE)

# Add our source files to the lib
target_sources(iot_light INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/iot_light.c
)

# Add the current directory as an include directory.
target_include_directories(iot_light INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}
)

# Link our INTERFACE library to the usermod target.
target_link_libraries(usermod INTERFACE iot_light)
