#include <glad/glad.h>
#include <SFML/Window.hpp>
#include <iostream>
#include <optional>

// SHADER

// Vertex Shader: Elabora ogni vertice singolarmente, trasforma le coordinate da spazio oggetto a spazio clip
const char* vertexShaderSource = R"(
    #version 410 core
    layout (location = 0) in vec3 aPos;
    void main() {
        gl_Position = vec4(aPos.x, aPos.y, aPos.z, 1.0);
    }
)";

// Fragment Shader: Determina il colore finale di ogni pixel rasterizzato durante il rendering
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

    // COMPILAZIONE E LINKING DEGLI SHADER
    // Creazione, compilazione e collegamento dei programmi shader per il pipeline grafico
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

    // DEFINIZIONE DELLA GEOMETRIA
    // Coordinate (X, Y, Z) dei tre vertici del triangolo nello spazio 3D
    float vertices[] = {
        -0.5f, -0.5f, 0.0f, // Sinistra 
         0.5f, -0.5f, 0.0f, // Destra
         0.0f,  0.5f, 0.0f  // Cima
    };

    // VBO (Vertex Buffer Object): Allocazione memoria GPU per l'archiviazione dei dati geometrici
    // VAO (Vertex Array Object): Configurazione che specifica il layout dei vertici e come accedere ai dati nel VBO
    unsigned int VBO, VAO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // Configurazione dell'attributo di vertice: stride di 3 float, tipo GL_FLOAT, nessuna normalizzazione
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // GAME LOOP
    // Elaborazione degli eventi e rendering del frame fino alla chiusura della finestra 
    while (window.isOpen()) {
        while (const std::optional<sf::Event> event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                window.close();
            }
        }

        // Impostazione del colore di sfondo e cancellazione dei buffer (colore e profondità)
        glClearColor(0.2f, 0.5f, 0.8f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Attivazione del programma shader e rendering della geometria
        glUseProgram(shaderProgram);
        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, 3);

        window.display();
    }

    // Liberazione delle risorse GPU allocate
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteProgram(shaderProgram);

    return 0;
}