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
typedef function<void(uint32_t, bool)> FileLookupCompletedCallback;

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
  void StoreFile(uint32_t fileId);
  void GetFile(uint32_t fileId);
  vector<uint32_t> GetStoredFiles();
  
  // Metodo per registrare il callback per il completamento della ricerca di file
  void SetFileLookupCompletedCallback(FileLookupCompletedCallback callback);

  // Metodo di supporto
  bool isInRange(uint32_t id, uint32_t fromId, uint32_t toId) {
    if (fromId < toId) {
      return (id > fromId && id <= toId);
    } else {
      return (id > fromId || id <= toId);
    }
  }

private:
  virtual void StartApplication(void);
  virtual void StopApplication(void);

  void SendLookupRequest(uint32_t key, uint32_t targetNodeIdx);
  void HandleLookupRequest(ChordMessage msg, Address from);
  void SendLookupResponse(uint32_t key, uint32_t targetId, uint32_t responseNodeId);
  void HandleLookupResponse(ChordMessage msg);
  void HandleRead(Ptr<Socket> socket);

  // Nuovi metodi privati per la gestione dei messaggi di file
  void SendStoreFileRequest(uint32_t fileId, uint32_t targetNodeIdx);
  void HandleStoreFileRequest(ChordMessage msg);
  void SendStoreFileResponse(uint32_t fileId, uint32_t targetId, bool success);
  void HandleStoreFileResponse(ChordMessage msg);
  void SendGetFileRequest(uint32_t fileId, uint32_t targetNodeIdx);
  void HandleGetFileRequest(ChordMessage msg);
  void SendGetFileResponse(uint32_t fileId, uint32_t targetId, bool hasFile);
  void SendForwardingResponse(uint32_t fileId, uint32_t targetId, uint32_t forwardedToNodeId);
  void HandleGetFileResponse(ChordMessage msg);

  Ptr<Socket> m_socket;
  uint32_t m_chordId;
  uint32_t m_myIndex;
  vector<ChordInfo> m_chordNodes;
  bool m_running;
  uint32_t m_packetsSent;
  uint32_t m_packetsReceived;
  
  // Nuovi membri per tenere traccia delle ricerche di file
  map<uint32_t, bool> m_pendingFileLookups;  // Mappa degli ID file in attesa di risposta
  FileLookupCompletedCallback m_fileLookupCompletedCallback;
};

#endif // CHORD_APPLICATION_H 