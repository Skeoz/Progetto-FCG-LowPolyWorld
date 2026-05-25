#include <glad/glad.h>
#include <SFML/Window.hpp>
#include <iostream>
#include <vector>
#include <optional>
#include "dem.hh"

// Librerie matematiche GLM
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>


// Vertex Shader: Trasforma le coordinate dei vertici utilizzando matrici di modello, vista e proiezione
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

// Fragment Shader: Assegna un colore fisso ai pixel rasterizzati(arancione)
const char* fragmentShaderSource = R"(
    #version 410 core
    out vec4 FragColor;
    void main() {
        FragColor = vec4(1.0f, 0.5f, 0.2f, 1.0f);
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

    // GENERAZIONE DELLA MESH CON RIDUZIONE DELLA COMPLESSITA' (LOD - LEVEL OF DETAIL)
    // Campionamento del DEM a intervalli regolari riduce il numero di vertici mantenendo la forma generale
    std::vector<float> vertices;
    std::vector<unsigned int> indices;

    int step = 10; // Intervallo di campionamento per ridurre la risoluzione della mesh
    int cols = (W + step - 1) / step;
    int rows = (H + step - 1) / step;

    // GENERAZIONE DEI VERTICI
    // Campionamento dei dati DEM e normalizzazione a coordinate NDC (Normalized Device Coordinates)
    for (int y = 0; y < H; y += step) {
        for (int x = 0; x < W; x += step) {
            // Conversione delle coordinate pixel a spazio [-0.5, 0.5]
            float xNdc = ((float)x / (W - 1)) - 0.5f;
            float yNdc = ((float)y / (H - 1)) - 0.5f;
            // Normalizzazione dell'elevazione tra 0 e 1, poi scalatura e offset per enfatizzare il rilievo
            float zNorm = (ghiacciaio(x, y) - zMin) / (zMax - zMin);
            float zNdc = (zNorm * 0.4f) - 0.2f;

            vertices.push_back(xNdc);
            vertices.push_back(yNdc);
            vertices.push_back(zNdc);
        }
    }

    // GENERAZIONE DELLA TOPOLOGIA DELLA MESH
    // Creazione degli indici per formare triangoli a partire dalla griglia rettangolare di vertici
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

    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(sf::Context::getFunction))) {
        std::cout << "Errore inizializzazione GLAD!" << std::endl;
        return -1;
    }

    // Configurazione dello stato di rendering
    glViewport(0, 0, 800, 600);
    glEnable(GL_DEPTH_TEST);  // Attivazione del depth testing per l'occlusion
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);  // Rendering in wireframe per visualizzare la topologia

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

    // ALLOCAZIONE E CONFIGURAZIONE DEI BUFFER GPU
    // VAO (Vertex Array Object) registra la configurazione dei buffer e degli attributi
    // VBO (Vertex Buffer Object) memorizza i dati dei vertici
    // EBO (Element Buffer Object) memorizza gli indici per il rendering indicizzato
    unsigned int VAO, VBO, EBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);
    // Caricamento dei dati geometrici nel VBO
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
    // Caricamento degli indici di rendering nell'EBO
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    // Configurazione dell'attributo di vertice: 3 componenti float per vertice
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Acquisizione delle locazioni dei uniform per le matrici di trasformazione
    int modelLoc = glGetUniformLocation(shaderProgram, "model");
    int viewLoc  = glGetUniformLocation(shaderProgram, "view");
    int projLoc  = glGetUniformLocation(shaderProgram, "projection");

    // GAME LOOP
    // Elaborazione degli eventi e rendering del frame fino alla chiusura della finestra
    while (window.isOpen()) {
        while (const std::optional<sf::Event> event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                window.close();
            }
            // Gestione del ridimensionamento dinamico della viewport
            else if (const auto* resized = event->getIf<sf::Event::Resized>()) {
                glViewport(0, 0, resized->size.x, resized->size.y);
            }
        }

        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(shaderProgram);

        // Calcolo dinamico dell'aspect ratio per evitare distorsioni durante il ridimensionamento
        float width = static_cast<float>(window.getSize().x);
        float height = static_cast<float>(window.getSize().y);
        float aspectRatio = width / height;

        // COSTRUZIONE DELLE MATRICI DI TRASFORMAZIONE (GLM)
        // Matrice di proiezione: prospettiva con FOV 45°, near plane 0.1, far plane 100
        glm::mat4 projection = glm::perspective(glm::radians(45.0f), aspectRatio, 0.1f, 100.0f);
        
        // Matrice di vista: posizionamento e orientamento della telecamera nello spazio mondo
        glm::mat4 view = glm::lookAt(glm::vec3(0.0f, -1.2f, 0.8f),  // Posizione telecamera
                                     glm::vec3(0.0f, 0.0f, 0.0f),  // Punto osservato (centro)
                                     glm::vec3(0.0f, 0.0f, 1.0f)); // Vettore up (asse Z)
        
        // Matrice di modello: posizionamento e orientamento della geometria nello spazio mondo
        glm::mat4 model = glm::mat4(1.0f);

        // Trasferimento delle matrici ai uniform del programma shader
        glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projection));
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

        // Rendering della mesh indicizzata
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