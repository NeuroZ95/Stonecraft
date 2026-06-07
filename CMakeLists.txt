cmake_minimum_required(VERSION 3.16)

# Насильно подключаем vcpkg на Windows для автоматического обнаружения библиотек
if(WIN32)
    include("E:/vcpkg-2026.05.25/vcpkg-2026.05.25/scripts/buildsystems/vcpkg.cmake")
endif()

project(Stonecraft LANGUAGES CXX)

# По умолчанию настраиваем проект на сборку в Release
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "No build type specified, defaulting to Release configuration.")
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build." FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Защита от системных макросов min/max на Windows
if(WIN32)
    add_compile_definitions(NOMINMAX)
endif()

# Поиск необходимых библиотек
find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)
find_package(Freetype REQUIRED)

find_package(glfw3 CONFIG)
if(NOT glfw3_FOUND)
    find_package(GLFW3 REQUIRED)
endif()

find_package(glm CONFIG)
if(NOT glm_FOUND)
    find_package(GLM REQUIRED)
endif()

# Исходные файлы проекта (включая заголовки для корректного отслеживания зависимостей сборщиком)
set(SOURCES
    main.cpp
    defs.h
    
    tools/logger/logger.cpp
    tools/logger/logger.h
    tools/texture_generator/generator.cpp
    tools/texture_generator/generator.h
    
    engine/window/window.cpp
    engine/window/window.h
    
    engine/gameplay/game.cpp
    engine/gameplay/game.h
    engine/gameplay/camera.cpp
    engine/gameplay/camera.h
    engine/gameplay/player.cpp
    engine/gameplay/player.h
    engine/gameplay/menu.cpp
    engine/gameplay/menu.h
    engine/gameplay/settings.h
    
    engine/renderer/shader.cpp
    engine/renderer/shader.h
    engine/renderer/texture.cpp
    engine/renderer/texture.h
    engine/renderer/crosshair.cpp
    engine/renderer/crosshair.h
    engine/renderer/hud.cpp
    engine/renderer/hud.h
    engine/renderer/selection.cpp
    engine/renderer/selection.h
    engine/renderer/text_renderer.cpp
    engine/renderer/text_renderer.h
    
    engine/world/chunk.cpp
    engine/world/chunk.h
    engine/world/perlin.h
    engine/world/world.cpp
    engine/world/world.h
    engine/world/world_utils.cpp
    engine/world/world_utils.h
    engine/world/save_manager.cpp
    engine/world/save_manager.h
    
    # Сетевой код
    engine/network/enet_impl.cpp
    engine/network/client.cpp
    engine/network/client.h
    engine/network/network_manager.cpp
    engine/network/network_manager.h
    engine/network/protocol.h
    engine/network/server.cpp
    engine/network/server.h
    engine/network/lan_discovery.cpp
    engine/network/lan_discovery.h
)

add_executable(Stonecraft ${SOURCES})

# Флаги оптимизации для Release конфигурации
if(MSVC)
    # Оптимизации MSVC для максимального быстродействия
    target_compile_options(Stonecraft PRIVATE $<$<CONFIG:Release>:/O2 /Oi /Gy /GL>)
    target_link_options(Stonecraft PRIVATE $<$<CONFIG:Release>:/LTCG>)
else()
    # Оптимизации GCC / Clang для максимального быстродействия
    target_compile_options(Stonecraft PRIVATE $<$<CONFIG:Release>:-O3 -march=native -ffast-math>)
endif()

target_include_directories(Stonecraft PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/libs
)

# Системные библиотеки для работы ENet (вещание, сокеты, таймеры)
if(WIN32)
    target_link_libraries(Stonecraft PRIVATE ws2_32 winmm)
elseif(UNIX AND NOT APPLE)
    target_link_libraries(Stonecraft PRIVATE pthread)
endif()

target_link_libraries(Stonecraft PRIVATE
    OpenGL::GL
    GLEW::GLEW
    glfw
    glm::glm
    Freetype::Freetype
)

# Автоматическое копирование ресурсов при каждой сборке проекта
add_custom_target(copy_resources ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/shaders ${CMAKE_CURRENT_BINARY_DIR}/shaders
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/textures ${CMAKE_CURRENT_BINARY_DIR}/textures
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/fonts ${CMAKE_CURRENT_BINARY_DIR}/fonts
    COMMENT "Copying shaders, textures and fonts to binary directory"
)
add_dependencies(Stonecraft copy_resources)