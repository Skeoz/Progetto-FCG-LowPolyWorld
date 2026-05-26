cmake_minimum_required(VERSION 3.16)

set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "Bypassa blocco vecchie policy" FORCE)

set(CMAKE_WARN_DEPRECATED OFF CACHE BOOL "Disabilita avvisi di deprecazione" FORCE)

project(LowPolyWorld LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# =========================================================================
# 1. LIBRERIE STANDARD C++
# =========================================================================
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++")

# =========================================================================
# 2. COMPILAZIONE STATICA (SFML Dentro eseguibili)
# =========================================================================
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build static libraries" FORCE)

find_package(OpenGL REQUIRED)
include(FetchContent)

# =========================================================================
# 3. DOWNLOAD AUTOMATICO DELLE DIPENDENZE (FETCHCONTENT)
# =========================================================================
message(STATUS "Scaricamento di GLM in corso...")
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.3
    GIT_SHALLOW ON
)
FetchContent_MakeAvailable(glm)

message(STATUS "Scaricamento di SFML in corso...")
FetchContent_Declare(
    SFML
    GIT_REPOSITORY https://github.com/SFML/SFML.git
    GIT_TAG 3.0.0
    GIT_SHALLOW ON
)
FetchContent_MakeAvailable(SFML)

# =========================================================================
# 4. GENERAZIONE AUTOMATICA DELLE 15 TAPPE
# =========================================================================
foreach(i RANGE 1 15)
    # Formattazione nome a due cifre (Tappa01, Tappa02...)
    if(i LESS 10)
        set(tappa "Tappa0${i}")
    else()
        set(tappa "Tappa${i}")
    endif()

    # Controlliamo che la cartella della tappa esista davvero nel progetto
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Cartella-${tappa}")
        
        # File base sempre presenti
        set(SOURCES 
            Cartella-${tappa}/main.cpp 
            librerie/glad/src/glad.c
        )

        # Dalla Tappa 03 in poi, il ghiacciaio entra in gioco
        if(i GREATER_EQUAL 3)
            list(APPEND SOURCES Cartella-${tappa}/dem.cc)
        endif()

        add_executable(${tappa} ${SOURCES})

        # Includiamo GLAD locale e GLM scaricato
        target_include_directories(${tappa} PRIVATE 
            librerie/glad/include
            ${glm_SOURCE_DIR}
        )

        # Linkiamo SFML (ora statico) e OpenGL
        target_link_libraries(${tappa} PRIVATE 
            SFML::Graphics 
            SFML::Window 
            SFML::System 
            OpenGL::GL
        )
    endif()
endforeach()

add_custom_command(
    TARGET Tappa15 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/Cartella-risorse"
    "$<TARGET_FILE_DIR:Tappa15>/Cartella-risorse"
)