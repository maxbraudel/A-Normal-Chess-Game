cmake_minimum_required(VERSION 3.20)
project(ANormalChessGame LANGUAGES CXX)

if(APPLE)
    enable_language(OBJCXX)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Point CMake at platform-specific SFML 2.6 locations when needed.
if(WIN32)
    list(PREPEND CMAKE_PREFIX_PATH "C:/msys64/ucrt64")
elseif(APPLE)
    if(EXISTS "/opt/homebrew/opt/sfml@2")
        list(PREPEND CMAKE_PREFIX_PATH "/opt/homebrew/opt/sfml@2")
    elseif(EXISTS "/usr/local/opt/sfml@2")
        list(PREPEND CMAKE_PREFIX_PATH "/usr/local/opt/sfml@2")
    endif()
endif()

# Find SFML first (needed by TGUI)
find_package(SFML 2.6 COMPONENTS graphics window system network REQUIRED)

if(APPLE)
    find_library(COCOA_FRAMEWORK Cocoa REQUIRED)
endif()

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

# Explicit source list keeps module membership intentional and avoids silent build drift.
set(CORE_SOURCES
    src/Projection/ForwardModel.cpp
    src/Assets/AssetManager.cpp
    src/Board/Board.cpp
    src/Board/BoardGenerator.cpp
    src/Board/CellTraversal.cpp
    src/Buildings/Building.cpp
    src/Buildings/BuildingFactory.cpp
    src/Buildings/StructureChunkRegistry.cpp
    src/Buildings/StructurePlacementProfile.cpp
    src/Config/GameConfig.cpp
    src/Core/Game.cpp
    src/Core/GameClock.cpp
    src/Core/GameEngine.cpp
    src/Core/GameStateValidator.cpp
    src/Core/InteractionPermissions.cpp
    src/Core/MacOSTrackpadAdapter.hpp
    src/Core/TurnDraft.cpp
    src/Debug/GameStateDebugRecorder.cpp
    src/Input/InputHandler.cpp
    src/Input/LayeredSelection.cpp
    src/Kingdom/Kingdom.cpp
    src/Objects/MapObject.hpp
    src/Multiplayer/MultiplayerClient.cpp
    src/Multiplayer/MultiplayerRuntime.cpp
    src/Multiplayer/MultiplayerServer.cpp
    src/Multiplayer/Protocol.cpp
    src/Render/Camera.cpp
    src/Render/OverlayRenderer.cpp
    src/Render/Renderer.cpp
    src/Render/StructureOverlay.cpp
    src/Runtime/BuildOverlayCoordinator.cpp
    src/Runtime/FrontendCoordinator.cpp
    src/Runtime/InGamePresentationCoordinator.cpp
    src/Runtime/InputCoordinator.cpp
    src/Runtime/MultiplayerJoinCoordinator.cpp
    src/Runtime/MultiplayerEventCoordinator.cpp
    src/Runtime/MultiplayerRuntimeCoordinator.cpp
    src/Runtime/PanelActionCoordinator.cpp
    src/Runtime/RenderCoordinator.cpp
    src/Runtime/SelectionQueryCoordinator.cpp
    src/Runtime/SessionFlow.cpp
    src/Runtime/SessionPresentationCoordinator.cpp
    src/Runtime/SessionRuntimeCoordinator.cpp
    src/Runtime/TurnCoordinator.cpp
    src/Runtime/TurnDraftCoordinator.cpp
    src/Runtime/TurnLifecycleCoordinator.cpp
    src/Runtime/UICallbackBinder.cpp
    src/Runtime/UICallbackCoordinator.cpp
    src/Runtime/UpdateCoordinator.cpp
    src/Runtime/WeatherVisibility.cpp
    src/Save/SaveManager.cpp
    src/Systems/BuildReachRules.cpp
    src/Systems/BuildSystem.cpp
    src/Systems/ChestSystem.cpp
    src/Systems/CheckResponseRules.cpp
    src/Systems/CheckSystem.cpp
    src/Systems/CombatSystem.cpp
    src/Systems/EconomySystem.cpp
    src/Systems/EventLog.cpp
    src/Systems/FormationSystem.cpp
    src/Systems/InfernalSystem.cpp
    src/Systems/MarriageSystem.cpp
    src/Systems/PendingTurnProjection.cpp
    src/Systems/ProductionSpawnRules.cpp
    src/Systems/ProductionSystem.cpp
    src/Systems/PublicBuildingOccupation.cpp
    src/Systems/SelectionMoveRules.cpp
    src/Systems/StructureIntegrityRules.cpp
    src/Systems/TurnPointRules.cpp
    src/Systems/TurnSystem.cpp
    src/Systems/WeatherSystem.cpp
    src/Systems/XPSystem.cpp
    src/UI/AutonomousUnitPanel.cpp
    src/UI/BarracksPanel.cpp
    src/UI/BuildingPanel.cpp
    src/UI/BuildToolPanel.cpp
    src/UI/CellPanel.cpp
    src/UI/EventLogPanel.cpp
    src/UI/GameMenuUI.cpp
    src/UI/HUD.cpp
    src/UI/InGameViewModelBuilder.cpp
    src/UI/KingdomBalancePanel.cpp
    src/UI/MainMenuUI.cpp
    src/UI/MapObjectPanel.cpp
    src/UI/PiecePanel.cpp
    src/UI/PlannedActionsPanel.cpp
    src/UI/ToolBar.cpp
    src/UI/UIManager.cpp
    src/Units/MovementRules.cpp
    src/Units/Piece.cpp
    src/Units/PieceFactory.cpp
)

if(APPLE)
    list(APPEND CORE_SOURCES
        src/Core/MacOSTrackpadAdapter.mm
    )
endif()

set(APP_SOURCES
    src/main.cpp
)

# Core library
add_library(${PROJECT_NAME}Core STATIC ${CORE_SOURCES})

target_include_directories(${PROJECT_NAME}Core PUBLIC ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(${PROJECT_NAME}Core PUBLIC
    sfml-graphics
    sfml-network
    sfml-window
    sfml-system
    TGUI::TGUI
)

if(APPLE)
    target_link_libraries(${PROJECT_NAME}Core PUBLIC ${COCOA_FRAMEWORK})
endif()

if(WIN32)
    target_link_libraries(${PROJECT_NAME}Core PUBLIC "C:/msys64/ucrt64/lib/libpthread.dll.a")
endif()

if(MSVC)
    target_compile_options(${PROJECT_NAME}Core PRIVATE /W4 /permissive-)
else()
    target_compile_options(${PROJECT_NAME}Core PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Executable
add_executable(${PROJECT_NAME} ${APP_SOURCES})

target_link_libraries(${PROJECT_NAME} PRIVATE ${PROJECT_NAME}Core)

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4 /permissive-)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Sync assets into the build directory whenever any source asset changes.
file(GLOB_RECURSE ASSET_INPUTS CONFIGURE_DEPENDS
    ${CMAKE_SOURCE_DIR}/assets/*
)

set(ASSET_SYNC_STAMP ${CMAKE_BINARY_DIR}/assets.stamp)

add_custom_command(
    OUTPUT ${ASSET_SYNC_STAMP}
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/assets
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/assets
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets ${CMAKE_BINARY_DIR}/assets
    COMMAND ${CMAKE_COMMAND} -E touch ${ASSET_SYNC_STAMP}
    DEPENDS ${ASSET_INPUTS}
    COMMENT "Syncing assets into build directory"
)

add_custom_target(${PROJECT_NAME}Assets ALL DEPENDS ${ASSET_SYNC_STAMP})
add_dependencies(${PROJECT_NAME} ${PROJECT_NAME}Assets)

# Copy saves directory
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
    $<TARGET_FILE_DIR:${PROJECT_NAME}>/saves
)

if(WIN32)
    # Copy ALL required MSYS2 ucrt64 runtime DLLs (recursively resolved via objdump)
    # This is the complete transitive dependency list -- no more missing DLL errors.
    set(MSYS2_BIN "C:/msys64/ucrt64/bin")
    set(RUNTIME_DLLS
        # SFML core
        "${MSYS2_BIN}/libsfml-graphics-2.dll"
        "${MSYS2_BIN}/libsfml-network-2.dll"
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
endif()
