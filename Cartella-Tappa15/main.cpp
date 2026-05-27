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
#include <cctype>
#include "dem.hh"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// ==========================================
// STRUTTURE DATI BASE PER FRUSTUM CULLING E RENDERING
// Gestione spaziale del terreno suddiviso in chunk per ottimizzazione
// ==========================================
struct Vertex { glm::vec3 position; glm::vec3 normal; glm::vec2 texCoords; };
// AABB: AXIS-ALIGNED BOUNDING BOX per test di visibilità rapido e collisioni
struct AABB { glm::vec3 minP; glm::vec3 maxP; };

// CHUNK: Una porzione di terreno indipendente con buffer GPU e bounding box
struct Chunk { unsigned int VAO, VBO, EBO; int indexCount; AABB aabb; };
// PLANE: Rappresentazione di un piano nello spazio 3D
struct Plane { glm::vec3 normal; float distance; };

// FRUSTUM: La piramide di visione della telecamera composta da 6 piani
struct Frustum { Plane planes[6]; };

// ESTRAZIONE DEL FRUSTUM DALLA MATRICE DI PROIEZIONE*VISTA
// Converti la matrice di trasformazione composita in 6 piani geometrici
Frustum extractFrustum(const glm::mat4& vp) {
    Frustum f;
    // Estrazione di 6 piani dai componenti della matrice di proiezione
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 2; ++j) {
            float sign = (j == 0) ? 1.0f : -1.0f;
            f.planes[i*2+j].normal.x = vp[0][3] + sign * vp[0][i];
            f.planes[i*2+j].normal.y = vp[1][3] + sign * vp[1][i];
            f.planes[i*2+j].normal.z = vp[2][3] + sign * vp[2][i];
            f.planes[i*2+j].distance = vp[3][3] + sign * vp[3][i];
            // Normalizzazione del piano
            float length = glm::length(f.planes[i*2+j].normal);
            f.planes[i*2+j].normal /= length; f.planes[i*2+j].distance /= length;
        }
    }
    return f;
}

// TEST DI VISIBILITÀ: AABB vs FRUSTUM
// Controlla se una scatola di delimitazione interseca il frustum di visione
bool isAABBVisible(const AABB& aabb, const Frustum& f) {
    for (int i = 0; i < 6; ++i) {
        glm::vec3 p = aabb.minP;
        // Selezione dello spigolo più positivo dell'AABB rispetto al piano
        if (f.planes[i].normal.x >= 0.0f) p.x = aabb.maxP.x;
        if (f.planes[i].normal.y >= 0.0f) p.y = aabb.maxP.y;
        if (f.planes[i].normal.z >= 0.0f) p.z = aabb.maxP.z;
        // Se anche lo spigolo più positivo è dietro il piano, la scatola è invisibile
        if (glm::dot(f.planes[i].normal, p) + f.planes[i].distance < 0.0f) return false; 
    }
    return true; 
}

// ==========================================
// GENERATORE DI CARATTERI VETTORIALI PER L'HUD 2D (ASSET-FREE)
// Converte caratteri ASCII in segmenti di linea per rendering diretto su GPU
// ==========================================
void getSegmentsForChar(char c, std::vector<glm::vec2>& lines, float x, float y, float sx, float sy) {
    auto addL = [&](float x1, float y1, float x2, float y2) { lines.push_back(glm::vec2(x + x1 * sx, y + y1 * sy)); lines.push_back(glm::vec2(x + x2 * sx, y + y2 * sy)); };
    switch(c) {
        case '0': addL(0,0,1,0); addL(1,0,1,1); addL(1,1,0,1); addL(0,1,0,0); addL(0,0,1,1); break;
        case '1': addL(0.5,0,0.5,1); addL(0.2,0.8,0.5,1); break;
        case '2': addL(0,1,1,1); addL(1,1,1,0.5); addL(1,0.5,0,0.5); addL(0,0.5,0,0); addL(0,0,1,0); break;
        case '3': addL(0,1,1,1); addL(1,1,1,0); addL(1,0,0,0); addL(0,0.5,1,0.5); break;
        case '4': addL(0,1,0,0.5); addL(0,0.5,1,0.5); addL(1,1,1,0); break;
        case '5': addL(1,1,0,1); addL(0,1,0,0.5); addL(0,0.5,1,0.5); addL(1,0.5,1,0); addL(1,0,0,0); break;
        case '6': addL(1,1,0,1); addL(0,1,0,0); addL(0,0,1,0); addL(1,0,1,0.5); addL(1,0.5,0,0.5); break;
        case '7': addL(0,1,1,1); addL(1,1,1,0); break;
        case '8': addL(0,0,1,0); addL(1,0,1,1); addL(1,1,0,1); addL(0,1,0,0); addL(0,0.5,1,0.5); break;
        case '9': addL(0,0.5,1,0.5); addL(0,0.5,0,1); addL(0,1,1,1); addL(1,1,1,0); addL(1,0,0,0); break;
        case 'A': addL(0,0,0,1); addL(0,1,1,1); addL(1,1,1,0); addL(0,0.5,1,0.5); break;
        case 'B': addL(0,0,0,1); addL(0,1,0.8,1); addL(0.8,1,0.8,0.5); addL(0.8,0.5,0,0.5); addL(0.8,0.5,0.8,0); addL(0.8,0,0,0); break;
        case 'C': addL(1,1,0,1); addL(0,1,0,0); addL(0,0,1,0); break;
        case 'D': addL(0,0,0,1); addL(0,1,0.7,1); addL(0.7,1,1,0.7); addL(1,0.7,1,0.3); addL(1,0.3,0.7,0); addL(0.7,0,0,0); break;
        case 'E': addL(0,0,0,1); addL(0,1,1,1); addL(0,0.5,0.7,0.5); addL(0,0,1,0); break;
        case 'F': addL(0,0,0,1); addL(0,1,1,1); addL(0,0.5,0.7,0.5); break;
        case 'G': addL(1,1,0,1); addL(0,1,0,0); addL(0,0,1,0); addL(1,0,1,0.4); addL(1,0.4,0.5,0.4); break;
        case 'H': addL(0,0,0,1); addL(1,0,1,1); addL(0,0.5,1,0.5); break;
        case 'I': addL(0.5,0,0.5,1); addL(0.2,1,0.8,1); addL(0.2,0,0.8,0); break;
        case 'J': addL(0,0.2,0.3,0); addL(0.3,0,0.7,0); addL(0.7,0,0.7,1); break;
        case 'K': addL(0,0,0,1); addL(0,0.5,1,1); addL(0,0.5,1,0); break;
        case 'L': addL(0,1,0,0); addL(0,0,1,0); break;
        case 'M': addL(0,0,0,1); addL(0,1,0.5,0.5); addL(0.5,0.5,1,1); addL(1,1,1,0); break;
        case 'N': addL(0,0,0,1); addL(0,1,1,0); addL(1,0,1,1); break;
        case 'O': addL(0,0,1,0); addL(1,0,1,1); addL(1,1,0,1); addL(0,1,0,0); break;
        case 'P': addL(0,0,0,1); addL(0,1,1,1); addL(1,1,1,0.5); addL(1,0.5,0,0.5); break;
        case 'Q': addL(0,0,1,0); addL(1,0,1,1); addL(1,1,0,1); addL(0,1,0,0); addL(0.5,0.5,1,0); break;
        case 'R': addL(0,0,0,1); addL(0,1,1,1); addL(1,1,1,0.5); addL(1,0.5,0,0.5); addL(0,0.5,1,0); break;
        case 'S': addL(1,1,0,1); addL(0,1,0,0.5); addL(0,0.5,1,0.5); addL(1,0.5,1,0); addL(1,0,0,0); break;
        case 'T': addL(0,1,1,1); addL(0.5,1,0.5,0); break;
        case 'U': addL(0,1,0,0); addL(0,0,1,0); addL(1,0,1,1); break;
        case 'V': addL(0,1,0.5,0); addL(0.5,0,1,1); break;
        case 'W': addL(0,1,0,0); addL(0,0,0.5,0.4); addL(0.5,0.4,1,0); addL(1,0,1,1); break;
        case 'X': addL(0,0,1,1); addL(0,1,1,0); break;
        case 'Y': addL(0,1,0.5,0.5); addL(1,1,0.5,0.5); addL(0.5,0.5,0.5,0); break;
        case 'Z': addL(0,1,1,1); addL(1,1,0,0); addL(0,0,1,0); break;
        case ':': addL(0.5,0.7,0.5,0.75); addL(0.5,0.25,0.5,0.3); break;
        case '.': addL(0.4,0,0.6,0); addL(0.6,0,0.6,0.1); addL(0.6,0.1,0.4,0.1); break;
        case '-': addL(0,0.5,1,0.5); break;
        case '=': addL(0,0.6,1,0.6); addL(0,0.4,1,0.4); break;
        case ' ': break;
    }
}

// ==========================================
// VARIABILI GLOBALI DELLA TELECAMERA 
// Spawn spostato a sud estremo (Y=-2000) e altitudine altissima (Z=2500)
// per una panoramica totale della mappa e per testare l'illuminazione su grandi distanze
// ==========================================
glm::vec3 cameraPos = glm::vec3(0.0f, -2000.0f, 2500.0f);
glm::vec3 cameraFront = glm::vec3(0.0f, 1.0f, -0.2f);
glm::vec3 cameraUp    = glm::vec3(0.0f, 0.0f, 1.0f);
float yaw = 90.0f; float pitch = -10.0f; 
bool isMouseGrabbed = true; bool firstMouse = true;
// VELOCITÀ JET: 300 unità/sec. Indispensabile per navigare una mappa di 5km^2
float cameraSpeed = 300.0f; 
float yawSensitivity = 0.1f; float pitchSensitivity = 0.08f;

// ==========================================
// PARSER OBJ OTTIMIZZATO PER CARICAMENTO MODELLI 3D
// ==========================================
bool loadOBJ(const std::string& path, std::vector<Vertex>& out_vertices) {
    std::vector<glm::vec3> temp_vertices, temp_normals; std::vector<glm::vec2> temp_uvs;
    std::vector<unsigned int> vertexIndices, uvIndices, normalIndices;
    std::ifstream file(path);
    if (!file.is_open()) return false;
    std::string line;
    while (std::getline(file, line)) {
        std::stringstream ss(line); std::string header; ss >> header;
        if (header == "v") { glm::vec3 v; ss >> v.x >> v.y >> v.z; temp_vertices.push_back(v); }
        else if (header == "vt") { glm::vec2 uv; ss >> uv.x >> uv.y; temp_uvs.push_back(uv); }
        else if (header == "vn") { glm::vec3 n; ss >> n.x >> n.y >> n.z; temp_normals.push_back(n); }
        else if (header == "f") {
            std::string v1, v2, v3; ss >> v1 >> v2 >> v3;
            auto parseToken = [&](const std::string& token) {
                std::stringstream ts(token); std::string vi, vti, vni;
                std::getline(ts, vi, '/'); std::getline(ts, vti, '/'); std::getline(ts, vni, '/');
                vertexIndices.push_back(std::stoi(vi) - 1);
                if (!vti.empty()) uvIndices.push_back(std::stoi(vti) - 1);
                if (!vni.empty()) normalIndices.push_back(std::stoi(vni) - 1);
            };
            parseToken(v1); parseToken(v2); parseToken(v3);
        }
    }
    // Assemblaggio dei vertici finali
    for (size_t i = 0; i < vertexIndices.size(); i++) {
        Vertex v; v.position = temp_vertices[vertexIndices[i]];
        v.texCoords = uvIndices.empty() ? glm::vec2(0.0f) : temp_uvs[uvIndices[i]];
        v.normal = normalIndices.empty() ? glm::vec3(0.0f, 0.0f, 1.0f) : temp_normals[normalIndices[i]];
        out_vertices.push_back(v);
    }
    return true;
}

// ==========================================
// CODE SHADER SORGENTI - SCALA 5KM E ATMOSFERA RAREFATTA
// Tappa 15: Attenuazione luce spinta al limite e nebbia diradata per l'orizzonte a 8000 unità
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
uniform vec3 viewPos;
uniform vec3 fogColor;

void main() {
    // Illuminazione diffusa solare
    vec3 norm = normalize(Normal);
    float diffDir = max(dot(norm, normalize(lightDir)), 0.0);
    vec3 dirRes = lightColor * diffDir;
    
    // Illuminazione puntiforme bivacco
    vec3 lightDirPt = normalize(pointLightPos - FragPos);
    float diffPt = max(dot(norm, lightDirPt), 0.0);
    float distToLight = length(pointLightPos - FragPos);
    float att = 1.0 / (1.0 + 0.002 * distToLight + 0.0005 * (distToLight * distToLight));
    vec3 ptRes = pointLightColor * diffPt * att;
    
    // Colorazione per biomi: valle, roccia, neve
    vec3 cV = vec3(0.25, 0.45, 0.15);
    vec3 cR = vec3(0.28, 0.28, 0.28);
    vec3 cS = vec3(0.9, 0.95, 1.0);
    
    vec3 tC = (FragPos.z < 1400.0) ?
        mix(cV, cR, smoothstep(-50.0, 1400.0, FragPos.z)) :
        mix(cR, cS, smoothstep(1400.0, 2000.0, FragPos.z));
    
    vec3 finalTerrainColor = (ambientColor + dirRes + ptRes) * tC;

    // Nebbia atmosferica depth-based
    float fogDist = length(viewPos - FragPos);
    float fogDensity = 0.00025;
    float fogFactor = exp(-pow(fogDist * fogDensity, 2.0));
    fogFactor = clamp(fogFactor, 0.0, 1.0);
    
    FragColor = mix(vec4(fogColor, 1.0), vec4(finalTerrainColor, 1.0), fogFactor);
}
)";

// SHADER SKYBOX
const char* skyboxVertexShader = R"(
#version 410 core
layout (location = 0) in vec3 aPos;
out vec3 TexCoords;

uniform mat4 projection;
uniform mat4 view;

void main() {
    TexCoords = aPos;
    gl_Position = (projection * view * vec4(aPos, 1.0)).xyww;
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

float rand(vec3 co) {
    return fract(sin(dot(co, vec3(12.9898, 78.233, 45.164))) * 43758.5453);
}

void main() {
    vec3 dir = normalize(TexCoords);
    float t = clamp((dir.z + 0.2) / 0.8, 0.0, 1.0);
    vec3 bg = mix(horizonColor, zenithColor, t);
    
    float sD = dot(dir, normalize(lightDir));
    float sG = pow(max(sD, 0.0), 64.0);
    float sDi = smoothstep(0.995, 0.998, sD);
    float sV = smoothstep(-0.1, 0.0, lightDir.z);
    
    vec3 sC = vec3(0.0);
    float nV = smoothstep(0.1, -0.15, lightDir.z);
    if (nV > 0.0) {
        vec3 cP = floor(dir * 120.0);
        if (rand(cP) < 0.001) {
            sC = vec3(0.95, 0.98, 1.0) *
                 smoothstep(0.003, 0.0, length(dir - (cP + 0.5)/120.0)) *
                 nV * 1.5;
        }
    }
    
    FragColor = vec4(bg + (sunColor * sG * 0.6 * sV) + (sunColor * sDi * sV) + sC, 1.0);
}
)";

// ==========================================
// SHADER BIVACCO (SOLID) CON DEPTH FOG ALLINEATO
// ==========================================
const char* solidVertexShader = R"(
#version 410 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;

out vec3 FaceNormal;
out vec2 TexCoords;
out vec3 FragPos;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main() {
    FragPos = vec3(model * vec4(aPos, 1.0));
    FaceNormal = mat3(transpose(inverse(model))) * aNormal;
    TexCoords = aTexCoords;
    gl_Position = projection * view * vec4(FragPos, 1.0);
}
)";

const char* solidFragmentShader = R"(
#version 410 core
out vec4 FragColor;
in vec3 FaceNormal;
in vec2 TexCoords;
in vec3 FragPos;

uniform sampler2D tex;
uniform vec3 ambientColor;
uniform vec3 lightDir;
uniform vec3 viewPos;
uniform vec3 fogColor;

void main() {
    float diff = max(dot(normalize(FaceNormal), normalize(lightDir)), 0.0);
    vec3 objColor = (ambientColor + vec3(diff)) * texture(tex, TexCoords).rgb;
    
    // Nebbia atmosferica allineata al terreno
    float fogDist = length(viewPos - FragPos);
    float fogDensity = 0.00025;
    float fogFactor = exp(-pow(fogDist * fogDensity, 2.0));
    fogFactor = clamp(fogFactor, 0.0, 1.0);
    
    FragColor = mix(vec4(fogColor, 1.0), vec4(objColor, 1.0), fogFactor);
}
)";

// ==========================================
// SHADER HUD 2D - VISUALIZZAZIONE INFORMAZIONI SCHERMO
// ==========================================
const char* hudVertexShaderSource = R"(
#version 410 core
layout (location = 0) in vec2 aPos;

uniform vec2 screenSize;

void main() {
    float x = (aPos.x / screenSize.x) * 2.0 - 1.0;
    float y = (aPos.y / screenSize.y) * 2.0 - 1.0;
    gl_Position = vec4(x, y, 0.0, 1.0);
}
)";

const char* hudFragmentShaderSource = R"(
#version 410 core
out vec4 FragColor;

uniform vec3 textColor;

void main() {
    FragColor = vec4(textColor, 1.0);
}
)";

float skyboxVertices[] = {
    -1.0f, 1.0f, -1.0f, -1.0f, -1.0f, -1.0f, 1.0f, -1.0f, -1.0f, 1.0f, -1.0f, -1.0f, 1.0f, 1.0f, -1.0f, -1.0f, 1.0f, -1.0f,
    -1.0f, -1.0f, 1.0f, -1.0f, -1.0f, -1.0f, -1.0f, 1.0f, -1.0f, -1.0f, 1.0f, -1.0f, -1.0f, 1.0f, 1.0f, -1.0f, -1.0f, 1.0f,
     1.0f, -1.0f, -1.0f, 1.0f, -1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, -1.0f, 1.0f, -1.0f, -1.0f,
    -1.0f, -1.0f, 1.0f, -1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, -1.0f, 1.0f, -1.0f, -1.0f, 1.0f,
    -1.0f, 1.0f, -1.0f, 1.0f, 1.0f, -1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, -1.0f, 1.0f, 1.0f, -1.0f, 1.0f, -1.0f,
    -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, 1.0f, 1.0f, -1.0f, -1.0f, 1.0f, -1.0f, -1.0f, -1.0f, -1.0f, 1.0f, 1.0f, -1.0f, 1.0f
};

int main() {
    // ==========================================
    // 1. CARICAMENTO E SCALA DEL DEM
    // Tappa 15: MapScale spinto a 5000.0f. Stanno venendo renderizzati 5 chilometri quadrati.
    // ==========================================
    const char* filepath = "../Cartella-risorse/aletsch_32T.asc";
    Dem ghiacciaio(filepath);
    int W = ghiacciaio.header.width; int H = ghiacciaio.header.height;
    double zMin = ghiacciaio.min; double zMax = ghiacciaio.max;
    int step = 1; int cols = (W + step - 1) / step; int rows = (H + step - 1) / step;

    float mapScale = 5000.0f;

    // FASE 1: GENERAZIONE DELLA GRIGLIA DI POSIZIONI
    std::vector<std::vector<glm::vec3>> gridPositions(rows, std::vector<glm::vec3>(cols));
    std::vector<std::vector<glm::vec3>> gridNormals(rows, std::vector<glm::vec3>(cols));

    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            float xNdc = ((float)(c * step) / (W - 1)) - 0.5f;
            float yNdc = ((float)(r * step) / (H - 1)) - 0.5f;
            float zNorm = (ghiacciaio(c * step, r * step) - zMin) / (zMax - zMin);
            // Z-scale proporzionato a 0.5f per bilanciare l'enorme stiramento planare
            gridPositions[r][c] = glm::vec3(xNdc * mapScale, yNdc * mapScale, (zNorm * 0.5f) * mapScale);
        }
    }

    // FASE 2: CALCOLO DELLE NORMALI
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

    // ==========================================
    // GROUND CLAMPING (FUNZIONE ALTIMETRICA)
    // Interpolazione bilineare per altitudine precisa
    // ==========================================
    auto getTerrainHeight = [&](float worldX, float worldY) -> float {
        float normX = (worldX / mapScale) + 0.5f;
        float normY = (worldY / mapScale) + 0.5f;
        if (normX < 0.0f || normX >= 1.0f || normY < 0.0f || normY >= 1.0f) {
            return -1000.0f;
        }

        float gridX = normX * (cols - 1);
        float gridY = normY * (rows - 1);
        int x0 = (int)gridX;
        int x1 = std::min(x0 + 1, cols - 1);
        int y0 = (int)gridY;
        int y1 = std::min(y0 + 1, rows - 1);

        float tx = gridX - x0;
        float ty = gridY - y0;

        float z0 = glm::mix(gridPositions[y0][x0].z, gridPositions[y0][x1].z, tx);
        float z1 = glm::mix(gridPositions[y1][x0].z, gridPositions[y1][x1].z, tx);
        return glm::mix(z0, z1, ty);
    };

    sf::ContextSettings settings;
    settings.depthBits = 24;
    settings.majorVersion = 4;
    settings.minorVersion = 1;
    settings.attributeFlags = sf::ContextSettings::Core;

    sf::Window window(sf::VideoMode({1024, 768}), "Progetto FCG - LowPolyWorld",
                     sf::State::Windowed, settings);

    // VSYNC: sincronizzazione verticale per stabilizzare framerate
    window.setVerticalSyncEnabled(true);

    window.setMouseCursorGrabbed(isMouseGrabbed);
    window.setMouseCursorVisible(!isMouseGrabbed);
    gladLoadGLLoader(reinterpret_cast<GLADloadproc>(sf::Context::getFunction));
    glEnable(GL_DEPTH_TEST);

    // ==========================================
    // FASE 3: CREAZIONE CHUNK CON FRUSTUM CULLING
    // Suddivisione del terreno in chunk per ottimizzazione dello spazio
    // ==========================================
    std::vector<Chunk> terrainChunks;
    int CHUNK_SIZE = 64;

    for (int startY = 0; startY < rows - 1; startY += CHUNK_SIZE - 1) {
        for (int startX = 0; startX < cols - 1; startX += CHUNK_SIZE - 1) {
            int endY = std::min(startY + CHUNK_SIZE, rows);
            int endX = std::min(startX + CHUNK_SIZE, cols);

            std::vector<Vertex> chunkVertices;
            std::vector<unsigned int> chunkIndices;
            AABB chunkAABB = { glm::vec3(1e10f), glm::vec3(-1e10f) };

            // Raccolta vertici e calcolo bounding box
            for (int y = startY; y < endY; ++y) {
                for (int x = startX; x < endX; ++x) {
                    glm::vec3 pos = gridPositions[y][x];
                    chunkVertices.push_back({pos, gridNormals[y][x], glm::vec2(0.0f)});
                    chunkAABB.minP = glm::min(chunkAABB.minP, pos);
                    chunkAABB.maxP = glm::max(chunkAABB.maxP, pos);
                }
            }

            // Generazione indici triangolari
            int w = endX - startX;
            int h = endY - startY;
            for (int y = 0; y < h - 1; ++y) {
                for (int x = 0; x < w - 1; ++x) {
                    unsigned int tl = y * w + x;
                    chunkIndices.push_back(tl);
                    chunkIndices.push_back(tl + w);
                    chunkIndices.push_back(tl + 1);
                    chunkIndices.push_back(tl + 1);
                    chunkIndices.push_back(tl + w);
                    chunkIndices.push_back(tl + w + 1);
                }
            }

            // Allocazione buffer GPU
            Chunk chunk;
            chunk.indexCount = chunkIndices.size();
            chunk.aabb = chunkAABB;

            glGenVertexArrays(1, &chunk.VAO);
            glGenBuffers(1, &chunk.VBO);
            glGenBuffers(1, &chunk.EBO);

            glBindVertexArray(chunk.VAO);
            glBindBuffer(GL_ARRAY_BUFFER, chunk.VBO);
            glBufferData(GL_ARRAY_BUFFER, chunkVertices.size() * sizeof(Vertex),
                        chunkVertices.data(), GL_STATIC_DRAW);

            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, chunk.EBO);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, chunkIndices.size() * sizeof(unsigned int),
                        chunkIndices.data(), GL_STATIC_DRAW);

            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                                 (void*)(offsetof(Vertex, normal)));
            glEnableVertexAttribArray(1);

            terrainChunks.push_back(chunk);
        }
    }

    // COMPILAZIONE SHADER PROGRAMMI
    auto compile = [](unsigned int t, const char* s) {
        unsigned int sh = glCreateShader(t);
        glShaderSource(sh, 1, &s, NULL);
        glCompileShader(sh);
        return sh;
    };

    // Shader del terreno
    unsigned int shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, compile(GL_VERTEX_SHADER, vertexShaderSource));
    glAttachShader(shaderProgram, compile(GL_FRAGMENT_SHADER, fragmentShaderSource));
    glLinkProgram(shaderProgram);

    // Shader skybox
    unsigned int skyboxProgram = glCreateProgram();
    glAttachShader(skyboxProgram, compile(GL_VERTEX_SHADER, skyboxVertexShader));
    glAttachShader(skyboxProgram, compile(GL_FRAGMENT_SHADER, skyboxFragmentShader));
    glLinkProgram(skyboxProgram);

    // Shader bivacco
    unsigned int solidProgram = glCreateProgram();
    glAttachShader(solidProgram, compile(GL_VERTEX_SHADER, solidVertexShader));
    glAttachShader(solidProgram, compile(GL_FRAGMENT_SHADER, solidFragmentShader));
    glLinkProgram(solidProgram);

    // Shader HUD
    unsigned int hudProgram = glCreateProgram();
    glAttachShader(hudProgram, compile(GL_VERTEX_SHADER, hudVertexShaderSource));
    glAttachShader(hudProgram, compile(GL_FRAGMENT_SHADER, hudFragmentShaderSource));
    glLinkProgram(hudProgram);

    // ALLOCAZIONE BUFFER GPU - SKYBOX
    unsigned int skyboxVAO, skyboxVBO;
    glGenVertexArrays(1, &skyboxVAO);
    glGenBuffers(1, &skyboxVBO);
    glBindVertexArray(skyboxVAO);
    glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // BUFFER HUD DINAMICO PER RENDERING STRINGHE 2D
    unsigned int hudVAO, hudVBO;
    glGenVertexArrays(1, &hudVAO);
    glGenBuffers(1, &hudVBO);
    glBindVertexArray(hudVAO);
    glBindBuffer(GL_ARRAY_BUFFER, hudVBO);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(glm::vec2), (void*)0);
    glEnableVertexAttribArray(0);

    // POSIZIONAMENTO DEL BIVACCO SULLA MAPPA
    glm::vec3 housePos = gridPositions[(int)(rows * 0.60f)][(int)(cols * 0.55f)];
    std::vector<Vertex> houseLoadedVertices;
    loadOBJ("../Cartella-risorse/bivacco.obj", houseLoadedVertices);

    unsigned int houseVAO, houseVBO;
    glGenVertexArrays(1, &houseVAO);
    glGenBuffers(1, &houseVBO);
    glBindVertexArray(houseVAO);
    glBindBuffer(GL_ARRAY_BUFFER, houseVBO);
    if (!houseLoadedVertices.empty()) {
        glBufferData(GL_ARRAY_BUFFER, houseLoadedVertices.size() * sizeof(Vertex),
                     houseLoadedVertices.data(), GL_STATIC_DRAW);
    }

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                         (void*)(offsetof(Vertex, normal)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                         (void*)(offsetof(Vertex, texCoords)));
    glEnableVertexAttribArray(2);

    // CARICAMENTO TEXTURE PNG
    unsigned int texID;
    glGenTextures(1, &texID);
    glBindTexture(GL_TEXTURE_2D, texID);
    sf::Image texImg;
    if (texImg.loadFromFile("../Cartella-risorse/texture.png")) {
        texImg.flipVertically();
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texImg.getSize().x, texImg.getSize().y,
                     0, GL_RGBA, GL_UNSIGNED_BYTE, texImg.getPixelsPtr());
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }

    // ==========================================
    // DEFINIZIONE VOLUME SOLIDO BIVACCO (AABB WORLD SPACE) - SCALA 15.0f
    // L'hitbox è stata dilatata (raggi +/-15) per abbracciare l'ingrandimento del modello
    // e chiudere le compenetrazioni dei muri perimetrali.
    // ==========================================
    AABB houseCollisionAABB;
    float verticalOffsetBivacco = 25.0f; // Ancoraggio altimetrico del modello
    houseCollisionAABB.minP = housePos + glm::vec3(-15.0f, -15.0f, -10.0f);
    houseCollisionAABB.maxP = housePos + glm::vec3( 15.0f,  15.0f,  30.0f);


    // SETUP VARIABILI TEMPORALI E COLORI CIELO
    sf::Clock deltaClock;
    bool isTimePaused = false; float currentSunAngle = 0.0f; float daySpeed = 0.1f;
    glm::vec3 skyDay(0.5, 0.7, 0.9), skyGold(0.9, 0.6, 0.3), skySun(0.8, 0.3, 0.45), skyTwi(0.1, 0.15, 0.3), skyNight(0.02, 0.02, 0.08);

    // VARIABILI FPS E TELEMETRIA
    float fpsTimer = 0.0f; int frameCount = 0; int displayFPS = 0;

    // ==========================================
    // FUNZIONE DI SUPPORTO PER DISEGNARE STRINGHE HUD CON CARATTERI VETTORIALI
    // Converte una stringa in segmenti di linea e li renderizza su GPU
    // ==========================================
    auto drawHUDString = [&](const std::string& text, float startX, float startY, float cw, float ch, float spacing, glm::vec3 color) {
        std::vector<glm::vec2> lines; float curX = startX;
        for (char c : text) { getSegmentsForChar(std::toupper(c), lines, curX, startY, cw, ch); curX += cw + spacing; }
        if (lines.empty()) return;
        glUseProgram(hudProgram); glUniform2f(glGetUniformLocation(hudProgram, "screenSize"), (float)window.getSize().x, (float)window.getSize().y); glUniform3fv(glGetUniformLocation(hudProgram, "textColor"), 1, glm::value_ptr(color));
        glBindVertexArray(hudVAO); glBindBuffer(GL_ARRAY_BUFFER, hudVBO); glBufferData(GL_ARRAY_BUFFER, lines.size() * sizeof(glm::vec2), lines.data(), GL_DYNAMIC_DRAW);
        glDisable(GL_DEPTH_TEST); glLineWidth(2.0f); glDrawArrays(GL_LINES, 0, (GLsizei)lines.size()); glEnable(GL_DEPTH_TEST);
    };

    // ==========================================
    // GAME LOOP PRINCIPALE
    // ==========================================
    while (window.isOpen()) {
        float deltaTime = deltaClock.restart().asSeconds();
        
        // ELABORAZIONE DEGLI EVENTI
        while (const std::optional<sf::Event> event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) window.close();
            if (const auto* key = event->getIf<sf::Event::KeyPressed>()) {
                if (key->scancode == sf::Keyboard::Scan::Escape) window.close();
                if (key->scancode == sf::Keyboard::Scan::Tab) { isMouseGrabbed = !isMouseGrabbed; window.setMouseCursorGrabbed(isMouseGrabbed); window.setMouseCursorVisible(!isMouseGrabbed); if(isMouseGrabbed) firstMouse=true; }
                if (key->scancode == sf::Keyboard::Scan::P) isTimePaused = !isTimePaused;
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
                    cos(glm::radians(yaw)) * cos(glm::radians(pitch)),
                    sin(glm::radians(yaw)) * cos(glm::radians(pitch)),
                    sin(glm::radians(pitch))
                ));
            }

            // Movimento della telecamera
            float vel = cameraSpeed * deltaTime;
            glm::vec3 right = glm::normalize(glm::cross(cameraFront, cameraUp));

            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Scan::W))
                cameraPos += cameraFront * vel;
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Scan::S))
                cameraPos -= cameraFront * vel;
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Scan::A))
                cameraPos -= right * vel;
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Scan::D))
                cameraPos += right * vel;
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Scan::Space))
                cameraPos += cameraUp * vel;
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Scan::LShift))
                cameraPos -= cameraUp * vel;
        }

        // ========================================
        // GESTIONE COLLISIONI - BIVACCO
        // Calcolo push-back per evitare compenetrazioni
        // ========================================
        float playerRadius = 2.0f;
        AABB& box = houseCollisionAABB;

        if (cameraPos.x >= box.minP.x - playerRadius && cameraPos.x <= box.maxP.x + playerRadius &&
            cameraPos.y >= box.minP.y - playerRadius && cameraPos.y <= box.maxP.y + playerRadius &&
            cameraPos.z >= box.minP.z && cameraPos.z <= box.maxP.z) {

            // Calcolo penetrazioni sui 4 muri laterali
            float pMinX = cameraPos.x - (box.minP.x - playerRadius);
            float pMaxX = (box.maxP.x + playerRadius) - cameraPos.x;
            float pMinY = cameraPos.y - (box.minP.y - playerRadius);
            float pMaxY = (box.maxP.y + playerRadius) - cameraPos.y;

            float minDist = std::min({pMinX, pMaxX, pMinY, pMaxY});
            float ep = 0.05f;

            if (minDist == pMinX)
                cameraPos.x = box.minP.x - playerRadius - ep;
            else if (minDist == pMaxX)
                cameraPos.x = box.maxP.x + playerRadius + ep;
            else if (minDist == pMinY)
                cameraPos.y = box.minP.y - playerRadius - ep;
            else if (minDist == pMaxY)
                cameraPos.y = box.maxP.y + playerRadius + ep;
        }

        // ========================================
        // GROUND CLAMPING - VINCOLO AL TERRENO
        // ========================================
        float terrainZ = getTerrainHeight(cameraPos.x, cameraPos.y);
        if (cameraPos.z < terrainZ + 1.78f) {
            cameraPos.z = terrainZ + 1.78f;
        }

        // ========================================
        // CALCOLO DEL CICLO GIORNO/NOTTE
        // ========================================
        if (!isTimePaused) {
            currentSunAngle += deltaTime * daySpeed;
        }
        glm::vec3 lDir = glm::normalize(glm::vec3(
            cos(currentSunAngle), -0.4f, sin(currentSunAngle)
        ));
        float sH = glm::clamp(lDir.z, -1.0f, 1.0f);

        glm::vec3 curH, curL, curA;
        if (sH > 0.3f) {
            curH = skyDay;
            curL = glm::vec3(1, 0.95, 0.9);
            curA = glm::vec3(0.25);
        } else if (sH > 0.1f) {
            float t = glm::smoothstep(0.0f, 1.0f, (sH - 0.1f) / 0.2f);
            curH = mix(skyGold, skyDay, t);
            curL = mix(glm::vec3(1, 0.6, 0.2), glm::vec3(1, 0.95, 0.9), t);
            curA = mix(glm::vec3(0.2), glm::vec3(0.25), t);
        } else if (sH > -0.05f) {
            float t = glm::smoothstep(0.0f, 1.0f, (sH + 0.05f) / 0.15f);
            curH = mix(skySun, skyGold, t);
            curL = mix(glm::vec3(0.8, 0.2, 0.1), glm::vec3(1, 0.6, 0.2), t);
            curA = mix(glm::vec3(0.1), glm::vec3(0.2), t);
        } else if (sH > -0.2f) {
            float t = glm::smoothstep(0.0f, 1.0f, (sH + 0.2f) / 0.15f);
            curH = mix(skyTwi, skySun, t);
            curL = mix(glm::vec3(0), glm::vec3(0.8, 0.2, 0.1), t);
            curA = mix(glm::vec3(0.05), glm::vec3(0.1), t);
        } else {
            curH = skyNight;
            curL = glm::vec3(0);
            curA = glm::vec3(0.05);
        }

        float bivAct = glm::smoothstep(0.1f, -0.1f, sH);
        glm::vec3 bivColor = glm::vec3(1.0, 0.55, 0.1) * bivAct * 3.0f;

        float verticalOffset = 10.0f;
        glm::vec3 wLightPos = housePos + glm::vec3(0, 0, verticalOffset + 30.0f); 

        // SETUP MATRICI DI TRASFORMAZIONE
        glViewport(0, 0, window.getSize().x, window.getSize().y);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Matrici di proiezione vista (far plane a 8000 unità per scala 5km)
        glm::mat4 projection = glm::perspective(
            glm::radians(45.0f),
            (float)window.getSize().x / window.getSize().y,
            0.1f, 8000.0f
        );
        glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
        glm::mat4 viewProj = projection * view; 

        // ========================================
        // RENDERING SKYBOX
        // ========================================
        glDepthFunc(GL_LEQUAL);
        glUseProgram(skyboxProgram);
        glUniformMatrix4fv(glGetUniformLocation(skyboxProgram, "view"), 1, GL_FALSE,
                          glm::value_ptr(glm::mat4(glm::mat3(view))));
        glUniformMatrix4fv(glGetUniformLocation(skyboxProgram, "projection"), 1, GL_FALSE,
                          glm::value_ptr(projection));
        glUniform3fv(glGetUniformLocation(skyboxProgram, "horizonColor"), 1, glm::value_ptr(curH));
        glUniform3fv(glGetUniformLocation(skyboxProgram, "zenithColor"), 1, glm::value_ptr(curH * 0.4f));
        glUniform3fv(glGetUniformLocation(skyboxProgram, "lightDir"), 1, glm::value_ptr(lDir));
        glUniform3fv(glGetUniformLocation(skyboxProgram, "sunColor"), 1, glm::value_ptr(curL));
        glBindVertexArray(skyboxVAO);
        glDrawArrays(GL_TRIANGLES, 0, 36);
        glDepthFunc(GL_LESS); 

        // ========================================
        // RENDERING TERRENO CON FRUSTUM CULLING
        // ========================================
        glUseProgram(shaderProgram);
        glUniform3fv(glGetUniformLocation(shaderProgram, "lightDir"), 1, glm::value_ptr(lDir));
        glUniform3fv(glGetUniformLocation(shaderProgram, "lightColor"), 1, glm::value_ptr(curL));
        glUniform3fv(glGetUniformLocation(shaderProgram, "ambientColor"), 1, glm::value_ptr(curA));
        glUniform3fv(glGetUniformLocation(shaderProgram, "pointLightPos"), 1, glm::value_ptr(wLightPos));
        glUniform3fv(glGetUniformLocation(shaderProgram, "pointLightColor"), 1, glm::value_ptr(bivColor));
        glUniform3fv(glGetUniformLocation(shaderProgram, "viewPos"), 1, glm::value_ptr(cameraPos));
        glUniform3fv(glGetUniformLocation(shaderProgram, "fogColor"), 1, glm::value_ptr(curH));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(glm::mat4(1.0f)));

        // Estrazione frustum e rendering chunk visibili
        Frustum cameraFrustum = extractFrustum(viewProj);
        for (const auto& chunk : terrainChunks) {
            if (isAABBVisible(chunk.aabb, cameraFrustum)) {
                glBindVertexArray(chunk.VAO);
                glDrawElements(GL_TRIANGLES, chunk.indexCount, GL_UNSIGNED_INT, 0);
            }
        }

        // ========================================
        // RENDERING BIVACCO (MODELLO 3D SOLIDO)
        // ========================================
        if (!houseLoadedVertices.empty()) {
            glUseProgram(solidProgram);

            // Trasformazione: traslazione + rotazione + scalatura
            glm::mat4 hModel = glm::translate(glm::mat4(1.0f),
                                            housePos + glm::vec3(0, 0, verticalOffset));
            hModel = glm::rotate(hModel, glm::radians(90.0f), glm::vec3(1, 0, 0));
            hModel = glm::scale(hModel, glm::vec3(15.0f));

            // Uniform per nebbia allineata al terreno
            glUniform3fv(glGetUniformLocation(solidProgram, "viewPos"), 1, glm::value_ptr(cameraPos));
            glUniform3fv(glGetUniformLocation(solidProgram, "fogColor"), 1, glm::value_ptr(curH));
            glUniformMatrix4fv(glGetUniformLocation(solidProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
            glUniformMatrix4fv(glGetUniformLocation(solidProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
            glUniformMatrix4fv(glGetUniformLocation(solidProgram, "model"), 1, GL_FALSE, glm::value_ptr(hModel));
            glUniform3fv(glGetUniformLocation(solidProgram, "ambientColor"), 1, glm::value_ptr(curA));
            glUniform3fv(glGetUniformLocation(solidProgram, "lightDir"), 1, glm::value_ptr(lDir));

            // Binding texture
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, texID);
            glUniform1i(glGetUniformLocation(solidProgram, "texture_diffuse"), 0);

            glBindVertexArray(houseVAO);
            glDrawArrays(GL_TRIANGLES, 0, (GLsizei)houseLoadedVertices.size());
        }

        // ========================================
        // RENDERING HUD - TELEMETRIA 2D
        // ========================================
        fpsTimer += deltaTime;
        frameCount++;
        if (fpsTimer >= 0.5f) {
            displayFPS = (int)(frameCount / fpsTimer);
            frameCount = 0;
            fpsTimer = 0.0f;
        }

        // Calcolo fase della giornata
        float wrappedAngle = fmod(currentSunAngle, 2.0f * 3.14159265f);
        if (wrappedAngle < 0.0f) wrappedAngle += 2.0f * 3.14159265f;
        std::string momentoGiornata = "NOTTE";
        if (wrappedAngle >= 0.0f && wrappedAngle < 3.14159265f) {
            float daytimeProgress = wrappedAngle / 3.14159265f;
            if (daytimeProgress < 0.20f)
                momentoGiornata = "MATTINA";
            else if (daytimeProgress >= 0.20f && daytimeProgress < 0.45f)
                momentoGiornata = "MEZZOGIORNO";
            else if (daytimeProgress >= 0.45f && daytimeProgress < 0.75f)
                momentoGiornata = "POMERIGGIO";
            else
                momentoGiornata = "SERA";
        }

        std::string fpsText = "FPS: " + std::to_string(displayFPS);
        std::string timeText = "FASE: " + momentoGiornata;
        std::string gpsText = "GPS: X=" + std::to_string((int)cameraPos.x) +
                             " Y=" + std::to_string((int)cameraPos.y) +
                             " Z=" + std::to_string((int)cameraPos.z);
        std::string pauseText = isTimePaused ? "TEMPO: IN PAUSA" : "TEMPO: SCORRE";

        float hudTopMargin = (float)window.getSize().y - 30.0f;
        glm::vec3 hudColor = glm::vec3(1.0f, 0.6f, 0.0f);

        drawHUDString(fpsText, 20.0f, hudTopMargin, 12.0f, 18.0f, 4.0f, hudColor);
        drawHUDString(timeText, 20.0f, hudTopMargin - 30.0f, 12.0f, 18.0f, 4.0f, hudColor);
        drawHUDString(gpsText, 20.0f, hudTopMargin - 60.0f, 12.0f, 18.0f, 4.0f, hudColor);
        drawHUDString(pauseText, 20.0f, hudTopMargin - 90.0f, 12.0f, 18.0f, 4.0f, hudColor);

        window.display();
    }

    // CLEANUP CHUNK E BUFFER MEMORIA
    for (auto& chunk : terrainChunks) { glDeleteVertexArrays(1, &chunk.VAO); glDeleteBuffers(1, &chunk.VBO); glDeleteBuffers(1, &chunk.EBO); }
    glDeleteVertexArrays(1, &skyboxVAO); glDeleteBuffers(1, &skyboxVBO);
    glDeleteVertexArrays(1, &houseVAO); glDeleteBuffers(1, &houseVBO);
    glDeleteVertexArrays(1, &hudVAO); glDeleteBuffers(1, &hudVBO);
    glDeleteProgram(shaderProgram); glDeleteProgram(skyboxProgram); glDeleteProgram(solidProgram); glDeleteProgram(hudProgram);
    return 0;
}