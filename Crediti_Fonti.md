# Crediti e Fonti

Il presente progetto è stato sviluppato come elaborato finale per l'esame di Fondamenti di Computer Grafica. Si dichiara che l'architettura di base (motore di rendering, gestione della telecamera, shader procedurali, HUD 2D e calcolo delle collisioni) è stata progettata e sviluppata autonomamente.

Tuttavia, in conformità con le direttive d'esame, si riconoscono e si citano esplicitamente le seguenti risorse di terze parti, fornite dal **Prof. Luigi Rocca**, che si sono rivelate fondamentali per l'implementazione del caricamento di dati reali del terreno:

## 1. Parser C++ per il formato ESRI ASC (Digital Elevation Model)
Il modulo software utilizzato per la lettura e l'interpretazione dei file DEM in formato `.asc` è stato gentilmente fornito dal Prof. Rocca.
* **Modulo fornito:** `AscDEM.zip` (comprendente le classi e i file sorgente `dem.cc`, `dem.hh`, ecc.)
* **Scopo e Utilizzo:** Il codice è stato integrato all'interno del progetto per estrarre i dati altimetrici strutturati in griglia (tramite la classe `Dem`). Questi dati sono stati successivamente elaborati dall'algoritmo del motore grafico per generare dinamicamente i vertici e le facce indicizzate (strutture VAO, VBO, EBO) necessarie al rendering OpenGL.

## 2. Dataset Altimetrico Reale (Ghiacciaio dell'Aletsch)
La topografia del paesaggio montuoso renderizzato nell'applicazione si basa su dati altimetrici reali (Digital Elevation Model), anch'essi forniti dal docente in fase di approvazione del progetto.
* **File sorgente:** `aletsch_32T.asc`
* **Scopo e Utilizzo:** Questo file contiene le rilevazioni altimetriche reali del Ghiacciaio dell'Aletsch. Costituisce la mappa di base caricata dal motore grafico ed esplorabile in tempo reale, interpolata ed espansa spazialmente fino a coprire grandi distanze (5 km quadrati) nella ultima build(Tappa 15).

Si ringrazia sentitamente il Prof. Rocca per la fornitura del materiale e per l'incoraggiamento a spingere il progetto oltre il semplice rendering procedurale per abbracciare l'elaborazione di dati cartografici reali.