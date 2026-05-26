#include <glad/glad.h>
#include <SFML/Window.hpp>
#include <iostream>
#include <vector>
#include <optional>
#include "dem.hh"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// STRUTTURA DATI PER I VERTICI
// Ogni vertice contiene sia la posizione che la normale per il calcolo dell'illuminazione
struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
};

// VARIABILI GLOBALI DELLA TELECAMERA
// Parametri che descrivono la posizione, orientamento e comportamento interattivo della telecamera
glm::vec3 cameraPos   = glm::vec3(0.0f, -1.2f, 0.6f); 
glm::vec3 cameraFront = glm::vec3(0.0f, 1.0f, -0.4f);
glm::vec3 cameraUp    = glm::vec3(0.0f, 0.0f, 1.0f);

// Angoli di rotazione (yaw-pitch-roll) per il controllo orientamento
float yaw   = 90.0f;  
float pitch = -20.0f; 
bool firstMouse = true;

// Stato di acquisizione del mouse per il controllo della telecamera
bool isMouseGrabbed = true; 

// Parametri di movimento e sensibilità input
float cameraSpeed = 0.5f;
float yawSensitivity   = 0.1f;
float pitchSensitivity = 0.08f;

// DEFINIZIONE DEGLI SHADER SOURCES
// Programmi GLSL compilati a runtime per il pipeline di rendering

// Vertex Shader: Riceve posizione e normale, trasforma le coordinate e calcola le normali nello spazio mondo
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

// Fragment Shader: Calcola illuminazione dinamica con parametri luce passati da CPU
// Applica colorazione procedurale basata su altitudine e illuminazione solare variabile
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
        // ILLUMINAZIONE DINAMICA CON LUCE SOLARE VARIABILE
        // La direzione, colore e intensità della luce provengono da uniform calcolati dal C++
        vec3 norm = normalize(Normal);
        float diff = max(dot(norm, normalize(lightDir)), 0.0);
        vec3 diffuseResult = lightColor * diff; 

        // COLORAZIONE PROCEDURALE DEL TERRENO BASATA SU ALTITUDINE
        vec3 colorValley = vec3(0.25f, 0.45f, 0.15f); 
        vec3 colorRock   = vec3(0.45f, 0.43f, 0.4f);  
        vec3 colorSnow   = vec3(0.9f, 0.95f, 1.0f);   

        vec3 terrainColor;
        if (FragPos.z < 0.02) {
            terrainColor = mix(colorValley, colorRock, smoothstep(-0.2, 0.02, FragPos.z));
        } else {
            terrainColor = mix(colorRock, colorSnow, smoothstep(0.02, 0.12, FragPos.z));
        }

        // COMBINAZIONE FINALE: illuminazione dinamica applicata al colore del bioma
        vec3 finalLighting = (ambientColor + diffuseResult) * terrainColor;
        FragColor = vec4(finalLighting, 1.0);
    }
)";

// SHADER DELLO SKYBOX CON EFFETTI PROCEDURALI
// Questo shader disegna una sfera celeste che comprende: cielo graduale, sole dinamico e stelle procedurali
// La geometria è un cubo, ma il vettore di direzione viene utilizzato per campionare la sfera celeste
const char* skyboxVertexShader = R"(
    #version 410 core
    layout (location = 0) in vec3 aPos;
    out vec3 TexCoords;
    out vec3 StarCoords; 

    uniform mat4 projection;
    uniform mat4 view;

    void main() {
        // Uso le coordinate di posizione del cubo come direzione per il campionamento della sfera celeste
        TexCoords = aPos;
        StarCoords = aPos; 
        vec4 pos = projection * view * vec4(aPos, 1.0);
        // Tecnica del "depth hijacking": scrivendo xyww, il valore di profondità è massimo (1.0)
        // Questo garantisce che lo skybox rimane sempre dietro a tutti gli altri elementi
        gl_Position = pos.xyww;
    }
)";

// Fragment Shader dello Skybox: Disegna il gradiente di cielo, sole e stelle procedurali
// Il shader utilizza la direzione del raggio per campionare il cielo e applicare effetti
const char* skyboxFragmentShader = R"(
    #version 410 core
    out vec4 FragColor;
    in vec3 TexCoords;
    in vec3 StarCoords; 

    uniform vec3 lightDir;
    uniform vec3 horizonColor;
    uniform vec3 zenithColor;
    uniform vec3 sunColor;

    // Funzione di rumore pseudocasuale basata su sin e dot product
    // Genera valori apparentemente casuali a partire da coordinate 3D per la generazione procedurale delle stelle
    float rand(vec3 co) {
        return fract(sin(dot(co, vec3(12.9898, 78.233, 45.164))) * 43758.5453);
    }

    void main() {
        vec3 dir = normalize(TexCoords);
        
        // SEZIONE 1: GRADIENTE DEL CIELO DALL'ORIZZONTE ALLO ZENIT
        // Interpolazione lineare tra il colore dell'orizzonte e dello zenit basata su dir.z
        // dir.z = -1 corrisponde all'orizzonte inferiore, dir.z = +1 allo zenit superiore
        float t = clamp((dir.z + 0.2) / 0.8, 0.0, 1.0);
        vec3 bgColor = mix(horizonColor, zenithColor, t);

        // SEZIONE 2: RENDERIZZAZIONE DEL SOLE
        // Calcolo del prodotto scalare tra la direzione del raggio e quella del sole
        float sunDot = dot(dir, normalize(lightDir));
        // sunGlow: alone sfumato attorno al sole con decadimento esponenziale (pow)
        float sunGlow = pow(max(sunDot, 0.0), 64.0); 
        // sunDisk: disco solido e netto del sole utilizzando smoothstep
        float sunDisk = smoothstep(0.995, 0.998, sunDot); 
        // sunVisibility: il sole è visibile solo quando l'altitudine è sopra l'orizzonte
        float sunVisibility = smoothstep(-0.1, 0.0, lightDir.z);

        // SEZIONE 3: GENERAZIONE PROCEDURALE DELLE STELLE
        vec3 starColor = vec3(0.0);
        // nightVisibility: le stelle sono visibili solo di notte (quando il sole è sottoterra)
        float nightVisibility = smoothstep(0.1, -0.15, lightDir.z);

        if (nightVisibility > 0.0) {
            // Suddivisione della sfera celeste in celle per la distribuzione spaziale delle stelle
            float cellSize = 120.0;
            vec3 cellPos = floor(dir * cellSize);
            // Generazione di un valore casuale per cella: determina se una stella è presente
            float starRand = rand(cellPos);

            // Solo lo 0.1% delle celle contiene una stella (starRand < 0.001)
            if (starRand < 0.001) {
                // Centro della cella per il calcolo della distanza e dell'intensità
                vec3 starCenter = (cellPos + 0.5) / cellSize;
                float dist = length(dir - starCenter);
                // Smussamento della stella: transizione graduale da opaco a trasparente
                float starIntensity = smoothstep(0.003, 0.0, dist); 
                
                // Colore base bianco-azzurrognolo per le stelle
                vec3 baseStarColor = vec3(0.95, 0.98, 1.0); 
                // Varianza di colore per simulare stelle di temperature diverse (più rosse o blu)
                vec3 colorVariance = vec3(rand(cellPos+1.0), rand(cellPos+2.0), rand(cellPos+3.0)) * 0.15;
                
                // Composizione finale della stella con visibilità notturna
                starColor = (baseStarColor + colorVariance) * starIntensity * nightVisibility * 1.5;
            }
        }

        // COMPOSIZIONE FINALE DEL COLORE DELLO SKYBOX
        // Combinazione di: sfondo graduale + alone solare + disco solare + stelle procedurali
        vec3 finalColor = bgColor + (sunColor * sunGlow * 0.6 * sunVisibility) + (sunColor * sunDisk * sunVisibility) + starColor;
        FragColor = vec4(finalColor, 1.0);
    }
)";

// COORDINATE DEL CUBO 3D PER LO SKYBOX
// Un cubo centrato nell'origine con vertici a distanza 1 dalle facce
// Questi vertici vengono usati dal vertex shader per determinare la direzione di campionamento della sfera celeste
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
            float xNdc = ((float)x / (W - 1)) - 0.5f;
            float yNdc = ((float)y / (H - 1)) - 0.5f;
            float zNorm = (ghiacciaio(x, y) - zMin) / (zMax - zMin);
            float zNdc = (zNorm * 0.4f) - 0.2f; 
            gridPositions[r][c] = glm::vec3(xNdc, yNdc, zNdc);
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

    // FASE 3: GENERAZIONE DELLA TOPOLOGIA DELLA MESH
    // Creazione degli indici per formare triangoli a partire dalla griglia rettangolare di vertici
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

    // CONFIGURAZIONE DEL CONTESTO OPENGL
    // Impostazione di OpenGL 4.1 Core con buffer di profondità a 24 bit
    sf::ContextSettings settings;
    settings.depthBits = 24;
    settings.majorVersion = 4;
    settings.minorVersion = 1;
    settings.attributeFlags = sf::ContextSettings::Core;

    // Creazione della finestra con risoluzione aumentata per maggior dettaglio visivo
    sf::Window window(sf::VideoMode({1024, 768}), "Progetto FCG - LowPolyWorld", sf::State::Windowed, settings);
    // Configurazione dello stato del mouse e del cursore
    window.setMouseCursorGrabbed(isMouseGrabbed);
    window.setMouseCursorVisible(!isMouseGrabbed);

    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(sf::Context::getFunction))) {
        std::cout << "Errore GLAD!" << std::endl;
        return -1;
    }

    // Configurazione dello stato di rendering
    glViewport(0, 0, window.getSize().x, window.getSize().y);
    glEnable(GL_DEPTH_TEST);  // Attivazione del depth testing per l'occlusion

    // COMPILAZIONE E LINKING DEI PROGRAMMI SHADER
    // Creazione, compilazione e collegamento dei programmi vertex e fragment
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

    // COMPILAZIONE E LINKING DEL PROGRAMMA SHADER SKYBOX
    // Lo skybox è reso con uno shader separato per gestire effetti procedurali (sole e stelle)
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
    glDeleteShader(skyboxVert);
    glDeleteShader(skyboxFrag);

    // ALLOCAZIONE E CONFIGURAZIONE DEI BUFFER GPU CON ATTRIBUTI MULTIPLI
    // VAO (Vertex Array Object) registra la configurazione dei buffer e degli attributi
    // VBO (Vertex Buffer Object) memorizza i dati dei vertici (posizione + normale)
    // EBO (Element Buffer Object) memorizza gli indici per il rendering indicizzato
    unsigned int VAO, VBO, EBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, meshVertices.size() * sizeof(Vertex), meshVertices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    // CONFIGURAZIONE DELL'ATTRIBUTO 0: POSIZIONE DEL VERTICE
    // Layout: 3 float per vertice, stride = sizeof(Vertex), offset = 0
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
    glEnableVertexAttribArray(0);
    // CONFIGURAZIONE DELL'ATTRIBUTO 1: NORMALE DEL VERTICE
    // Layout: 3 float per vertice, stride = sizeof(Vertex), offset = posizione di 'normal' nella struct
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(offsetof(Vertex, normal)));
    glEnableVertexAttribArray(1);

    // ALLOCAZIONE DEI BUFFER GPU PER LO SKYBOX
    // Il skybox utilizza solo le posizioni (3 float per vertice), nessuna normale necessaria
    unsigned int skyboxVAO, skyboxVBO;
    glGenVertexArrays(1, &skyboxVAO);
    glGenBuffers(1, &skyboxVBO);
    glBindVertexArray(skyboxVAO);
    glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);
    // CONFIGURAZIONE DELL'ATTRIBUTO POSIZIONE PER LO SKYBOX
    // Il skybox ha solo attributo 0 (posizione), senza normali né altri attributi
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // GAME LOOP CON SISTEMA GIORNO/NOTTE DINAMICO
    // Gestione del tempo, evoluzione della posizione solare e variazione dei colori cielo
    sf::Clock deltaClock;
    
    // VARIABILI DI CONTROLLO DEL TEMPO SIMULATO
    // isTimePaused permette di fermare l'evoluzione del ciclo solare (pressione di 'P')
    bool isTimePaused = false;
    // currentSunAngle rappresenta la posizione angolare del sole nella sfera celeste (radianti)
    float currentSunAngle = 0.0f;
    // daySpeed determina la velocità di avanzamento del ciclo giorno/notte (radianti per secondo)
    float daySpeed = 0.1f;

    // PALETTE DI COLORI PER IL CIELO DURANTE IL CICLO GIORNO/NOTTE
    // Questa tavolozza rappresenta stati atmosferici diversi durante le varie fasi solari
    glm::vec3 skyDay        = glm::vec3(0.5f, 0.7f, 0.9f);      // Azzurro diurno puro
    glm::vec3 skyGoldenHour = glm::vec3(0.9f, 0.6f, 0.3f);      // Arancione "golden hour" (alba/tramonto)
    glm::vec3 skySunset     = glm::vec3(0.8f, 0.3f, 0.45f);     // Magenta alpenglow (enrosadira alpina)
    glm::vec3 skyTwilight   = glm::vec3(0.1f, 0.15f, 0.3f);     // Blu crepuscolare ("blue hour")
    glm::vec3 skyNight      = glm::vec3(0.02f, 0.02f, 0.08f);   // Blu profondo notturno

    // GAME LOOP CON INTERAZIONE E ANIMAZIONE
    // Elaborazione degli eventi, calcolo timing, aggiornamento ciclo solare e rendering del frame
    while (window.isOpen()) {
        // Calcolo del tempo trascorso dal frame precedente per animazioni e movimenti indipendenti dal frame rate
        float deltaTime = deltaClock.restart().asSeconds();

        while (const std::optional<sf::Event> event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) 
                window.close();
            else if (const auto* resized = event->getIf<sf::Event::Resized>()) {
                // Gestione del ridimensionamento dinamico della viewport
                glViewport(0, 0, resized->size.x, resized->size.y);
            }
            else if (const auto* keyPressed = event->getIf<sf::Event::KeyPressed>()) {
                // Gestione dei tasti speciali per chiusura finestra e controlli vari
                if (keyPressed->scancode == sf::Keyboard::Scan::Escape) 
                    window.close();
                // Alternanza tra mouse libero e acquisito per il controllo della telecamera
                if (keyPressed->scancode == sf::Keyboard::Scan::Tab) {
                    isMouseGrabbed = !isMouseGrabbed;
                    window.setMouseCursorGrabbed(isMouseGrabbed);
                    window.setMouseCursorVisible(!isMouseGrabbed);
                    if (isMouseGrabbed) firstMouse = true;
                }
                // Pausa/Ripresa dell'evoluzione del ciclo solare
                if (keyPressed->scancode == sf::Keyboard::Scan::P) {
                    isTimePaused = !isTimePaused;
                }
            }
        }

        // TRACCIAMENTO E ELABORAZIONE DELL'INPUT DEL MOUSE
        // Calcolo della rotazione della telecamera basato sul movimento del mouse rispetto al centro schermo
        // CONTROLLI DI MOVIMENTO DELLA TELECAMERA DA TASTIERA
        // Implementazione di uno schema di controllo simile a Minecraft Creative Mode con 6 gradi di libertà
        if (isMouseGrabbed && window.hasFocus()) {
            // Mouse tracking per rotazione della vista
            sf::Vector2i windowCenter(static_cast<int>(window.getSize().x / 2), static_cast<int>(window.getSize().y / 2));
            sf::Vector2i mousePos = sf::Mouse::getPosition(window);
            float xoffset = static_cast<float>(mousePos.x - windowCenter.x);
            float yoffset = static_cast<float>(windowCenter.y - mousePos.y);

            if (xoffset != 0.0f || yoffset != 0.0f) {
                // Ri-posizionamento del mouse al centro della finestra per il tracciamento continuo
                sf::Mouse::setPosition(windowCenter, window); 
                // Applicazione dei coefficienti di sensibilità indipendenti ai due assi di rotazione
                yaw   -= xoffset * yawSensitivity;
                pitch += yoffset * pitchSensitivity; 
                // Limitazione dell'angolo di pitch per evitare il capovolgimento della telecamera
                if (pitch > 89.0f)  pitch = 89.0f;
                if (pitch < -89.0f) pitch = -89.0f;

                // Conversione degli angoli di Eulero nel vettore di direzione della telecamera
                glm::vec3 front;
                front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
                front.y = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
                front.z = sin(glm::radians(pitch));
                cameraFront = glm::normalize(front);
            }

            // Controlli WASD e movimento verticale
            float velocity = cameraSpeed * deltaTime;
            // Vettore perpendicolare al piano di vista per i movimenti laterali
            glm::vec3 cameraRight = glm::normalize(glm::cross(cameraFront, cameraUp));

            // Movimenti di traslazione rispetto al frame di riferimento della telecamera
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Scan::W)) 
                cameraPos += cameraFront * velocity; 
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Scan::S)) 
                cameraPos -= cameraFront * velocity; 
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Scan::A)) 
                cameraPos -= cameraRight * velocity; 
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Scan::D)) 
                cameraPos += cameraRight * velocity; 
            // Movimenti verticali assoluti rispetto all'asse Z del mondo
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Scan::Space))  
                cameraPos += cameraUp * velocity; 
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Scan::LShift)) 
                cameraPos -= cameraUp * velocity; 
        }

        // CALCOLO DEL CICLO GIORNO/NOTTE DINAMICO
        // Evoluzione temporale della posizione solare nello spazio 3D e conseguente variazione di illuminazione
        // L'angolo solare viene incrementato solo quando il tempo non è in pausa
        if (!isTimePaused) {
            currentSunAngle += deltaTime * daySpeed;
        }

        // CALCOLO DELLA DIREZIONE DELLA LUCE SOLARE
        // Parametrizzazione angolare della posizione del sole in coordinate sferiche
        // La luce orbita attorno all'asse Y (nord-sud) con variazione in X e Z
        glm::vec3 currentLightDir = glm::normalize(glm::vec3(cos(currentSunAngle), -0.4f, sin(currentSunAngle)));
        // Estrazione dell'altitudine solare: il componente Z della direzione luce
        // sunHeight va da -1 (sole sotto l'orizzonte) a +1 (sole allo zenit)
        float sunHeight = glm::clamp(currentLightDir.z, -1.0f, 1.0f);

        // CALCOLO DINAMICO DEI PARAMETRI DI ILLUMINAZIONE E CIELO
        // La scena cambia colore e illuminazione in base all'altitudine solare attuale
        glm::vec3 currentHorizon;
        glm::vec3 currentLightColor;
        glm::vec3 currentAmbient;

        // GIORNO PIENO (sunHeight > 0.3)
        // Il sole è sufficientemente alto nel cielo per fornire illuminazione diurna completa
        if (sunHeight > 0.3f) {
            currentHorizon = skyDay;
            currentLightColor = glm::vec3(1.0f, 0.95f, 0.9f);
            currentAmbient = glm::vec3(0.25f);
        } 
        // GOLDEN HOUR (0.1 < sunHeight <= 0.3)
        // Il sole è vicino all'orizzonte: luce calda arancione con sfumatura verso il blu diurno
        else if (sunHeight > 0.1f) {
            float t = (sunHeight - 0.1f) / 0.2f;
            // Applicazione di smoothstep per transizioni più naturali
            t = glm::smoothstep(0.0f, 1.0f, t); 
            // Interpolazione lineare tra golden hour e giorno pieno
            currentHorizon = glm::mix(skyGoldenHour, skyDay, t);
            currentLightColor = glm::mix(glm::vec3(1.0f, 0.6f, 0.2f), glm::vec3(1.0f, 0.95f, 0.9f), t);
            currentAmbient = glm::mix(glm::vec3(0.2f), glm::vec3(0.25f), t);
        } 
        // TRAMONTO / ALPENGLOW (−0.05 < sunHeight <= 0.1)
        // Luce magenta caratteristica dell'alpenglow delle Alpi
        else if (sunHeight > -0.05f) {
            float t = (sunHeight + 0.05f) / 0.15f;
            t = glm::smoothstep(0.0f, 1.0f, t);
            // Transizione dal tramonto magenta alla golden hour
            currentHorizon = glm::mix(skySunset, skyGoldenHour, t);
            currentLightColor = glm::mix(glm::vec3(0.8f, 0.2f, 0.1f), glm::vec3(1.0f, 0.6f, 0.2f), t);
            currentAmbient = glm::mix(glm::vec3(0.1f), glm::vec3(0.2f), t);
        } 
        // CREPUSCOLO / ORO BLU (-0.2 < sunHeight <= -0.05)
        // Fase del crepuscolo con cielo blu profondo: "blue hour"
        else if (sunHeight > -0.2f) {
            float t = (sunHeight + 0.2f) / 0.15f;
            t = glm::smoothstep(0.0f, 1.0f, t);
            // Transizione dal blu crepuscolare al tramonto
            currentHorizon = glm::mix(skyTwilight, skySunset, t);
            // Durante il crepuscolo la luce solare è quasi assente (luce ambientale residua)
            currentLightColor = glm::mix(glm::vec3(0.0f), glm::vec3(0.8f, 0.2f, 0.1f), t); 
            currentAmbient = glm::mix(glm::vec3(0.05f), glm::vec3(0.1f), t);
        } 
        // NOTTE PROFONDA (sunHeight <= -0.2)
        // Il sole è completamente sotto l'orizzonte: illuminazione notturna minima
        else {
            currentHorizon = skyNight;
            currentLightColor = glm::vec3(0.0f); 
            currentAmbient = glm::vec3(0.05f); 
        }

        // Lo zenit è sempre leggermente più scuro dell'orizzonte per un effetto atmosferico più realistico
        glm::vec3 currentZenith = currentHorizon * 0.4f;

        // RENDERING DELLA SCENA CON ILLUMINAZIONE E SFONDO DINAMICI
        // Impostazione del colore di sfondo in base alla fase del ciclo solare
        glClearColor(currentHorizon.r, currentHorizon.g, currentHorizon.b, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Calcolo dinamico dell'aspect ratio per evitare distorsioni durante il ridimensionamento
        float width = static_cast<float>(window.getSize().x);
        float height = static_cast<float>(window.getSize().y);
        float aspectRatio = width / height;

        // COSTRUZIONE DELLE MATRICI DI TRASFORMAZIONE (GLM)
        // Matrice di proiezione: prospettiva con FOV 45°, near plane 0.1, far plane 100
        glm::mat4 projection = glm::perspective(glm::radians(45.0f), aspectRatio, 0.1f, 100.0f);
        // Matrice di vista: posizionamento e orientamento della telecamera interattiva nello spazio mondo
        glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);

        // FASE 1: DISEGNO DELLO SKYBOX CON EFFETTI PROCEDURALI
        // Lo skybox viene renderizzato per primo con depth testing speciale per garantire che rimanga sempre dietro
        glDepthFunc(GL_LEQUAL);
        glUseProgram(skyboxProgram);
        
        // Rimozione della componente di traslazione dalla View Matrix
        // Così il cielo rimane concentrico alla telecamera indipendentemente da dove si muove
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
        // Ripristino del depth test classico per il rendering del terreno
        glDepthFunc(GL_LESS);

        // FASE 2: DISEGNO DEL TERRENO CON ILLUMINAZIONE DINAMICA
        // Il terreno viene renderizzato con i parametri di luce calcolati dal ciclo giorno/notte
        glUseProgram(shaderProgram);
        // Matrice di modello: posizionamento e orientamento della geometria nello spazio mondo
        glm::mat4 model = glm::mat4(1.0f);
        
        // Trasferimento dei parametri di illuminazione dinamica al programma shader
        // Questi valori cambiano continuamente durante il ciclo giorno/notte
        glUniform3fv(glGetUniformLocation(shaderProgram, "lightDir"), 1, glm::value_ptr(currentLightDir));
        glUniform3fv(glGetUniformLocation(shaderProgram, "lightColor"), 1, glm::value_ptr(currentLightColor));
        glUniform3fv(glGetUniformLocation(shaderProgram, "ambientColor"), 1, glm::value_ptr(currentAmbient));
        
        // Trasferimento delle matrici di trasformazione ai uniform del programma shader
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));

        // Rendering della mesh indicizzata con illuminazione e colorazione procedurale dinamiche
        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indices.size()), GL_UNSIGNED_INT, 0);

        window.display();
    }

    // LIBERAZIONE DELLE RISORSE GPU
    // Deallocazione di tutti i buffer e dei programmi shader
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
    glDeleteVertexArrays(1, &skyboxVAO);
    glDeleteBuffers(1, &skyboxVBO);
    glDeleteProgram(shaderProgram);
    glDeleteProgram(skyboxProgram);

    return 0;
}