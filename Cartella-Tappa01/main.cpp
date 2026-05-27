#include <glad/glad.h>
#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>
#include <iostream>

int main() {
    // Richiesta a SFML di preparare un contesto OpenGL 4.1 Core
    sf::ContextSettings settings;
    settings.depthBits = 24;
    settings.majorVersion = 4;
    settings.minorVersion = 1;
    settings.attributeFlags = sf::ContextSettings::Core;

    // Creazione della finestra 
    sf::Window window(sf::VideoMode({800, 600}), "Progetto FCG - LowPolyWorld", sf::State::Windowed, settings);

    // Caricamento puntatori di OpenGL tramite glad
    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(sf::Context::getFunction))) {
        std::cout << "Errore critico: Impossibile inizializzare GLAD!" << std::endl;
        return -1;
    }

    // Impostazioni area disegno OpenGL
    glViewport(0, 0, 800, 600);

    // Game Loop (Ciclo principale)
    while (window.isOpen()) {
        
        // Gestione degli eventi SFML
        while (const std::optional<sf::Event> event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                window.close();
            }
        }

        // 5. Rendering: Pulizia e colore azzurro
        glClearColor(0.2f, 0.5f, 0.8f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Output finestra
        window.display();
    }

    return 0;
}