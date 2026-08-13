# APPS will be a static library linked to the board executable
add_library(apps STATIC src/main_app.c)

# Add our board specific pin definitions to include path
target_include_directories(apps PRIVATE ../BOARDS/${BOARD_NAME})
target_include_directories(apps PRIVATE inc)

# Link to our custom STM32 configuration interface library
target_link_libraries(apps PRIVATE ${BOARD_NAME}_config)

# Link APPS to the board executable
if(TARGET ${BOARD_NAME})
    target_link_libraries(${BOARD_NAME} PRIVATE apps)
endif()
