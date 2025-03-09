# Simulatore del Protocollo Chord in ns-3

## Descrizione

Questo progetto implementa una simulazione del protocollo Chord utilizzando il framework di simulazione di rete ns-3. Chord è un protocollo di lookup distribuito che fornisce un servizio di hash distribuito (DHT) per applicazioni peer-to-peer, permettendo di localizzare efficacemente risorse in una rete decentralizzata.

La simulazione permette di:

- Creare una rete di nodi Chord con ID distribuiti in uno spazio circolare
- Inserire file nella rete
- Simulare guasti di nodi
- Eseguire operazioni di lookup per trovare file
- Raccogliere statistiche dettagliate sulle prestazioni

## Requisiti

- ns-3 (versione 3.43 o superiore)
- Compilatore C++ con supporto C++11
- Ambiente Linux/Unix (testato su Ubuntu 22.04)

## Installazione

1. Assicurarsi di avere ns-3 installato correttamente:

   ```bash
   cd ~/ns-allinone-3.43/ns-3.43/
   ```

2. Copiare il file `chord.cc` nella directory `scratch/chord/`:

   ```bash
   mkdir -p scratch/chord
   cp /path/to/chord.cc scratch/chord/
   ```

3. Compilare il progetto:
   ```bash
   ./ns3 build scratch/chord/chord.cc
   ```

## Utilizzo

### Esecuzione base

Per eseguire la simulazione con i parametri predefiniti:

```bash
./ns3 build scratch/chord/chord.cc
```

### Parametri configurabili

La simulazione accetta diversi parametri da riga di comando:

```bash
./ns3 run "scratch/new_chord/chord --m=14 --nodes=100 --files=50 --lookups=25 --failing=10 --seed=1 --csv=results.csv"
```

Parametri disponibili:

- `--m`: Numero di bit per lo spazio degli ID (default: 14)
- `--nodes`: Numero di nodi nella rete (default: 10)
- `--files`: Numero di file da inserire nella rete (default: 5)
- `--lookups`: Numero di lookup da eseguire (default: 3)
- `--failing`: Numero di nodi che falliranno durante la simulazione (default: 0)
- `--seed`: Seed per il generatore di numeri casuali (default: 1)
- `--csv`: Nome del file CSV in cui salvare le statistiche (default: "chord_stats.csv")

## Output e statistiche

### Output a console

Durante l'esecuzione, la simulazione mostra informazioni dettagliate su:

- Inizializzazione della rete
- Creazione delle finger table
- Inserimento dei file
- Fallimenti dei nodi
- Operazioni di lookup
- Statistiche in tempo reale

### Statistiche finali

Al termine della simulazione, vengono mostrate statistiche complete:

- Parametri della simulazione (nodi, file, lookup, nodi falliti)
- Numero totale di lookup eseguiti
- Numero e percentuale di lookup riusciti/falliti
- Media, minimo e massimo numero di hop per i lookup riusciti
- Media teorica (log₂(N)) per confronto

### File CSV

Le statistiche vengono salvate in un file CSV con le seguenti colonne:

- NumNodes: Numero di nodi nella rete
- NumFiles: Numero di file inseriti
- NumLookups: Numero di lookup eseguiti
- FailingNodes: Numero di nodi che hanno fallito
- TotalLookups: Totale lookup effettivamente eseguiti
- SuccessfulLookups: Numero di lookup riusciti
- FailedLookups: Numero di lookup falliti
- SuccessRate: Percentuale di successo (%)
- AverageHops: Media degli hop per i lookup riusciti
- MinHops: Minimo numero di hop per un lookup riuscito
- MaxHops: Massimo numero di hop per un lookup riuscito
- TheoreticalAverage: Media teorica (log₂(N))

## Struttura del codice

Il codice è organizzato nelle seguenti componenti principali:

- **ChordMessage**: Struttura per i messaggi scambiati tra i nodi
- **ChordNode**: Struttura che rappresenta un nodo nella rete Chord
- **ChordApplication**: Classe che implementa l'applicazione Chord su ogni nodo
- **ChordNetwork**: Classe che gestisce la rete Chord e la simulazione

### Fasi della simulazione

1. **Inizializzazione della rete**:

   - Creazione dei nodi fisici
   - Assegnazione di ID Chord casuali
   - Inizializzazione delle finger table e successor list

2. **Inserimento dei file**:

   - Generazione di ID casuali per i file
   - Determinazione del nodo responsabile per ogni file
   - Invio di messaggi STORE_FILE

3. **Simulazione dei fallimenti**:

   - Selezione casuale di nodi da far fallire
   - Disattivazione dei nodi selezionati

4. **Esecuzione dei lookup**:

   - Selezione di file da cercare
   - Avvio di lookup da nodi casuali
   - Routing dei messaggi attraverso la rete

5. **Raccolta delle statistiche**:
   - Conteggio di lookup riusciti/falliti
   - Calcolo della media, minimo e massimo degli hop
   - Scrittura dei risultati su console e file CSV

## Il protocollo Chord

Chord è un protocollo di lookup distribuito che organizza i nodi in un anello logico. Ogni nodo e risorsa ha un identificatore (ID) nello spazio [0, 2ᵐ-1]. Un nodo è responsabile per le chiavi che sono uguali o precedono il suo ID ma seguono l'ID del suo predecessore.

Caratteristiche principali:

- Routing efficiente: O(log N) hop per trovare una risorsa
- Scalabilità: funziona bene anche con migliaia di nodi
- Robustezza: continua a funzionare anche quando alcuni nodi falliscono
