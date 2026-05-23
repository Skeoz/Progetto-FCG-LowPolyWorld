#include <glad/glad.h>
#include <SFML/Window.hpp>
#include <iostream>
#include <vector>
#include <optional>
#include "dem.hh"

// Librerie matematiche GLM
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// --- VERTEX SHADER ---
// Riceve le matrici di trasformazione per generare la prospettiva 3D
const char* vertexShaderSource = R"(
    #version 410 core
    layout (location = 0) in vec3 aPos;

    uniform mat4 model;
    uniform mat4 view;
    uniform mat4 projection;

    void main() {
        gl_Position = projection * view * model * vec4(aPos, 1.0);
    }
)";

// --- FRAGMENT SHADER ---
// Colora le linee del wireframe (Arancione Low-Poly)
const char* fragmentShaderSource = R"(
    #version 410 core
    out vec4 FragColor;
    void main() {
        FragColor = vec4(1.0f, 0.5f, 0.2f, 1.0f);
    }
)";

int main() {
    // 1. CARICAMENTO DATI ALTIMETRICI (PARSER DEL PROF)
    const char* filepath = "../Cartella-risorse/aletsch_32T.asc";
    Dem ghiacciaio(filepath);
    
    int W = ghiacciaio.header.width;
    int H = ghiacciaio.header.height;
    double zMin = ghiacciaio.min;
    double zMax = ghiacciaio.max;

    // 2. GENERAZIONE DELLA MESH CON LOD (Level of Detail)
    std::vector<float> vertices;
    std::vector<unsigned int> indices;

    int step = 10; // Salta 10 punti per sfoltire la griglia ed evitare il muro arancione
    int cols = (W + step - 1) / step;
    int rows = (H + step - 1) / step;

    for (int y = 0; y < H; y += step) {
        for (int x = 0; x < W; x += step) {
            float xNdc = ((float)x / (W - 1)) - 0.5f;
            float yNdc = ((float)y / (H - 1)) - 0.5f;
            float zNorm = (ghiacciaio(x, y) - zMin) / (zMax - zMin);
            float zNdc = (zNorm * 0.4f) - 0.2f; // Esasperiamo leggermente le vette

            vertices.push_back(xNdc);
            vertices.push_back(yNdc);
            vertices.push_back(zNdc);
        }
    }

    for (int r = 0; r < rows - 1; ++r) {
        for (int c = 0; c < cols - 1; ++c) {
            unsigned int topLeft     = r * cols + c;
            unsigned int topRight    = topLeft + 1;
            unsigned int bottomLeft  = (r + 1) * cols + c;
            unsigned int bottomRight = bottomLeft + 1;

            indices.push_back(topLeft);
            indices.push_back(bottomLeft);
            indices.push_back(topRight);
            indices.push_back(topRight);
            indices.push_back(bottomLeft);
            indices.push_back(bottomRight);
        }
    }

    // 3. INIZIALIZZAZIONE FINESTRA E CONTESTO (SFML 3.0 & OPENGL 4.1)
    sf::ContextSettings settings;
    settings.depthBits = 24;
    settings.majorVersion = 4;
    settings.minorVersion = 1;
    settings.attributeFlags = sf::ContextSettings::Core;

    sf::Window window(sf::VideoMode({800, 600}), "Progetto FCG - LowPolyWorld", sf::State::Windowed, settings);

    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(sf::Context::getFunction))) {
        std::cout << "Errore inizializzazione GLAD!" << std::endl;
        return -1;
    }

    // Configurazione iniziale dello stato di OpenGL
    glViewport(0, 0, 800, 600);
    glEnable(GL_DEPTH_TEST);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); // Modalità Wireframe attiva

    // 4. COMPILAZIONE E LINKING DEGLI SHADER
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
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    // 5. ALLOCAZIONE BUFFER SULLA GPU (VAO, VBO, EBO)
    unsigned int VAO, VBO, EBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Identifichiamo dove risiedono le matrici nello shader
    int modelLoc = glGetUniformLocation(shaderProgram, "model");
    int viewLoc  = glGetUniformLocation(shaderProgram, "view");
    int projLoc  = glGetUniformLocation(shaderProgram, "projection");

    // 6. GAME LOOP PRINCIPALE
    while (window.isOpen()) {
        while (const std::optional<sf::Event> event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                window.close();
            }
            // Gestione del ridimensionamento dinamico dello schermo
            else if (const auto* resized = event->getIf<sf::Event::Resized>()) {
                glViewport(0, 0, resized->size.x, resized->size.y);
            }
        }

        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(shaderProgram);

        // Calcolo dinamico dell'aspect ratio per evitare distorsioni a tutto schermo
        float width = static_cast<float>(window.getSize().x);
        float height = static_cast<float>(window.getSize().y);
        float aspectRatio = width / height;

        // Costruzione delle matrici matematiche (GLM)
        glm::mat4 projection = glm::perspective(glm::radians(45.0f), aspectRatio, 0.1f, 100.0f);
        
        // Inquadratura panoramica "dall'elicottero"
        glm::mat4 view = glm::lookAt(glm::vec3(0.0f, -1.2f, 0.8f),  // Posizione telecamera
                                     glm::vec3(0.0f, 0.0f, 0.0f),  // Punto guardato (centro)
                                     glm::vec3(0.0f, 0.0f, 1.0f)); // Vettore "Alto" (Asse Z)
        
        glm::mat4 model = glm::mat4(1.0f); // Montagna ferma al centro del mondo

        // Spediamo le matrici aggiornate alla GPU
        glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projection));
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

        // Disegniamo la mesh usando la tabella degli indici
        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indices.size()), GL_UNSIGNED_INT, 0);

        window.display();
    }

    // Pulizia finale della memoria
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
    glDeleteProgram(shaderProgram);

    return 0;
}