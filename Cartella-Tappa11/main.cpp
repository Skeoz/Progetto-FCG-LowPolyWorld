#include <glad/glad.h>
#include <SFML/Window.hpp>
#include <SFML/Graphics/Image.hpp> 
#include <iostream>
#include <vector>
#include <fstream>
#include <sstream>
#include <string>
#include <optional>
#include <algorithm>
#include "dem.hh"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// DEFINIZIONE DELLE STRUTTURE DATI BASE E FRUSTUM CULLING
// Questa sezione contiene le strutture per l'ottimizzazione spaziale del rendering:
// - Vertex: dati di ogni vertice della mesh (posizione, normale, UV)
// - AABB: scatola di delimitazione assiale per i chunk di terreno
// - Chunk: una "piastrella" indipendente di terreno con buffer GPU e bounding box
// - Plane e Frustum: rappresentazione matematica della piramide di visione della telecamera

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texCoords; 
};

// AABB: AXIS-ALIGNED BOUNDING BOX
// Scatola di delimitazione cubica con spigoli paralleli agli assi dello spazio mondo
// Usata per test di visibilità rapido durante il Frustum Culling
struct AABB {
    glm::vec3 minP;  // Spigolo inferiore della scatola
    glm::vec3 maxP;  // Spigolo superiore della scatola
};

// CHUNK: UNA PIASTRELLA DI TERRENO INDIPENDENTE
// Suddivisione spaziale del terreno totale in tasselli gestibili
// Ogni chunk contiene propri buffer GPU (VAO, VBO, EBO) per il rendering selettivo
struct Chunk {
    unsigned int VAO, VBO, EBO;  // Buffer GPU per il chunk
    int indexCount;               // Numero di indici (triangoli * 3) nel chunk
    AABB aabb;                    // Bounding box del chunk per test di visibilità
};

// PLANE: PIANO MATEMATICO
// Rappresentazione di un piano nello spazio 3D utilizzato nel Frustum culling
// Equazione: dot(normal, point) + distance = 0
struct Plane {
    glm::vec3 normal;    // Vettore normale al piano
    float distance;      // Distanza dall'origine
};

// FRUSTUM: PIRAMIDE DI VISIONE
// Rappresenta la regione di spazio visibile dalla telecamera
// Composta da 6 piani: sinistra, destra, basso, alto, vicino (near), lontano (far)
struct Frustum {
    Plane planes[6];
};

// ESTRAZIONE DEL FRUSTUM DALLA MATRICE DI PROIEZIONE*VISTA
// Converte la matrice di trasformazione composita in 6 piani geometrici
// Questi piani definiscono l'esatto volume di spazio visibile dalla telecamera
Frustum extractFrustum(const glm::mat4& vp) {
    Frustum f;
    // Estrazione di 6 piani dagli ultimi assi della matrice di proiezione
    // Indici: 0-1 asse X (left/right), 2-3 asse Y (bottom/top), 4-5 asse Z (near/far)
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 2; ++j) {
            float sign = (j == 0) ? 1.0f : -1.0f;
            f.planes[i*2+j].normal.x = vp[0][3] + sign * vp[0][i];
            f.planes[i*2+j].normal.y = vp[1][3] + sign * vp[1][i];
            f.planes[i*2+j].normal.z = vp[2][3] + sign * vp[2][i];
            f.planes[i*2+j].distance = vp[3][3] + sign * vp[3][i];
            
            // Normalizzazione del piano (lunghezza del vettore normale = 1.0)
            float length = glm::length(f.planes[i*2+j].normal);
            f.planes[i*2+j].normal /= length;
            f.planes[i*2+j].distance /= length;
        }
    }
    return f;
}

// TEST DI VISIBILITÀ: AABB vs FRUSTUM
// Controlla se una scatola di delimitazione (AABB) interseca il frustum di visione
// Restituisce false se la scatola è completamente fuori da qualsiasi piano del frustum
bool isAABBVisible(const AABB& aabb, const Frustum& f) {
    // Ciclo su tutti i 6 piani del frustum
    for (int i = 0; i < 6; ++i) {
        // Selezione dello spigolo "più positivo" dell'AABB rispetto alla normale del piano
        // Questo spigolo ha la massima distanza dal piano (se visibile)
        glm::vec3 p = aabb.minP;
        if (f.planes[i].normal.x >= 0.0f) p.x = aabb.maxP.x;
        if (f.planes[i].normal.y >= 0.0f) p.y = aabb.maxP.y;
        if (f.planes[i].normal.z >= 0.0f) p.z = aabb.maxP.z;

        // Test di distanza: se anche lo spigolo più positivo è dietro il piano, la scatola è invisibile
        if (glm::dot(f.planes[i].normal, p) + f.planes[i].distance < 0.0f) {
            return false;  // La scatola è completamente fuori da questo piano
        }
    }
    return true;  // La scatola è visibile (interseca il frustum)
}

// VARIABILI GLOBALI DELLA TELECAMERA
// Parametri per la posizione, orientamento e controllo interattivo della telecamera
glm::vec3 cameraPos   = glm::vec3(0.0f, -1.2f, 0.6f); 
glm::vec3 cameraFront = glm::vec3(0.0f, 1.0f, -0.4f);
glm::vec3 cameraUp    = glm::vec3(0.0f, 0.0f, 1.0f);
float yaw = 90.0f; 
float pitch = -20.0f; 
bool isMouseGrabbed = true; 
bool firstMouse = true;
float cameraSpeed = 0.5f; 
float yawSensitivity = 0.1f; 
float pitchSensitivity = 0.08f;

// PARSER OBJ OTTIMIZZATO PER CARICAMENTO MODELLI 3D
// Legge file Wavefront OBJ e converte i dati in un vettore di vertici indicizzati
bool loadOBJ(const std::string& path, std::vector<Vertex>& out_vertices) {
    // Vettori temporanei per i dati grezzi del file OBJ
    std::vector<glm::vec3> temp_vertices, temp_normals; 
    std::vector<glm::vec2> temp_uvs;
    std::vector<unsigned int> vertexIndices, uvIndices, normalIndices;
    // Apertura del file OBJ
    std::ifstream file(path);
    if (!file.is_open()) return false;
    
    // Lettura riga per riga del file OBJ
    std::string line;
    while (std::getline(file, line)) {
        std::stringstream ss(line); 
        std::string header; 
        ss >> header;
        
        // Parsing dei vari tipi di elemento OBJ
        if (header == "v") { 
            glm::vec3 v; ss >> v.x >> v.y >> v.z; temp_vertices.push_back(v); 
        }
        else if (header == "vt") { 
            glm::vec2 uv; ss >> uv.x >> uv.y; temp_uvs.push_back(uv); 
        }
        else if (header == "vn") { 
            glm::vec3 n; ss >> n.x >> n.y >> n.z; temp_normals.push_back(n); 
        }
        else if (header == "f") {
            // Parsing di una faccia triangolare
            std::string v1, v2, v3; 
            ss >> v1 >> v2 >> v3;
            
            // Lambda per parsing di token "v/vt/vn"
            auto parseToken = [&](const std::string& token) {
                std::stringstream ts(token); 
                std::string vi, vti, vni;
                std::getline(ts, vi, '/'); 
                std::getline(ts, vti, '/'); 
                std::getline(ts, vni, '/');
                vertexIndices.push_back(std::stoi(vi) - 1);
                if (!vti.empty()) uvIndices.push_back(std::stoi(vti) - 1);
                if (!vni.empty()) normalIndices.push_back(std::stoi(vni) - 1);
            };
            parseToken(v1); 
            parseToken(v2); 
            parseToken(v3);
        }
    }
    
    // Assemblaggio dei vertici finali
    for (size_t i = 0; i < vertexIndices.size(); i++) {
        Vertex v; 
        v.position = temp_vertices[vertexIndices[i]];
        v.texCoords = uvIndices.empty() ? glm::vec2(0.0f) : temp_uvs[uvIndices[i]];
        v.normal = normalIndices.empty() ? glm::vec3(0.0f, 0.0f, 1.0f) : temp_normals[normalIndices[i]];
        out_vertices.push_back(v);
    }
    return true;
}

// DEFINIZIONE DEGLI SHADER SOURCES
// Programmi GLSL compilati a runtime per il pipeline di rendering

// Vertex Shader: Trasformazione di posizioni e normali, pass-through delle coordinate
const char* vertexShaderSource = R"(#version 410 core
layout (location=0) in vec3 aPos; layout (location=1) in vec3 aNormal;
out vec3 FragPos; out vec3 Normal; uniform mat4 model; uniform mat4 view; uniform mat4 projection;
void main() { FragPos = vec3(model * vec4(aPos, 1.0)); Normal = mat3(transpose(inverse(model))) * aNormal; gl_Position = projection * view * vec4(FragPos, 1.0); })";;

// Fragment Shader: Illuminazione ibrida (solare + puntiforme) e colorazione procedurale del terreno
const char* fragmentShaderSource = R"(#version 410 core
out vec4 FragColor; in vec3 FragPos; in vec3 Normal;
uniform vec3 lightDir; uniform vec3 lightColor; uniform vec3 ambientColor;
uniform vec3 pointLightPos; uniform vec3 pointLightColor;
void main() {
    vec3 norm = normalize(Normal);
    float diffDir = max(dot(norm, normalize(lightDir)), 0.0); vec3 dirRes = lightColor * diffDir; 
    vec3 lightDirPt = normalize(pointLightPos - FragPos); float diffPt = max(dot(norm, lightDirPt), 0.0); float dist = length(pointLightPos - FragPos);
    float att = 1.0 / (1.0 + 25.0 * dist + 180.0 * (dist * dist)); vec3 ptRes = pointLightColor * diffPt * att;
    vec3 cV = vec3(0.25, 0.45, 0.15); vec3 cR = vec3(0.45, 0.43, 0.4); vec3 cS = vec3(0.9, 0.95, 1.0);   
    vec3 tC = (FragPos.z < 0.02) ? mix(cV, cR, smoothstep(-0.2, 0.02, FragPos.z)) : mix(cR, cS, smoothstep(0.02, 0.12, FragPos.z));
    FragColor = vec4((ambientColor + dirRes + ptRes) * tC, 1.0);
})";

// Skybox Vertex Shader: Mantiene il cielo concentrico alla telecamera
const char* skyboxVertexShader = R"(#version 410 core
layout (location=0) in vec3 aPos; out vec3 TexCoords; uniform mat4 projection; uniform mat4 view;
void main() { TexCoords = aPos; gl_Position = (projection * view * vec4(aPos, 1.0)).xyww; })";

// Skybox Fragment Shader: Disegna cielo graduale e sole con stelle procedurali
const char* skyboxFragmentShader = R"(#version 410 core
out vec4 FragColor; in vec3 TexCoords; uniform vec3 lightDir; uniform vec3 horizonColor; uniform vec3 zenithColor; uniform vec3 sunColor;
float rand(vec3 co) { return fract(sin(dot(co, vec3(12.9898, 78.233, 45.164))) * 43758.5453); }
void main() {
    vec3 dir = normalize(TexCoords); float t = clamp((dir.z + 0.2) / 0.8, 0.0, 1.0); vec3 bg = mix(horizonColor, zenithColor, t);
    float sD = dot(dir, normalize(lightDir)); float sG = pow(max(sD, 0.0), 64.0); float sDi = smoothstep(0.995, 0.998, sD); float sV = smoothstep(-0.1, 0.0, lightDir.z);
    vec3 sC = vec3(0.0); float nV = smoothstep(0.1, -0.15, lightDir.z);
    if(nV > 0.0) { vec3 cP = floor(dir * 120.0); if(rand(cP) < 0.001) { sC = vec3(0.95, 0.98, 1.0) * smoothstep(0.003, 0.0, length(dir - (cP + 0.5)/120.0)) * nV * 1.5; } }
    FragColor = vec4(bg + (sunColor * sG * 0.6 * sV) + (sunColor * sDi * sV) + sC, 1.0);
})";

// Solid Vertex Shader: Trasformazione con texture mapping per modelli importati
const char* solidVertexShader = R"(#version 410 core
layout (location=0) in vec3 aPos; layout (location=1) in vec3 aNormal; layout (location=2) in vec2 aTexCoords;
out vec3 FaceNormal; out vec2 TexCoords; uniform mat4 model; uniform mat4 view; uniform mat4 projection;
void main() { FaceNormal = mat3(transpose(inverse(model))) * aNormal; TexCoords = aTexCoords; gl_Position = projection * view * model * vec4(aPos, 1.0); })";

// Solid Fragment Shader: Illuminazione e texture diffuse per il bivacco
const char* solidFragmentShader = R"(#version 410 core
out vec4 FragColor; in vec3 FaceNormal; in vec2 TexCoords; uniform sampler2D tex; uniform vec3 ambientColor; uniform vec3 lightDir;
void main() { float diff = max(dot(normalize(FaceNormal), normalize(lightDir)), 0.0); FragColor = vec4((ambientColor + vec3(diff)) * texture(tex, TexCoords).rgb, 1.0); })";

// COORDINATE DEL CUBO 3D PER LO SKYBOX
float skyboxVertices[] = {
    -1.0f,  1.0f, -1.0f, -1.0f, -1.0f, -1.0f,  1.0f, -1.0f, -1.0f, 1.0f, -1.0f, -1.0f,  1.0f,  1.0f, -1.0f, -1.0f,  1.0f, -1.0f,
    -1.0f, -1.0f,  1.0f, -1.0f, -1.0f, -1.0f, -1.0f,  1.0f, -1.0f, -1.0f,  1.0f, -1.0f, -1.0f,  1.0f,  1.0f, -1.0f, -1.0f,  1.0f,
     1.0f, -1.0f, -1.0f,  1.0f, -1.0f,  1.0f,  1.0f,  1.0f,  1.0f, 1.0f,  1.0f,  1.0f,  1.0f,  1.0f, -1.0f,  1.0f, -1.0f, -1.0f,
    -1.0f, -1.0f,  1.0f, -1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f, 1.0f,  1.0f,  1.0f,  1.0f, -1.0f,  1.0f, -1.0f, -1.0f,  1.0f,
    -1.0f,  1.0f, -1.0f,  1.0f,  1.0f, -1.0f,  1.0f,  1.0f,  1.0f, 1.0f,  1.0f,  1.0f, -1.0f,  1.0f,  1.0f, -1.0f,  1.0f, -1.0f,
    -1.0f, -1.0f, -1.0f, -1.0f, -1.0f,  1.0f,  1.0f, -1.0f, -1.0f, 1.0f, -1.0f, -1.0f, -1.0f, -1.0f,  1.0f,  1.0f, -1.0f,  1.0f
};

int main() {
    // CARICAMENTO DEL MODELLO DIGITALE DI ELEVAZIONE (DEM)
    // Lettura dei dati topografici da file ASCII per la ricostruzione 3D del terreno
    const char* filepath = "../Cartella-risorse/aletsch_32T.asc";
    Dem ghiacciaio(filepath);
    int W = ghiacciaio.header.width; 
    int H = ghiacciaio.header.height;
    double zMin = ghiacciaio.min; 
    double zMax = ghiacciaio.max;
    int step = 1; 
    int cols = (W + step - 1) / step; 
    int rows = (H + step - 1) / step;

    // FASE 1: GENERAZIONE DELLA GRIGLIA DI POSIZIONI E NORMALI
    // Mappatura delle coordinate pixel del DEM nello spazio NDC 3D
    std::vector<std::vector<glm::vec3>> gridPositions(rows, std::vector<glm::vec3>(cols));
    std::vector<std::vector<glm::vec3>> gridNormals(rows, std::vector<glm::vec3>(cols));

    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            float xNdc = ((float)(c * step) / (W - 1)) - 0.5f;
            float yNdc = ((float)(r * step) / (H - 1)) - 0.5f;
            float zNorm = (ghiacciaio(c * step, r * step) - zMin) / (zMax - zMin);
            gridPositions[r][c] = glm::vec3(xNdc, yNdc, (zNorm * 0.4f) - 0.2f);
        }
    }

    // FASE 2: CALCOLO DELLE NORMALI CON PRODOTTO VETTORIALE
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            glm::vec3 pos = gridPositions[r][c];
            glm::vec3 tX(1.0f, 0.0f, 0.0f), tY(0.0f, 1.0f, 0.0f);
            if (c > 0 && c < cols - 1) tX = gridPositions[r][c+1] - gridPositions[r][c-1];
            if (r > 0 && r < rows - 1) tY = gridPositions[r+1][c] - gridPositions[r-1][c];
            glm::vec3 normal = glm::normalize(glm::cross(tX, tY));
            if (normal.z < 0.0f) normal = -normal;
            gridNormals[r][c] = normal;
        }
    }

    // CONFIGURAZIONE DEL CONTESTO OPENGL
    sf::ContextSettings settings; 
    settings.depthBits = 24; 
    settings.majorVersion = 4; 
    settings.minorVersion = 1; 
    settings.attributeFlags = sf::ContextSettings::Core;
    
    sf::Window window(sf::VideoMode({1024, 768}), "Progetto FCG - LowPolyWorld", sf::State::Windowed, settings);
    window.setMouseCursorGrabbed(isMouseGrabbed); 
    window.setMouseCursorVisible(!isMouseGrabbed);
    
    gladLoadGLLoader(reinterpret_cast<GLADloadproc>(sf::Context::getFunction));
    glEnable(GL_DEPTH_TEST);

    // CREAZIONE E SUDDIVISIONE DEI CHUNK (L'ELEMENTO CHIAVE DI TAPPA 11)
    // Suddivisione del terreno totale in tasselli per il Frustum Culling
    // Ogni chunk contiene propri buffer GPU e AABB per test di visibilità rapido
    std::vector<Chunk> terrainChunks;
    int CHUNK_SIZE = 64;  // Numero di vertici per lato del chunk (area = CHUNK_SIZE * CHUNK_SIZE)

    // Creazione dei chunk iterando sulla griglia in blocchi
    for (int startY = 0; startY < rows - 1; startY += CHUNK_SIZE - 1) {
        for (int startX = 0; startX < cols - 1; startX += CHUNK_SIZE - 1) {
            
            // Calcolo dei limiti del chunk
            int endY = std::min(startY + CHUNK_SIZE, rows);
            int endX = std::min(startX + CHUNK_SIZE, cols);

            // Raccolta dei vertici e indici per questo chunk
            std::vector<Vertex> chunkVertices;
            std::vector<unsigned int> chunkIndices;
            // Inizializzazione AABB con valori estremi (min massimo, max minimo)
            AABB chunkAABB = { glm::vec3(1e10f), glm::vec3(-1e10f) };

            // Raccolta dei vertici del chunk e aggiornamento della AABB
            for (int y = startY; y < endY; ++y) {
                for (int x = startX; x < endX; ++x) {
                    glm::vec3 pos = gridPositions[y][x];
                    chunkVertices.push_back({pos, gridNormals[y][x], glm::vec2(0.0f)});
                    // Aggiornamento dei limiti della AABB
                    chunkAABB.minP = glm::min(chunkAABB.minP, pos);
                    chunkAABB.maxP = glm::max(chunkAABB.maxP, pos);
                }
            }

            // Generazione della topologia (indici) del chunk
            int w = endX - startX;
            int h = endY - startY;
            for (int y = 0; y < h - 1; ++y) {
                for (int x = 0; x < w - 1; ++x) {
                    unsigned int tl = y * w + x;
                    // Due triangoli per ogni quad
                    chunkIndices.push_back(tl); chunkIndices.push_back(tl + w); chunkIndices.push_back(tl + 1);
                    chunkIndices.push_back(tl + 1); chunkIndices.push_back(tl + w); chunkIndices.push_back(tl + w + 1);
                }
            }

            // Allocazione dei buffer GPU per il chunk
            Chunk chunk;
            chunk.indexCount = chunkIndices.size();
            chunk.aabb = chunkAABB;
            
            glGenVertexArrays(1, &chunk.VAO); 
            glGenBuffers(1, &chunk.VBO); 
            glGenBuffers(1, &chunk.EBO);
            
            glBindVertexArray(chunk.VAO);
            glBindBuffer(GL_ARRAY_BUFFER, chunk.VBO); 
            glBufferData(GL_ARRAY_BUFFER, chunkVertices.size() * sizeof(Vertex), chunkVertices.data(), GL_STATIC_DRAW);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, chunk.EBO); 
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, chunkIndices.size() * sizeof(unsigned int), chunkIndices.data(), GL_STATIC_DRAW);
            
            // Configurazione degli attributi di vertice
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0); 
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(offsetof(Vertex, normal))); 
            glEnableVertexAttribArray(1);
            
            terrainChunks.push_back(chunk);
        }
    }
    std::cout << "Terreno suddiviso in " << terrainChunks.size() << " chunks per il Frustum Culling." << std::endl;

    // COMPILAZIONE E LINKING DEI PROGRAMMI SHADER
    // Lambda helper per compilazione veloce degli shader
    auto compile = [](unsigned int t, const char* s) { 
        unsigned int sh = glCreateShader(t); 
        glShaderSource(sh, 1, &s, NULL); 
        glCompileShader(sh); 
        return sh; 
    };
    
    // Creazione dei programmi shader (terreno, skybox, bivacco)
    unsigned int shaderProgram = glCreateProgram(); 
    glAttachShader(shaderProgram, compile(GL_VERTEX_SHADER, vertexShaderSource)); 
    glAttachShader(shaderProgram, compile(GL_FRAGMENT_SHADER, fragmentShaderSource)); 
    glLinkProgram(shaderProgram);
    
    unsigned int skyboxProgram = glCreateProgram(); 
    glAttachShader(skyboxProgram, compile(GL_VERTEX_SHADER, skyboxVertexShader)); 
    glAttachShader(skyboxProgram, compile(GL_FRAGMENT_SHADER, skyboxFragmentShader)); 
    glLinkProgram(skyboxProgram);
    
    unsigned int solidProgram = glCreateProgram(); 
    glAttachShader(solidProgram, compile(GL_VERTEX_SHADER, solidVertexShader)); 
    glAttachShader(solidProgram, compile(GL_FRAGMENT_SHADER, solidFragmentShader)); 
    glLinkProgram(solidProgram);

    // ALLOCAZIONE BUFFER GPU - SKYBOX
    unsigned int skyboxVAO, skyboxVBO; 
    glGenVertexArrays(1, &skyboxVAO); 
    glGenBuffers(1, &skyboxVBO); 
    glBindVertexArray(skyboxVAO); 
    glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO); 
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW); 
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0); 
    glEnableVertexAttribArray(0);

    // CARICAMENTO E POSIZIONAMENTO DEL BIVACCO (CASA OBJ)
    glm::vec3 housePos = gridPositions[(int)(rows * 0.60f)][(int)(cols * 0.55f)]; 
    housePos.z += 0.001f;
    
    std::vector<Vertex> houseLoadedVertices; 
    loadOBJ("../Cartella-risorse/bivacco.obj", houseLoadedVertices);
    
    // Allocazione buffer GPU per il bivacco
    unsigned int houseVAO, houseVBO; 
    glGenVertexArrays(1, &houseVAO); 
    glGenBuffers(1, &houseVBO); 
    glBindVertexArray(houseVAO); 
    glBindBuffer(GL_ARRAY_BUFFER, houseVBO);
    if (!houseLoadedVertices.empty()) 
        glBufferData(GL_ARRAY_BUFFER, houseLoadedVertices.size() * sizeof(Vertex), houseLoadedVertices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0); 
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(offsetof(Vertex, normal))); 
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(offsetof(Vertex, texCoords))); 
    glEnableVertexAttribArray(2);

    // CARICAMENTO E CONFIGURAZIONE DELLA TEXTURE PNG
    unsigned int texID; 
    glGenTextures(1, &texID); 
    glBindTexture(GL_TEXTURE_2D, texID);
    sf::Image texImg; 
    if (texImg.loadFromFile("../Cartella-risorse/texture.png")) { 
        texImg.flipVertically(); 
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texImg.getSize().x, texImg.getSize().y, 0, GL_RGBA, GL_UNSIGNED_BYTE, texImg.getPixelsPtr()); 
        glGenerateMipmap(GL_TEXTURE_2D); 
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT); 
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT); 
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR); 
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR); 
    }

    // GAME LOOP CON SISTEMA GIORNO/NOTTE DINAMICO
    // Gestione del tempo, evoluzione della posizione solare e Frustum Culling nel render
    sf::Clock deltaClock, fpsClock;
    bool isTimePaused = false; 
    float currentSunAngle = 0.0f; 
    float daySpeed = 0.1f;
    
    // Palette di colori per il cielo
    glm::vec3 skyDay(0.5, 0.7, 0.9), skyGold(0.9, 0.6, 0.3), skySun(0.8, 0.3, 0.45), skyTwi(0.1, 0.15, 0.3), skyNight(0.02, 0.02, 0.08);

    while (window.isOpen()) {
        float deltaTime = deltaClock.restart().asSeconds();
        
        // ELABORAZIONE DEGLI EVENTI
        while (const std::optional<sf::Event> event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) 
                window.close();
            if (const auto* key = event->getIf<sf::Event::KeyPressed>()) {
                if (key->scancode == sf::Keyboard::Scan::Escape) 
                    window.close();
                if (key->scancode == sf::Keyboard::Scan::Tab) { 
                    isMouseGrabbed = !isMouseGrabbed; 
                    window.setMouseCursorGrabbed(isMouseGrabbed); 
                    window.setMouseCursorVisible(!isMouseGrabbed); 
                    if(isMouseGrabbed) firstMouse=true; 
                }
                if (key->scancode == sf::Keyboard::Scan::P) 
                    isTimePaused = !isTimePaused;
            }
        }

        // CONTROLLI TELECAMERA INTERATTIVA
        if (isMouseGrabbed && window.hasFocus()) {
            sf::Vector2i center(window.getSize().x / 2, window.getSize().y / 2);
            sf::Vector2i mPos = sf::Mouse::getPosition(window);
            float xo = mPos.x - center.x; 
            float yo = center.y - mPos.y;
            if (xo != 0 || yo != 0) {
                sf::Mouse::setPosition(center, window); 
                yaw -= xo * yawSensitivity; 
                pitch += yo * pitchSensitivity;
                if (pitch > 89.0f) pitch = 89.0f; 
                if (pitch < -89.0f) pitch = -89.0f;
                cameraFront = glm::normalize(glm::vec3(
                    cos(glm::radians(yaw))*cos(glm::radians(pitch)), 
                    sin(glm::radians(yaw))*cos(glm::radians(pitch)), 
                    sin(glm::radians(pitch))
                ));
            }
            float vel = cameraSpeed * deltaTime; 
            glm::vec3 right = glm::normalize(glm::cross(cameraFront, cameraUp));
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Scan::W)) cameraPos += cameraFront * vel; 
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Scan::S)) cameraPos -= cameraFront * vel;
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Scan::A)) cameraPos -= right * vel; 
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Scan::D)) cameraPos += right * vel;
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Scan::Space)) cameraPos += cameraUp * vel; 
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Scan::LShift)) cameraPos -= cameraUp * vel;
        }

        // CALCOLO DEL CICLO GIORNO/NOTTE DINAMICO
        if (!isTimePaused) 
            currentSunAngle += deltaTime * daySpeed;
        
        glm::vec3 lDir = glm::normalize(glm::vec3(cos(currentSunAngle), -0.4f, sin(currentSunAngle))); 
        float sH = glm::clamp(lDir.z, -1.0f, 1.0f);
        
        // Calcolo dei parametri di illuminazione e cielo
        glm::vec3 curH, curL, curA;
        if (sH > 0.3f) { 
            curH=skyDay; curL=glm::vec3(1,0.95,0.9); curA=glm::vec3(0.25); 
        }
        else if (sH > 0.1f) { 
            float t=glm::smoothstep(0.0f,1.0f,(sH-0.1f)/0.2f); 
            curH=mix(skyGold,skyDay,t); 
            curL=mix(glm::vec3(1,0.6,0.2),glm::vec3(1,0.95,0.9),t); 
            curA=mix(glm::vec3(0.2),glm::vec3(0.25),t); 
        }
        else if (sH > -0.05f) { 
            float t=glm::smoothstep(0.0f,1.0f,(sH+0.05f)/0.15f); 
            curH=mix(skySun,skyGold,t); 
            curL=mix(glm::vec3(0.8,0.2,0.1),glm::vec3(1,0.6,0.2),t); 
            curA=mix(glm::vec3(0.1),glm::vec3(0.2),t); 
        }
        else if (sH > -0.2f) { 
            float t=glm::smoothstep(0.0f,1.0f,(sH+0.2f)/0.15f); 
            curH=mix(skyTwi,skySun,t); 
            curL=mix(glm::vec3(0),glm::vec3(0.8,0.2,0.1),t); 
            curA=mix(glm::vec3(0.05),glm::vec3(0.1),t); 
        }
        else { 
            curH=skyNight; curL=glm::vec3(0); curA=glm::vec3(0.05); 
        }

        // Sistema di attivazione del bivacco
        float bivAct = glm::smoothstep(0.1f, -0.1f, sH); 
        glm::vec3 bivColor = glm::vec3(1.0, 0.55, 0.1) * bivAct * 2.0f;
        glm::vec3 wLightPos = housePos + glm::vec3(0, 0, 0.009f); 

        glViewport(0, 0, window.getSize().x, window.getSize().y);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f); 
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Costruzione delle matrici di trasformazione
        glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)window.getSize().x / window.getSize().y, 0.1f, 100.0f);
        glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
        // MATRICE FONDAMENTALE PER IL FRUSTUM CULLING: viewProj = projection * view
        glm::mat4 viewProj = projection * view;

        // FASE 1: DISEGNO DELLO SKYBOX
        glDepthFunc(GL_LEQUAL); 
        glUseProgram(skyboxProgram);
        glUniformMatrix4fv(glGetUniformLocation(skyboxProgram, "view"), 1, GL_FALSE, glm::value_ptr(glm::mat4(glm::mat3(view))));
        glUniformMatrix4fv(glGetUniformLocation(skyboxProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
        glUniform3fv(glGetUniformLocation(skyboxProgram, "horizonColor"), 1, glm::value_ptr(curH)); 
        glUniform3fv(glGetUniformLocation(skyboxProgram, "zenithColor"), 1, glm::value_ptr(curH * 0.4f));
        glUniform3fv(glGetUniformLocation(skyboxProgram, "lightDir"), 1, glm::value_ptr(lDir)); 
        glUniform3fv(glGetUniformLocation(skyboxProgram, "sunColor"), 1, glm::value_ptr(curL));
        glBindVertexArray(skyboxVAO); 
        glDrawArrays(GL_TRIANGLES, 0, 36); 
        glDepthFunc(GL_LESS);

        // FASE 2: DISEGNO DEL TERRENO CON FRUSTUM CULLING
        // LA PARTE PIU' IMPORTANTE DI TAPPA 11: Rendering selettivo dei chunk visibili
        glUseProgram(shaderProgram);
        glUniform3fv(glGetUniformLocation(shaderProgram, "lightDir"), 1, glm::value_ptr(lDir));
        glUniform3fv(glGetUniformLocation(shaderProgram, "lightColor"), 1, glm::value_ptr(curL));
        glUniform3fv(glGetUniformLocation(shaderProgram, "ambientColor"), 1, glm::value_ptr(curA));
        glUniform3fv(glGetUniformLocation(shaderProgram, "pointLightPos"), 1, glm::value_ptr(wLightPos));
        glUniform3fv(glGetUniformLocation(shaderProgram, "pointLightColor"), 1, glm::value_ptr(bivColor));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(glm::mat4(1.0f)));

        // Estrazione del Frustum dalla matrice di proiezione*vista
        Frustum cameraFrustum = extractFrustum(viewProj);
        int chunksDrawn = 0;

        // CICLO PRINCIPALE DEL FRUSTUM CULLING
        // Itera su tutti i chunk e disegna SOLO quelli che intersecano il frustum di visione
        // Questa è l'ottimizzazione che rende Tappa11 molto più efficiente
        for (const auto& chunk : terrainChunks) {
            // Test di visibilità: controlla se l'AABB del chunk interseca il frustum
            if (isAABBVisible(chunk.aabb, cameraFrustum)) {
                glBindVertexArray(chunk.VAO);
                glDrawElements(GL_TRIANGLES, chunk.indexCount, GL_UNSIGNED_INT, 0);
                chunksDrawn++;
            }
        }

        // OUTPUT DI DEBUG: Stampa il numero di chunk disegnati ogni secondo
        // Utile per visualizzare il beneficio del Frustum Culling in azione
        if (fpsClock.getElapsedTime().asSeconds() >= 1.0f) {
            std::cout << "Chunks disegnati: " << chunksDrawn << " su " << terrainChunks.size() << std::endl;
            fpsClock.restart();
        }

        // FASE 3: DISEGNO DEL BIVACCO (CASA OBJ)
        if (!houseLoadedVertices.empty()) {
            glUseProgram(solidProgram);
            glm::mat4 hModel = glm::translate(glm::mat4(1.0f), housePos + glm::vec3(0, 0, 0.005f));
            hModel = glm::rotate(hModel, glm::radians(90.0f), glm::vec3(1, 0, 0)); 
            hModel = glm::scale(hModel, glm::vec3(0.009f)); 
            glUniformMatrix4fv(glGetUniformLocation(solidProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
            glUniformMatrix4fv(glGetUniformLocation(solidProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
            glUniformMatrix4fv(glGetUniformLocation(solidProgram, "model"), 1, GL_FALSE, glm::value_ptr(hModel));
            glUniform3fv(glGetUniformLocation(solidProgram, "ambientColor"), 1, glm::value_ptr(curA)); 
            glUniform3fv(glGetUniformLocation(solidProgram, "lightDir"), 1, glm::value_ptr(lDir));
            glActiveTexture(GL_TEXTURE0); 
            glBindTexture(GL_TEXTURE_2D, texID); 
            glUniform1i(glGetUniformLocation(solidProgram, "texture_diffuse"), 0);
            glBindVertexArray(houseVAO); 
            glDrawArrays(GL_TRIANGLES, 0, (GLsizei)houseLoadedVertices.size());
        }
        window.display();
    }
    return 0;
}