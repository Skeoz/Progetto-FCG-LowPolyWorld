#include <glad/glad.h>
#include <SFML/Window.hpp>
#include <iostream>
#include <vector>
#include <optional>
#include "dem.hh"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
};

// --- VARIABILI TELECAMERA ---
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

// --- VERTEX SHADER ---
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
        Normal = mat3(transpose(inverse(model))) * aNormal; 
        gl_Position = projection * view * vec4(FragPos, 1.0);
    }
)";

// --- FRAGMENT SHADER (GIORNO/NOTTE) ---
const char* fragmentShaderSource = R"(
    #version 410 core
    out vec4 FragColor;

    in vec3 FragPos;
    in vec3 Normal;

    // Variabili dinamiche calcolate dal C++
    uniform vec3 lightDir;
    uniform vec3 lightColor;
    uniform vec3 ambientColor;

    void main() {
        // 1. Illuminazione (Sole Dinamico)
        vec3 norm = normalize(Normal);
        float diff = max(dot(norm, normalize(lightDir)), 0.0);
        vec3 diffuseResult = lightColor * diff; 

        // 2. Palette dei Colori del bioma (Altitudine)
        vec3 colorValley = vec3(0.25f, 0.45f, 0.15f); 
        vec3 colorRock   = vec3(0.45f, 0.43f, 0.4f);  
        vec3 colorSnow   = vec3(0.9f, 0.95f, 1.0f);   

        vec3 terrainColor;
        if (FragPos.z < 0.02) {
            terrainColor = mix(colorValley, colorRock, smoothstep(-0.2, 0.02, FragPos.z));
        } else {
            terrainColor = mix(colorRock, colorSnow, smoothstep(0.02, 0.12, FragPos.z));
        }

        // 3. Risultato finale
        vec3 finalLighting = (ambientColor + diffuseResult) * terrainColor;
        FragColor = vec4(finalLighting, 1.0);
    }
)";

int main() {
    // CARICAMENTO DEM
    const char* filepath = "../Cartella-risorse/aletsch_32T.asc";
    Dem ghiacciaio(filepath);
    
    int W = ghiacciaio.header.width;
    int H = ghiacciaio.header.height;
    double zMin = ghiacciaio.min;
    double zMax = ghiacciaio.max;

    int step = 1; 
    int cols = (W + step - 1) / step;
    int rows = (H + step - 1) / step;

    // MAPPATURA GRIGLIA 3D
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

    // CALCOLO NORMALI E VERTICI
    std::vector<Vertex> meshVertices;
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            glm::vec3 pos = gridPositions[r][c];
            glm::vec3 tangentX(1.0f, 0.0f, 0.0f);
            glm::vec3 tangentY(0.0f, 1.0f, 0.0f);

            if (c > 0 && c < cols - 1) tangentX = gridPositions[r][c+1] - gridPositions[r][c-1];
            else if (c < cols - 1) tangentX = gridPositions[r][c+1] - pos;
            else if (c > 0) tangentX = pos - gridPositions[r][c-1];

            if (r > 0 && r < rows - 1) tangentY = gridPositions[r+1][c] - gridPositions[r-1][c];
            else if (r < rows - 1) tangentY = gridPositions[r+1][c] - pos;
            else if (r > 0) tangentY = pos - gridPositions[r-1][c];

            glm::vec3 normal = glm::normalize(glm::cross(tangentX, tangentY));
            if (normal.z < 0.0f) normal = -normal; 
            meshVertices.push_back({pos, normal});
        }
    }

    // INDICI TRIANGOLI
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

    // SETUP SFML E OPENGL
    sf::ContextSettings settings;
    settings.depthBits = 24;
    settings.majorVersion = 4;
    settings.minorVersion = 1;
    settings.attributeFlags = sf::ContextSettings::Core;

    sf::Window window(sf::VideoMode({1024, 768}), "Progetto FCG - LowPolyWorld", sf::State::Windowed, settings);
    window.setMouseCursorGrabbed(isMouseGrabbed);
    window.setMouseCursorVisible(!isMouseGrabbed);

    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(sf::Context::getFunction))) {
        std::cout << "Errore GLAD!" << std::endl;
        return -1;
    }

    glViewport(0, 0, window.getSize().x, window.getSize().y);
    glEnable(GL_DEPTH_TEST);
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

    unsigned int VAO, VBO, EBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, meshVertices.size() * sizeof(Vertex), meshVertices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(offsetof(Vertex, normal)));
    glEnableVertexAttribArray(1);

    // Uniform locations
    int modelLoc = glGetUniformLocation(shaderProgram, "model");
    int viewLoc  = glGetUniformLocation(shaderProgram, "view");
    int projLoc  = glGetUniformLocation(shaderProgram, "projection");
    
    int lightDirLoc   = glGetUniformLocation(shaderProgram, "lightDir");
    int lightColorLoc = glGetUniformLocation(shaderProgram, "lightColor");
    int ambientLoc    = glGetUniformLocation(shaderProgram, "ambientColor");

    sf::Clock deltaClock;
    
    // Variabili per la gestione del tempo
    bool isTimePaused = false;
    float currentSunAngle = 0.0f;
    float daySpeed = 0.1f;

    // Palette Colori Cielo Avanzata
    glm::vec3 skyDay        = glm::vec3(0.5f, 0.7f, 0.9f);
    glm::vec3 skyGoldenHour = glm::vec3(0.9f, 0.6f, 0.3f);
    glm::vec3 skySunset     = glm::vec3(0.8f, 0.3f, 0.45f); // Alpenglow / Magenta
    glm::vec3 skyTwilight   = glm::vec3(0.1f, 0.15f, 0.3f); // Ora blu
    glm::vec3 skyNight      = glm::vec3(0.02f, 0.02f, 0.08f);

    // --- GAME LOOP ---
    while (window.isOpen()) {
        float deltaTime = deltaClock.restart().asSeconds();

        while (const std::optional<sf::Event> event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) window.close();
            else if (const auto* resized = event->getIf<sf::Event::Resized>()) {
                glViewport(0, 0, resized->size.x, resized->size.y);
            }
            else if (const auto* keyPressed = event->getIf<sf::Event::KeyPressed>()) {
                if (keyPressed->scancode == sf::Keyboard::Scan::Escape) window.close();
                // Sblocca/Blocca il mouse
                if (keyPressed->scancode == sf::Keyboard::Scan::Tab) {
                    isMouseGrabbed = !isMouseGrabbed;
                    window.setMouseCursorGrabbed(isMouseGrabbed);
                    window.setMouseCursorVisible(!isMouseGrabbed);
                    if (isMouseGrabbed) firstMouse = true;
                }
                // Metti in pausa o fai ripartire il tempo
                if (keyPressed->scancode == sf::Keyboard::Scan::P) {
                    isTimePaused = !isTimePaused;
                }
            }
        }

        // CONTROLLI TELECAMERA (Drone)
        if (isMouseGrabbed && window.hasFocus()) {
            sf::Vector2i windowCenter(static_cast<int>(window.getSize().x / 2), static_cast<int>(window.getSize().y / 2));
            sf::Vector2i mousePos = sf::Mouse::getPosition(window);
            float xoffset = static_cast<float>(mousePos.x - windowCenter.x);
            float yoffset = static_cast<float>(windowCenter.y - mousePos.y);

            if (xoffset != 0.0f || yoffset != 0.0f) {
                sf::Mouse::setPosition(windowCenter, window); 
                yaw   -= xoffset * yawSensitivity;
                pitch += yoffset * pitchSensitivity; 
                if (pitch > 89.0f)  pitch = 89.0f;
                if (pitch < -89.0f) pitch = -89.0f;

                glm::vec3 front;
                front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
                front.y = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
                front.z = sin(glm::radians(pitch));
                cameraFront = glm::normalize(front);
            }

            float velocity = cameraSpeed * deltaTime;
            glm::vec3 cameraRight = glm::normalize(glm::cross(cameraFront, cameraUp));

            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Scan::W)) cameraPos += cameraFront * velocity; 
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Scan::S)) cameraPos -= cameraFront * velocity; 
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Scan::A)) cameraPos -= cameraRight * velocity; 
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Scan::D)) cameraPos += cameraRight * velocity; 
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Scan::Space))  cameraPos += cameraUp * velocity; 
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Scan::LShift)) cameraPos -= cameraUp * velocity; 
        }

        // --- CALCOLO CICLO GIORNO/NOTTE DINAMICO ---
        // Incrementa l'angolo solare solo se il tempo non è in pausa
        if (!isTimePaused) {
            currentSunAngle += deltaTime * daySpeed;
        }

        glm::vec3 currentLightDir = glm::normalize(glm::vec3(cos(currentSunAngle), -0.4f, sin(currentSunAngle)));
        float sunHeight = glm::clamp(currentLightDir.z, -1.0f, 1.0f);

        glm::vec3 currentSky;
        glm::vec3 currentLightColor;
        glm::vec3 currentAmbient;

        if (sunHeight > 0.3f) {
            // Giorno pieno
            currentSky = skyDay;
            currentLightColor = glm::vec3(1.0f, 0.95f, 0.9f);
            currentAmbient = glm::vec3(0.25f);
        } else if (sunHeight > 0.1f) {
            // Golden Hour 
            float t = (sunHeight - 0.1f) / 0.2f;
            t = glm::smoothstep(0.0f, 1.0f, t); 
            currentSky = glm::mix(skyGoldenHour, skyDay, t);
            currentLightColor = glm::mix(glm::vec3(1.0f, 0.6f, 0.2f), glm::vec3(1.0f, 0.95f, 0.9f), t);
            currentAmbient = glm::mix(glm::vec3(0.2f), glm::vec3(0.25f), t);
        } else if (sunHeight > -0.05f) {
            // Tramonto Magenta / Enrosadira 
            float t = (sunHeight + 0.05f) / 0.15f;
            t = glm::smoothstep(0.0f, 1.0f, t);
            currentSky = glm::mix(skySunset, skyGoldenHour, t);
            currentLightColor = glm::mix(glm::vec3(0.8f, 0.2f, 0.1f), glm::vec3(1.0f, 0.6f, 0.2f), t);
            currentAmbient = glm::mix(glm::vec3(0.1f), glm::vec3(0.2f), t);
        } else if (sunHeight > -0.2f) {
            // Ora Blu / Crepuscolo 
            float t = (sunHeight + 0.2f) / 0.15f;
            t = glm::smoothstep(0.0f, 1.0f, t);
            currentSky = glm::mix(skyTwilight, skySunset, t);
            currentLightColor = glm::mix(glm::vec3(0.0f), glm::vec3(0.8f, 0.2f, 0.1f), t); 
            currentAmbient = glm::mix(glm::vec3(0.05f), glm::vec3(0.1f), t);
        } else {
            // Notte fonda
            currentSky = skyNight;
            currentLightColor = glm::vec3(0.0f); 
            currentAmbient = glm::vec3(0.05f); 
        }

        // RENDERING SCENA
        glClearColor(currentSky.r, currentSky.g, currentSky.b, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(shaderProgram);

        // Inviamo i parametri luce calcolati alla GPU
        glUniform3fv(lightDirLoc, 1, glm::value_ptr(currentLightDir));
        glUniform3fv(lightColorLoc, 1, glm::value_ptr(currentLightColor));
        glUniform3fv(ambientLoc, 1, glm::value_ptr(currentAmbient));

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