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

// STRUTTURA DATI PER I VERTICI CON TEXTURE MAPPING
// Estensione della struttura di base con coordinate UV per il texture mapping
// Position: coordinata 3D nello spazio modello
// Normal: vettore normale per l'illuminazione Phong
// TexCoords: coordinate UV per campionare la texture 2D (intervallo 0.0-1.0 per ogni asse)
struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texCoords; 
};

// VARIABILI GLOBALI DELLA TELECAMERA
// Parametri che descrivono la posizione, orientamento e comportamento interattivo della telecamera
glm::vec3 cameraPos   = glm::vec3(0.0f, -1.2f, 0.6f); 
glm::vec3 cameraFront = glm::vec3(0.0f, 1.0f, -0.4f);
glm::vec3 cameraUp    = glm::vec3(0.0f, 0.0f, 1.0f);

// Angoli di rotazione (yaw-pitch) per il controllo dell'orientamento
float yaw   = 90.0f;  
float pitch = -20.0f; 
bool firstMouse = true;
bool isMouseGrabbed = true; 

// Parametri di movimento e sensibilità dell'input mouse
float cameraSpeed = 0.5f;
float yawSensitivity   = 0.1f;
float pitchSensitivity = 0.08f;

// PARSER OBJ OTTIMIZZATO PER CARICAMENTO MODELLI 3D
// Legge file Wavefront OBJ e converte i dati in un vettore di vertici indicizzati
// Il parser gestisce: vertici (v), coordinate UV (vt), normali (vn), facce triangolari (f)
// Supporta il formato "v/vt/vn" per facce con tutti gli attributi
bool loadOBJ(const std::string& path, std::vector<Vertex>& out_vertices) {
    // Array temporanei per memorizzare i dati grezzi dal file OBJ
    std::vector<glm::vec3> temp_vertices;    // Posizioni non indicizzate
    std::vector<glm::vec3> temp_normals;     // Normali non indicizzate
    std::vector<glm::vec2> temp_uvs;         // Coordinate UV non indicizzate
    std::vector<unsigned int> vertexIndices, uvIndices, normalIndices;  // Indici per accesso ai dati

    temp_vertices.reserve(5000);
    temp_normals.reserve(5000);
    temp_uvs.reserve(5000);
    vertexIndices.reserve(15000);

    // Apertura del file OBJ con gestione degli errori
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "ERRORE: Impossibile aprire il file OBJ in: " << path << std::endl;
        return false;
    }

    // Lettura riga per riga del file OBJ
    std::string line;
    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string header;
        ss >> header;  // Legge il tipo di elemento (v, vt, vn, f, etc.)

        // PARSING VERTICI (v)
        if (header == "v") {
            glm::vec3 vertex;
            ss >> vertex.x >> vertex.y >> vertex.z;
            temp_vertices.push_back(vertex);
        } 
        // PARSING COORDINATE UV (vt)
        else if (header == "vt") {
            glm::vec2 uv;
            ss >> uv.x >> uv.y;
            temp_uvs.push_back(uv);
        } 
        // PARSING NORMALI (vn)
        else if (header == "vn") {
            glm::vec3 normal;
            ss >> normal.x >> normal.y >> normal.z;
            temp_normals.push_back(normal);
        } 
        // PARSING FACCE TRIANGOLARI (f)
        else if (header == "f") {
            // Lettura dei tre vertici di una faccia triangolare
            std::string v1, v2, v3;
            ss >> v1 >> v2 >> v3;

            // Lambda per parsificare un token di faccia nel formato "v/vt/vn"
            auto parseFaceToken = [&](const std::string& token) {
                std::stringstream tokenStream(token);
                std::string vIdx, vtIdx, vnIdx;
                // Separazione degli indici usando '/' come delimitatore
                std::getline(tokenStream, vIdx, '/');
                std::getline(tokenStream, vtIdx, '/');
                std::getline(tokenStream, vnIdx, '/');
                
                // Memorizzazione degli indici (OBJ usa indici da 1, convertiamo a 0)
                vertexIndices.push_back(std::stoi(vIdx) - 1);
                if (!vtIdx.empty()) uvIndices.push_back(std::stoi(vtIdx) - 1);
                if (!vnIdx.empty()) normalIndices.push_back(std::stoi(vnIdx) - 1);
            };

            // Parsing dei tre vertici della faccia
            parseFaceToken(v1);
            parseFaceToken(v2);
            parseFaceToken(v3);
        }
    }

    // ASSEMBLAGGIO DEI VERTICI FINALI
    // Conversione dai dati grezzi (separati per tipo) a vertici indicizzati (position + normal + UV)
    out_vertices.reserve(vertexIndices.size());
    for (size_t i = 0; i < vertexIndices.size(); i++) {
        Vertex v;
        // Assegnazione della posizione dal vettore grezzo
        v.position = temp_vertices[vertexIndices[i]];
        // Assegnazione delle coordinate UV (con fallback a 0.0 se non presenti)
        if (!uvIndices.empty()) v.texCoords = temp_uvs[uvIndices[i]];
        else v.texCoords = glm::vec2(0.0f, 0.0f);
        // Assegnazione della normale (con fallback a (0,0,1) se non presenti)
        if (!normalIndices.empty()) v.normal = temp_normals[normalIndices[i]];
        else v.normal = glm::vec3(0.0f, 0.0f, 1.0f);
        out_vertices.push_back(v);
    }
    return true;
}

// DEFINIZIONE DEGLI SHADER SOURCES
// Programmi GLSL compilati a runtime per il pipeline di rendering

// SHADER DEL TERRENO CON ILLUMINAZIONE SOLARE E LUCE PUNTIFORME
// Supporta sia la luce direzionale del sole che una luce puntiforme aggiuntiva (bivacco)
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

// Fragment Shader: Illuminazione ibrida (solare + puntiforme) con colorazione procedurale
const char* fragmentShaderSource = R"(
    #version 410 core
    out vec4 FragColor;
    in vec3 FragPos;
    in vec3 Normal;
    
    // Parametri di illuminazione solare dinamica
    uniform vec3 lightDir;
    uniform vec3 lightColor;
    uniform vec3 ambientColor;
    // Parametri della luce puntiforme (bivacco)
    uniform vec3 pointLightPos;
    uniform vec3 pointLightColor;
    
    void main() {
        vec3 norm = normalize(Normal);
        
        // ILLUMINAZIONE DIREZIONALE (SOLE)
        // Calcolo della componente diffusa dal sole
        float diffDir = max(dot(norm, normalize(lightDir)), 0.0);
        vec3 directionalResult = lightColor * diffDir; 

        // ILLUMINAZIONE PUNTIFORME (LUCE DEL BIVACCO)
        // Calcolo della direzione dalla luce al frammento
        vec3 lightDirPt = normalize(pointLightPos - FragPos);
        // Componente diffusa della luce puntiforme
        float diffPt = max(dot(norm, lightDirPt), 0.0);
        // Calcolo della distanza dal frammento alla sorgente luminosa
        float distance = length(pointLightPos - FragPos);
        
        // ATTENUAZIONE DELLA LUCE PUNTIFORME
        // Formula quadratica di attenuazione: attenuation = 1.0 / (constant + linear*d + quadratic*d²)
        // Questo modella il decadimento realistico della luce in uno spazio 3D
        float constant = 1.0;      // Termine costante (evita divisione per zero)
        float linear = 25.0;       // Coefficiente lineare (attenuazione media)
        float quadratic = 180.0;   // Coefficiente quadratico (attenuazione in lontananza)
        float attenuation = 1.0 / (constant + linear * distance + quadratic * (distance * distance));
        // Applicazione dell'attenuazione al risultato della luce puntiforme
        vec3 pointResult = pointLightColor * diffPt * attenuation;

        // COLORAZIONE PROCEDURALE DEL TERRENO BASATA SU ALTITUDINE
        vec3 colorValley = vec3(0.25f, 0.45f, 0.15f);  // Verde scuro per le valli
        vec3 colorRock   = vec3(0.45f, 0.43f, 0.4f);   // Grigio per le rocce
        vec3 colorSnow   = vec3(0.9f, 0.95f, 1.0f);    // Bianco per la neve

        // Selezione del colore del bioma in base all'altitudine del frammento
        vec3 terrainColor;
        if (FragPos.z < 0.02) {
            // Transizione dalla valle alle rocce
            terrainColor = mix(colorValley, colorRock, smoothstep(-0.2, 0.02, FragPos.z));
        } else {
            // Transizione dalle rocce alla neve
            terrainColor = mix(colorRock, colorSnow, smoothstep(0.02, 0.12, FragPos.z));
        }

        // COMBINAZIONE FINALE: illuminazione totale (ambiente + solare + puntiforme) applicata al colore del bioma
        vec3 finalLighting = (ambientColor + directionalResult + pointResult) * terrainColor;
        FragColor = vec4(finalLighting, 1.0);
    }
)";

// SHADER DELLO SKYBOX CON EFFETTI PROCEDURALI
// Disegna la sfera celeste con gradiente e sole dinamico
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

// SHADER DEL BIVACCO (CASA) CON TEXTURE DIFFUSA
// Shader specializzato per il rendering di oggetti importati con texture mapping
// Supporta illuminazione solare e texture diffuse per dettagli superficiali
const char* solidVertexShader = R"(
    #version 410 core
    layout (location = 0) in vec3 aPos;
    layout (location = 1) in vec3 aNormal;
    layout (location = 2) in vec2 aTexCoords;  // Coordinate UV dal file OBJ
    
    out vec3 FaceNormal;
    out vec2 TexCoords;
    
    uniform mat4 model;
    uniform mat4 view;
    uniform mat4 projection;
    
    void main() {
        // Trasformazione della normale nello spazio mondo (corretta per scaling non uniforme)
        FaceNormal = mat3(transpose(inverse(model))) * aNormal;
        // Pass-through delle coordinate UV al fragment shader
        TexCoords = aTexCoords;
        // Trasformazione della posizione al sistema di coordinate NDC (Normalized Device Coordinates)
        gl_Position = projection * view * model * vec4(aPos, 1.0);
    }
)";

// Fragment Shader: Illuminazione + Texture Diffuse
// Applica la texture e la illuminazione solare per il bivacco
const char* solidFragmentShader = R"(
    #version 410 core
    out vec4 FragColor;
    in vec3 FaceNormal;
    in vec2 TexCoords;
    
    // Sampler per la texture diffuse caricata da PNG
    uniform sampler2D texture_diffuse; 
    uniform vec3 ambientColor;    // Luce ambiente da giorno/notte
    uniform vec3 lightDir;        // Direzione della luce solare

    void main() {
        // Normalizzazione della normale (potrebbe essere de-normalizzata dall'interpolazione)
        vec3 norm = normalize(FaceNormal);
        // Calcolo della componente diffusa basata sull'angolo tra normale e direzione luce
        float diff = max(dot(norm, normalize(lightDir)), 0.0);
        // Campionamento della texture alle coordinate UV fornite dal modello OBJ
        vec4 textureColor = texture(texture_diffuse, TexCoords);
        // Combinazione di illuminazione ambiente e diffusa, applicata al colore della texture
        vec3 lighting = (ambientColor + vec3(diff)) * textureColor.rgb;
        FragColor = vec4(lighting, 1.0);
    }
)";

// COORDINATE DEL CUBO 3D PER LO SKYBOX
// Un cubo centrato nell'origine usato per campionare la sfera celeste
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
    // CARICAMENTO DEL MODELLO DIGITALE DI ELEVAZIONE (DEM)
    // Lettura dei dati topografici da file ASCII per la ricostruzione 3D del terreno
    const char* filepath = "../Cartella-risorse/aletsch_32T.asc";
    Dem ghiacciaio(filepath);
    
    // Estrazione dei parametri DEM
    int W = ghiacciaio.header.width; 
    int H = ghiacciaio.header.height;
    double zMin = ghiacciaio.min; 
    double zMax = ghiacciaio.max;
    
    // GENERAZIONE DELLA MESH CON MASSIMA RISOLUZIONE GEOMETRICA
    // Step = 1 significa campionamento di ogni singolo pixel del DEM per massimo dettaglio
    int step = 1; 
    int cols = (W + step - 1) / step; 
    int rows = (H + step - 1) / step;

    // FASE 1: MAPPATURA DELLE POSIZIONI GEOMETRICHE IN UNA GRIGLIA 2D TEMPORANEA
    // Memorizzazione delle coordinate 3D per facilitare il calcolo delle normali nei step successivi
    std::vector<std::vector<glm::vec3>> gridPositions(rows, std::vector<glm::vec3>(cols));
    int r = 0;
    for (int y = 0; y < H; y += step) {
        int c = 0;
        for (int x = 0; x < W; x += step) {
            // Normalizzazione da coordinate pixel a NDC (-0.5 a 0.5)
            float xNdc = ((float)x / (W - 1)) - 0.5f;
            float yNdc = ((float)y / (H - 1)) - 0.5f;
            // Normalizzazione dell'altitudine (0.0 a 1.0 -> -0.2 a 0.2)
            float zNorm = (ghiacciaio(x, y) - zMin) / (zMax - zMin);
            gridPositions[r][c] = glm::vec3(xNdc, yNdc, (zNorm * 0.4f) - 0.2f);
            c++;
        }
        r++;
    }

    // FASE 2: CALCOLO DELLE NORMALI SULLA SUPERFICIE E GENERAZIONE DELLA MESH FINALE
    // Calcolo delle normali per ogni vertice utilizzando vettori tangenti locali e loro prodotto vettoriale
    std::vector<Vertex> meshVertices;
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            glm::vec3 pos = gridPositions[r][c];
            // Calcolo dei vettori tangenti locali per approssimare la superficie
            glm::vec3 tangentX(1.0f, 0.0f, 0.0f), tangentY(0.0f, 1.0f, 0.0f);
            if (c > 0 && c < cols - 1) tangentX = gridPositions[r][c+1] - gridPositions[r][c-1];
            if (r > 0 && r < rows - 1) tangentY = gridPositions[r+1][c] - gridPositions[r-1][c];
            // Calcolo della normale come prodotto vettoriale dei tangenti
            glm::vec3 normal = glm::normalize(glm::cross(tangentX, tangentY));
            // Correzione dell'orientamento della normale (deve puntare verso l'alto)
            if (normal.z < 0.0f) normal = -normal; 
            // Creazione del vertice con texture coordinate a zero (il terreno non usa texture)
            meshVertices.push_back({pos, normal, glm::vec2(0.0f)});
        }
    }

    // FASE 3: GENERAZIONE DELLA TOPOLOGIA DELLA MESH
    // Creazione degli indici per formare triangoli a partire dalla griglia rettangolare di vertici
    std::vector<unsigned int> indices;
    for (int r = 0; r < rows - 1; ++r) {
        for (int c = 0; c < cols - 1; ++c) {
            unsigned int topLeft = r * cols + c;
            // Due triangoli per ogni quad della griglia
            indices.push_back(topLeft); indices.push_back(topLeft + cols); indices.push_back(topLeft + 1);
            indices.push_back(topLeft + 1); indices.push_back(topLeft + cols); indices.push_back(topLeft + cols + 1);
        }
    }

    // CARICAMENTO E POSIZIONAMENTO DEL BIVACCO (CASA OBJ)
    // Eseguito una sola volta all'avvio per minimizzare overhead
    // Calcolo della posizione sul terreno: 60% della profondità, 55% della larghezza
    int bRow = static_cast<int>(rows * 0.60f); 
    int bCol = static_cast<int>(cols * 0.55f);
    glm::vec3 housePos = gridPositions[bRow][bCol]; 
    housePos.z += 0.001f;  // Sollevamento leggero per evitare z-fighting col terreno 

    // Caricamento del modello 3D della casa dal file OBJ
    std::vector<Vertex> houseLoadedVertices;
    if (!loadOBJ("../Cartella-risorse/bivacco.obj", houseLoadedVertices)) {
        std::cout << "Avviso: Impossibile trovare o caricare bivacco.obj!" << std::endl;
    }

    // CONFIGURAZIONE DEL CONTESTO OPENGL
    // Impostazione di OpenGL 4.1 Core con buffer di profondità a 24 bit
    sf::ContextSettings settings;
    settings.depthBits = 24;
    settings.majorVersion = 4;
    settings.minorVersion = 1;
    settings.attributeFlags = sf::ContextSettings::Core;

    // Creazione della finestra con risoluzione 1024x768
    sf::Window window(sf::VideoMode({1024, 768}), "Progetto FCG - LowPolyWorld", sf::State::Windowed, settings);
    // Configurazione dello stato del mouse e del cursore
    window.setMouseCursorGrabbed(isMouseGrabbed); 
    window.setMouseCursorVisible(!isMouseGrabbed);

    // Caricamento dei function pointer OpenGL tramite GLAD
    gladLoadGLLoader(reinterpret_cast<GLADloadproc>(sf::Context::getFunction));
    // Configurazione dello stato di rendering
    glViewport(0, 0, window.getSize().x, window.getSize().y);
    glEnable(GL_DEPTH_TEST);  // Attivazione del depth testing per l'occlusion

    // COMPILAZIONE E LINKING DEI PROGRAMMI SHADER
    // Lambda helper per compilazione veloce degli shader
    auto compileShader = [](unsigned int type, const char* source) {
        unsigned int shader = glCreateShader(type);
        glShaderSource(shader, 1, &source, NULL); 
        glCompileShader(shader);
        return shader;
    };

    // Creazione, compilazione e collegamento del programma shader terreno
    unsigned int vertexShader = compileShader(GL_VERTEX_SHADER, vertexShaderSource);
    unsigned int fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);
    unsigned int shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader); 
    glAttachShader(shaderProgram, fragmentShader); 
    glLinkProgram(shaderProgram);

    // Creazione del programma shader skybox
    unsigned int skyboxProgram = glCreateProgram();
    glAttachShader(skyboxProgram, compileShader(GL_VERTEX_SHADER, skyboxVertexShader));
    glAttachShader(skyboxProgram, compileShader(GL_FRAGMENT_SHADER, skyboxFragmentShader)); 
    glLinkProgram(skyboxProgram);

    // Creazione del programma shader per la casa (bivacco) con texture mapping
    unsigned int solidProgram = glCreateProgram();
    glAttachShader(solidProgram, compileShader(GL_VERTEX_SHADER, solidVertexShader));
    glAttachShader(solidProgram, compileShader(GL_FRAGMENT_SHADER, solidFragmentShader)); 
    glLinkProgram(solidProgram);

    // ALLOCAZIONE E CONFIGURAZIONE DEI BUFFER GPU - TERRENO
    // VAO (Vertex Array Object), VBO (Vertex Buffer Object), EBO (Element Buffer Object)
    unsigned int VAO, VBO, EBO;
    glGenVertexArrays(1, &VAO); 
    glGenBuffers(1, &VBO); 
    glGenBuffers(1, &EBO);
    glBindVertexArray(VAO); 
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, meshVertices.size() * sizeof(Vertex), meshVertices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);
    // CONFIGURAZIONE DEGLI ATTRIBUTI VERTICE
    // Attributo 0: Posizione (3 float)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0); 
    glEnableVertexAttribArray(0);
    // Attributo 1: Normale (3 float)
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(offsetof(Vertex, normal))); 
    glEnableVertexAttribArray(1);

    // ALLOCAZIONE E CONFIGURAZIONE DEI BUFFER GPU - SKYBOX
    unsigned int skyboxVAO, skyboxVBO;
    glGenVertexArrays(1, &skyboxVAO); 
    glGenBuffers(1, &skyboxVBO);
    glBindVertexArray(skyboxVAO); 
    glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);
    // Attributo 0: Posizione del cubo (3 float)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0); 
    glEnableVertexAttribArray(0);

    // ALLOCAZIONE E CONFIGURAZIONE DEI BUFFER GPU - CASA (BIVACCO)
    // Buffer layout completo: posizione (3f) + normale (3f) + UV (2f)
    unsigned int houseVAO, houseVBO;
    glGenVertexArrays(1, &houseVAO); 
    glGenBuffers(1, &houseVBO);
    glBindVertexArray(houseVAO); 
    glBindBuffer(GL_ARRAY_BUFFER, houseVBO);
    if (!houseLoadedVertices.empty()) {
        glBufferData(GL_ARRAY_BUFFER, houseLoadedVertices.size() * sizeof(Vertex), houseLoadedVertices.data(), GL_STATIC_DRAW);
    }
    // CONFIGURAZIONE DEGLI ATTRIBUTI PER LA CASA
    // Attributo 0: Posizione (3 float)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0); 
    glEnableVertexAttribArray(0);
    // Attributo 1: Normale (3 float)
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(offsetof(Vertex, normal))); 
    glEnableVertexAttribArray(1);
    // Attributo 2: Coordinate UV (2 float)
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(offsetof(Vertex, texCoords))); 
    glEnableVertexAttribArray(2);

    // CARICAMENTO E CONFIGURAZIONE DELLA TEXTURE PNG
    // Procedura: creazione texture object -> caricamento immagine -> configurazione parametri
    unsigned int textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    
    // Caricamento dell'immagine PNG tramite SFML
    sf::Image textureImage;
    if (textureImage.loadFromFile("../Cartella-risorse/texture.png")) {
        // Flip verticale necessaria poiché OpenGL e file PNG hanno origine diversa
        textureImage.flipVertically(); 
        // Trasferimento dei dati immagine al GPU (formato RGBA, 8 bit per canale)
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, textureImage.getSize().x, textureImage.getSize().y, 0, GL_RGBA, GL_UNSIGNED_BYTE, textureImage.getPixelsPtr());
        // Generazione automatica della piramide mipmap per rendering ad alta qualità a distanza
        glGenerateMipmap(GL_TEXTURE_2D);
        // CONFIGURAZIONE PARAMETRI TEXTURE
        // Wrapping: ripetizione della texture quando le coordinate UV superano 1.0
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        // Filtraggio: interpolazione lineare sia per minification che magnification
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    } else {
        std::cout << "Errore: Impossibile trovare o caricare la texture.png!" << std::endl;
    }

    // GAME LOOP CON SISTEMA GIORNO/NOTTE DINAMICO
    // Gestione del tempo, evoluzione della posizione solare e variazione dei colori cielo
    sf::Clock deltaClock;
    
    // VARIABILI DI CONTROLLO DEL TEMPO SIMULATO
    bool isTimePaused = false; 
    float currentSunAngle = 0.0f; 
    float daySpeed = 0.1f;
    
    // PALETTE DI COLORI PER IL CIELO DURANTE IL CICLO GIORNO/NOTTE
    glm::vec3 skyDay(0.5f, 0.7f, 0.9f), 
              skyGoldenHour(0.9f, 0.6f, 0.3f), 
              skySunset(0.8f, 0.3f, 0.45f), 
              skyTwilight(0.1f, 0.15f, 0.3f), 
              skyNight(0.02f, 0.02f, 0.08f);

    // GAME LOOP CON INTERAZIONE E ANIMAZIONE
    // Elaborazione degli eventi, calcolo timing, aggiornamento ciclo solare e rendering del frame
    while (window.isOpen()) {
        // Calcolo del tempo trascorso dal frame precedente per animazioni indipendenti dal frame rate
        float deltaTime = deltaClock.restart().asSeconds();
        
        // ELABORAZIONE DEGLI EVENTI
        while (const std::optional<sf::Event> event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) 
                window.close();
            if (const auto* keyPressed = event->getIf<sf::Event::KeyPressed>()) {
                // Gestione dei tasti speciali
                if (keyPressed->scancode == sf::Keyboard::Scan::Escape) 
                    window.close();
                // Tab: alternanza mouse libero/acquisito
                if (keyPressed->scancode == sf::Keyboard::Scan::Tab) {
                    isMouseGrabbed = !isMouseGrabbed; 
                    window.setMouseCursorGrabbed(isMouseGrabbed); 
                    window.setMouseCursorVisible(!isMouseGrabbed);
                    if (isMouseGrabbed) firstMouse = true;
                }
                // P: pausa/ripresa del ciclo solare
                if (keyPressed->scancode == sf::Keyboard::Scan::P) 
                    isTimePaused = !isTimePaused;
            }
        }

        // CONTROLLI TELECAMERA INTERATTIVA
        // Tracciamento mouse e movimento da tastiera con 6 gradi di libertà
        if (isMouseGrabbed && window.hasFocus()) {
            // Mouse tracking per rotazione della vista
            sf::Vector2i windowCenter(static_cast<int>(window.getSize().x / 2), static_cast<int>(window.getSize().y / 2));
            sf::Vector2i mousePos = sf::Mouse::getPosition(window);
            float xoffset = static_cast<float>(mousePos.x - windowCenter.x); 
            float yoffset = static_cast<float>(windowCenter.y - mousePos.y);
            
            if (xoffset != 0.0f || yoffset != 0.0f) {
                // Ri-posizionamento del mouse al centro della finestra per il tracciamento continuo
                sf::Mouse::setPosition(windowCenter, window); 
                // Applicazione dei coefficienti di sensibilità indipendenti ai due assi
                yaw -= xoffset * yawSensitivity; 
                pitch += yoffset * pitchSensitivity;
                // Limitazione dell'angolo di pitch per evitare il capovolgimento
                if (pitch > 89.0f) pitch = 89.0f; 
                if (pitch < -89.0f) pitch = -89.0f;
                // Conversione degli angoli di Eulero nel vettore di direzione della telecamera
                cameraFront = glm::normalize(glm::vec3(
                    cos(glm::radians(yaw)) * cos(glm::radians(pitch)), 
                    sin(glm::radians(yaw)) * cos(glm::radians(pitch)), 
                    sin(glm::radians(pitch))
                ));
            }
            
            // Controlli WASD e movimento verticale
            float velocity = cameraSpeed * deltaTime;
            glm::vec3 cameraRight = glm::normalize(glm::cross(cameraFront, cameraUp));
            
            // Movimenti di traslazione
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Scan::W)) cameraPos += cameraFront * velocity; 
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Scan::S)) cameraPos -= cameraFront * velocity; 
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Scan::A)) cameraPos -= cameraRight * velocity; 
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Scan::D)) cameraPos += cameraRight * velocity; 
            // Movimenti verticali assoluti
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Scan::Space))  cameraPos += cameraUp * velocity; 
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Scan::LShift)) cameraPos -= cameraUp * velocity; 
        }

        // CALCOLO DEL CICLO GIORNO/NOTTE DINAMICO
        // Evoluzione temporale della posizione solare
        if (!isTimePaused) 
            currentSunAngle += deltaTime * daySpeed;
        
        // CALCOLO DELLA DIREZIONE DELLA LUCE SOLARE
        glm::vec3 currentLightDir = glm::normalize(glm::vec3(
            cos(currentSunAngle), 
            -0.4f, 
            sin(currentSunAngle)
        ));
        // Estrazione dell'altitudine solare (componente Z della direzione luce)
        float sunHeight = glm::clamp(currentLightDir.z, -1.0f, 1.0f);

        // CALCOLO DINAMICO DEI PARAMETRI DI ILLUMINAZIONE E CIELO
        // La scena cambia colore e illuminazione in base all'altitudine solare attuale
        glm::vec3 currentHorizon, currentLightColor, currentAmbient;
        
        if (sunHeight > 0.3f) {
            // GIORNO PIENO
            currentHorizon = skyDay; 
            currentLightColor = glm::vec3(1.0f, 0.95f, 0.9f); 
            currentAmbient = glm::vec3(0.25f);
        } else if (sunHeight > 0.1f) {
            // GOLDEN HOUR
            float t = glm::smoothstep(0.0f, 1.0f, (sunHeight - 0.1f) / 0.2f); 
            currentHorizon = glm::mix(skyGoldenHour, skyDay, t); 
            currentLightColor = glm::mix(glm::vec3(1.0f, 0.6f, 0.2f), glm::vec3(1.0f, 0.95f, 0.9f), t); 
            currentAmbient = glm::mix(glm::vec3(0.2f), glm::vec3(0.25f), t);
        } else if (sunHeight > -0.05f) {
            // TRAMONTO / ALPENGLOW
            float t = glm::smoothstep(0.0f, 1.0f, (sunHeight + 0.05f) / 0.15f); 
            currentHorizon = glm::mix(skySunset, skyGoldenHour, t); 
            currentLightColor = glm::mix(glm::vec3(0.8f, 0.2f, 0.1f), glm::vec3(1.0f, 0.6f, 0.2f), t); 
            currentAmbient = glm::mix(glm::vec3(0.1f), glm::vec3(0.2f), t);
        } else if (sunHeight > -0.2f) {
            // CREPUSCOLO / BLU HOUR
            float t = glm::smoothstep(0.0f, 1.0f, (sunHeight + 0.2f) / 0.15f); 
            currentHorizon = glm::mix(skyTwilight, skySunset, t); 
            currentLightColor = glm::mix(glm::vec3(0.0f), glm::vec3(0.8f, 0.2f, 0.1f), t); 
            currentAmbient = glm::mix(glm::vec3(0.05f), glm::vec3(0.1f), t);
        } else {
            // NOTTE PROFONDA
            currentHorizon = skyNight; 
            currentLightColor = glm::vec3(0.0f); 
            currentAmbient = glm::vec3(0.05f); 
        }

        // SISTEMA DI ATTIVAZIONE DEL BIVACCO
        // La luce del bivacco diventa visibile progressivamente al crepuscolo e raggiunge il massimo di notte
        float bivouacActivation = glm::smoothstep(0.1f, -0.1f, sunHeight); 
        glm::vec3 bivouacLightColor = glm::vec3(1.0f, 0.55f, 0.1f) * bivouacActivation * 2.0f; 

        // CALCOLO DELL'OFFSET VERTICALE E POSIZIONE DELLA LUCE PUNTIFORME
        // Offset: solleva il pivot del modello OBJ per evitare z-fighting col terreno
        float verticalOffset = 0.005f;
        // Posizione della luce puntiforme: leggermente sollevata sopra il bivacco per illuminare le "finestre"
        glm::vec3 windowLightPos = housePos + glm::vec3(0.0f, 0.0f, verticalOffset + 0.004f);

        // GESTIONE DELLA VIEWPORT E PROIEZIONE
        // Aggiornamento dinamico della viewport in caso di ridimensionamento della finestra
        sf::Vector2u currentSize = window.getSize();
        glViewport(0, 0, currentSize.x, currentSize.y);

        // CLEARING DEL BUFFER
        // Impostazione del colore di sfondo e pulizia dei buffer colore e profondità
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f); 
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // COSTRUZIONE DELLE MATRICI DI TRASFORMAZIONE
        // Aspect ratio dinamico per evitare distorsioni durante il ridimensionamento
        float aspectRatio = static_cast<float>(currentSize.x) / static_cast<float>(currentSize.y);
        // Matrice di proiezione: prospettiva con FOV 45°, near 0.1, far 100
        glm::mat4 projection = glm::perspective(glm::radians(45.0f), aspectRatio, 0.1f, 100.0f);
        // Matrice di vista: posizionamento e orientamento della telecamera nello spazio mondo
        glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);

        // FASE 1: DISEGNO DELLO SKYBOX
        // Depth function modificato per garantire che il skybox rimane sempre sullo sfondo
        glDepthFunc(GL_LEQUAL); 
        glUseProgram(skyboxProgram);
        // Rimozione della traslazione dalla view matrix per mantenere il cielo concentrico
        glUniformMatrix4fv(glGetUniformLocation(skyboxProgram, "view"), 1, GL_FALSE, glm::value_ptr(glm::mat4(glm::mat3(view))));
        glUniformMatrix4fv(glGetUniformLocation(skyboxProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
        glUniform3fv(glGetUniformLocation(skyboxProgram, "horizonColor"), 1, glm::value_ptr(currentHorizon));
        glUniform3fv(glGetUniformLocation(skyboxProgram, "zenithColor"), 1, glm::value_ptr(currentHorizon * 0.4f));
        glUniform3fv(glGetUniformLocation(skyboxProgram, "lightDir"), 1, glm::value_ptr(currentLightDir));
        glUniform3fv(glGetUniformLocation(skyboxProgram, "sunColor"), 1, glm::value_ptr(currentLightColor));
        glBindVertexArray(skyboxVAO); 
        glDrawArrays(GL_TRIANGLES, 0, 36); 
        // Ripristino del depth function classico per il rendering degli oggetti
        glDepthFunc(GL_LESS); 

        // FASE 2: DISEGNO DEL TERRENO
        // Il terreno riceve sia la luce solare che la luce puntiforme del bivacco
        glUseProgram(shaderProgram);
        // Parametri di illuminazione solare dinamica
        glUniform3fv(glGetUniformLocation(shaderProgram, "lightDir"), 1, glm::value_ptr(currentLightDir));
        glUniform3fv(glGetUniformLocation(shaderProgram, "lightColor"), 1, glm::value_ptr(currentLightColor));
        glUniform3fv(glGetUniformLocation(shaderProgram, "ambientColor"), 1, glm::value_ptr(currentAmbient));
        // Parametri della luce puntiforme (bivacco)
        glUniform3fv(glGetUniformLocation(shaderProgram, "pointLightPos"), 1, glm::value_ptr(windowLightPos));
        glUniform3fv(glGetUniformLocation(shaderProgram, "pointLightColor"), 1, glm::value_ptr(bivouacLightColor));
        // Trasferimento delle matrici di trasformazione
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(glm::mat4(1.0f)));
        // Rendering della mesh indicizzata
        glBindVertexArray(VAO); 
        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indices.size()), GL_UNSIGNED_INT, 0);

        // FASE 3: DISEGNO DEL BIVACCO (CASA OBJ)
        // Rendering del modello 3D importato con texture mapping e illuminazione solare
        if (!houseLoadedVertices.empty()) {
            glUseProgram(solidProgram);
            // COSTRUZIONE DELLA MATRICE DI TRASFORMAZIONE PER IL BIVACCO
            // Sequenza di trasformazioni: traslazione -> rotazione -> scaling
            glm::mat4 houseModel = glm::mat4(1.0f);
            
            // Traslazione al centro del bivacco + offset verticale
            houseModel = glm::translate(houseModel, housePos + glm::vec3(0.0f, 0.0f, verticalOffset));
            // Rotazione: il modello OBJ è in un'orientazione differente, correggiamo di 90° sull'asse X
            houseModel = glm::rotate(houseModel, glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
            // Scaling: ridimensionamento del modello alla scala appropriata (0.009f validato)
            houseModel = glm::scale(houseModel, glm::vec3(0.009f));
            
            // Trasferimento delle matrici di trasformazione allo shader
            glUniformMatrix4fv(glGetUniformLocation(solidProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
            glUniformMatrix4fv(glGetUniformLocation(solidProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
            glUniformMatrix4fv(glGetUniformLocation(solidProgram, "model"), 1, GL_FALSE, glm::value_ptr(houseModel));
            // Parametri di illuminazione
            glUniform3fv(glGetUniformLocation(solidProgram, "ambientColor"), 1, glm::value_ptr(currentAmbient));
            glUniform3fv(glGetUniformLocation(solidProgram, "lightDir"), 1, glm::value_ptr(currentLightDir));

            // BINDING E CONFIGURAZIONE DELLA TEXTURE
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, textureID);
            glUniform1i(glGetUniformLocation(solidProgram, "texture_diffuse"), 0);

            // Rendering del bivacco con le coordinate caricate dal file OBJ
            glBindVertexArray(houseVAO);
            glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(houseLoadedVertices.size()));
        }

        window.display();
    }

    // LIBERAZIONE DELLE RISORSE GPU
    // Deallocazione di tutti i buffer, VAO e programmi shader

    // Deallocazione dei buffer del terreno
    glDeleteVertexArrays(1, &VAO); 
    glDeleteBuffers(1, &VBO); 
    glDeleteBuffers(1, &EBO);
    // Deallocazione dei buffer dello skybox
    glDeleteVertexArrays(1, &skyboxVAO); 
    glDeleteBuffers(1, &skyboxVBO);
    // Deallocazione dei buffer del bivacco
    glDeleteVertexArrays(1, &houseVAO); 
    glDeleteBuffers(1, &houseVBO);
    // Deallocazione dei programmi shader
    glDeleteProgram(shaderProgram); 
    glDeleteProgram(skyboxProgram); 
    glDeleteProgram(solidProgram);

    return 0;
}