#include <glad/glad.h>
#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>
#include <iostream>

int main() {
    // 1. Chiediamo a SFML di prepararci un contesto OpenGL 4.1 Core
    sf::ContextSettings settings;
    settings.depthBits = 24;
    settings.majorVersion = 4;
    settings.minorVersion = 1;
    settings.attributeFlags = sf::ContextSettings::Core;

    // 2. Creiamo la finestra (Sintassi rigorosa SFML 3.0)
    sf::Window window(sf::VideoMode({800, 600}), "Progetto FCG - LowPolyWorld", sf::State::Windowed, settings);

    // 3. Carichiamo i puntatori di OpenGL tramite glad
    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(sf::Context::getFunction))) {
        std::cout << "Errore critico: Impossibile inizializzare GLAD!" << std::endl;
        return -1;
    }

    // Diciamo a OpenGL quanto è grande la nostra area di disegno
    glViewport(0, 0, 800, 600);

    // 4. Il Game Loop (Ciclo principale)
    while (window.isOpen()) {
        
        // Gestione degli eventi alla maniera di SFML 3.0
        while (const std::optional<sf::Event> event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                window.close();
            }
        }

        // 5. Rendering: Puliamo lo schermo e lo coloriamo di un bel cielo azzurro
        glClearColor(0.2f, 0.5f, 0.8f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Mostriamo a schermo quello che abbiamo disegnato
        window.display();
    }

    return 0;
}