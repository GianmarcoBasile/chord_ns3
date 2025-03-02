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
  LOOKUP_RESPONSE = 2,
  STORE_FILE_REQUEST = 3,
  STORE_FILE_RESPONSE = 4,
  GET_FILE_REQUEST = 5,
  GET_FILE_RESPONSE = 6,
  FORWARDING_RESPONSE = 7
};

// Struttura per i messaggi Chord
class ChordMessage : public Header {
public:
  ChordMessage() : 
    type(LOOKUP_REQUEST),
    sourceId(0),
    destinationId(0),
    fileId(0),
    responseNodeId(0),
    hopCount(0),
    fileSize(0),
    forwardedToNodeId(0),
    success(false)
  {}
  
  // Metodi richiesti dall'interfaccia Header
  static TypeId GetTypeId(void) {
    static TypeId tid = TypeId("ChordMessage")
      .SetParent<Header>()
      .AddConstructor<ChordMessage>();
    return tid;
  }
  
  virtual TypeId GetInstanceTypeId(void) const {
    return GetTypeId();
  }
  
  virtual uint32_t GetSerializedSize(void) const {
    return sizeof(uint8_t) + 7 * sizeof(uint32_t) + sizeof(bool);
  }
  
  virtual void Serialize(Buffer::Iterator start) const {
    start.WriteU8(static_cast<uint8_t>(type));
    start.WriteU32(sourceId);
    start.WriteU32(destinationId);
    start.WriteU32(fileId);
    start.WriteU32(responseNodeId);
    start.WriteU32(hopCount);
    start.WriteU32(fileSize);
    start.WriteU32(forwardedToNodeId);
    start.WriteU8(success ? 1 : 0);
  }
  
  virtual uint32_t Deserialize(Buffer::Iterator start) {
    type = static_cast<ChordMessageType>(start.ReadU8());
    sourceId = start.ReadU32();
    destinationId = start.ReadU32();
    fileId = start.ReadU32();
    responseNodeId = start.ReadU32();
    hopCount = start.ReadU32();
    fileSize = start.ReadU32();
    forwardedToNodeId = start.ReadU32();
    success = (start.ReadU8() == 1);
    return GetSerializedSize();
  }
  
  virtual void Print(std::ostream &os) const {
    os << "ChordMessage [Type=" << static_cast<int>(type) 
       << ", SourceId=" << sourceId 
       << ", DestinationId=" << destinationId 
       << ", FileId=" << fileId 
       << ", HopCount=" << hopCount << "]";
  }
  
  // Membri
  ChordMessageType type;
  uint32_t sourceId;
  uint32_t destinationId;
  uint32_t fileId;
  uint32_t responseNodeId;
  uint32_t hopCount;
  uint32_t fileSize;
  uint32_t forwardedToNodeId;
  bool success;
};

// Struttura per memorizzare le informazioni di ogni nodo nel network Chord
struct ChordInfo {
  uint32_t chordId;
  Ipv4Address realIp;
  Ptr<Node> node;
  vector<uint32_t> fingerTable;  // Indici dei nodi nella finger table
  uint32_t predecessor;          // Indice del predecessore
  vector<uint32_t> fileIds;      // Lista degli ID dei file memorizzati dal nodo
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