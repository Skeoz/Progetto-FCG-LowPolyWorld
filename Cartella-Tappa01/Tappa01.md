# Istruzioni di Build Generali

Il progetto è gestito interamente tramite **CMake**. Tutte le dipendenze necessarie (SFML 3.0, GLAD, GLM) sono state incluse localmente nel progetto utilizzando rigorosamente percorsi relativi. Questo garantisce che la compilazione vada a buon fine su qualsiasi macchina senza richiedere installazioni di librerie a livello di sistema.

Per compilare e avviare il progetto:
1. Aprire la cartella radice del progetto.
2. Utilizzare CMake per configurare il progetto e generare i file di build.
3. Selezionare il target desiderato
4. Compilare ed eseguire. Il file `CMakeLists.txt` è stato istruito per gestire automaticamente le dipendenze a runtime (dettagli nella Tappa 01).

---

# Tappa 01: Inizializzazione Finestra e Contesto OpenGL

## Obiettivo
Il primo step del progetto è consistito nel gettare le fondamenta dell'applicazione grafica. L'obiettivo era inizializzare un contesto OpenGL 4.1 Core Profile e generare una finestra di rendering funzionante utilizzando la libreria SFML (aggiornata alla versione 3.0), pulendo i buffer grafici per ottenere uno schermo a tinta unita (azzurro cielo).

## Comandi per il Giocatore
In questa fase esplorativa non sono previsti comandi interattivi per l'utente, eccezion fatta per la chiusura della finestra tramite il pulsante standard del sistema operativo o l'interruzione del processo.

## Problemi Riscontrati e Soluzioni
Durante la primissima fase di test, il codice compilava correttamente generando l'eseguibile, ma all'avvio il programma andava in crash senza mostrare alcuna finestra. 
Analizzando il problema, è emerso che l'eseguibile non riusciva a trovare le librerie dinamiche (i file `.dll` di SFML) necessarie per il runtime, poiché queste si trovavano in una cartella separata rispetto all'output della build.
Invece di costringere l'utente a copiare manualmente i file a ogni compilazione, ho risolto il problema alla radice intervenendo sul sistema di build. Ho inserito un comando `add_custom_command(POST_BUILD ...)` all'interno del `CMakeLists.txt`: in questo modo, al termine di ogni compilazione riuscita, CMake copia in automatico e silenziosamente tutte le `.dll` necessarie direttamente nella cartella di destinazione dell'eseguibile, garantendo un avvio immediato e "plug-and-play".

## Screenshot
![Finestra Iniziale](screenshot1.png)

---