#include <glad/glad.h>
#include <SFML/Window.hpp>
#include <iostream>
#include <optional>

// --- 1. GLI SHADER ---
// Vertex Shader: Prende le coordinate 3D e le passa alla GPU
const char* vertexShaderSource = R"(
    #version 410 core
    layout (location = 0) in vec3 aPos;
    void main() {
        gl_Position = vec4(aPos.x, aPos.y, aPos.z, 1.0);
    }
)";

// Fragment Shader: Colora i pixel (qui usiamo un bell'arancione acceso)
const char* fragmentShaderSource = R"(
    #version 410 core
    out vec4 FragColor;
    void main() {
        FragColor = vec4(1.0f, 0.5f, 0.2f, 1.0f);
    }
)";

int main() {
    // Configurazione SFML e OpenGL 4.1 Core
    sf::ContextSettings settings;
    settings.depthBits = 24;
    settings.majorVersion = 4;
    settings.minorVersion = 1;
    settings.attributeFlags = sf::ContextSettings::Core;

    sf::Window window(sf::VideoMode({800, 600}), "Progetto FCG - LowPolyWorld", sf::State::Windowed, settings);

    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(sf::Context::getFunction))) {
        std::cout << "Errore GLAD!" << std::endl;
        return -1;
    }

    glViewport(0, 0, 800, 600);

    // --- 2. COMPILAZIONE DEGLI SHADER ---
    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);

    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);

    unsigned int shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    // Pulizia: gli shader compilati sono nel programma, possiamo scartare i file originali
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    // --- 3. I DATI DEL TRIANGOLO ---
    // Le coordinate (X, Y, Z) dei tre vertici
    float vertices[] = {
        -0.5f, -0.5f, 0.0f, // Sinistra 
         0.5f, -0.5f, 0.0f, // Destra
         0.0f,  0.5f, 0.0f  // Cima
    };

    // VBO (Vertex Buffer Object): La memoria sulla GPU dove salviamo i dati
    // VAO (Vertex Array Object): Il "libretto di istruzioni" che spiega a OpenGL come leggere il VBO
    unsigned int VBO, VAO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // Diciamo a OpenGL come interpretare l'array (gruppi di 3 float)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // --- 4. IL GAME LOOP ---
    while (window.isOpen()) {
        while (const std::optional<sf::Event> event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                window.close();
            }
        }

        // Sfondo azzurro cielo
        glClearColor(0.2f, 0.5f, 0.8f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Attiviamo lo shader e disegniamo il triangolo
        glUseProgram(shaderProgram);
        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, 3);

        window.display();
    }

    // Pulizia finale
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteProgram(shaderProgram);

    return 0;
}