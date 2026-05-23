#include <glad/glad.h>
#include <SFML/Window.hpp>
#include <SFML/Graphics/Image.hpp> 
#include <iostream>
#include <vector>
#include <fstream>
#include <sstream>
#include <string>
#include <optional>
#include "dem.hh"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// Struttura Vertex con coordinate UV per le Texture
struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texCoords; 
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

// --- PARSER OBJ OTTIMIZZATO ---
bool loadOBJ(const std::string& path, std::vector<Vertex>& out_vertices) {
    std::vector<glm::vec3> temp_vertices;
    std::vector<glm::vec3> temp_normals;
    std::vector<glm::vec2> temp_uvs;
    std::vector<unsigned int> vertexIndices, uvIndices, normalIndices;

    temp_vertices.reserve(5000);
    temp_normals.reserve(5000);
    temp_uvs.reserve(5000);
    vertexIndices.reserve(15000);

    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "ERRORE: Impossibile aprire il file OBJ in: " << path << std::endl;
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string header;
        ss >> header;

        if (header == "v") {
            glm::vec3 vertex;
            ss >> vertex.x >> vertex.y >> vertex.z;
            temp_vertices.push_back(vertex);
        } else if (header == "vt") {
            glm::vec2 uv;
            ss >> uv.x >> uv.y;
            temp_uvs.push_back(uv);
        } else if (header == "vn") {
            glm::vec3 normal;
            ss >> normal.x >> normal.y >> normal.z;
            temp_normals.push_back(normal);
        } else if (header == "f") {
            std::string v1, v2, v3;
            ss >> v1 >> v2 >> v3;

            auto parseFaceToken = [&](const std::string& token) {
                std::stringstream tokenStream(token);
                std::string vIdx, vtIdx, vnIdx;
                std::getline(tokenStream, vIdx, '/');
                std::getline(tokenStream, vtIdx, '/');
                std::getline(tokenStream, vnIdx, '/');
                
                vertexIndices.push_back(std::stoi(vIdx) - 1);
                if (!vtIdx.empty()) uvIndices.push_back(std::stoi(vtIdx) - 1);
                if (!vnIdx.empty()) normalIndices.push_back(std::stoi(vnIdx) - 1);
            };

            parseFaceToken(v1);
            parseFaceToken(v2);
            parseFaceToken(v3);
        }
    }

    out_vertices.reserve(vertexIndices.size());
    for (size_t i = 0; i < vertexIndices.size(); i++) {
        Vertex v;
        v.position = temp_vertices[vertexIndices[i]];
        if (!uvIndices.empty()) v.texCoords = temp_uvs[uvIndices[i]];
        else v.texCoords = glm::vec2(0.0f, 0.0f);
        if (!normalIndices.empty()) v.normal = temp_normals[normalIndices[i]];
        else v.normal = glm::vec3(0.0f, 0.0f, 1.0f);
        out_vertices.push_back(v);
    }
    return true;
}

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
    uniform vec3 pointLightPos;
    uniform vec3 pointLightColor;
    
    void main() {
        vec3 norm = normalize(Normal);
        float diffDir = max(dot(norm, normalize(lightDir)), 0.0);
        vec3 directionalResult = lightColor * diffDir; 

        vec3 lightDirPt = normalize(pointLightPos - FragPos);
        float diffPt = max(dot(norm, lightDirPt), 0.0);
        float distance = length(pointLightPos - FragPos);
        
        float constant = 1.0;
        float linear = 25.0;
        float quadratic = 180.0;
        float attenuation = 1.0 / (constant + linear * distance + quadratic * (distance * distance));
        vec3 pointResult = pointLightColor * diffPt * attenuation;

        vec3 colorValley = vec3(0.25f, 0.45f, 0.15f); 
        vec3 colorRock   = vec3(0.45f, 0.43f, 0.4f);  
        vec3 colorSnow   = vec3(0.9f, 0.95f, 1.0f);   

        vec3 terrainColor;
        if (FragPos.z < 0.02) {
            terrainColor = mix(colorValley, colorRock, smoothstep(-0.2, 0.02, FragPos.z));
        } else {
            terrainColor = mix(colorRock, colorSnow, smoothstep(0.02, 0.12, FragPos.z));
        }

        vec3 finalLighting = (ambientColor + directionalResult + pointResult) * terrainColor;
        FragColor = vec4(finalLighting, 1.0);
    }
)";

// ==========================================
// SHADER SKYBOX
// ==========================================
const char* skyboxVertexShader = R"(
    #version 410 core
    layout (location = 0) in vec3 aPos;
    out vec3 TexCoords;
    uniform mat4 projection;
    uniform mat4 view;
    void main() {
        TexCoords = aPos;
        vec4 pos = projection * view * vec4(aPos, 1.0);
        gl_Position = pos.xyww; 
    }
)";

const char* skyboxFragmentShader = R"(
    #version 410 core
    out vec4 FragColor;
    in vec3 TexCoords;
    uniform vec3 lightDir;
    uniform vec3 horizonColor;
    uniform vec3 zenithColor;
    uniform vec3 sunColor;

    float rand(vec3 co) { return fract(sin(dot(co, vec3(12.9898, 78.233, 45.164))) * 43758.5453); }

    void main() {
        vec3 dir = normalize(TexCoords);
        float t = clamp((dir.z + 0.2) / 0.8, 0.0, 1.0);
        vec3 bgColor = mix(horizonColor, zenithColor, t);
        float sunDot = dot(dir, normalize(lightDir));
        float sunGlow = pow(max(sunDot, 0.0), 64.0); 
        float sunDisk = smoothstep(0.995, 0.998, sunDot); 
        float sunVisibility = smoothstep(-0.1, 0.0, lightDir.z);

        vec3 starColor = vec3(0.0);
        float nightVisibility = smoothstep(0.1, -0.15, lightDir.z);
        if (nightVisibility > 0.0) {
            float cellSize = 120.0;
            vec3 cellPos = floor(dir * cellSize);
            if (rand(cellPos) < 0.001) {
                vec3 starCenter = (cellPos + 0.5) / cellSize;
                float starIntensity = smoothstep(0.003, 0.0, length(dir - starCenter)); 
                starColor = vec3(0.95, 0.98, 1.0) * starIntensity * nightVisibility * 1.5;
            }
        }
        vec3 finalColor = bgColor + (sunColor * sunGlow * 0.6 * sunVisibility) + (sunColor * sunDisk * sunVisibility) + starColor;
        FragColor = vec4(finalColor, 1.0);
    }
)";

// ==========================================
// SHADER CASA CON TEXTURE
// ==========================================
const char* solidVertexShader = R"(
    #version 410 core
    layout (location = 0) in vec3 aPos;
    layout (location = 1) in vec3 aNormal;
    layout (location = 2) in vec2 aTexCoords;
    
    out vec3 FaceNormal;
    out vec2 TexCoords;
    
    uniform mat4 model;
    uniform mat4 view;
    uniform mat4 projection;
    
    void main() {
        FaceNormal = mat3(transpose(inverse(model))) * aNormal;
        TexCoords = aTexCoords;
        gl_Position = projection * view * model * vec4(aPos, 1.0);
    }
)";

const char* solidFragmentShader = R"(
    #version 410 core
    out vec4 FragColor;
    in vec3 FaceNormal;
    in vec2 TexCoords;
    
    uniform sampler2D texture_diffuse; 
    uniform vec3 ambientColor;
    uniform vec3 lightDir;

    void main() {
        vec3 norm = normalize(FaceNormal);
        float diff = max(dot(norm, normalize(lightDir)), 0.0);
        vec4 textureColor = texture(texture_diffuse, TexCoords);
        vec3 lighting = (ambientColor + vec3(diff)) * textureColor.rgb;
        FragColor = vec4(lighting, 1.0);
    }
)";

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
    // SETUP DATI DEM TERRENO
    const char* filepath = "../Cartella-risorse/aletsch_32T.asc";
    Dem ghiacciaio(filepath);
    
    int W = ghiacciaio.header.width; int H = ghiacciaio.header.height;
    double zMin = ghiacciaio.min; double zMax = ghiacciaio.max;
    int step = 1; int cols = (W + step - 1) / step; int rows = (H + step - 1) / step;

    std::vector<std::vector<glm::vec3>> gridPositions(rows, std::vector<glm::vec3>(cols));
    int r = 0;
    for (int y = 0; y < H; y += step) {
        int c = 0;
        for (int x = 0; x < W; x += step) {
            float xNdc = ((float)x / (W - 1)) - 0.5f;
            float yNdc = ((float)y / (H - 1)) - 0.5f;
            float zNorm = (ghiacciaio(x, y) - zMin) / (zMax - zMin);
            gridPositions[r][c] = glm::vec3(xNdc, yNdc, (zNorm * 0.4f) - 0.2f);
            c++;
        }
        r++;
    }

    std::vector<Vertex> meshVertices;
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            glm::vec3 pos = gridPositions[r][c];
            glm::vec3 tangentX(1.0f, 0.0f, 0.0f), tangentY(0.0f, 1.0f, 0.0f);
            if (c > 0 && c < cols - 1) tangentX = gridPositions[r][c+1] - gridPositions[r][c-1];
            if (r > 0 && r < rows - 1) tangentY = gridPositions[r+1][c] - gridPositions[r-1][c];
            glm::vec3 normal = glm::normalize(glm::cross(tangentX, tangentY));
            if (normal.z < 0.0f) normal = -normal; 
            meshVertices.push_back({pos, normal, glm::vec2(0.0f)});
        }
    }

    std::vector<unsigned int> indices;
    for (int r = 0; r < rows - 1; ++r) {
        for (int c = 0; c < cols - 1; ++c) {
            unsigned int topLeft = r * cols + c;
            indices.push_back(topLeft); indices.push_back(topLeft + cols); indices.push_back(topLeft + 1);
            indices.push_back(topLeft + 1); indices.push_back(topLeft + cols); indices.push_back(topLeft + cols + 1);
        }
    }

    // CARICAMENTO FILE ESTERNI (Eseguito una sola volta all'avvio!)
    int bRow = static_cast<int>(rows * 0.60f); int bCol = static_cast<int>(cols * 0.55f);
    glm::vec3 housePos = gridPositions[bRow][bCol]; 
    housePos.z += 0.001f; 

    std::vector<Vertex> houseLoadedVertices;
    if (!loadOBJ("../Cartella-risorse/bivacco.obj", houseLoadedVertices)) {
        std::cout << "Avviso: Impossibile trovare o caricare bivacco.obj!" << std::endl;
    }

    // IMPOSTAZIONI SFML CLASSICHE RIGIDE E SICURE
    sf::ContextSettings settings;
    settings.depthBits = 24;
    settings.majorVersion = 4;
    settings.minorVersion = 1;
    settings.attributeFlags = sf::ContextSettings::Core;

    sf::Window window(sf::VideoMode({1024, 768}), "Progetto FCG - LowPolyWorld", sf::State::Windowed, settings);
    window.setMouseCursorGrabbed(isMouseGrabbed); window.setMouseCursorVisible(!isMouseGrabbed);

    gladLoadGLLoader(reinterpret_cast<GLADloadproc>(sf::Context::getFunction));
    glViewport(0, 0, window.getSize().x, window.getSize().y);
    glEnable(GL_DEPTH_TEST);

    auto compileShader = [](unsigned int type, const char* source) {
        unsigned int shader = glCreateShader(type);
        glShaderSource(shader, 1, &source, NULL); glCompileShader(shader);
        return shader;
    };

    unsigned int vertexShader = compileShader(GL_VERTEX_SHADER, vertexShaderSource);
    unsigned int fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);
    unsigned int shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader); glAttachShader(shaderProgram, fragmentShader); glLinkProgram(shaderProgram);

    unsigned int skyboxProgram = glCreateProgram();
    glAttachShader(skyboxProgram, compileShader(GL_VERTEX_SHADER, skyboxVertexShader));
    glAttachShader(skyboxProgram, compileShader(GL_FRAGMENT_SHADER, skyboxFragmentShader)); glLinkProgram(skyboxProgram);

    unsigned int solidProgram = glCreateProgram();
    glAttachShader(solidProgram, compileShader(GL_VERTEX_SHADER, solidVertexShader));
    glAttachShader(solidProgram, compileShader(GL_FRAGMENT_SHADER, solidFragmentShader)); glLinkProgram(solidProgram);

    // BINDING BUFFER TERRENO
    unsigned int VAO, VBO, EBO;
    glGenVertexArrays(1, &VAO); glGenBuffers(1, &VBO); glGenBuffers(1, &EBO);
    glBindVertexArray(VAO); glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, meshVertices.size() * sizeof(Vertex), meshVertices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0); glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(offsetof(Vertex, normal))); glEnableVertexAttribArray(1);

    // BINDING BUFFER SKYBOX
    unsigned int skyboxVAO, skyboxVBO;
    glGenVertexArrays(1, &skyboxVAO); glGenBuffers(1, &skyboxVBO);
    glBindVertexArray(skyboxVAO); glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0); glEnableVertexAttribArray(0);

    // BINDING BUFFER CASA CON LAYOUT COMPLETO (Pos, Normal, UV)
    unsigned int houseVAO, houseVBO;
    glGenVertexArrays(1, &houseVAO); glGenBuffers(1, &houseVBO);
    glBindVertexArray(houseVAO); glBindBuffer(GL_ARRAY_BUFFER, houseVBO);
    if (!houseLoadedVertices.empty()) {
        glBufferData(GL_ARRAY_BUFFER, houseLoadedVertices.size() * sizeof(Vertex), houseLoadedVertices.data(), GL_STATIC_DRAW);
    }
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0); glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(offsetof(Vertex, normal))); glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(offsetof(Vertex, texCoords))); glEnableVertexAttribArray(2);

    // ENGINE CARICAMENTO TEXTURE PNG
    unsigned int textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    
    sf::Image textureImage;
    if (textureImage.loadFromFile("../Cartella-risorse/texture.png")) {
        textureImage.flipVertically(); 
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, textureImage.getSize().x, textureImage.getSize().y, 0, GL_RGBA, GL_UNSIGNED_BYTE, textureImage.getPixelsPtr());
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    } else {
        std::cout << "Errore: Impossibile trovare o caricare la texture.png!" << std::endl;
    }

    sf::Clock deltaClock;
    bool isTimePaused = false; float currentSunAngle = 0.0f; float daySpeed = 0.1f;
    glm::vec3 skyDay(0.5f, 0.7f, 0.9f), skyGoldenHour(0.9f, 0.6f, 0.3f), skySunset(0.8f, 0.3f, 0.45f), skyTwilight(0.1f, 0.15f, 0.3f), skyNight(0.02f, 0.02f, 0.08f);

    while (window.isOpen()) {
        float deltaTime = deltaClock.restart().asSeconds();
        while (const std::optional<sf::Event> event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) window.close();
            if (const auto* keyPressed = event->getIf<sf::Event::KeyPressed>()) {
                if (keyPressed->scancode == sf::Keyboard::Scan::Escape) window.close();
                if (keyPressed->scancode == sf::Keyboard::Scan::Tab) {
                    isMouseGrabbed = !isMouseGrabbed; window.setMouseCursorGrabbed(isMouseGrabbed); window.setMouseCursorVisible(!isMouseGrabbed);
                    if (isMouseGrabbed) firstMouse = true;
                }
                if (keyPressed->scancode == sf::Keyboard::Scan::P) isTimePaused = !isTimePaused;
            }
        }

        if (isMouseGrabbed && window.hasFocus()) {
            sf::Vector2i windowCenter(static_cast<int>(window.getSize().x / 2), static_cast<int>(window.getSize().y / 2));
            sf::Vector2i mousePos = sf::Mouse::getPosition(window);
            float xoffset = static_cast<float>(mousePos.x - windowCenter.x); float yoffset = static_cast<float>(windowCenter.y - mousePos.y);
            if (xoffset != 0.0f || yoffset != 0.0f) {
                sf::Mouse::setPosition(windowCenter, window); 
                yaw -= xoffset * yawSensitivity; pitch += yoffset * pitchSensitivity;
                if (pitch > 89.0f) pitch = 89.0f; if (pitch < -89.0f) pitch = -89.0f;
                cameraFront = glm::normalize(glm::vec3(cos(glm::radians(yaw)) * cos(glm::radians(pitch)), sin(glm::radians(yaw)) * cos(glm::radians(pitch)), sin(glm::radians(pitch))));
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

        if (!isTimePaused) currentSunAngle += deltaTime * daySpeed;
        glm::vec3 currentLightDir = glm::normalize(glm::vec3(cos(currentSunAngle), -0.4f, sin(currentSunAngle)));
        float sunHeight = glm::clamp(currentLightDir.z, -1.0f, 1.0f);

        glm::vec3 currentHorizon, currentLightColor, currentAmbient;
        if (sunHeight > 0.3f) {
            currentHorizon = skyDay; currentLightColor = glm::vec3(1.0f, 0.95f, 0.9f); currentAmbient = glm::vec3(0.25f);
        } else if (sunHeight > 0.1f) {
            float t = glm::smoothstep(0.0f, 1.0f, (sunHeight - 0.1f) / 0.2f); currentHorizon = glm::mix(skyGoldenHour, skyDay, t); currentLightColor = glm::mix(glm::vec3(1.0f, 0.6f, 0.2f), glm::vec3(1.0f, 0.95f, 0.9f), t); currentAmbient = glm::mix(glm::vec3(0.2f), glm::vec3(0.25f), t);
        } else if (sunHeight > -0.05f) {
            float t = glm::smoothstep(0.0f, 1.0f, (sunHeight + 0.05f) / 0.15f); currentHorizon = glm::mix(skySunset, skyGoldenHour, t); currentLightColor = glm::mix(glm::vec3(0.8f, 0.2f, 0.1f), glm::vec3(1.0f, 0.6f, 0.2f), t); currentAmbient = glm::mix(glm::vec3(0.1f), glm::vec3(0.2f), t);
        } else if (sunHeight > -0.2f) {
            float t = glm::smoothstep(0.0f, 1.0f, (sunHeight + 0.2f) / 0.15f); currentHorizon = glm::mix(skyTwilight, skySunset, t); currentLightColor = glm::mix(glm::vec3(0.0f), glm::vec3(0.8f, 0.2f, 0.1f), t); currentAmbient = glm::mix(glm::vec3(0.05f), glm::vec3(0.1f), t);
        } else {
            currentHorizon = skyNight; currentLightColor = glm::vec3(0.0f); currentAmbient = glm::vec3(0.05f); 
        }

        float bivouacActivation = glm::smoothstep(0.1f, -0.1f, sunHeight); 
        glm::vec3 bivouacLightColor = glm::vec3(1.0f, 0.55f, 0.1f) * bivouacActivation * 2.0f; 

        // --- CALCOLO OFFSET E ALTEZZE CALIBRATE ---
        float verticalOffset = 0.005f; // Solleva il pivot dell'OBJ dal ghiacciaio
        glm::vec3 windowLightPos = housePos + glm::vec3(0.0f, 0.0f, verticalOffset + 0.004f); // Luce sollevata alle finestre

        // --- GESTIONE DINAMICA SCHERMO INTERO ED EVENTO DI RESIZE FORZATO ---
        sf::Vector2u currentSize = window.getSize();
        glViewport(0, 0, currentSize.x, currentSize.y); 

        glClearColor(0.1f, 0.1f, 0.1f, 1.0f); glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        float aspectRatio = static_cast<float>(currentSize.x) / static_cast<float>(currentSize.y);
        glm::mat4 projection = glm::perspective(glm::radians(45.0f), aspectRatio, 0.1f, 100.0f);
        glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);

        // 1. SKYBOX
        glDepthFunc(GL_LEQUAL); glUseProgram(skyboxProgram);
        glUniformMatrix4fv(glGetUniformLocation(skyboxProgram, "view"), 1, GL_FALSE, glm::value_ptr(glm::mat4(glm::mat3(view))));
        glUniformMatrix4fv(glGetUniformLocation(skyboxProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
        glUniform3fv(glGetUniformLocation(skyboxProgram, "horizonColor"), 1, glm::value_ptr(currentHorizon));
        glUniform3fv(glGetUniformLocation(skyboxProgram, "zenithColor"), 1, glm::value_ptr(currentHorizon * 0.4f));
        glUniform3fv(glGetUniformLocation(skyboxProgram, "lightDir"), 1, glm::value_ptr(currentLightDir));
        glUniform3fv(glGetUniformLocation(skyboxProgram, "sunColor"), 1, glm::value_ptr(currentLightColor));
        glBindVertexArray(skyboxVAO); glDrawArrays(GL_TRIANGLES, 0, 36); glDepthFunc(GL_LESS); 

        // 2. TERRENO
        glUseProgram(shaderProgram);
        glUniform3fv(glGetUniformLocation(shaderProgram, "lightDir"), 1, glm::value_ptr(currentLightDir));
        glUniform3fv(glGetUniformLocation(shaderProgram, "lightColor"), 1, glm::value_ptr(currentLightColor));
        glUniform3fv(glGetUniformLocation(shaderProgram, "ambientColor"), 1, glm::value_ptr(currentAmbient));
        glUniform3fv(glGetUniformLocation(shaderProgram, "pointLightPos"), 1, glm::value_ptr(windowLightPos));
        glUniform3fv(glGetUniformLocation(shaderProgram, "pointLightColor"), 1, glm::value_ptr(bivouacLightColor));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(glm::mat4(1.0f)));
        glBindVertexArray(VAO); glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indices.size()), GL_UNSIGNED_INT, 0);

        // 3. DISEGNO CASA OBJ CON SCALA 0.009f E FIX COMPENETRAZIONE
        if (!houseLoadedVertices.empty()) {
            glUseProgram(solidProgram);
            glm::mat4 houseModel = glm::mat4(1.0f);
            
            houseModel = glm::translate(houseModel, housePos + glm::vec3(0.0f, 0.0f, verticalOffset));
            houseModel = glm::rotate(houseModel, glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
            houseModel = glm::scale(houseModel, glm::vec3(0.009f)); // La tua misura perfetta validata!
            
            glUniformMatrix4fv(glGetUniformLocation(solidProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
            glUniformMatrix4fv(glGetUniformLocation(solidProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
            glUniformMatrix4fv(glGetUniformLocation(solidProgram, "model"), 1, GL_FALSE, glm::value_ptr(houseModel));
            glUniform3fv(glGetUniformLocation(solidProgram, "ambientColor"), 1, glm::value_ptr(currentAmbient));
            glUniform3fv(glGetUniformLocation(solidProgram, "lightDir"), 1, glm::value_ptr(currentLightDir));

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, textureID);
            glUniform1i(glGetUniformLocation(solidProgram, "texture_diffuse"), 0);

            glBindVertexArray(houseVAO);
            glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(houseLoadedVertices.size()));
        }

        window.display();
    }

    glDeleteVertexArrays(1, &VAO); glDeleteBuffers(1, &VBO); glDeleteBuffers(1, &EBO);
    glDeleteVertexArrays(1, &skyboxVAO); glDeleteBuffers(1, &skyboxVBO);
    glDeleteVertexArrays(1, &houseVAO); glDeleteBuffers(1, &houseVBO);
    glDeleteProgram(shaderProgram); glDeleteProgram(skyboxProgram); glDeleteProgram(solidProgram);

    return 0;
}