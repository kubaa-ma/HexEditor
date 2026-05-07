cmake_minimum_required(VERSION 3.10)

project(HexEditor)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(${CMAKE_PROJECT_NAME}
  ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp

  ${CMAKE_CURRENT_SOURCE_DIR}/third-party/imgui-1.92.7/imgui.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/third-party/imgui-1.92.7/imgui_demo.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/third-party/imgui-1.92.7/imgui_draw.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/third-party/imgui-1.92.7/imgui_tables.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/third-party/imgui-1.92.7/imgui_widgets.cpp

  ${CMAKE_CURRENT_SOURCE_DIR}/third-party/imgui-1.92.7/backends/imgui_impl_opengl3.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/third-party/imgui-1.92.7/backends/imgui_impl_sdl3.cpp

  ${CMAKE_CURRENT_SOURCE_DIR}/src/app.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/file.cpp

)

target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
  IMGUI_IMPL_OPENGL_LOADER_GLAD
)
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/third-party/SDL3/lib/x64
)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/third-party/imgui-1.92.7
  ${CMAKE_CURRENT_SOURCE_DIR}/third-party/imgui-1.92.7/backends
  ${CMAKE_CURRENT_SOURCE_DIR}/third-party/SDL3/include
  ${CMAKE_CURRENT_SOURCE_DIR}/include/
)

target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE
  SDL3
  opengl32
)