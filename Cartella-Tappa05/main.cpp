#include <glad/glad.h>
#include <SFML/Window.hpp>
#include <iostream>
#include <vector>
#include <optional>
#include "dem.hh"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// --- VARIABILI GLOBALI DELLA TELECAMERA ---
glm::vec3 cameraPos   = glm::vec3(0.0f, -1.2f, 0.4f);
glm::vec3 cameraFront = glm::vec3(0.0f, 1.0f, -0.3f);
glm::vec3 cameraUp    = glm::vec3(0.0f, 0.0f, 1.0f);

float yaw   = 90.0f;  
float pitch = -20.0f; 
bool firstMouse = true;

bool isMouseGrabbed = true; 

float cameraSpeed = 0.5f;
float yawSensitivity   = 0.1f;  // Sensibilità asse X (Destra/Sinistra)
float pitchSensitivity = 0.08f; // Sensibilità asse Y (Sopra/Sotto)

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

const char* fragmentShaderSource = R"(
    #version 410 core
    out vec4 FragColor;
    void main() {
        FragColor = vec4(1.0f, 0.5f, 0.2f, 1.0f);
    }
)";

int main() {
    const char* filepath = "../Cartella-risorse/aletsch_32T.asc";
    Dem ghiacciaio(filepath);
    
    int W = ghiacciaio.header.width;
    int H = ghiacciaio.header.height;
    double zMin = ghiacciaio.min;
    double zMax = ghiacciaio.max;

    std::vector<float> vertices;
    std::vector<unsigned int> indices;

    int step = 10; 
    int cols = (W + step - 1) / step;
    int rows = (H + step - 1) / step;

    for (int y = 0; y < H; y += step) {
        for (int x = 0; x < W; x += step) {
            float xNdc = ((float)x / (W - 1)) - 0.5f;
            float yNdc = ((float)y / (H - 1)) - 0.5f;
            float zNorm = (ghiacciaio(x, y) - zMin) / (zMax - zMin);
            float zNdc = (zNorm * 0.4f) - 0.2f; 

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
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

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
                    if (isMouseGrabbed) {
                        firstMouse = true;
                    }
                }
            }
        }

        // --- NUOVA LOGICA TRACCIAMENTO MOUSE (FUORI DAL POLL_EVENT) ---
        if (isMouseGrabbed && window.hasFocus()) {
            sf::Vector2i windowCenter(static_cast<int>(window.getSize().x / 2), static_cast<int>(window.getSize().y / 2));
            sf::Vector2i mousePos = sf::Mouse::getPosition(window);

            float xoffset = static_cast<float>(mousePos.x - windowCenter.x);
            float yoffset = static_cast<float>(windowCenter.y - mousePos.y);

            if (xoffset != 0.0f || yoffset != 0.0f) {
                sf::Mouse::setPosition(windowCenter, window); // Re-centra l'hardware

                // Usiamo le due sensibilità separate
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

        // --- CONTROLLI TASTIERA ORIZZONTALI E VERTICALI (STILE MINECRAFT CREATIVE) ---
        if (isMouseGrabbed && window.hasFocus()) {
            float velocity = cameraSpeed * deltaTime;
            glm::vec3 cameraRight = glm::normalize(glm::cross(cameraFront, cameraUp));

            // Spostamenti Standard (WASD)
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Scan::W)) cameraPos += cameraFront * velocity; 
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Scan::S)) cameraPos -= cameraFront * velocity; 
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Scan::A)) cameraPos -= cameraRight * velocity; 
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Scan::D)) cameraPos += cameraRight * velocity; 

            // --- NUOVI COMANDI VERTICALI ---
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Scan::Space))  
                cameraPos += cameraUp * velocity; // SPAZIO: Sale dritto verso il cielo (Asse Z+)
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Scan::LShift)) 
                cameraPos -= cameraUp * velocity; // SHIFT SINISTRO: Scende in picchiata (Asse Z-)
        }

        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
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