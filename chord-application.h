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

using namespace ns3;
using namespace std;

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
  void HandleGetFileResponse(ChordMessage msg);

  Ptr<Socket> m_socket;
  uint32_t m_chordId;
  uint32_t m_myIndex;
  vector<ChordInfo> m_chordNodes;
  bool m_running;
  uint32_t m_packetsSent;
  uint32_t m_packetsReceived;
};

#endif // CHORD_APPLICATION_H 