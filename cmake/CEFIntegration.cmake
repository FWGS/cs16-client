cmake_policy(SET CMP0074 NEW)

function(cs16client_setup_cef target_name)
    if(NOT CS16CLIENT_ENABLE_CEF)
        return()
    endif()

    if(NOT CS16CLIENT_CEF_ROOT)
        message(FATAL_ERROR "CEF enabled but CS16CLIENT_CEF_ROOT is empty")
    endif()

    set(CEF_ROOT "${CS16CLIENT_CEF_ROOT}")

    # --------------------------------------------------
    # C++ standard (важно для CEF 147+)
    # --------------------------------------------------
    target_compile_features(${target_name} PRIVATE cxx_std_20)

    # --------------------------------------------------
    # CMake find CEF
    # --------------------------------------------------
    list(APPEND CMAKE_MODULE_PATH "${CEF_ROOT}/cmake")
    find_package(CEF REQUIRED)

    # --------------------------------------------------
    # libcef_dll_wrapper
    # --------------------------------------------------
    if(NOT TARGET libcef_dll_wrapper)
        add_subdirectory(
            "${CEF_LIBCEF_DLL_WRAPPER_PATH}"
            "${CMAKE_BINARY_DIR}/cef_libcef_dll_wrapper"
        )
    endif()

    target_link_libraries(${target_name} PRIVATE libcef_dll_wrapper)

    target_include_directories(${target_name} PRIVATE
        "${CEF_INCLUDE_PATH}"
    )

    target_compile_definitions(${target_name} PRIVATE
        CS16CLIENT_ENABLE_CEF=1
    )

    # --------------------------------------------------
    # Platform-specific paths
    # --------------------------------------------------
    set(CEF_BIN "${CEF_ROOT}/Release")
    set(CEF_RES "${CEF_ROOT}/Resources")

    # ==================================================
    # WINDOWS
    # ==================================================
    if(WIN32)
        target_compile_definitions(${target_name} PRIVATE NOMINMAX)

        target_link_directories(${target_name} PRIVATE "${CEF_BIN}")

        target_link_libraries(${target_name} PRIVATE
            "${CEF_BIN}/libcef.lib"
        )

        # --------------------------------------------------
        # Install binaries
        # --------------------------------------------------
        if(EXISTS "${CEF_BIN}")
            install(DIRECTORY "${CEF_BIN}/"
                DESTINATION "${GAME_DIR}/cef/"
                FILES_MATCHING
                    PATTERN "*.dll"
                    PATTERN "*.bin"
                    PATTERN "*.pak"
            )
        endif()

        # --------------------------------------------------
        # Resources (IMPORTANT!)
        # --------------------------------------------------
        if(EXISTS "${CEF_RES}")
            install(DIRECTORY "${CEF_RES}/"
                DESTINATION "${GAME_DIR}/cef/"
                FILES_MATCHING
                    PATTERN "*.pak"
                    PATTERN "*.dat"
                    PATTERN "*.bin"
            )
        endif()

        # --------------------------------------------------
        # Locales
        # --------------------------------------------------
        if(EXISTS "${CEF_RES}/locales")
            install(DIRECTORY "${CEF_RES}/locales/"
                DESTINATION "${GAME_DIR}/cef/locales/"
            )
        endif()

    # ==================================================
    # APPLE
    # ==================================================
    elseif(APPLE)

        set(CEF_FW "${CEF_ROOT}/Release/Chromium Embedded Framework.framework")

        if(NOT EXISTS "${CEF_FW}/Chromium Embedded Framework")
            message(FATAL_ERROR "Invalid CEF framework at ${CEF_FW}")
        endif()

        target_link_libraries(${target_name} PRIVATE
            "${CEF_FW}/Chromium Embedded Framework"
        )

        set_target_properties(${target_name} PROPERTIES
            BUILD_RPATH "@loader_path"
            INSTALL_RPATH "@loader_path"
        )

        add_custom_command(TARGET ${target_name} POST_BUILD
            COMMAND /usr/bin/install_name_tool -change
                "@executable_path/../Frameworks/Chromium Embedded Framework.framework/Chromium Embedded Framework"
                "@loader_path/../Chromium Embedded Framework.framework/Chromium Embedded Framework"
                "$<TARGET_FILE:${target_name}>"
        )

        install(DIRECTORY "${CEF_FW}"
            DESTINATION "${GAME_DIR}"
        )

    # ==================================================
    # LINUX
    # ==================================================
    elseif(UNIX AND NOT APPLE)

        target_link_directories(${target_name} PRIVATE "${CEF_BIN}")

        target_link_libraries(${target_name} PRIVATE
            "${CEF_BIN}/libcef.so"
        )

        set_target_properties(${target_name} PROPERTIES
            BUILD_RPATH "\$ORIGIN"
        )

        # --------------------------------------------------
        # binaries
        # --------------------------------------------------
        if(EXISTS "${CEF_BIN}")
            install(DIRECTORY "${CEF_BIN}/"
                DESTINATION "${GAME_DIR}/cef/"
                FILES_MATCHING
                    PATTERN "*.so"
                    PATTERN "*.bin"
            )
        endif()

        # --------------------------------------------------
        # resources
        # --------------------------------------------------
        if(EXISTS "${CEF_RES}")
            install(DIRECTORY "${CEF_RES}/"
                DESTINATION "${GAME_DIR}/cef/"
                FILES_MATCHING
                    PATTERN "*.pak"
                    PATTERN "*.dat"
            )
        endif()

        # --------------------------------------------------
        # locales FIX
        # --------------------------------------------------
        if(EXISTS "${CEF_RES}/locales")
            install(DIRECTORY "${CEF_RES}/locales/"
                DESTINATION "${GAME_DIR}/cef/locales/"
            )
        endif()

    # ==================================================
    # ANDROID
    # ==================================================
    elseif(ANDROID)

        target_compile_definitions(${target_name} PRIVATE
            CS16CLIENT_CEF_ANDROID=1
        )

    endif()

endfunction()