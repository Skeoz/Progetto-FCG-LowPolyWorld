# Crediti e Fonti

Il presente progetto è stato sviluppato come elaborato finale per l'esame di Fondamenti di Computer Grafica. Si dichiara che l'architettura di base (motore di rendering, gestione della telecamera, shader procedurali, ground clamping, parser OBJ custom e calcolo delle collisioni vettoriali) è stata progettata, assemblata e strutturata autonomamente.

In conformità con le direttive d'esame e le moderne pratiche di sviluppo software, si riconoscono e si citano esplicitamente le seguenti risorse di terze parti e strumenti di supporto utilizzati durante l'implementazione:

## 1. Parser C++ per il formato ESRI ASC (Digital Elevation Model)
Il modulo software utilizzato per la lettura e l'interpretazione dei file DEM in formato .asc è stato gentilmente fornito dal **Prof. Luigi Rocca**.

* **Modulo fornito**: AscDEM.zip (comprendente le classi e i file sorgente dem.cc, dem.hh, ecc.)

* **Scopo e Utilizzo**: Il codice è stato integrato all'interno del progetto per estrarre i dati altimetrici strutturati in griglia (tramite la classe Dem). Questi dati sono stati successivamente elaborati dall'algoritmo del motore grafico per generare dinamicamente i vertici e le facce indicizzate (strutture VAO, VBO, EBO) necessarie al rendering OpenGL.

## 2. Dataset Altimetrico Reale (Ghiacciaio dell'Aletsch)
La topografia del paesaggio montuoso renderizzato nell'applicazione si basa su dati altimetrici reali (Digital Elevation Model), anch'essi forniti dal docente in fase di approvazione del progetto.

* **File sorgente**: aletsch_32T.asc

* **Scopo e Utilizzo**: Questo file contiene le rilevazioni altimetriche reali del Ghiacciaio dell'Aletsch. Costituisce la mappa di base caricata dal motore grafico ed esplorabile in tempo reale, interpolata ed espansa spazialmente fino a coprire 5 km quadrati nella build finale (Tappa 15).

## 3. Assistenza allo Sviluppo (Intelligenza Artificiale)
Per ottimizzare i tempi di sviluppo, effettuare debugging e garantire la solidità del codice, ci si è avvalsi del supporto di un Large Language Model (LLM) utilizzato in veste di strumento di pair-programming e problem solving avanzato. Nello specifico, l'IA è stata interrogata per la stesura e l'ottimizzazione dei seguenti task isolati:

* **Matematica del Frustum Culling**: Generazione dell'algoritmo di estrazione algebrica dei sei piani di taglio a partire dalla matrice composita (View-Projection).

* **Generazione HUD Vettoriale**: Calcolo delle coordinate dei segmenti geometrici necessari per il rendering a schermo dei caratteri alfanumerici in puro OpenGL Core Profile, aggirando la necessità di librerie esterne per i font.

* **DevOps e Build System**: Stesura e ottimizzazione del file CMakeLists.txt per il linking statico delle dipendenze, la risoluzione di conflitti DLL e la gestione del download automatico tramite FetchContent.

Si ringrazia sentitamente il **Prof. Rocca** per la fornitura del materiale cartografico e per l'incoraggiamento a spingere il progetto oltre il semplice rendering procedurale.