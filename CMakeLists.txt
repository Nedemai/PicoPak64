
set(PICO_DEFAULT_BINARY_TYPE copy_to_ram)
# gets PROJECT name for the example (e.g. <BOARD>-<DIR_NAME>)
family_get_project_name(PROJECT ${CMAKE_CURRENT_LIST_DIR})

project(${PROJECT})

# Checks this example is valid for the family and initializes the project
family_initialize_project(${PROJECT} ${CMAKE_CURRENT_LIST_DIR})

add_executable(${PROJECT})

# Example source
target_sources(${PROJECT} PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/main.c
        ${CMAKE_CURRENT_SOURCE_DIR}/msc_disk.c
        ${CMAKE_CURRENT_SOURCE_DIR}/usb_descriptors.c
        )

# Example include
target_include_directories(${PROJECT} PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/
        )

        target_link_libraries(${PROJECT} PUBLIC pico_stdlib pico_multicore hardware_flash)

# Configure compilation flags and libraries for the example... see the corresponding function
# in hw/bsp/FAMILY/family.cmake for details.

#pico_add_extra_outputs(${PROJECT})

#example_auto_set_url(${PROJECT})
family_configure_device_example(${PROJECT} )

pico_set_binary_type(${PROJECT} copy_to_ram)
