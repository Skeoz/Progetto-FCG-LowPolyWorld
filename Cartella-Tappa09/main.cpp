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

// ==========================================
// SHADER DEL TERRENO
// ==========================================
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

const char* fragmentShaderSource = R"(
    #version 410 core
    out vec4 FragColor;
    in vec3 FragPos;
    in vec3 Normal;
    
    uniform vec3 lightDir;
    uniform vec3 lightColor;
    uniform vec3 ambientColor;
    
    void main() {
        vec3 norm = normalize(Normal);
        float diff = max(dot(norm, normalize(lightDir)), 0.0);
        vec3 diffuseResult = lightColor * diff; 

        vec3 colorValley = vec3(0.25f, 0.45f, 0.15f); 
        vec3 colorRock   = vec3(0.45f, 0.43f, 0.4f);  
        vec3 colorSnow   = vec3(0.9f, 0.95f, 1.0f);   

        vec3 terrainColor;
        if (FragPos.z < 0.02) {
            terrainColor = mix(colorValley, colorRock, smoothstep(-0.2, 0.02, FragPos.z));
        } else {
            terrainColor = mix(colorRock, colorSnow, smoothstep(0.02, 0.12, FragPos.z));
        }

        vec3 finalLighting = (ambientColor + diffuseResult) * terrainColor;
        FragColor = vec4(finalLighting, 1.0);
    }
)";

// ==========================================
// SHADER DELLO SKYBOX (Con Sole e Stelle Procedurali)
// ==========================================
const char* skyboxVertexShader = R"(
    #version 410 core
    layout (location = 0) in vec3 aPos;
    out vec3 TexCoords;
    out vec3 StarCoords; 

    uniform mat4 projection;
    uniform mat4 view;

    void main() {
        TexCoords = aPos;
        StarCoords = aPos; 
        vec4 pos = projection * view * vec4(aPos, 1.0);
        gl_Position = pos.xyww; // Inganna il depth buffer
    }
)";

const char* skyboxFragmentShader = R"(
    #version 410 core
    out vec4 FragColor;
    in vec3 TexCoords;
    in vec3 StarCoords; 

    uniform vec3 lightDir;
    uniform vec3 horizonColor;
    uniform vec3 zenithColor;
    uniform vec3 sunColor;

    // Funzione di rumore pseudocasuale per le stelle
    float rand(vec3 co) {
        return fract(sin(dot(co, vec3(12.9898, 78.233, 45.164))) * 43758.5453);
    }

    void main() {
        vec3 dir = normalize(TexCoords);
        
        // 1. Sfumatura del cielo dall'orizzonte allo zenit
        float t = clamp((dir.z + 0.2) / 0.8, 0.0, 1.0);
        vec3 bgColor = mix(horizonColor, zenithColor, t);

        // 2. Disegno del sole
        float sunDot = dot(dir, normalize(lightDir));
        float sunGlow = pow(max(sunDot, 0.0), 64.0); 
        float sunDisk = smoothstep(0.995, 0.998, sunDot); 
        float sunVisibility = smoothstep(-0.1, 0.0, lightDir.z);

        // 3. Aggiunta Stelle Procedurali
        vec3 starColor = vec3(0.0);
        float nightVisibility = smoothstep(0.1, -0.15, lightDir.z);

        if (nightVisibility > 0.0) {
            float cellSize = 120.0;
            vec3 cellPos = floor(dir * cellSize);
            float starRand = rand(cellPos);

            if (starRand < 0.001) {
                vec3 starCenter = (cellPos + 0.5) / cellSize;
                float dist = length(dir - starCenter);
                float starIntensity = smoothstep(0.003, 0.0, dist); 
                
                vec3 baseStarColor = vec3(0.95, 0.98, 1.0); 
                vec3 colorVariance = vec3(rand(cellPos+1.0), rand(cellPos+2.0), rand(cellPos+3.0)) * 0.15;
                
                starColor = (baseStarColor + colorVariance) * starIntensity * nightVisibility * 1.5;
            }
        }

        // Unione finale
        vec3 finalColor = bgColor + (sunColor * sunGlow * 0.6 * sunVisibility) + (sunColor * sunDisk * sunVisibility) + starColor;
        FragColor = vec4(finalColor, 1.0);
    }
)";

// Coordinate del cubo 3D per lo Skybox
float skyboxVertices[] = {
    -1.0f,  1.0f, -1.0f, -1.0f, -1.0f, -1.0f,  1.0f, -1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,  1.0f,  1.0f, -1.0f, -1.0f,  1.0f, -1.0f,
    -1.0f, -1.0f,  1.0f, -1.0f, -1.0f, -1.0f, -1.0f,  1.0f, -1.0f,
    -1.0f,  1.0f, -1.0f, -1.0f,  1.0f,  1.0f, -1.0f, -1.0f,  1.0f,
     1.0f, -1.0f, -1.0f,  1.0f, -1.0f,  1.0f,  1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,  1.0f,  1.0f, -1.0f,  1.0f, -1.0f, -1.0f,
    -1.0f, -1.0f,  1.0f, -1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,  1.0f, -1.0f,  1.0f, -1.0f, -1.0f,  1.0f,
    -1.0f,  1.0f, -1.0f,  1.0f,  1.0f, -1.0f,  1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f, -1.0f,  1.0f,  1.0f, -1.0f,  1.0f, -1.0f,
    -1.0f, -1.0f, -1.0f, -1.0f, -1.0f,  1.0f,  1.0f, -1.0f, -1.0f,
     1.0f, -1.0f, -1.0f, -1.0f, -1.0f,  1.0f,  1.0f, -1.0f,  1.0f
};

int main() {
    // 1. CARICAMENTO DEM
    const char* filepath = "../Cartella-risorse/aletsch_32T.asc";
    Dem ghiacciaio(filepath);
    
    int W = ghiacciaio.header.width;
    int H = ghiacciaio.header.height;
    double zMin = ghiacciaio.min;
    double zMax = ghiacciaio.max;

    int step = 1; 
    int cols = (W + step - 1) / step;
    int rows = (H + step - 1) / step;

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

    // 2. SETUP SFML E OPENGL
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

    // 3. COMPILAZIONE SHADER TERRENO
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

    // COMPILAZIONE SHADER SKYBOX
    unsigned int skyboxVert = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(skyboxVert, 1, &skyboxVertexShader, NULL);
    glCompileShader(skyboxVert);
    unsigned int skyboxFrag = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(skyboxFrag, 1, &skyboxFragmentShader, NULL);
    glCompileShader(skyboxFrag);
    unsigned int skyboxProgram = glCreateProgram();
    glAttachShader(skyboxProgram, skyboxVert);
    glAttachShader(skyboxProgram, skyboxFrag);
    glLinkProgram(skyboxProgram);

    // 4. SETUP MEMORIA GPU TERRENO
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

    // SETUP MEMORIA GPU SKYBOX
    unsigned int skyboxVAO, skyboxVBO;
    glGenVertexArrays(1, &skyboxVAO);
    glGenBuffers(1, &skyboxVBO);
    glBindVertexArray(skyboxVAO);
    glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    sf::Clock deltaClock;
    
    // Variabili per il controllo del tempo
    bool isTimePaused = false;
    float currentSunAngle = 0.0f;
    float daySpeed = 0.1f;

    // Palette Colori Cielo Avanzata
    glm::vec3 skyDay        = glm::vec3(0.5f, 0.7f, 0.9f);
    glm::vec3 skyGoldenHour = glm::vec3(0.9f, 0.6f, 0.3f);
    glm::vec3 skySunset     = glm::vec3(0.8f, 0.3f, 0.45f); 
    glm::vec3 skyTwilight   = glm::vec3(0.1f, 0.15f, 0.3f); 
    glm::vec3 skyNight      = glm::vec3(0.02f, 0.02f, 0.08f);

    while (window.isOpen()) {
        float deltaTime = deltaClock.restart().asSeconds();

        while (const std::optional<sf::Event> event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) window.close();
            else if (const auto* resized = event->getIf<sf::Event::Resized>()) {
                glViewport(0, 0, resized->size.x, resized->size.y);
            }
            else if (const auto* keyPressed = event->getIf<sf::Event::KeyPressed>()) {
                if (keyPressed->scancode == sf::Keyboard::Scan::Escape) window.close();
                if (keyPressed->scancode == sf::Keyboard::Scan::Tab) {
                    isMouseGrabbed = !isMouseGrabbed;
                    window.setMouseCursorGrabbed(isMouseGrabbed);
                    window.setMouseCursorVisible(!isMouseGrabbed);
                    if (isMouseGrabbed) firstMouse = true;
                }
                // Pausa del tempo
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

        // CALCOLO CICLO GIORNO/NOTTE
        if (!isTimePaused) {
            currentSunAngle += deltaTime * daySpeed;
        }

        glm::vec3 currentLightDir = glm::normalize(glm::vec3(cos(currentSunAngle), -0.4f, sin(currentSunAngle)));
        float sunHeight = glm::clamp(currentLightDir.z, -1.0f, 1.0f);

        glm::vec3 currentHorizon;
        glm::vec3 currentLightColor;
        glm::vec3 currentAmbient;

        if (sunHeight > 0.3f) {
            currentHorizon = skyDay;
            currentLightColor = glm::vec3(1.0f, 0.95f, 0.9f);
            currentAmbient = glm::vec3(0.25f);
        } else if (sunHeight > 0.1f) {
            float t = glm::smoothstep(0.0f, 1.0f, (sunHeight - 0.1f) / 0.2f); 
            currentHorizon = glm::mix(skyGoldenHour, skyDay, t);
            currentLightColor = glm::mix(glm::vec3(1.0f, 0.6f, 0.2f), glm::vec3(1.0f, 0.95f, 0.9f), t);
            currentAmbient = glm::mix(glm::vec3(0.2f), glm::vec3(0.25f), t);
        } else if (sunHeight > -0.05f) {
            float t = glm::smoothstep(0.0f, 1.0f, (sunHeight + 0.05f) / 0.15f);
            currentHorizon = glm::mix(skySunset, skyGoldenHour, t);
            currentLightColor = glm::mix(glm::vec3(0.8f, 0.2f, 0.1f), glm::vec3(1.0f, 0.6f, 0.2f), t);
            currentAmbient = glm::mix(glm::vec3(0.1f), glm::vec3(0.2f), t);
        } else if (sunHeight > -0.2f) {
            float t = glm::smoothstep(0.0f, 1.0f, (sunHeight + 0.2f) / 0.15f);
            currentHorizon = glm::mix(skyTwilight, skySunset, t);
            currentLightColor = glm::mix(glm::vec3(0.0f), glm::vec3(0.8f, 0.2f, 0.1f), t); 
            currentAmbient = glm::mix(glm::vec3(0.05f), glm::vec3(0.1f), t);
        } else {
            currentHorizon = skyNight;
            currentLightColor = glm::vec3(0.0f); 
            currentAmbient = glm::vec3(0.05f); 
        }

        // Il cielo sopra la nostra testa (zenit) è sempre un po' più scuro dell'orizzonte
        glm::vec3 currentZenith = currentHorizon * 0.4f;

        // Pulizia dello schermo (Fondo fisso neutro, il cielo ora lo disegna lo Skybox)
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Calcolo Matrici Base
        float width = static_cast<float>(window.getSize().x);
        float height = static_cast<float>(window.getSize().y);
        float aspectRatio = width / height;
        glm::mat4 projection = glm::perspective(glm::radians(45.0f), aspectRatio, 0.1f, 100.0f);
        glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);

        // --- 1. DISEGNO DELLO SKYBOX ---
        glDepthFunc(GL_LEQUAL); // Obbliga lo skybox a stare sullo sfondo assoluto
        glUseProgram(skyboxProgram);
        
        // Rimuoviamo la traslazione dalla View Matrix così non possiamo mai raggiungere il bordo
        glm::mat4 viewSkybox = glm::mat4(glm::mat3(view)); 
        
        glUniformMatrix4fv(glGetUniformLocation(skyboxProgram, "view"), 1, GL_FALSE, glm::value_ptr(viewSkybox));
        glUniformMatrix4fv(glGetUniformLocation(skyboxProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
        glUniform3fv(glGetUniformLocation(skyboxProgram, "horizonColor"), 1, glm::value_ptr(currentHorizon));
        glUniform3fv(glGetUniformLocation(skyboxProgram, "zenithColor"), 1, glm::value_ptr(currentZenith));
        glUniform3fv(glGetUniformLocation(skyboxProgram, "lightDir"), 1, glm::value_ptr(currentLightDir));
        glUniform3fv(glGetUniformLocation(skyboxProgram, "sunColor"), 1, glm::value_ptr(currentLightColor));

        glBindVertexArray(skyboxVAO);
        glDrawArrays(GL_TRIANGLES, 0, 36);
        glBindVertexArray(0);
        glDepthFunc(GL_LESS); // Ripristiniamo il depth test classico per il terreno

        // --- 2. DISEGNO DEL TERRENO ---
        glUseProgram(shaderProgram);
        glm::mat4 model = glm::mat4(1.0f);
        
        glUniform3fv(glGetUniformLocation(shaderProgram, "lightDir"), 1, glm::value_ptr(currentLightDir));
        glUniform3fv(glGetUniformLocation(shaderProgram, "lightColor"), 1, glm::value_ptr(currentLightColor));
        glUniform3fv(glGetUniformLocation(shaderProgram, "ambientColor"), 1, glm::value_ptr(currentAmbient));
        
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));

        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indices.size()), GL_UNSIGNED_INT, 0);

        window.display();
    }

    // PULIZIA MEMORIA
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
    glDeleteVertexArrays(1, &skyboxVAO);
    glDeleteBuffers(1, &skyboxVBO);
    glDeleteProgram(shaderProgram);
    glDeleteProgram(skyboxProgram);

    return 0;
}