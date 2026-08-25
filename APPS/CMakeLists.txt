# Find all source files in the src directory
file(GLOB APPS_SOURCES CONFIGURE_DEPENDS "src/*.c")

# APPS will be a static library linked to the board executable
add_library(apps STATIC ${APPS_SOURCES})

# Add our board specific pin definitions to include path
target_include_directories(apps PRIVATE ${CMAKE_SOURCE_DIR}/BOARDS/${BOARD_NAME})
target_include_directories(apps PUBLIC inc)

# Link to our custom STM32 configuration interface library
target_link_libraries(apps PRIVATE ${BOARD_NAME}_config)

# Apps consume the drivers; drivers never consume apps.
target_link_libraries(apps PRIVATE drivers)

# Link APPS to the board executable
if(TARGET ${BOARD_NAME})
    target_link_libraries(${BOARD_NAME} PRIVATE apps)
endif()
