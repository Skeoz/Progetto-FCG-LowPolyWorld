# Progetto FCG: LowPolyWorld - Esame di Grafica 3D

**Autore:** Andrea Peri 5415544  
**Corso:** Fondamenti di Computer Grafica

Questo progetto implementa un motore di rendering 3D scritto in C++ e OpenGL 4.1. L'obiettivo finale (Tappa 15) è l'esplorazione interattiva in prima persona di un modello digitale di elevazione (DEM) su scala di 5 km quadrati del ghiacciaio dell'Aletsch, completo di ciclo giorno/notte, nebbia atmosferica basata sulla distanza, frustum culling spaziale e fisica di collisione (ground clamping e push-back per i modelli solidi).

## Architettura e Dipendenze
Il progetto è stato ingegnerizzato per essere **plug-and-play**. Non è necessario scaricare manualmente alcuna libreria esterna, a patto di avere un ambiente di sviluppo C++ standard configurato (es. MSYS2/MinGW su Windows o GCC/Clang su Linux) e una connessione a internet.

Il sistema di build si occuperà di recuperare in automatico:
* **SFML 3.0** (Gestione finestra, input e contesto OpenGL)
* **GLM 1.0.3** (Matematica vettoriale e matriciale)

*Nota: La libreria **GLAD** (per il caricamento dei puntatori OpenGL 4.1) è già inclusa staticamente all'interno della directory `librerie/glad`.*

---

## Come compilare il progetto

Il file `CMakeLists.txt` presente nella root è programmato per rilevare, scansionare e compilare **tutte le 15 Tappe contemporaneamente** con un singolo comando. Le dipendenze verranno collegate staticamente per garantire che gli eseguibili funzionino senza richiedere file `.dll` esterni.

### Metodo 1: IDE Moderni (Visual Studio Code)
Se utilizzi VS Code con l'estensione **CMake Tools**, la configurazione è totalmente automatica:
1. Apri la cartella root del progetto con VS Code.
2. L'estensione creerà in automatico la cartella `build` e scaricherà le dipendenze.
3. Premi il pulsante "Build" (o compila tutte le tappe) dalla barra di stato inferiore.

### Metodo 2: Compilazione Universale da Terminale
Se preferisci compilare manualmente o utilizzi un ambiente privo di estensioni CMake, apri il terminale nella directory radice del progetto e lancia questa sequenza:

**Fase 1:** Crea e accedi alla cartella di build:
```bash
mkdir build
cd build
```

**Fase 2:** Genera i file di configurazione (scaricando le dipendenze in background):
```bash
cmake ..
```
*(Nota: su MSYS2 Windows potrebbe essere necessario specificare il generatore, es: `cmake .. -G Ninja`)*

**Fase 3:** Avvia la compilazione massiva di tutte le tappe:
```bash
cmake --build .
```

## Esecuzione delle Tappe

Durante la fase di build, CMake è istruito per copiare automaticamente la `Cartella-risorse` (contenente il file `.asc` del DEM, texture e modelli OBJ) all'interno della cartella `build`. 

Questo significa che **non è necessario passare argomenti speciali o file aggiuntivi da riga di comando**. 

Per avviare una qualsiasi tappa, rimani all'interno della cartella `build` e chiama direttamente l'eseguibile desiderato. Ad esempio, per lanciare la tappa finale:

**Su Windows:**
```bash
./Tappa15.exe
```
*(Oppure fai semplicemente doppio clic sull'eseguibile dall'Esplora File di Windows).*

**Su Linux/Mac:**
```bash
./Tappa15
```

---

## Controlli e Interfaccia Utente (UI)

Il sistema di controllo simula una telecamera in prima persona in stile "drone", con la particolarità (nelle tappe avanzate) di essere vincolata alla topografia del terreno per evitare di compenetrare il suolo (Ground Clamping) o i modelli fisici come il bivacco.

### Mouse
* **Movimento del Mouse:** Controlla la visuale (Pitch e Yaw). Il cursore è catturato e nascosto al centro dello schermo per permettere una rotazione continua.

### Tastiera (Navigazione)
* **W**: Avanza nella direzione dello sguardo.
* **S**: Indietreggia.
* **A**: Traslazione laterale a sinistra (Strafe).
* **D**: Traslazione laterale a destra (Strafe).
* **Barra Spaziatrice**: Guadagna quota (Sali verticalmente lungo l'asse Z globale).
* **Shift Sinistro**: Perdi quota (Scendi verticalmente lungo l'asse Z globale).

### Tastiera (Funzioni di Sistema / UI)
* **TAB (Cruciale):** Sgancia/Aggancia il cursore del mouse. Usalo per liberare il cursore e poter chiudere la finestra, spostarla o interagire con il sistema operativo senza far impazzire la telecamera.
* **P**: Metti in pausa / Riprendi lo scorrimento del tempo (arresta il ciclo solare e l'aggiornamento delle ombre).
* **ESC**: Termina immediatamente l'applicazione.

### Telemetria HUD (Tappa 13 in poi)
In alto a sinistra dello schermo è sovraimpresso un HUD vettoriale color ambra che fornisce feedback istantaneo su:
* **FPS:** Aggiornamento frame rate reale.
* **FASE:** Momento della giornata calcolato in base all'angolo del sole (Mattina, Mezzogiorno, Pomeriggio, Sera, Notte).
* **GPS:** Coordinate XYZ mondiali per orientarsi nella mappa da 5km quadrati.
* **TEMPO:** Stato del motore (In pausa / Scorre).