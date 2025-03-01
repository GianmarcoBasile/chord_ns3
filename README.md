# Chord su NS-3

Implementazione di una rete Chord su NS-3.

## Struttura del progetto

- `chord.cc`: Implementazione completa della rete Chord con finger table e operazioni di lookup
- `net_base.cc`: File di base che costruisce la rete sottostante (rinominato da net.cc)

## Come compilare ed eseguire

1. Rinominare `net.cc` in `net_base.cc` per evitare conflitti di funzioni main:

   ```
   mv net.cc net_base.cc
   ```

2. Compilare ed eseguire il codice:
   ```
   cd /home/gianmarco/Code/Università/p2p/ns-allinone-3.43/ns-3.43
   ./ns3 run scratch/chord_ns3/chord
   ```

## Funzionalità implementate

- Creazione di una rete Chord con ID a 14 bit (spazio di 16384 ID)
- Finger table complete per ogni nodo
- Operazioni di lookup efficienti con routing logaritmico
- Visualizzazione delle finger table e dei risultati delle operazioni di lookup

## Funzionalità da implementare

- Operazioni di join/leave
- Stabilizzazione della rete
- Replicazione dei dati
