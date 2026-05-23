#include <glad/glad.h>
#include <SFML/Window.hpp>
#include <iostream>
#include <vector>
#include <optional>
#include "dem.hh"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// --- NUOVA STRUTTURA DATI ---
// Ora ogni vertice ha una posizione 3D e una Normale 3D (per calcolare la luce)
struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
};

// Variabili Telecamera (Ereditate e testate nella Tappa 05)
glm::vec3 cameraPos   = glm::vec3(0.0f, -1.2f, 0.6f); 
glm::vec3 cameraFront = glm::vec3(0.0f, 1.0f, -0.4f);
glm::vec3 cameraUp    = glm::vec3(0.0f, 0.0f, 1.0f);

float yaw   = 90.0f;  
float pitch = -20.0f; 
bool firstMouse = true;
bool isMouseGrabbed = true; 

float cameraSpeed = 0.5f;
float yawSensitivity   = 0.1f;
float pitchSensitivity = 0.08f;

// --- VERTEX SHADER AGGIORNATO ---
// Riceve posizione (location 0) e normale (location 1) e le passa al Fragment Shader
const char* vertexShaderSource = R"(
    #version 410 core
    layout (location = 0) in vec3 aPos;
    layout (location = 1) in vec3 aNormal;

    out vec3 FragPos;
    out vec3 Normal;

    uniform mat4 model;
    uniform mat4 view;
    uniform mat4 projection;

    void main() {
        FragPos = vec3(model * vec4(aPos, 1.0));
        // Normal matrix: previene distorsioni delle normali in caso di scalatura
        Normal = mat3(transpose(inverse(model))) * aNormal; 
        gl_Position = projection * view * vec4(FragPos, 1.0);
    }
)";

// --- FRAGMENT SHADER AGGIORNATO (ILLUMINAZIONE) ---
const char* fragmentShaderSource = R"(
    #version 410 core
    out vec4 FragColor;

    in vec3 FragPos;
    in vec3 Normal;

    void main() {
        // 1. Luce Ambientale (simula la luce sparsa nel cielo)
        float ambientStrength = 0.2;
        vec3 ambientColor = vec3(1.0, 1.0, 1.0) * ambientStrength;
        
        // 2. Luce Diffusa (impatto diretto del sole)
        vec3 norm = normalize(Normal);
        // Direzione del sole: Immaginiamolo alto nel cielo, leggermente a sud-est
        vec3 lightDir = normalize(vec3(0.5f, -0.5f, 1.0f)); 
        
        float diff = max(dot(norm, lightDir), 0.0);
        vec3 diffuseColor = vec3(1.0, 0.95, 0.8) * diff; // Luce solare calda

        // Colore base della roccia (un grigio granito realistico)
        vec3 rockColor = vec3(0.5f, 0.5f, 0.5f);

        // Risultato finale
        vec3 result = (ambientColor + diffuseColor) * rockColor;
        FragColor = vec4(result, 1.0);
    }
)";

int main() {
    const char* filepath = "../Cartella-risorse/aletsch_32T.asc";
    Dem ghiacciaio(filepath);
    
    int W = ghiacciaio.header.width;
    int H = ghiacciaio.header.height;
    double zMin = ghiacciaio.min;
    double zMax = ghiacciaio.max;

    int step = 1; 
    int cols = (W + step - 1) / step;
    int rows = (H + step - 1) / step;

    // FASE A: Mappatura temporanea delle posizioni 3D in una griglia 2D
    std::vector<std::vector<glm::vec3>> gridPositions(rows, std::vector<glm::vec3>(cols));
    int r = 0;
    for (int y = 0; y < H; y += step) {
        int c = 0;
        for (int x = 0; x < W; x += step) {
            float xNdc = ((float)x / (W - 1)) - 0.5f;
            float yNdc = ((float)y / (H - 1)) - 0.5f;
            float zNorm = (ghiacciaio(x, y) - zMin) / (zMax - zMin);
            float zNdc = (zNorm * 0.4f) - 0.2f; 
            gridPositions[r][c] = glm::vec3(xNdc, yNdc, zNdc);
            c++;
        }
        r++;
    }

    // FASE B: Calcolo delle Normali e generazione della Mesh finale
    std::vector<Vertex> meshVertices;
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            glm::vec3 pos = gridPositions[r][c];
            
            // Calcoliamo la pendenza sull'asse X e sull'asse Y usando i punti vicini
            glm::vec3 tangentX(1.0f, 0.0f, 0.0f);
            glm::vec3 tangentY(0.0f, 1.0f, 0.0f);

            if (c > 0 && c < cols - 1) tangentX = gridPositions[r][c+1] - gridPositions[r][c-1];
            else if (c < cols - 1) tangentX = gridPositions[r][c+1] - pos;
            else if (c > 0) tangentX = pos - gridPositions[r][c-1];

            if (r > 0 && r < rows - 1) tangentY = gridPositions[r+1][c] - gridPositions[r-1][c];
            else if (r < rows - 1) tangentY = gridPositions[r+1][c] - pos;
            else if (r > 0) tangentY = pos - gridPositions[r-1][c];

            // Il prodotto vettoriale (cross) genera il vettore perpendicolare alla superficie (la Normale!)
            glm::vec3 normal = glm::normalize(glm::cross(tangentX, tangentY));
            if (normal.z < 0.0f) normal = -normal; // Forza la normale verso l'alto

            // Inseriamo Posizione e Normale accoppiati
            meshVertices.push_back({pos, normal});
        }
    }

    // FASE C: Indici EBO per disegnare i triangoli
    std::vector<unsigned int> indices;
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

    sf::ContextSettings settings;
    settings.depthBits = 24;
    settings.majorVersion = 4;
    settings.minorVersion = 1;
    settings.attributeFlags = sf::ContextSettings::Core;

    sf::Window window(sf::VideoMode({800, 600}), "Progetto FCG - LowPolyWorld", sf::State::Windowed, settings);

    window.setMouseCursorGrabbed(isMouseGrabbed);
    window.setMouseCursorVisible(!isMouseGrabbed);

    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(sf::Context::getFunction))) {
        std::cout << "Errore GLAD!" << std::endl;
        return -1;
    }

    glViewport(0, 0, 800, 600);
    glEnable(GL_DEPTH_TEST);
    
    // BAM! Spegniamo il wireframe e riempiamo i poligoni
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

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

    // --- SETUP DELLA MEMORIA GPU (VAO, VBO, EBO) ---
    unsigned int VAO, VBO, EBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);
    
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    // Nota il sizeof(Vertex): ora carichiamo l'intera struct
    glBufferData(GL_ARRAY_BUFFER, meshVertices.size() * sizeof(Vertex), meshVertices.data(), GL_STATIC_DRAW);
    
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    // Attributo 0: Posizione (X, Y, Z). Stride = sizeof(Vertex), Offset = 0
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
    glEnableVertexAttribArray(0);

    // Attributo 1: Normale (X, Y, Z). Stride = sizeof(Vertex), Offset = spazio occupato dalla posizione
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(offsetof(Vertex, normal)));
    glEnableVertexAttribArray(1);

    int modelLoc = glGetUniformLocation(shaderProgram, "model");
    int viewLoc  = glGetUniformLocation(shaderProgram, "view");
    int projLoc  = glGetUniformLocation(shaderProgram, "projection");

    sf::Clock deltaClock;

    while (window.isOpen()) {
        float deltaTime = deltaClock.restart().asSeconds();

        while (const std::optional<sf::Event> event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                window.close();
            }
            else if (const auto* resized = event->getIf<sf::Event::Resized>()) {
                glViewport(0, 0, resized->size.x, resized->size.y);
            }
            else if (const auto* keyPressed = event->getIf<sf::Event::KeyPressed>()) {
                if (keyPressed->scancode == sf::Keyboard::Scan::Escape) {
                    window.close();
                }
                if (keyPressed->scancode == sf::Keyboard::Scan::Tab) {
                    isMouseGrabbed = !isMouseGrabbed;
                    window.setMouseCursorGrabbed(isMouseGrabbed);
                    window.setMouseCursorVisible(!isMouseGrabbed);
                    if (isMouseGrabbed) firstMouse = true;
                }
            }
        }

        if (isMouseGrabbed && window.hasFocus()) {
            sf::Vector2i windowCenter(static_cast<int>(window.getSize().x / 2), static_cast<int>(window.getSize().y / 2));
            sf::Vector2i mousePos = sf::Mouse::getPosition(window);

            float xoffset = static_cast<float>(mousePos.x - windowCenter.x);
            float yoffset = static_cast<float>(windowCenter.y - mousePos.y);

            if (xoffset != 0.0f || yoffset != 0.0f) {
                sf::Mouse::setPosition(windowCenter, window); 

                xoffset *= yawSensitivity;
                yoffset *= pitchSensitivity; 

                yaw   -= xoffset;
                pitch += yoffset;

                if (pitch > 89.0f)  pitch = 89.0f;
                if (pitch < -89.0f) pitch = -89.0f;

                glm::vec3 front;
                front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
                front.y = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
                front.z = sin(glm::radians(pitch));
                cameraFront = glm::normalize(front);
            }
        }

        if (isMouseGrabbed && window.hasFocus()) {
            float velocity = cameraSpeed * deltaTime;
            glm::vec3 cameraRight = glm::normalize(glm::cross(cameraFront, cameraUp));

            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Scan::W)) cameraPos += cameraFront * velocity; 
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Scan::S)) cameraPos -= cameraFront * velocity; 
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Scan::A)) cameraPos -= cameraRight * velocity; 
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Scan::D)) cameraPos += cameraRight * velocity; 
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Scan::Space))  cameraPos += cameraUp * velocity; 
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Scan::LShift)) cameraPos -= cameraUp * velocity; 
        }

        // Nuovo colore di sfondo: Azzurro cielo per contrastare la roccia!
        glClearColor(0.5f, 0.7f, 0.9f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(shaderProgram);

        float width = static_cast<float>(window.getSize().x);
        float height = static_cast<float>(window.getSize().y);
        float aspectRatio = width / height;

        glm::mat4 projection = glm::perspective(glm::radians(45.0f), aspectRatio, 0.1f, 100.0f);
        glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
        glm::mat4 model = glm::mat4(1.0f);

        glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projection));
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indices.size()), GL_UNSIGNED_INT, 0);

        window.display();
    }

    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
    glDeleteProgram(shaderProgram);

    return 0;
}