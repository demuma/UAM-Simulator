cmake_minimum_required(VERSION 3.10)

project(UAM-Simulator)

# Determine the Homebrew prefix based on the system architecture (for M4 Mac and Homebrew)
if(CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64")
    set(HOMEBREW_PREFIX "/opt/homebrew/opt")
else()
    set(HOMEBREW_PREFIX "/usr/local/Cellar")
endif()

# Set the SFML_DIR to the path containing SFMLConfig.cmake
set(SFML_DIR "${HOMEBREW_PREFIX}/sfml@3/lib/cmake/SFML")
<<<<<<< HEAD
set(GLEW_DIR "${HOMEBREW_PREFIX}/glew/lib/cmake/GL")
=======
>>>>>>> 5386a9f (- updated simulator)

find_package(SFML COMPONENTS Graphics Window System REQUIRED)
# find_package(SFML 2.5 COMPONENTS graphics window system REQUIRED)
find_package(yaml-cpp REQUIRED)
find_package(glm REQUIRED)
find_package(GLEW REQUIRED)
find_package(OpenGL REQUIRED)

include_directories(include)

add_executable(UAM-Simulator src/main.cpp)


# Suppress OpenGL deprecation warnings on macOS
target_compile_definitions(UAM-Simulator PRIVATE GL_SILENCE_DEPRECATION)

# Enable experimental GLM extensions
target_compile_definitions(UAM-Simulator PRIVATE GLM_ENABLE_EXPERIMENTAL)

target_include_directories(UAM-Simulator PUBLIC
    ${SFML_INCLUDE_DIR}
    ${yaml-cpp_INCLUDE_DIRS}
    ${GLM_INCLUDE_DIRS}
    ${OPENGL_INCLUDE_DIR}
)

# target_link_libraries(UAM-Simulator
#     sfml-graphics
#     sfml-window
#     sfml-system
#     yaml-cpp::yaml-cpp
#     glm::glm
#     ${OPENGL_LIBRARIES}
# )

target_link_libraries(UAM-Simulator 
    SFML::Graphics 
    SFML::Window 
    SFML::System 
    yaml-cpp::yaml-cpp 
    glm::glm
    GLEW::GLEW
    ${OPENGL_LIBRARIES}
) # for macOS SFML 3.0.0

target_compile_features(UAM-Simulator PRIVATE cxx_std_17)
target_compile_options(UAM-Simulator PRIVATE -O3)
target_compile_definitions(UAM-Simulator PRIVATE DEBUG)
