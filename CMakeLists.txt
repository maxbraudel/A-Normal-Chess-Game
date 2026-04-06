cmake_minimum_required(VERSION 3.20)
project(ANormalChessGame LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Point CMake at the MSYS2 ucrt64 libraries (SFML 2.6)
list(PREPEND CMAKE_PREFIX_PATH "C:/msys64/ucrt64")

# Find SFML first (needed by TGUI)
find_package(SFML 2.6 COMPONENTS graphics window system REQUIRED)

# Build TGUI from source with SFML_GRAPHICS backend
include(FetchContent)
FetchContent_Declare(
    TGUI
    GIT_REPOSITORY https://github.com/texus/TGUI.git
    GIT_TAG        v1.10.0
    GIT_SHALLOW    TRUE
)
set(TGUI_BACKEND "SFML_GRAPHICS" CACHE STRING "" FORCE)
set(TGUI_BUILD_GUI_BUILDER OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(TGUI)

# Collect source files
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
    src/*.cpp
)

# Executable
add_executable(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(${PROJECT_NAME} PRIVATE
    sfml-graphics
    sfml-window
    sfml-system
    TGUI::TGUI
    "C:/msys64/ucrt64/lib/libpthread.dll.a"
)

# Copy assets to build directory
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:${PROJECT_NAME}>/assets
)

# Copy saves directory
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
    $<TARGET_FILE_DIR:${PROJECT_NAME}>/saves
)

# Copy ALL required MSYS2 ucrt64 runtime DLLs (recursively resolved via objdump)
# This is the complete transitive dependency list — no more missing DLL errors.
set(MSYS2_BIN "C:/msys64/ucrt64/bin")
set(RUNTIME_DLLS
    # SFML core
    "${MSYS2_BIN}/libsfml-graphics-2.dll"
    "${MSYS2_BIN}/libsfml-system-2.dll"
    "${MSYS2_BIN}/libsfml-window-2.dll"
    # C++ / MinGW runtime
    "${MSYS2_BIN}/libgcc_s_seh-1.dll"
    "${MSYS2_BIN}/libstdc++-6.dll"
    "${MSYS2_BIN}/libwinpthread-1.dll"
    # FreeType and its transitive deps
    "${MSYS2_BIN}/libfreetype-6.dll"
    "${MSYS2_BIN}/libbz2-1.dll"
    "${MSYS2_BIN}/libpng16-16.dll"
    "${MSYS2_BIN}/zlib1.dll"
    "${MSYS2_BIN}/libbrotlidec.dll"
    "${MSYS2_BIN}/libbrotlicommon.dll"
    "${MSYS2_BIN}/libharfbuzz-0.dll"
    "${MSYS2_BIN}/libgraphite2.dll"
    "${MSYS2_BIN}/libglib-2.0-0.dll"
    "${MSYS2_BIN}/libintl-8.dll"
    "${MSYS2_BIN}/libiconv-2.dll"
    "${MSYS2_BIN}/libpcre2-8-0.dll"
)
foreach(DLL ${RUNTIME_DLLS})
    if(EXISTS "${DLL}")
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${DLL}" $<TARGET_FILE_DIR:${PROJECT_NAME}>
        )
    endif()
endforeach()
