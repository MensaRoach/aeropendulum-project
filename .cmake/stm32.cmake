include_guard(GLOBAL)

function(stm32_list_boards OUT_VAR)
    file(GLOB manifests "${CMAKE_SOURCE_DIR}/BOARDS/*/board.cmake")
    set(boards "")
    foreach(manifest ${manifests})
        get_filename_component(board_dir "${manifest}" DIRECTORY)
        get_filename_component(board_name "${board_dir}" NAME)
        list(APPEND boards "${board_name}")
    endforeach()
    set(${OUT_VAR} ${boards} PARENT_SCOPE)
endfunction()

function(stm32_board)
    set(options "")
    set(oneValueArgs MCU_FAMILY DEVICE CPU_PROFILE DRIVER_MODE DEBUG_DEVICE FAMILY_DIR LINKER_SCRIPT SPECS PRINTF_FLOAT)
    set(multiValueArgs EXTRA_DEFINES EXTRA_INCLUDE_DIRS)
    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(ARG_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "stm32_board: Unrecognized arguments: ${ARG_UNPARSED_ARGUMENTS}")
    endif()

    if(NOT ARG_MCU_FAMILY)
        message(FATAL_ERROR "stm32_board: MCU_FAMILY is required")
    endif()
    if(NOT ARG_DEVICE)
        message(FATAL_ERROR "stm32_board: DEVICE is required")
    endif()
    if(NOT ARG_CPU_PROFILE)
        message(FATAL_ERROR "stm32_board: CPU_PROFILE is required")
    endif()
    if(NOT ARG_DRIVER_MODE)
        message(FATAL_ERROR "stm32_board: DRIVER_MODE is required")
    endif()
    if(NOT ARG_DEBUG_DEVICE)
        message(FATAL_ERROR "stm32_board: DEBUG_DEVICE is required")
    endif()

    if(NOT ARG_FAMILY_DIR)
        set(ARG_FAMILY_DIR "${ARG_MCU_FAMILY}xx")
    endif()

    if(NOT ARG_SPECS)
        set(ARG_SPECS "nano.specs")
    endif()

    if(NOT ARG_DRIVER_MODE STREQUAL "LL" AND NOT ARG_DRIVER_MODE STREQUAL "HAL" AND NOT ARG_DRIVER_MODE STREQUAL "BOTH")
        message(FATAL_ERROR "stm32_board: Unknown DRIVER_MODE '${ARG_DRIVER_MODE}'. Must be LL, HAL, or BOTH.")
    endif()

    set(STM32_BOARD_MCU_FAMILY "${ARG_MCU_FAMILY}" PARENT_SCOPE)
    set(STM32_BOARD_DEVICE "${ARG_DEVICE}" PARENT_SCOPE)
    set(STM32_BOARD_CPU_PROFILE "${ARG_CPU_PROFILE}" PARENT_SCOPE)
    set(STM32_BOARD_DRIVER_MODE "${ARG_DRIVER_MODE}" PARENT_SCOPE)
    set(STM32_BOARD_DEBUG_DEVICE "${ARG_DEBUG_DEVICE}" PARENT_SCOPE)
    set(STM32_BOARD_FAMILY_DIR "${ARG_FAMILY_DIR}" PARENT_SCOPE)
    set(STM32_BOARD_LINKER_SCRIPT "${ARG_LINKER_SCRIPT}" PARENT_SCOPE)
    set(STM32_BOARD_SPECS "${ARG_SPECS}" PARENT_SCOPE)
    set(STM32_BOARD_EXTRA_DEFINES "${ARG_EXTRA_DEFINES}" PARENT_SCOPE)
    set(STM32_BOARD_EXTRA_INCLUDE_DIRS "${ARG_EXTRA_INCLUDE_DIRS}" PARENT_SCOPE)
    set(STM32_BOARD_PRINTF_FLOAT "${ARG_PRINTF_FLOAT}" PARENT_SCOPE)
endfunction()

function(add_stm32_board BOARD)
    set(BOARD_DIR "${CMAKE_SOURCE_DIR}/BOARDS/${BOARD}")

    if(NOT EXISTS "${BOARD_DIR}/board.cmake")
        stm32_list_boards(AVAILABLE_BOARDS)
        string(REPLACE ";" ", " AVAILABLE_BOARDS_STR "${AVAILABLE_BOARDS}")
        message(FATAL_ERROR "Unknown board '${BOARD}'. Available: ${AVAILABLE_BOARDS_STR}")
    endif()

    include("${BOARD_DIR}/board.cmake")

    cortex_flags("${STM32_BOARD_CPU_PROFILE}" TARGET_FLAGS)

    # -----------------------------------------------------------------------
    # _cpu  — CPU compile/link flags only. No board-specific include paths or
    #         defines. LIB/ targets link this so the compiler accepts the MCU
    #         ABI without gaining access to STM32 peripheral headers.
    # _config — links _cpu PUBLIC, then adds board include paths and defines.
    #           Everything that linked _config before still works unchanged.
    # -----------------------------------------------------------------------
    add_library(${BOARD}_cpu INTERFACE)
    target_compile_options(${BOARD}_cpu INTERFACE ${TARGET_FLAGS})
    target_link_options(${BOARD}_cpu INTERFACE ${TARGET_FLAGS})

    add_library(${BOARD}_config INTERFACE)
    target_link_libraries(${BOARD}_config INTERFACE ${BOARD}_cpu)

    target_include_directories(${BOARD}_config INTERFACE
        "${BOARD_DIR}/Core/Inc"
        "${BOARD_DIR}/Drivers/${STM32_BOARD_FAMILY_DIR}_HAL_Driver/Inc"
        "${BOARD_DIR}/Drivers/${STM32_BOARD_FAMILY_DIR}_HAL_Driver/Inc/Legacy"
        "${BOARD_DIR}/Drivers/CMSIS/Device/ST/${STM32_BOARD_FAMILY_DIR}/Include"
        "${BOARD_DIR}/Drivers/CMSIS/Include"
        ${STM32_BOARD_EXTRA_INCLUDE_DIRS}
    )

    set(DRIVER_DEFINES "")
    if(STM32_BOARD_DRIVER_MODE STREQUAL "LL")
        list(APPEND DRIVER_DEFINES USE_FULL_LL_DRIVER)
    elseif(STM32_BOARD_DRIVER_MODE STREQUAL "HAL")
        list(APPEND DRIVER_DEFINES USE_HAL_DRIVER)
    elseif(STM32_BOARD_DRIVER_MODE STREQUAL "BOTH")
        list(APPEND DRIVER_DEFINES USE_FULL_LL_DRIVER USE_HAL_DRIVER)
    endif()

    # -----------------------------------------------------------------------
    # Board identity macros
    # -----------------------------------------------------------------------
    # Derived from BOARD_NAME, so the presets stay the single source of truth:
    # a preset sets BOARD_NAME, and the matching macro follows automatically.
    # Adding a board needs no edit here.
    #
    #   NUCLEO-F401RE      -> BOARD_NUCLEO_F401RE   + BOARD_NAME_STR "NUCLEO-F401RE"
    #   STM32F407G-DISC1   -> BOARD_STM32F407G_DISC1
    #
    # These live on _config, not _cpu, which means LIB/ cannot see them. That is
    # deliberate: portable device code must never branch on which board it is
    # running on. DRIVERS/ and APPS/ can see them.
    #
    # Intended use is conditional *availability* — a peripheral or generated
    # middleware that exists on one board and not the other (see usb.c). Using
    # them to paper over register-level differences inside DRIVERS/stm32f4/
    # would defeat the per-family sharing this layer exists for; prefer
    # pin_definitions.h and runtime configuration for that.
    string(TOUPPER "${BOARD}" _board_macro)
    string(REGEX REPLACE "[^A-Z0-9]" "_" _board_macro "${_board_macro}")

    target_compile_definitions(${BOARD}_config INTERFACE
        ${DRIVER_DEFINES}
        ${STM32_BOARD_DEVICE}
        BOARD_${_board_macro}
        BOARD_NAME_STR="${BOARD}"
        ${STM32_BOARD_EXTRA_DEFINES}
    )

    # Promote MCU_FAMILY to a cache variable so subdirectories added after
    # add_stm32_board() (DRIVERS, LIB) can read it without relying on scope.
    set(MCU_FAMILY "${STM32_BOARD_MCU_FAMILY}" CACHE STRING "MCU family for the selected board" FORCE)

    file(GLOB STM32_SOURCES CONFIGURE_DEPENDS
        "${BOARD_DIR}/Core/Src/*.c"
        "${BOARD_DIR}/Drivers/${STM32_BOARD_FAMILY_DIR}_HAL_Driver/Src/*.c"
    )

    file(GLOB STM32_STARTUP CONFIGURE_DEPENDS
        "${BOARD_DIR}/Core/Startup/*.s"
    )

    add_executable(${BOARD} ${STM32_SOURCES} ${STM32_STARTUP})

    target_link_libraries(${BOARD} PRIVATE ${BOARD}_config)
    target_link_libraries(${BOARD} PRIVATE m)

    if(STM32_BOARD_LINKER_SCRIPT)
        if(IS_ABSOLUTE "${STM32_BOARD_LINKER_SCRIPT}")
            set(LINKER_SCRIPT "${STM32_BOARD_LINKER_SCRIPT}")
        else()
            set(LINKER_SCRIPT "${BOARD_DIR}/${STM32_BOARD_LINKER_SCRIPT}")
        endif()
    else()
        file(GLOB STM32_LINKER_SCRIPTS "${BOARD_DIR}/*FLASH.ld")
        if(STM32_LINKER_SCRIPTS)
            list(GET STM32_LINKER_SCRIPTS 0 LINKER_SCRIPT)
        else()
            message(FATAL_ERROR "Could not find a FLASH.ld linker script for board '${BOARD}' in ${BOARD_DIR}")
        endif()
    endif()

    target_link_options(${BOARD} PRIVATE
        -T${LINKER_SCRIPT}
        --specs=${STM32_BOARD_SPECS}
        -Wl,-Map=$<TARGET_FILE_DIR:${BOARD}>/${BOARD}.map
        -Wl,--gc-sections
        -Wl,--print-memory-usage
    )

    if(STM32_BOARD_PRINTF_FLOAT)
        target_link_options(${BOARD} PRIVATE -u _printf_float)
    endif()
endfunction()
