#ifndef CHORD_HEADER_H
#define CHORD_HEADER_H

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include <vector>
#include <string>
#include <functional>

using namespace ns3;
using namespace std;

// Costanti per la rete Chord
const uint32_t CHORD_BITS = 14;  // 2^14 = 16384 possibili ID
const uint32_t CHORD_SIZE = 1 << CHORD_BITS;
const uint32_t FINGER_TABLE_SIZE = CHORD_BITS;
const uint16_t CHORD_PORT = 8000;  // Porta per le comunicazioni Chord

// Tipi di messaggi Chord
enum ChordMessageType {
  LOOKUP_REQUEST = 1,
  LOOKUP_RESPONSE = 2
};

// Struttura per i messaggi Chord
struct ChordMessage {
  ChordMessageType type;
  uint32_t sourceId;
  uint32_t targetId;
  uint32_t key;
  uint32_t responseNodeId;
};

// Struttura per memorizzare le informazioni di ogni nodo nel network Chord
struct ChordInfo {
  uint32_t chordId;
  Ipv4Address realIp;
  Ptr<Node> node;
  vector<uint32_t> fingerTable;  // Indici dei nodi nella finger table
  uint32_t predecessor;          // Indice del predecessore
};

// Funzione per verificare se id è nell'intervallo (start, end) nel ring Chord
inline bool isInRange(uint32_t id, uint32_t start, uint32_t end) {
  if (start < end) {
    return (id > start && id <= end);
  } else {  // L'intervallo attraversa lo zero
    return (id > start || id <= end);
  }
}

// Funzione per trovare il successore di un ID nella rete Chord
uint32_t findSuccessor(uint32_t id, const vector<ChordInfo>& chordNodes);

#endif // CHORD_HEADER_H 