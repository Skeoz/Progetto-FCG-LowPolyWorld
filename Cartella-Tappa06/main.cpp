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
        // Normal matrix: previene distorsioni delle normali in caso di scalatura
        Normal = mat3(transpose(inverse(model))) * aNormal; 
        gl_Position = projection * view * vec4(FragPos, 1.0);
    }
)";  

// Fragment Shader: Calcola l'illuminazione basata su componenti ambiente, diffusa e speculare (Phong)
const char* fragmentShaderSource = R"(
    #version 410 core
    out vec4 FragColor;

    in vec3 FragPos;
    in vec3 Normal;

    void main() {
        // COMPONENTE DI ILLUMINAZIONE AMBIENTE (AMBIENT)
        // Simula la luce diffusa proveniente da tutte le direzioni nel cielo
        float ambientStrength = 0.2;
        vec3 ambientColor = vec3(1.0, 1.0, 1.0) * ambientStrength;
        
        // COMPONENTE DI ILLUMINAZIONE DIFFUSA (DIFFUSE)
        // Calcula l'effetto della luce direzionale primaria (sole) sulla superficie
        vec3 norm = normalize(Normal);
        // Direzione della luce solare: vettore che punta verso la sorgente luminosa
        vec3 lightDir = normalize(vec3(0.5f, -0.5f, 1.0f)); 
        
        // Prodotto scalare tra normale e direzione luce determina l'intensità diffusa
        float diff = max(dot(norm, lightDir), 0.0);
        // Colore della luce solare con temperatura calda (6500K)
        vec3 diffuseColor = vec3(1.0, 0.95, 0.8) * diff;

        // COLORE BASE DEL MATERIALE
        // Granito grigio realistico con riflettanza diffusa
        vec3 rockColor = vec3(0.5f, 0.5f, 0.5f);

        // COMBINAZIONE FINALE DELLE COMPONENTI DI ILLUMINAZIONE
        // Risultato = (Ambiente + Diffusa) * Colore del materiale
        vec3 result = (ambientColor + diffuseColor) * rockColor;
        FragColor = vec4(result, 1.0);
    }
)";

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

    // FASE 1: MAPPATURA DELLE POSIZIONI GEOMETRICHE IN UNA GRIGLIA 2D TEMPORANEA
    // Memorizzazione delle coordinate 3D per facilitare il calcolo delle normali nei step successivi
    std::vector<std::vector<glm::vec3>> gridPositions(rows, std::vector<glm::vec3>(cols));
    int r = 0;
    for (int y = 0; y < H; y += step) {
        int c = 0;
        for (int x = 0; x < W; x += step) {
            // Conversione delle coordinate pixel a spazio [-0.5, 0.5]
            float xNdc = ((float)x / (W - 1)) - 0.5f;
            float yNdc = ((float)y / (H - 1)) - 0.5f;
            // Normalizzazione dell'elevazione tra 0 e 1, poi scalatura e offset per enfatizzare il rilievo
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
            
            // CALCOLO DEI VETTORI TANGENTI (TANGENT VECTORS)
            // Rappresentano la pendenza della superficie lungo gli assi X e Y della griglia
            glm::vec3 tangentX(1.0f, 0.0f, 0.0f);
            glm::vec3 tangentY(0.0f, 1.0f, 0.0f);

            // Calcolo della pendenza lungo X usando differenze finite centrali quando possibile
            if (c > 0 && c < cols - 1) 
                tangentX = gridPositions[r][c+1] - gridPositions[r][c-1];
            else if (c < cols - 1) 
                tangentX = gridPositions[r][c+1] - pos;
            else if (c > 0) 
                tangentX = pos - gridPositions[r][c-1];

            // Calcolo della pendenza lungo Y usando differenze finite centrali quando possibile
            if (r > 0 && r < rows - 1) 
                tangentY = gridPositions[r+1][c] - gridPositions[r-1][c];
            else if (r < rows - 1) 
                tangentY = gridPositions[r+1][c] - pos;
            else if (r > 0) 
                tangentY = pos - gridPositions[r-1][c];

            // CALCOLO DELLA NORMALE TRAMITE PRODOTTO VETTORIALE
            // Il prodotto vettoriale tra due vettori tangenti genera un vettore perpendicolare alla superficie
            glm::vec3 normal = glm::normalize(glm::cross(tangentX, tangentY));
            // Correzione dell'orientamento: forza la componente Z positiva per garantire normali verso l'alto
            if (normal.z < 0.0f) 
                normal = -normal;

            // Accoppiamento di posizione e normale nel vertice finale
            meshVertices.push_back({pos, normal});
        }
    }

    // FASE 3: GENERAZIONE DELLA TOPOLOGIA DELLA MESH
    // Creazione degli indici per formare triangoli a partire dalla griglia rettangolare di vertici
    std::vector<unsigned int> indices;
    for (int r = 0; r < rows - 1; ++r) {
        for (int c = 0; c < cols - 1; ++c) {
            // Calcolo degli indici dei quattro vertici del quadrilatero corrente
            unsigned int topLeft     = r * cols + c;
            unsigned int topRight    = topLeft + 1;
            unsigned int bottomLeft  = (r + 1) * cols + c;
            unsigned int bottomRight = bottomLeft + 1;

            // Primo triangolo: top-left, bottom-left, top-right
            indices.push_back(topLeft);
            indices.push_back(bottomLeft);
            indices.push_back(topRight);
            // Secondo triangolo: top-right, bottom-left, bottom-right
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

    sf::Window window(sf::VideoMode({800, 600}), "Progetto FCG - LowPolyWorld", sf::State::Windowed, settings);

    // Configurazione dello stato del mouse e del cursore
    window.setMouseCursorGrabbed(isMouseGrabbed);
    window.setMouseCursorVisible(!isMouseGrabbed);

    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(sf::Context::getFunction))) {
        std::cout << "Errore GLAD!" << std::endl;
        return -1;
    }

    // Configurazione dello stato di rendering
    glViewport(0, 0, 800, 600);
    glEnable(GL_DEPTH_TEST);  // Attivazione del depth testing per l'occlusion
    // Disattivazione della modalità wireframe: rendering pieno dei poligoni per visualizzare l'illuminazione
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

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

    // ALLOCAZIONE E CONFIGURAZIONE DEI BUFFER GPU CON ATTRIBUTI MULTIPLI
    // VAO (Vertex Array Object) registra la configurazione dei buffer e degli attributi
    // VBO (Vertex Buffer Object) memorizza i dati dei vertici (posizione + normale)
    // EBO (Element Buffer Object) memorizza gli indici per il rendering indicizzato
    unsigned int VAO, VBO, EBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);
    
    // Caricamento dei dati della struttura Vertex nel VBO
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    // Nota: sizeof(Vertex) include sia la posizione che la normale
    glBufferData(GL_ARRAY_BUFFER, meshVertices.size() * sizeof(Vertex), meshVertices.data(), GL_STATIC_DRAW);
    
    // Caricamento degli indici di rendering nell'EBO
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

    int modelLoc = glGetUniformLocation(shaderProgram, "model");
    int viewLoc  = glGetUniformLocation(shaderProgram, "view");
    int projLoc  = glGetUniformLocation(shaderProgram, "projection");

    // GAME LOOP
    // Elaborazione degli eventi, calcolo timing, aggiornamento dello stato della telecamera e rendering del frame
    sf::Clock deltaClock;

    while (window.isOpen()) {
        // Calcolo del tempo trascorso dal frame precedente per movimenti e rotazioni indipendenti dal frame rate
        float deltaTime = deltaClock.restart().asSeconds();

        while (const std::optional<sf::Event> event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                window.close();
            }
            else if (const auto* resized = event->getIf<sf::Event::Resized>()) {
                // Gestione del ridimensionamento dinamico della viewport
                glViewport(0, 0, resized->size.x, resized->size.y);
            }
            else if (const auto* keyPressed = event->getIf<sf::Event::KeyPressed>()) {
                // Gestione dei tasti speciali per chiusura finestra e toggle mouse grabbing
                if (keyPressed->scancode == sf::Keyboard::Scan::Escape) {
                    window.close();
                }
                if (keyPressed->scancode == sf::Keyboard::Scan::Tab) {
                    // Alternanza tra mouse libero e acquisito per il controllo della telecamera
                    isMouseGrabbed = !isMouseGrabbed;
                    window.setMouseCursorGrabbed(isMouseGrabbed);
                    window.setMouseCursorVisible(!isMouseGrabbed);
                    if (isMouseGrabbed) firstMouse = true;
                }
            }
        }

        // TRACCIAMENTO E ELABORAZIONE DELL'INPUT DEL MOUSE
        // Calcolo della rotazione della telecamera basato sul movimento del mouse rispetto al centro schermo
        if (isMouseGrabbed && window.hasFocus()) {
            sf::Vector2i windowCenter(static_cast<int>(window.getSize().x / 2), static_cast<int>(window.getSize().y / 2));
            sf::Vector2i mousePos = sf::Mouse::getPosition(window);

            float xoffset = static_cast<float>(mousePos.x - windowCenter.x);
            float yoffset = static_cast<float>(windowCenter.y - mousePos.y);

            if (xoffset != 0.0f || yoffset != 0.0f) {
                // Ri-posizionamento del mouse al centro della finestra per il tracciamento continuo
                sf::Mouse::setPosition(windowCenter, window); 

                // Applicazione dei coefficienti di sensibilità indipendenti ai due assi di rotazione
                xoffset *= yawSensitivity;
                yoffset *= pitchSensitivity; 

                yaw   -= xoffset;
                pitch += yoffset;

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
        }

        // CONTROLLI DI MOVIMENTO DELLA TELECAMERA DA TASTIERA
        // Implementazione di uno schema di controllo simile a Minecraft Creative Mode con 6 gradi di libertà
        if (isMouseGrabbed && window.hasFocus()) {
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

        // Impostazione del colore di sfondo: azzurro cielo per il contrasto con la geometria in grigio
        glClearColor(0.5f, 0.7f, 0.9f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(shaderProgram);

        // Calcolo dinamico dell'aspect ratio per evitare distorsioni durante il ridimensionamento
        float width = static_cast<float>(window.getSize().x);
        float height = static_cast<float>(window.getSize().y);
        float aspectRatio = width / height;

        // COSTRUZIONE DELLE MATRICI DI TRASFORMAZIONE (GLM)
        // Matrice di proiezione: prospettiva con FOV 45°, near plane 0.1, far plane 100
        glm::mat4 projection = glm::perspective(glm::radians(45.0f), aspectRatio, 0.1f, 100.0f);
        // Matrice di vista: posizionamento e orientamento della telecamera interattiva nello spazio mondo
        glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
        // Matrice di modello: posizionamento e orientamento della geometria nello spazio mondo
        glm::mat4 model = glm::mat4(1.0f);

        // Trasferimento delle matrici ai uniform del programma shader
        glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projection));
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

        // Rendering della mesh indicizzata con illuminazione
        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indices.size()), GL_UNSIGNED_INT, 0);

        window.display();
    }

    // LIBERAZIONE DELLE RISORSE GPU
    // Deallocazione di tutti i buffer e del programma shader
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
    glDeleteProgram(shaderProgram);

    return 0;
}