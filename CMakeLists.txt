cmake_minimum_required(VERSION 3.21)

project(Aetherion
    VERSION 0.1.0
    DESCRIPTION "Aetherion 2D engine scaffolding (architecture only)"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

include(CMake/ProjectOptions.cmake OPTIONAL)

# Platform-specific Qt configuration
if(WIN32)
    # Windows: Add Qt installation paths
    if(NOT Qt6_DIR)
        # Check common Qt installation paths
        if(EXISTS "C:/Qt/6.9.1/mingw_64/lib/cmake/Qt6")
            set(Qt6_DIR "C:/Qt/6.9.1/mingw_64/lib/cmake/Qt6")
        elseif(EXISTS "C:/Qt/6.9.1/msvc2022_64/lib/cmake/Qt6")
            set(Qt6_DIR "C:/Qt/6.9.1/msvc2022_64/lib/cmake/Qt6")
        elseif(EXISTS "C:/Qt/6.9/mingw_64/lib/cmake/Qt6")
            set(Qt6_DIR "C:/Qt/6.9/mingw_64/lib/cmake/Qt6")
        elseif(EXISTS "C:/Qt/6.9/msvc2022_64/lib/cmake/Qt6")
            set(Qt6_DIR "C:/Qt/6.9/msvc2022_64/lib/cmake/Qt6")
        endif()
    endif()
elseif(APPLE)
    # macOS: Qt is typically found via Homebrew or standard paths
    if(NOT Qt6_DIR)
        # Homebrew installation path
        if(EXISTS "/opt/homebrew/opt/qt/lib/cmake/Qt6")
            set(Qt6_DIR "/opt/homebrew/opt/qt/lib/cmake/Qt6")
        elseif(EXISTS "/usr/local/opt/qt/lib/cmake/Qt6")
            set(Qt6_DIR "/usr/local/opt/qt/lib/cmake/Qt6")
        endif()
    endif()
endif()

find_package(Qt6 6.2 COMPONENTS Widgets REQUIRED)
find_package(Vulkan REQUIRED)

set(RUNTIME_SOURCES
    Engine/Core/src/Core.cpp
    Engine/Runtime/src/EngineApplication.cpp
    Engine/Runtime/src/EngineContext.cpp
    Engine/Scene/src/Scene.cpp
    Engine/Scene/src/Entity.cpp
    Engine/Scene/src/Component.cpp
    Engine/Scene/src/System.cpp
    Engine/Assets/src/AssetRegistry.cpp
    Engine/Platform/src/PlatformAbstraction.cpp
    Engine/Rendering/src/RenderingPlaceholder.cpp
    Engine/Rendering/src/VulkanContext.cpp
    Engine/Physics/src/PhysicsPlaceholder.cpp
    Engine/Audio/src/AudioPlaceholder.cpp
    Engine/Scripting/src/ScriptingPlaceholder.cpp
)

add_library(AetherionRuntime STATIC ${RUNTIME_SOURCES})
target_include_directories(AetherionRuntime
    PUBLIC
        Engine/Core/include
        Engine/Runtime/include
        Engine/Scene/include
        Engine/Assets/include
        Engine/Platform/include
        Engine/Rendering/include
        Engine/Physics/include
        Engine/Audio/include
        Engine/Scripting/include
)
target_compile_features(AetherionRuntime PRIVATE cxx_std_20)
    target_link_libraries(AetherionRuntime
        PUBLIC
        Vulkan::Vulkan
    )

set(EDITOR_SOURCES
    Engine/Editor/src/main.cpp
    Engine/Editor/src/EditorApplication.cpp
    Engine/Editor/src/EditorMainWindow.cpp
    Engine/Editor/src/EditorViewport.cpp
    Engine/Editor/src/EditorHierarchyPanel.cpp
    Engine/Editor/src/EditorInspectorPanel.cpp
    Engine/Editor/src/EditorAssetBrowser.cpp
    Engine/Editor/src/EditorConsole.cpp
)

add_executable(AetherionEditor ${EDITOR_SOURCES})
target_sources(AetherionEditor
    PRIVATE
        Engine/Editor/include/Aetherion/Editor/EditorApplication.h
        Engine/Editor/include/Aetherion/Editor/EditorMainWindow.h
        Engine/Editor/include/Aetherion/Editor/EditorViewport.h
        Engine/Editor/include/Aetherion/Editor/EditorHierarchyPanel.h
        Engine/Editor/include/Aetherion/Editor/EditorInspectorPanel.h
        Engine/Editor/include/Aetherion/Editor/EditorAssetBrowser.h
        Engine/Editor/include/Aetherion/Editor/EditorConsole.h
)
target_include_directories(AetherionEditor
    PRIVATE
        Engine/Editor/include
        Engine/Core/include
        Engine/Runtime/include
        Engine/Scene/include
        Engine/Assets/include
)

target_link_libraries(AetherionEditor
    PRIVATE
        AetherionRuntime
        Qt6::Widgets
    Vulkan::Vulkan
)

target_compile_features(AetherionEditor PRIVATE cxx_std_20)
