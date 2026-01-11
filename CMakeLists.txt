cmake_minimum_required(VERSION 3.21)

project(Aetherion
    VERSION 0.0.1
    DESCRIPTION "Aetherion 3D engine scaffolding (architecture only)"
    LANGUAGES C CXX
)

add_compile_definitions(AETHERION_APP_VERSION="${PROJECT_VERSION}")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

include(CMake/ProjectOptions.cmake OPTIONAL)

option(AETHERION_ENABLE_PYTHON "Enable embedded Python scripting runtime" OFF)
option(AETHERION_ENABLE_PYBIND11 "Enable pybind11 bindings for Python scripting" OFF)
option(AETHERION_ENABLE_LUA "Enable Lua scripting runtime (recommended)" ON)

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

# Vulkan SDK path hints
if(WIN32)
    if(NOT Vulkan_DIR AND NOT VULKAN_SDK)
        if(EXISTS "C:/VulkanSDK")
            file(GLOB VULKAN_VERSIONS RELATIVE "C:/VulkanSDK" "C:/VulkanSDK/*")
            list(SORT VULKAN_VERSIONS)
            list(REVERSE VULKAN_VERSIONS)
            list(GET VULKAN_VERSIONS 0 LATEST_VULKAN)
            set(VULKAN_SDK "C:/VulkanSDK/${LATEST_VULKAN}")
        endif()
    endif()
elseif(APPLE)
    if(NOT VULKAN_SDK)
        # Vulkan from Homebrew on macOS
        execute_process(COMMAND brew --prefix vulkan OUTPUT_VARIABLE BREW_VULKAN OUTPUT_STRIP_TRAILING_WHITESPACE)
        if(BREW_VULKAN)
            set(VULKAN_SDK "${BREW_VULKAN}")
        endif()
    endif()
endif()

find_package(Qt6 6.2 COMPONENTS Widgets Concurrent Network REQUIRED)
find_package(Vulkan REQUIRED)
if(AETHERION_ENABLE_PYTHON)
    find_package(Python3 COMPONENTS Interpreter Development REQUIRED)
endif()

if(AETHERION_ENABLE_PYBIND11 AND NOT AETHERION_ENABLE_PYTHON)
    message(WARNING "AETHERION_ENABLE_PYBIND11 requires AETHERION_ENABLE_PYTHON. Disabling pybind11 bindings.")
    set(AETHERION_ENABLE_PYBIND11 OFF)
endif()

if(AETHERION_ENABLE_PYBIND11)
    include(FetchContent)
    FetchContent_Declare(
        pybind11
        GIT_REPOSITORY https://github.com/pybind/pybind11.git
        GIT_TAG v2.13.6
    )
    FetchContent_MakeAvailable(pybind11)
endif()

# Lua scripting with sol2 (header-only C++ bindings)
if(AETHERION_ENABLE_LUA)
    include(FetchContent)
    
    # sol2 - Modern C++ bindings for Lua
    FetchContent_Declare(
        sol2
        GIT_REPOSITORY https://github.com/ThePhD/sol2.git
        GIT_TAG v3.3.0
    )
    FetchContent_MakeAvailable(sol2)
    
    # Lua 5.4 (bundled with sol2 or system)
    find_package(Lua QUIET)
    if(NOT Lua_FOUND)
        # Use bundled lua if system lua not found
        FetchContent_Declare(
            lua
            GIT_REPOSITORY https://github.com/lua/lua.git
            GIT_TAG v5.4.6
        )
        FetchContent_MakeAvailable(lua)

        # The upstream Lua repo doesn't ship CMake targets.
        # Provide a minimal static library + include path so sol2 can compile.
        if(NOT TARGET lua_static)
            set(LUA_BUNDLED_SOURCES
                ${lua_SOURCE_DIR}/lapi.c
                ${lua_SOURCE_DIR}/lauxlib.c
                ${lua_SOURCE_DIR}/lbaselib.c
                ${lua_SOURCE_DIR}/lcode.c
                ${lua_SOURCE_DIR}/lcorolib.c
                ${lua_SOURCE_DIR}/lctype.c
                ${lua_SOURCE_DIR}/ldblib.c
                ${lua_SOURCE_DIR}/ldebug.c
                ${lua_SOURCE_DIR}/ldo.c
                ${lua_SOURCE_DIR}/ldump.c
                ${lua_SOURCE_DIR}/lfunc.c
                ${lua_SOURCE_DIR}/lgc.c
                ${lua_SOURCE_DIR}/linit.c
                ${lua_SOURCE_DIR}/liolib.c
                ${lua_SOURCE_DIR}/llex.c
                ${lua_SOURCE_DIR}/lmathlib.c
                ${lua_SOURCE_DIR}/lmem.c
                ${lua_SOURCE_DIR}/loadlib.c
                ${lua_SOURCE_DIR}/lobject.c
                ${lua_SOURCE_DIR}/lopcodes.c
                ${lua_SOURCE_DIR}/loslib.c
                ${lua_SOURCE_DIR}/lparser.c
                ${lua_SOURCE_DIR}/lstate.c
                ${lua_SOURCE_DIR}/lstring.c
                ${lua_SOURCE_DIR}/lstrlib.c
                ${lua_SOURCE_DIR}/ltable.c
                ${lua_SOURCE_DIR}/ltablib.c
                ${lua_SOURCE_DIR}/ltm.c
                ${lua_SOURCE_DIR}/lundump.c
                ${lua_SOURCE_DIR}/lutf8lib.c
                ${lua_SOURCE_DIR}/lvm.c
                ${lua_SOURCE_DIR}/lzio.c
            )

            add_library(lua_static STATIC ${LUA_BUNDLED_SOURCES})
            target_include_directories(lua_static PUBLIC ${lua_SOURCE_DIR})

            if(WIN32)
                target_compile_definitions(lua_static PUBLIC LUA_USE_WINDOWS)
            endif()
        endif()

        set(LUA_INCLUDE_DIR ${lua_SOURCE_DIR})
        set(LUA_LIBRARIES lua_static)
        set(Lua_FOUND TRUE)
    endif()
endif()

# Jolt Physics
set(PHYSICS_REPO_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/Engine/ThirdParty/JoltPhysics)
set(ENABLE_ALL_WARNINGS OFF CACHE BOOL "" FORCE)
set(ENABLE_OBJECT_STREAM OFF CACHE BOOL "" FORCE)
set(USE_STATIC_MSVC_RUNTIME OFF CACHE BOOL "" FORCE)
add_subdirectory(${PHYSICS_REPO_ROOT}/Build ${CMAKE_BINARY_DIR}/JoltPhysics)

set(RUNTIME_SOURCES
    Engine/Core/src/Core.cpp
    Engine/Core/src/UUID.cpp
    Engine/Runtime/src/EngineApplication.cpp
    Engine/Runtime/src/EngineContext.cpp
    Engine/Scene/src/Scene.cpp
    Engine/Scene/src/Entity.cpp
    Engine/Scene/src/Component.cpp
    Engine/Scene/src/TransformComponent.cpp
    Engine/Scene/src/MeshRendererComponent.cpp
    Engine/Scene/src/LightComponent.cpp
    Engine/Scene/src/CameraComponent.cpp
    Engine/Scene/src/System.cpp
    Engine/Scene/src/SceneSerializer.cpp
    Engine/Scene/src/AnimatorComponent.cpp
    Engine/Scene/src/AnimationSystem.cpp
    Engine/Assets/src/AssetRegistry.cpp
    Engine/Assets/src/AssetGenerator.cpp
    Engine/Assets/src/Animation.cpp
    Engine/Assets/src/AnimationLoader.cpp
    Engine/Assets/src/LLMClient.cpp
    Engine/Assets/src/LLMAssetGenerator.cpp
    Engine/Platform/src/PlatformAbstraction.cpp
    Engine/Rendering/src/RenderingPlaceholder.cpp
    Engine/Rendering/src/VulkanContext.cpp
    Engine/Rendering/src/VulkanViewport.cpp
    Engine/Physics/src/PhysicsWorld.cpp
    Engine/Physics/src/PhysicsSystem.cpp
    Engine/Physics/src/PhysicsContactListener.cpp
    Engine/Scene/src/RigidbodyComponent.cpp
    Engine/Scene/src/ColliderComponent.cpp
    Engine/Scene/src/AudioSourceComponent.cpp
    Engine/Scene/src/AudioListenerComponent.cpp
    Engine/Scene/src/AIBehaviorComponent.cpp
    Engine/Scene/src/AIBehaviorSystem.cpp
    Engine/Scene/src/ParticleEmitterComponent.cpp
    Engine/Scene/src/SemanticComponent.cpp
    Engine/Scene/src/SemanticGraph.cpp
    Engine/AI/src/LocalVisionModel.cpp
    Engine/Assets/src/LatentAssetLoader.cpp
    Engine/Audio/src/AudioEngine.cpp
    Engine/Audio/src/AudioSystem.cpp
    Engine/Scripting/src/ScriptingSystem.cpp
    Engine/Scripting/src/NullScriptEngine.cpp
    Engine/Scene/src/ScriptComponent.cpp
    Engine/Scripting/src/ScriptingPlaceholder.cpp
    Engine/Scripting/src/PythonBindings.cpp
    Engine/Scripting/src/LogicCopilot.cpp
    Engine/Scripting/src/ModuleLoader.cpp
    Engine/Scripting/src/LuaScriptEngine.cpp
    Engine/Scripting/src/LuaScriptInstance.cpp
    Engine/Scripting/src/LuaBindings.cpp
    Engine/Runtime/src/SystemScheduler.cpp
    Engine/Runtime/src/OnDeviceInference.cpp
)

# Compile Vulkan shaders to SPIR-V (output: <build>/shaders/*.spv)
set(AETHERION_SHADER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/Engine/Rendering/shaders)
set(AETHERION_SHADER_OUT_DIR ${CMAKE_BINARY_DIR}/shaders)

file(MAKE_DIRECTORY ${AETHERION_SHADER_OUT_DIR})

set(AETHERION_SHADER_SOURCES
    ${AETHERION_SHADER_DIR}/viewport_triangle.vert
    ${AETHERION_SHADER_DIR}/viewport_triangle.frag
    ${AETHERION_SHADER_DIR}/viewport_picking.frag
    ${AETHERION_SHADER_DIR}/viewport_picking_uint.frag
    ${AETHERION_SHADER_DIR}/viewport_postprocess.vert
    ${AETHERION_SHADER_DIR}/viewport_postprocess.frag
    ${AETHERION_SHADER_DIR}/viewport_postprocess_uint.frag
    ${AETHERION_SHADER_DIR}/viewport_shadow.vert
    ${AETHERION_SHADER_DIR}/viewport_shadow.frag
    ${AETHERION_SHADER_DIR}/viewport_cull.comp
    ${AETHERION_SHADER_DIR}/viewport_particle.vert
    ${AETHERION_SHADER_DIR}/viewport_particle.frag
)

set(AETHERION_SHADER_SPV
    ${AETHERION_SHADER_OUT_DIR}/viewport_triangle.vert.spv
    ${AETHERION_SHADER_OUT_DIR}/viewport_triangle.frag.spv
    ${AETHERION_SHADER_OUT_DIR}/viewport_picking.frag.spv
    ${AETHERION_SHADER_OUT_DIR}/viewport_picking_uint.frag.spv
    ${AETHERION_SHADER_OUT_DIR}/viewport_postprocess.vert.spv
    ${AETHERION_SHADER_OUT_DIR}/viewport_postprocess.frag.spv
    ${AETHERION_SHADER_OUT_DIR}/viewport_postprocess_uint.frag.spv
    ${AETHERION_SHADER_OUT_DIR}/viewport_shadow.vert.spv
    ${AETHERION_SHADER_OUT_DIR}/viewport_shadow.frag.spv
    ${AETHERION_SHADER_OUT_DIR}/viewport_cull.comp.spv
    ${AETHERION_SHADER_OUT_DIR}/viewport_particle.vert.spv
    ${AETHERION_SHADER_OUT_DIR}/viewport_particle.frag.spv
)

add_custom_command(
    OUTPUT ${AETHERION_SHADER_SPV}
    COMMAND ${Vulkan_GLSLANG_VALIDATOR_EXECUTABLE} -V ${AETHERION_SHADER_DIR}/viewport_triangle.vert -o ${AETHERION_SHADER_OUT_DIR}/viewport_triangle.vert.spv  
    COMMAND ${Vulkan_GLSLANG_VALIDATOR_EXECUTABLE} -V ${AETHERION_SHADER_DIR}/viewport_triangle.frag -o ${AETHERION_SHADER_OUT_DIR}/viewport_triangle.frag.spv  
    COMMAND ${Vulkan_GLSLANG_VALIDATOR_EXECUTABLE} -V ${AETHERION_SHADER_DIR}/viewport_picking.frag -o ${AETHERION_SHADER_OUT_DIR}/viewport_picking.frag.spv  
    COMMAND ${Vulkan_GLSLANG_VALIDATOR_EXECUTABLE} -V ${AETHERION_SHADER_DIR}/viewport_picking_uint.frag -o ${AETHERION_SHADER_OUT_DIR}/viewport_picking_uint.frag.spv  
    COMMAND ${Vulkan_GLSLANG_VALIDATOR_EXECUTABLE} -V ${AETHERION_SHADER_DIR}/viewport_postprocess.vert -o ${AETHERION_SHADER_OUT_DIR}/viewport_postprocess.vert.spv  
    COMMAND ${Vulkan_GLSLANG_VALIDATOR_EXECUTABLE} -V ${AETHERION_SHADER_DIR}/viewport_postprocess.frag -o ${AETHERION_SHADER_OUT_DIR}/viewport_postprocess.frag.spv  
    COMMAND ${Vulkan_GLSLANG_VALIDATOR_EXECUTABLE} -V ${AETHERION_SHADER_DIR}/viewport_postprocess_uint.frag -o ${AETHERION_SHADER_OUT_DIR}/viewport_postprocess_uint.frag.spv
    COMMAND ${Vulkan_GLSLANG_VALIDATOR_EXECUTABLE} -V ${AETHERION_SHADER_DIR}/viewport_shadow.vert -o ${AETHERION_SHADER_OUT_DIR}/viewport_shadow.vert.spv
    COMMAND ${Vulkan_GLSLANG_VALIDATOR_EXECUTABLE} -V ${AETHERION_SHADER_DIR}/viewport_shadow.frag -o ${AETHERION_SHADER_OUT_DIR}/viewport_shadow.frag.spv
    COMMAND ${Vulkan_GLSLANG_VALIDATOR_EXECUTABLE} -V ${AETHERION_SHADER_DIR}/viewport_cull.comp -o ${AETHERION_SHADER_OUT_DIR}/viewport_cull.comp.spv
    COMMAND ${Vulkan_GLSLANG_VALIDATOR_EXECUTABLE} -V ${AETHERION_SHADER_DIR}/viewport_particle.vert -o ${AETHERION_SHADER_OUT_DIR}/viewport_particle.vert.spv
    COMMAND ${Vulkan_GLSLANG_VALIDATOR_EXECUTABLE} -V ${AETHERION_SHADER_DIR}/viewport_particle.frag -o ${AETHERION_SHADER_OUT_DIR}/viewport_particle.frag.spv
    DEPENDS ${AETHERION_SHADER_SOURCES}
    VERBATIM
)

add_custom_target(AetherionShaders ALL DEPENDS ${AETHERION_SHADER_SPV})

add_library(AetherionRuntime STATIC ${RUNTIME_SOURCES})
target_include_directories(AetherionRuntime
    PUBLIC
        Engine/Core/include
        Engine/Runtime/include
        Engine/Scene/include
        Engine/Assets/include
        Engine/AI/include
        Engine/Platform/include
        Engine/Rendering/include
        Engine/Physics/include
        Engine/Audio/include
        Engine/Scripting/include
        Engine/ThirdParty
        Engine/ThirdParty/miniaudio
)
target_compile_features(AetherionRuntime PRIVATE cxx_std_20)
target_link_libraries(AetherionRuntime
    PUBLIC
        Vulkan::Vulkan
        Jolt
        Qt6::Network
        $<$<BOOL:${AETHERION_ENABLE_PYTHON}>:Python3::Python>
        $<$<BOOL:${AETHERION_ENABLE_PYBIND11}>:pybind11::embed>
        $<$<BOOL:${AETHERION_ENABLE_LUA}>:sol2::sol2>
        $<$<AND:$<BOOL:${AETHERION_ENABLE_LUA}>,$<BOOL:${Lua_FOUND}>>:${LUA_LIBRARIES}>
        $<$<NOT:$<PLATFORM_ID:Windows>>:${CMAKE_DL_LIBS}>
)
if(AETHERION_ENABLE_LUA AND Lua_FOUND)
    target_include_directories(AetherionRuntime PUBLIC ${LUA_INCLUDE_DIR})
endif()
target_compile_definitions(AetherionRuntime
    PUBLIC
        $<$<BOOL:${AETHERION_ENABLE_PYTHON}>:AETHERION_ENABLE_PYTHON=1>
        $<$<BOOL:${AETHERION_ENABLE_PYBIND11}>:AETHERION_ENABLE_PYBIND11=1>
        $<$<BOOL:${AETHERION_ENABLE_LUA}>:AETHERION_ENABLE_LUA=1>
)

set(EDITOR_SOURCES
    Engine/Editor/src/main.cpp
    Engine/Editor/src/EditorApplication.cpp
    Engine/Editor/src/EditorMainWindow.cpp
    Engine/Editor/src/EditorViewport.cpp
    Engine/Editor/src/EditorHierarchyPanel.cpp
    Engine/Editor/src/EditorInspectorPanel.cpp
    Engine/Editor/src/EditorSelection.cpp
    Engine/Editor/src/EditorAssetBrowser.cpp
    Engine/Editor/src/EditorConsole.cpp
    Engine/Editor/src/EditorCameraPreview.cpp
    Engine/Editor/src/EditorMeshPreview.cpp
    Engine/Editor/src/EditorCommandPalette.cpp
    Engine/Editor/src/AICopilotPanel.cpp
    Engine/Editor/src/AICopilotProcessor.cpp
    Engine/Editor/src/AICopilotAgent.cpp
    Engine/Editor/src/AICopilotTools.cpp
    Engine/Editor/src/EditorAssetGenerationPanel.cpp
    Engine/Editor/src/EditorStatisticsPanel.cpp
    Engine/Editor/src/EditorAnimationPanel.cpp
    Engine/Editor/src/EditorLogicCopilotPanel.cpp
    Engine/Editor/src/EditorSettings.cpp
    Engine/Editor/src/EditorSettingsDialog.cpp
    Engine/Editor/src/TabPanelManager.cpp
    Engine/Editor/resources/AetherionEditor.qrc
)

add_executable(AetherionEditor ${EDITOR_SOURCES})
target_sources(AetherionEditor
    PRIVATE
        Engine/Editor/include/Aetherion/Editor/EditorApplication.h
        Engine/Editor/include/Aetherion/Editor/EditorMainWindow.h
        Engine/Editor/include/Aetherion/Editor/EditorViewport.h
        Engine/Editor/include/Aetherion/Editor/EditorHierarchyPanel.h
        Engine/Editor/include/Aetherion/Editor/EditorInspectorPanel.h
        Engine/Editor/include/Aetherion/Editor/EditorSelection.h
        Engine/Editor/include/Aetherion/Editor/EditorAssetBrowser.h
        Engine/Editor/include/Aetherion/Editor/EditorConsole.h
        Engine/Editor/include/Aetherion/Editor/EditorCameraPreview.h
        Engine/Editor/include/Aetherion/Editor/EditorMeshPreview.h
        Engine/Editor/include/Aetherion/Editor/EditorCommandPalette.h
        Engine/Editor/include/Aetherion/Editor/AICopilotPanel.h
        Engine/Editor/include/Aetherion/Editor/AICopilotProcessor.h
        Engine/Editor/include/Aetherion/Editor/EditorAssetGenerationPanel.h
        Engine/Editor/include/Aetherion/Editor/EditorStatisticsPanel.h
        Engine/Editor/include/Aetherion/Editor/EditorAnimationPanel.h
        Engine/Editor/include/Aetherion/Editor/EditorLogicCopilotPanel.h
        Engine/Editor/include/Aetherion/Editor/EditorSettings.h
        Engine/Editor/include/Aetherion/Editor/EditorSettingsDialog.h
        Engine/Editor/include/Aetherion/Editor/TabPanelManager.h
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
        Qt6::Concurrent
        Qt6::Network
        Vulkan::Vulkan
)

add_dependencies(AetherionEditor AetherionShaders)

target_compile_features(AetherionEditor PRIVATE cxx_std_20)

if(MSVC)
    target_compile_options(AetherionEditor PRIVATE /Zc:__cplusplus)
    target_compile_options(AetherionRuntime PRIVATE /Zc:__cplusplus)
endif()

# Lightweight self-test target (opt-in)
add_executable(AetherionSelfTests
    tests/SelfTest.cpp
)
target_include_directories(AetherionSelfTests
    PRIVATE
        Engine/Assets/include
        Engine/Scripting/include
        Engine/Core/include
        Engine/ThirdParty
)
target_link_libraries(AetherionSelfTests
    PRIVATE
        AetherionRuntime
)

add_executable(AetherionCopilotTest
    tests/CopilotIntentTest.cpp
    Engine/Editor/src/AICopilotProcessor.cpp
    Engine/Editor/src/AICopilotAgent.cpp
    Engine/Editor/src/AICopilotTools.cpp
)
target_include_directories(AetherionCopilotTest
    PRIVATE
        Engine/Editor/include
        Engine/Scene/include
        Engine/Core/include
        Engine/Runtime/include
        Engine/Assets/include
        Engine/ThirdParty
)
target_link_libraries(AetherionCopilotTest
    PRIVATE
        AetherionRuntime
        Qt6::Widgets
)
