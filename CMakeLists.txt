cmake_minimum_required(VERSION 3.16)

project(LowPolyWorld LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

find_package(OpenGL REQUIRED)

set(SFML_DIR "${CMAKE_CURRENT_SOURCE_DIR}/librerie/SFML/lib/cmake/SFML")
find_package(SFML 3.0 COMPONENTS Graphics Window System REQUIRED)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/librerie/SFML/bin")
    file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/librerie/SFML/bin/" DESTINATION "${CMAKE_BINARY_DIR}")
endif()

set(tappa "Tappa01")
add_executable(${tappa} 
    Cartella-${tappa}/main.cpp 
    librerie/glad/src/glad.c
)
target_include_directories(${tappa} PRIVATE 
    librerie/glad/include
    librerie/glm
)
target_link_libraries(${tappa} PRIVATE 
    SFML::Graphics 
    SFML::Window 
    SFML::System 
    OpenGL::GL
)

set(tappa "Tappa02")
add_executable(${tappa} 
    Cartella-${tappa}/main.cpp 
    librerie/glad/src/glad.c
)
target_include_directories(${tappa} PRIVATE 
    librerie/glad/include
    librerie/glm
)
target_link_libraries(${tappa} PRIVATE 
    SFML::Graphics 
    SFML::Window 
    SFML::System 
    OpenGL::GL
)

set(tappa "Tappa03")
add_executable(${tappa} 
    Cartella-${tappa}/main.cpp 
    Cartella-${tappa}/dem.cc
    librerie/glad/src/glad.c
)
target_include_directories(${tappa} PRIVATE 
    librerie/glad/include
    librerie/glm
)
target_link_libraries(${tappa} PRIVATE 
    SFML::Graphics 
    SFML::Window 
    SFML::System 
    OpenGL::GL
)

# --- CONFIGURAZIONE DELLA TAPPA 04 ---
set(tappa "Tappa04")
add_executable(${tappa} 
    Cartella-${tappa}/main.cpp 
    Cartella-${tappa}/dem.cc
    librerie/glad/src/glad.c
)
target_include_directories(${tappa} PRIVATE 
    librerie/glad/include
    librerie/glm
)
target_link_libraries(${tappa} PRIVATE 
    SFML::Graphics 
    SFML::Window 
    SFML::System 
    OpenGL::GL
)

set(tappa "Tappa05")
add_executable(${tappa} 
    Cartella-${tappa}/main.cpp 
    Cartella-${tappa}/dem.cc
    librerie/glad/src/glad.c
)
target_include_directories(${tappa} PRIVATE 
    librerie/glad/include
    librerie/glm
)
target_link_libraries(${tappa} PRIVATE 
    SFML::Graphics 
    SFML::Window 
    SFML::System 
    OpenGL::GL
)

set(tappa "Tappa06")
add_executable(${tappa} 
    Cartella-${tappa}/main.cpp 
    Cartella-${tappa}/dem.cc
    librerie/glad/src/glad.c
)
target_include_directories(${tappa} PRIVATE 
    librerie/glad/include
    librerie/glm
)
target_link_libraries(${tappa} PRIVATE 
    SFML::Graphics 
    SFML::Window 
    SFML::System 
    OpenGL::GL
)

# --- CONFIGURAZIONE DELLA TAPPA 07 ---
set(tappa "Tappa07")
add_executable(${tappa} 
    Cartella-${tappa}/main.cpp 
    Cartella-${tappa}/dem.cc
    librerie/glad/src/glad.c
)
target_include_directories(${tappa} PRIVATE 
    librerie/glad/include
    librerie/glm
)
target_link_libraries(${tappa} PRIVATE 
    SFML::Graphics 
    SFML::Window 
    SFML::System 
    OpenGL::GL
)

# --- CONFIGURAZIONE DELLA TAPPA 08 ---
set(tappa "Tappa08")
add_executable(${tappa} 
    Cartella-${tappa}/main.cpp 
    Cartella-${tappa}/dem.cc
    librerie/glad/src/glad.c
)
target_include_directories(${tappa} PRIVATE 
    librerie/glad/include
    librerie/glm
)
target_link_libraries(${tappa} PRIVATE 
    SFML::Graphics 
    SFML::Window 
    SFML::System 
    OpenGL::GL
)

# --- CONFIGURAZIONE DELLA TAPPA 09 ---
set(tappa "Tappa09")
add_executable(${tappa} 
    Cartella-${tappa}/main.cpp 
    Cartella-${tappa}/dem.cc
    librerie/glad/src/glad.c
)
target_include_directories(${tappa} PRIVATE 
    librerie/glad/include
    librerie/glm
)
target_link_libraries(${tappa} PRIVATE 
    SFML::Graphics 
    SFML::Window 
    SFML::System 
    OpenGL::GL
)

# --- CONFIGURAZIONE DELLA TAPPA 10 ---
set(tappa "Tappa10")
add_executable(${tappa} 
    Cartella-${tappa}/main.cpp 
    Cartella-${tappa}/dem.cc
    librerie/glad/src/glad.c
)
target_include_directories(${tappa} PRIVATE 
    librerie/glad/include
    librerie/glm
)
target_link_libraries(${tappa} PRIVATE 
    SFML::Graphics 
    SFML::Window 
    SFML::System 
    OpenGL::GL
)

# --- CONFIGURAZIONE DELLA TAPPA 11 ---
set(tappa "Tappa11")
add_executable(${tappa} 
    Cartella-${tappa}/main.cpp 
    Cartella-${tappa}/dem.cc
    librerie/glad/src/glad.c
)
target_include_directories(${tappa} PRIVATE 
    librerie/glad/include
    librerie/glm
)
target_link_libraries(${tappa} PRIVATE 
    SFML::Graphics 
    SFML::Window 
    SFML::System 
    OpenGL::GL
)

# --- CONFIGURAZIONE DELLA TAPPA 12 ---
set(tappa "Tappa12")
add_executable(${tappa} 
    Cartella-${tappa}/main.cpp 
    Cartella-${tappa}/dem.cc
    librerie/glad/src/glad.c
)
target_include_directories(${tappa} PRIVATE 
    librerie/glad/include
    librerie/glm
)
target_link_libraries(${tappa} PRIVATE 
    SFML::Graphics 
    SFML::Window 
    SFML::System 
    OpenGL::GL
)

# --- CONFIGURAZIONE DELLA TAPPA 13 ---
set(tappa "Tappa13")
add_executable(${tappa} 
    Cartella-${tappa}/main.cpp 
    Cartella-${tappa}/dem.cc
    librerie/glad/src/glad.c
)
target_include_directories(${tappa} PRIVATE 
    librerie/glad/include
    librerie/glm
)
target_link_libraries(${tappa} PRIVATE 
    SFML::Graphics 
    SFML::Window 
    SFML::System 
    OpenGL::GL
)

# --- CONFIGURAZIONE DELLA TAPPA 14 ---
set(tappa "Tappa14")
add_executable(${tappa} 
    Cartella-${tappa}/main.cpp 
    Cartella-${tappa}/dem.cc
    librerie/glad/src/glad.c
)
target_include_directories(${tappa} PRIVATE 
    librerie/glad/include
    librerie/glm
)
target_link_libraries(${tappa} PRIVATE 
    SFML::Graphics 
    SFML::Window 
    SFML::System 
    OpenGL::GL
)

# --- CONFIGURAZIONE DELLA TAPPA 15 ---
set(tappa "Tappa15")
add_executable(${tappa} 
    Cartella-${tappa}/main.cpp 
    Cartella-${tappa}/dem.cc
    librerie/glad/src/glad.c
)
target_include_directories(${tappa} PRIVATE 
    librerie/glad/include
    librerie/glm
)
target_link_libraries(${tappa} PRIVATE 
    SFML::Graphics 
    SFML::Window 
    SFML::System 
    OpenGL::GL
)