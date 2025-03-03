#ifndef CHORD_APPLICATION_H
#define CHORD_APPLICATION_H

#include "ns3/application.h"
#include "ns3/socket.h"
#include "ns3/socket-factory.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"
#include "ns3/simulator.h"
#include "ns3/udp-socket-factory.h"
#include "chord-header.h"
#include <functional>  // Per std::function

using namespace ns3;
using namespace std;

// Definizione del tipo di callback per le ricerche di file completate
typedef function<void(uint32_t, bool, uint32_t)> FileLookupCompletedCallback;

// Struttura per memorizzare i dati dei file
struct FileData {
  uint32_t fileId;
  uint32_t fileSize;
};

// Classe per l'applicazione Chord
class ChordApplication : public Application {
public:
  static TypeId GetTypeId(void);

  ChordApplication();
  virtual ~ChordApplication();

  void Setup(uint32_t chordId, const vector<ChordInfo>& chordNodes);
  void LookupKey(uint32_t key);
  void PrintStats();

  // Metodo per ottenere il chordId del nodo
  uint32_t GetChordId() const { return m_chordId; }

  // Nuovi metodi per la gestione dei file
  void AddFile(uint32_t fileId);
  bool HasFile(uint32_t fileId);
  void StoreFile(uint32_t fileId, uint32_t fileSize, uint32_t hopCount);
  void GetFile(uint32_t fileId, uint32_t hopCount);
  vector<uint32_t> GetStoredFiles();
  
  // Metodi per iniziare operazioni di archiviazione e ricerca
  void InitiateStoreFile(uint32_t fileId, uint32_t fileSize);
  void InitiateGetFile(uint32_t fileId);
  
  // Metodo per verificare se il nodo è responsabile per una chiave
  bool IsResponsibleForKey(uint32_t key);
  
  // Metodo per trovare il nodo precedente più lontano per il routing
  uint32_t FindFarthestPrecedingNode(uint32_t key);
  
  // Metodo per registrare il callback per il completamento della ricerca di file
  void SetFileLookupCompletedCallback(FileLookupCompletedCallback callback);

  // Nota: per verificare se un ID è in un intervallo, utilizzare la funzione globale isInRange definita in chord-header.h

private:
  virtual void StartApplication(void);
  virtual void StopApplication(void);

  void SendLookupRequest(uint32_t key, uint32_t targetNodeIdx);
  void HandleLookupRequest(ChordMessage msg, Address from);
  void SendLookupResponse(uint32_t key, uint32_t targetId, uint32_t responseNodeId);
  void HandleLookupResponse(ChordMessage msg);
  void HandleRead(Ptr<Socket> socket);

  // Nuovi metodi privati per la gestione dei messaggi di file
  void SendStoreFileRequest(uint32_t fileId, uint32_t fileSize, uint32_t targetIdx);
  void HandleStoreFileRequest(ChordMessage message, Address sourceAddress);
  void SendStoreFileResponse(uint32_t fileId, uint32_t targetId, bool success);
  void HandleStoreFileResponse(ChordMessage msg);
  void SendGetFileRequest(uint32_t fileId, uint32_t targetNodeIdx);
  void SendGetFileRequestWithHopCount(uint32_t fileId, uint32_t targetIdx, uint32_t hopCount);
  void HandleGetFileRequest(ChordMessage message, Address sourceAddress);
  void SendGetFileResponse(uint32_t fileId, uint32_t targetIdx, bool hasFile, uint32_t hopCount);
  void SendForwardingResponse(uint32_t fileId, uint32_t targetId, uint32_t forwardedToNodeId, uint32_t hopCount);
  void SendForwardingRequest(uint32_t fileId, uint32_t targetIdx, uint32_t originalSourceId, uint32_t hopCount);
  void HandleGetFileResponse(ChordMessage msg);
  void HandleForwardingResponse(ChordMessage msg);
  
  // Metodo per creare un messaggio di archiviazione file
  ChordMessage CreateStoreFileMessage(uint32_t fileId, uint32_t fileSize, uint32_t hopCount);

  Ptr<Socket> m_socket;
  uint32_t m_chordId;
  uint32_t m_myIndex;
  vector<ChordInfo> m_chordNodes;
  bool m_running;
  uint32_t m_packetsSent;
  uint32_t m_packetsReceived;
  
  // Membri per la gestione dei file
  vector<FileData> m_storedFiles;
  uint32_t m_totalFilesStored;
  uint32_t m_totalBytesStored;
  
  // Callback per il completamento della ricerca di file
  FileLookupCompletedCallback m_fileLookupCompletedCallback;
  
  // Porta dell'applicazione
  uint16_t m_applicationPort;
  
  // Mappa per tenere traccia dei nodi visitati durante il routing
  // La chiave è l'ID del file, il valore è un set di ID dei nodi visitati
  map<uint32_t, set<uint32_t>> m_visitedNodes;
};

#endif // CHORD_APPLICATION_H 