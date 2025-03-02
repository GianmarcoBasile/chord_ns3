#include "chord-application.h"
#include "chord-helper.h"
#include <iostream>

using namespace ns3;
using namespace std;

NS_LOG_COMPONENT_DEFINE ("ChordApplication");

TypeId 
ChordApplication::GetTypeId(void) {
  static TypeId tid = TypeId("ChordApplication")
    .SetParent<Application>()
    .SetGroupName("Applications")
    .AddConstructor<ChordApplication>();
  return tid;
}

ChordApplication::ChordApplication() 
  : m_socket(0), m_chordId(0), m_running(false), 
    m_packetsSent(0), m_packetsReceived(0) {
  // Inizializziamo il callback come una funzione vuota
  m_fileLookupCompletedCallback = [](uint32_t, bool) {};
}

ChordApplication::~ChordApplication() {
}

void 
ChordApplication::Setup(uint32_t chordId, const vector<ChordInfo>& chordNodes) {
  m_chordId = chordId;
  m_chordNodes = chordNodes;
  
  // Troviamo il nostro indice nella lista dei nodi
  for (uint32_t i = 0; i < m_chordNodes.size(); i++) {
    if (m_chordNodes[i].chordId == m_chordId) {
      m_myIndex = i;
      break;
    }
  }
}

void ChordApplication::SendLookupRequest(uint32_t key, uint32_t targetNodeIdx) {
  if (!m_running) return;

  ChordMessage msg;
  msg.type = LOOKUP_REQUEST;
  msg.sourceId = m_chordId;
  msg.targetId = m_chordNodes[targetNodeIdx].chordId;
  msg.key = key;
  msg.responseNodeId = 0;

  // Serializziamo il messaggio
  uint8_t buffer[20]; // 5 uint32_t * 4 bytes
  buffer[0] = msg.type;
  buffer[1] = 0; // padding
  buffer[2] = 0; // padding
  buffer[3] = 0; // padding
  *((uint32_t*)(buffer + 4)) = msg.sourceId;
  *((uint32_t*)(buffer + 8)) = msg.targetId;
  *((uint32_t*)(buffer + 12)) = msg.key;
  *((uint32_t*)(buffer + 16)) = msg.responseNodeId;

  Ptr<Packet> packet = Create<Packet>(buffer, 20);
  
  // Inviamo il pacchetto al nodo target
  m_socket->Connect(InetSocketAddress(m_chordNodes[targetNodeIdx].realIp, CHORD_PORT));
  m_socket->Send(packet);
  m_packetsSent++;
  
  cout << "Nodo chordId: " << m_chordId 
       << " ha inviato una richiesta di lookup per la chiave " << key 
       << " al nodo chordId: " << m_chordNodes[targetNodeIdx].chordId << endl;
}

void 
ChordApplication::LookupKey(uint32_t key) {
  cout << "Nodo chordId: " << m_chordId 
       << " inizia lookup per chiave " << key << endl;
  
  uint32_t currentIdx = m_myIndex;
  
  // Se la chiave è tra il nodo corrente e il suo successore, il successore è responsabile
  uint32_t successorIdx = m_chordNodes[currentIdx].fingerTable[0];
  if (isInRange(key, m_chordNodes[currentIdx].chordId, m_chordNodes[successorIdx].chordId)) {
    cout << "Trovato localmente! La chiave " << key << " è gestita dal nodo chordId: " 
         << m_chordNodes[successorIdx].chordId << endl;
    return;
  }
  
  // Altrimenti, cerchiamo nella finger table il nodo più vicino ma che non superi la chiave
  for (int i = FINGER_TABLE_SIZE - 1; i >= 0; i--) {
    uint32_t fingerIdx = m_chordNodes[currentIdx].fingerTable[i];
    if (isInRange(m_chordNodes[fingerIdx].chordId, m_chordNodes[currentIdx].chordId, key)) {
      // Inviamo la richiesta al nodo più vicino
      SendLookupRequest(key, fingerIdx);
      return;
    }
  }
  
  // Se non troviamo un nodo migliore nella finger table, passiamo al successore
  SendLookupRequest(key, successorIdx);
}

void 
ChordApplication::HandleLookupRequest(ChordMessage msg, Address from) {
  cout << "Nodo chordId: " << m_chordId 
       << " ha ricevuto una richiesta di lookup per la chiave " << msg.key 
       << " dal nodo con chordId: " << msg.sourceId << endl;
  
  uint32_t currentIdx = m_myIndex;
  
  // Se la chiave è tra il nodo corrente e il suo successore, il successore è responsabile
  uint32_t successorIdx = m_chordNodes[currentIdx].fingerTable[0];
  if (isInRange(msg.key, m_chordNodes[currentIdx].chordId, m_chordNodes[successorIdx].chordId)) {
    // Inviamo la risposta al nodo che ha fatto la richiesta
    SendLookupResponse(msg.key, msg.sourceId, m_chordNodes[successorIdx].chordId);
    return;
  }
  
  // Altrimenti, cerchiamo nella finger table il nodo più vicino ma che non superi la chiave
  bool found = false;
  for (int i = FINGER_TABLE_SIZE - 1; i >= 0; i--) {
    uint32_t fingerIdx = m_chordNodes[currentIdx].fingerTable[i];
    if (isInRange(m_chordNodes[fingerIdx].chordId, m_chordNodes[currentIdx].chordId, msg.key)) {
      // Inoltriamo la richiesta al nodo più vicino
      SendLookupRequest(msg.key, fingerIdx);
      found = true;
      break;
    }
  }
  
  // Se non troviamo un nodo migliore nella finger table, passiamo al successore
  if (!found) {
    SendLookupRequest(msg.key, successorIdx);
  }
}

void 
ChordApplication::SendLookupResponse(uint32_t key, uint32_t targetId, uint32_t responseNodeId) {
  if (!m_running) return;

  // Troviamo l'indice del nodo target
  uint32_t targetIdx = 0;
  for (uint32_t i = 0; i < m_chordNodes.size(); i++) {
    if (m_chordNodes[i].chordId == targetId) {
      targetIdx = i;
      break;
    }
  }

  ChordMessage msg;
  msg.type = LOOKUP_RESPONSE;
  msg.sourceId = m_chordId;
  msg.targetId = targetId;
  msg.key = key;
  msg.responseNodeId = responseNodeId;

  // Serializziamo il messaggio
  uint8_t buffer[20]; // 5 uint32_t * 4 bytes
  buffer[0] = msg.type;
  buffer[1] = 0; // padding
  buffer[2] = 0; // padding
  buffer[3] = 0; // padding
  *((uint32_t*)(buffer + 4)) = msg.sourceId;
  *((uint32_t*)(buffer + 8)) = msg.targetId;
  *((uint32_t*)(buffer + 12)) = msg.key;
  *((uint32_t*)(buffer + 16)) = msg.responseNodeId;

  Ptr<Packet> packet = Create<Packet>(buffer, 20);
  
  // Inviamo il pacchetto al nodo target
  m_socket->Connect(InetSocketAddress(m_chordNodes[targetIdx].realIp, CHORD_PORT));
  m_socket->Send(packet);
  m_packetsSent++;
  
  cout << "Nodo chordId: " << m_chordId 
       << " ha inviato una risposta di lookup per la chiave " << key 
       << " al nodo chordId: " << targetId << endl;
}

void 
ChordApplication::HandleLookupResponse(ChordMessage msg) {
  cout << "Nodo chordId: " << m_chordId 
       << " ha ricevuto una risposta di lookup per la chiave " << msg.key 
       << ": la chiave è gestita dal nodo con chordId: " << msg.responseNodeId << endl;
}

void 
ChordApplication::PrintStats() {
  cout << "Nodo chordId: " << m_chordId 
       << " - Pacchetti inviati: " << m_packetsSent 
       << ", Pacchetti ricevuti: " << m_packetsReceived << endl;
}

void 
ChordApplication::StartApplication(void) {
  m_running = true;
  
  // Creiamo un socket UDP
  if (!m_socket) {
    m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
    m_socket->Bind(InetSocketAddress(Ipv4Address::GetAny(), CHORD_PORT));
    m_socket->SetRecvCallback(MakeCallback(&ChordApplication::HandleRead, this));
  }
}

void 
ChordApplication::StopApplication(void) {
  m_running = false;
  
  if (m_socket) {
    m_socket->Close();
  }
  
  // Stampiamo le statistiche quando l'applicazione si ferma
  PrintStats();
}

void 
ChordApplication::HandleRead(Ptr<Socket> socket) {
  Ptr<Packet> packet;
  Address from;
  while ((packet = socket->RecvFrom(from))) {
    m_packetsReceived++;
    
    uint8_t buffer[20];
    packet->CopyData(buffer, 20);
    
    ChordMessage msg;
    msg.type = (ChordMessageType)buffer[0];
    msg.sourceId = *((uint32_t*)(buffer + 4));
    msg.targetId = *((uint32_t*)(buffer + 8));
    msg.key = *((uint32_t*)(buffer + 12));
    msg.responseNodeId = *((uint32_t*)(buffer + 16));
    
    switch (msg.type) {
      case LOOKUP_REQUEST:
        HandleLookupRequest(msg, from);
        break;
      case LOOKUP_RESPONSE:
        HandleLookupResponse(msg);
        break;
      case STORE_FILE_REQUEST:
        HandleStoreFileRequest(msg);
        break;
      case STORE_FILE_RESPONSE:
        HandleStoreFileResponse(msg);
        break;
      case GET_FILE_REQUEST:
        HandleGetFileRequest(msg);
        break;
      case GET_FILE_RESPONSE:
        HandleGetFileResponse(msg);
        break;
    }
  }
}

// Metodo per aggiungere un file localmente
void 
ChordApplication::AddFile(uint32_t fileId) {
  // Verifichiamo se il file è già presente
  if (HasFile(fileId)) {
    cout << "Nodo chordId: " << m_chordId 
         << " - File con ID " << fileId << " già presente" << endl;
    return;
  }
  
  // Aggiungiamo il file alla lista
  m_chordNodes[m_myIndex].fileIds.push_back(fileId);
  
  cout << "Nodo chordId: " << m_chordId 
       << " - Aggiunto file con ID " << fileId << endl;
}

// Metodo per verificare se un file è presente localmente
bool 
ChordApplication::HasFile(uint32_t fileId) {
  vector<uint32_t>& files = m_chordNodes[m_myIndex].fileIds;
  return find(files.begin(), files.end(), fileId) != files.end();
}

// Metodo per memorizzare un file nella rete Chord
void 
ChordApplication::StoreFile(uint32_t fileId) {
  cout << "Nodo chordId: " << m_chordId 
       << " inizia processo di memorizzazione del file con ID " << fileId << endl;
  
  // In Chord, un file con ID key deve essere memorizzato nel primo nodo
  // il cui ID è uguale o maggiore di key (il successore di key)
  
  // Implementiamo direttamente la logica per trovare il successore qui
  // invece di chiamare FindSuccessor o findSuccessor
  
  // Caso base: un solo nodo
  if (m_chordNodes.size() == 1) {
    AddFile(fileId);
    return;
  }
  
  // Cerchiamo il primo nodo con ID >= fileId
  uint32_t successorIdx = m_myIndex; // Default a noi stessi
  bool found = false;
  
  for (uint32_t i = 0; i < m_chordNodes.size(); i++) {
    if (m_chordNodes[i].chordId >= fileId) {
      successorIdx = i;
      found = true;
      break;
    }
  }
  
  // Se non troviamo un nodo con ID >= fileId, il successore è il nodo con ID minimo (wrap-around)
  if (!found) {
    uint32_t minIdx = 0;
    uint32_t minId = m_chordNodes[0].chordId;
    
    for (uint32_t i = 1; i < m_chordNodes.size(); i++) {
      if (m_chordNodes[i].chordId < minId) {
        minIdx = i;
        minId = m_chordNodes[i].chordId;
      }
    }
    
    successorIdx = minIdx;
  }
  
  // Se siamo noi il successore, memorizziamo il file localmente
  if (m_chordNodes[successorIdx].chordId == m_chordId) {
    AddFile(fileId);
  } else {
    // Altrimenti, inviamo la richiesta al successore
    SendStoreFileRequest(fileId, successorIdx);
  }
}

// Metodo per inviare una richiesta di memorizzazione file
void 
ChordApplication::SendStoreFileRequest(uint32_t fileId, uint32_t targetNodeIdx) {
  if (!m_running) return;

  ChordMessage msg;
  msg.type = STORE_FILE_REQUEST;
  msg.sourceId = m_chordId;
  msg.targetId = m_chordNodes[targetNodeIdx].chordId;
  msg.key = fileId;
  msg.responseNodeId = 0;

  // Serializziamo il messaggio
  uint8_t buffer[20]; // 5 uint32_t * 4 bytes
  buffer[0] = msg.type;
  buffer[1] = 0; // padding
  buffer[2] = 0; // padding
  buffer[3] = 0; // padding
  *((uint32_t*)(buffer + 4)) = msg.sourceId;
  *((uint32_t*)(buffer + 8)) = msg.targetId;
  *((uint32_t*)(buffer + 12)) = msg.key;
  *((uint32_t*)(buffer + 16)) = msg.responseNodeId;

  Ptr<Packet> packet = Create<Packet>(buffer, 20);
  
  // Inviamo il pacchetto al nodo target
  m_socket->Connect(InetSocketAddress(m_chordNodes[targetNodeIdx].realIp, CHORD_PORT));
  m_socket->Send(packet);
  m_packetsSent++;
  
  cout << "Nodo chordId: " << m_chordId 
       << " ha inviato una richiesta di memorizzazione per il file " << fileId 
       << " al nodo chordId: " << m_chordNodes[targetNodeIdx].chordId << endl;
}

// Metodo per gestire una richiesta di memorizzazione file
void 
ChordApplication::HandleStoreFileRequest(ChordMessage msg) {
  cout << "Nodo chordId: " << m_chordId 
       << " ha ricevuto una richiesta di memorizzazione per il file " << msg.key 
       << " dal nodo chordId: " << msg.sourceId << endl;
  
  uint32_t fileId = msg.key;
  
  // Implementiamo direttamente la logica per trovare il successore qui
  // invece di chiamare FindSuccessor o findSuccessor
  
  // Caso base: un solo nodo
  if (m_chordNodes.size() == 1) {
    AddFile(fileId);
    SendStoreFileResponse(fileId, msg.sourceId, true);
    return;
  }
  
  // Cerchiamo il primo nodo con ID >= fileId
  uint32_t successorIdx = m_myIndex; // Default a noi stessi
  bool found = false;
  
  for (uint32_t i = 0; i < m_chordNodes.size(); i++) {
    if (m_chordNodes[i].chordId >= fileId) {
      successorIdx = i;
      found = true;
      break;
    }
  }
  
  // Se non troviamo un nodo con ID >= fileId, il successore è il nodo con ID minimo (wrap-around)
  if (!found) {
    uint32_t minIdx = 0;
    uint32_t minId = m_chordNodes[0].chordId;
    
    for (uint32_t i = 1; i < m_chordNodes.size(); i++) {
      if (m_chordNodes[i].chordId < minId) {
        minIdx = i;
        minId = m_chordNodes[i].chordId;
      }
    }
    
    successorIdx = minIdx;
  }
  
  // Se siamo noi il successore, memorizziamo il file localmente
  if (m_chordNodes[successorIdx].chordId == m_chordId) {
    AddFile(fileId);
    SendStoreFileResponse(fileId, msg.sourceId, true);
  } else {
    // Altrimenti, inoltriamo la richiesta al successore
    SendStoreFileRequest(fileId, successorIdx);
  }
}

// Metodo per inviare una risposta di memorizzazione file
void 
ChordApplication::SendStoreFileResponse(uint32_t fileId, uint32_t targetId, bool success) {
  if (!m_running) return;

  // Troviamo l'indice del nodo target
  uint32_t targetIdx = 0;
  for (uint32_t i = 0; i < m_chordNodes.size(); i++) {
    if (m_chordNodes[i].chordId == targetId) {
      targetIdx = i;
      break;
    }
  }

  ChordMessage msg;
  msg.type = STORE_FILE_RESPONSE;
  msg.sourceId = m_chordId;
  msg.targetId = targetId;
  msg.key = fileId;
  msg.responseNodeId = success ? 1 : 0; // Usiamo responseNodeId per indicare il successo

  // Serializziamo il messaggio
  uint8_t buffer[20]; // 5 uint32_t * 4 bytes
  buffer[0] = msg.type;
  buffer[1] = 0; // padding
  buffer[2] = 0; // padding
  buffer[3] = 0; // padding
  *((uint32_t*)(buffer + 4)) = msg.sourceId;
  *((uint32_t*)(buffer + 8)) = msg.targetId;
  *((uint32_t*)(buffer + 12)) = msg.key;
  *((uint32_t*)(buffer + 16)) = msg.responseNodeId;

  Ptr<Packet> packet = Create<Packet>(buffer, 20);
  
  // Inviamo il pacchetto al nodo target
  m_socket->Connect(InetSocketAddress(m_chordNodes[targetIdx].realIp, CHORD_PORT));
  m_socket->Send(packet);
  m_packetsSent++;
  
  cout << "Nodo chordId: " << m_chordId 
       << " ha inviato una risposta di memorizzazione per il file " << fileId 
       << " al nodo chordId: " << targetId 
       << " - Successo: " << (success ? "Sì" : "No") << endl;
}

// Metodo per gestire una risposta di memorizzazione file
void 
ChordApplication::HandleStoreFileResponse(ChordMessage msg) {
  bool success = (msg.responseNodeId == 1);
  cout << "Nodo chordId: " << m_chordId 
       << " ha ricevuto una risposta di memorizzazione per il file " << msg.key 
       << " dal nodo chordId: " << msg.sourceId 
       << " - Successo: " << (success ? "Sì" : "No") << endl;
}

// Metodo per cercare un file nella rete Chord
void 
ChordApplication::GetFile(uint32_t fileId) {
  cout << "Nodo chordId: " << m_chordId 
       << " inizia ricerca per file con ID " << fileId << endl;
  
  // Registriamo l'inizio della ricerca
  m_pendingFileLookups[fileId] = false;
  
  // Se il file è disponibile localmente, completiamo subito
  if (HasFile(fileId)) {
    cout << "File ID " << fileId << " trovato localmente nel nodo chordId: " << m_chordId << endl;
    m_pendingFileLookups[fileId] = true;
    // Chiamiamo il callback per segnalare il completamento
    m_fileLookupCompletedCallback(fileId, true);
    return;
  }
  
  uint32_t currentIdx = m_myIndex;
  
  // Se il fileId è tra il nodo corrente e il suo successore, il successore è responsabile
  uint32_t successorIdx = m_chordNodes[currentIdx].fingerTable[0];
  if (isInRange(fileId, m_chordNodes[currentIdx].chordId, m_chordNodes[successorIdx].chordId)) {
    // Questo nodo dovrebbe avere il file, ma non ce l'ha
    // Proviamo a inoltrare la richiesta al successore, forse lui ha il file
    cout << "Nodo chordId: " << m_chordId << " non ha trovato il file " << fileId 
         << " che dovrebbe essere in questo intervallo. Inoltra al successore." << endl;
    SendGetFileRequest(fileId, successorIdx);
    return;
  }
  
  // Altrimenti, cerchiamo nella finger table il nodo più vicino ma che non superi la chiave
  for (int i = FINGER_TABLE_SIZE - 1; i >= 0; i--) {
    uint32_t fingerIdx = m_chordNodes[currentIdx].fingerTable[i];
    if (isInRange(m_chordNodes[fingerIdx].chordId, m_chordNodes[currentIdx].chordId, fileId)) {
      SendGetFileRequest(fileId, fingerIdx);
      return;
    }
  }
  
  // Se non troviamo un nodo migliore nella finger table, passiamo al successore
  SendGetFileRequest(fileId, successorIdx);
}

// Metodo per inviare una richiesta di ricerca file
void 
ChordApplication::SendGetFileRequest(uint32_t fileId, uint32_t targetNodeIdx) {
  if (!m_running) return;

  ChordMessage msg;
  msg.type = GET_FILE_REQUEST;
  msg.sourceId = m_chordId;
  msg.targetId = m_chordNodes[targetNodeIdx].chordId;
  msg.key = fileId;
  msg.responseNodeId = 0;

  // Serializziamo il messaggio
  uint8_t buffer[20]; // 5 uint32_t * 4 bytes
  buffer[0] = msg.type;
  buffer[1] = 0; // padding
  buffer[2] = 0; // padding
  buffer[3] = 0; // padding
  *((uint32_t*)(buffer + 4)) = msg.sourceId;
  *((uint32_t*)(buffer + 8)) = msg.targetId;
  *((uint32_t*)(buffer + 12)) = msg.key;
  *((uint32_t*)(buffer + 16)) = msg.responseNodeId;

  Ptr<Packet> packet = Create<Packet>(buffer, 20);
  
  // Inviamo il pacchetto al nodo target
  m_socket->Connect(InetSocketAddress(m_chordNodes[targetNodeIdx].realIp, CHORD_PORT));
  m_socket->Send(packet);
  m_packetsSent++;
  
  cout << "Nodo chordId: " << m_chordId 
       << " ha inviato una richiesta di ricerca file con ID " << fileId 
       << " al nodo chordId: " << m_chordNodes[targetNodeIdx].chordId << endl;
}

// Metodo per gestire una richiesta di ricerca file
void 
ChordApplication::HandleGetFileRequest(ChordMessage msg) {
  cout << "Nodo chordId: " << m_chordId 
       << " ha ricevuto una richiesta di ricerca per il file " << msg.key 
       << " dal nodo chordId: " << msg.sourceId << endl;
  
  uint32_t fileId = msg.key;
  uint32_t sourceId = msg.sourceId;
  uint32_t currentIdx = m_myIndex;
  
  // Verifichiamo se abbiamo il file
  if (HasFile(fileId)) {
    // Abbiamo trovato il file! Inviamo la risposta al nodo che ha fatto la richiesta
    cout << "Nodo chordId: " << m_chordId << " ha trovato il file " << fileId << endl;
    SendGetFileResponse(fileId, sourceId, true);
    return;
  }
  
  // Se il fileId è tra il nodo corrente e il suo successore, il successore è responsabile
  uint32_t successorIdx = m_chordNodes[currentIdx].fingerTable[0];
  if (isInRange(fileId, m_chordNodes[currentIdx].chordId, m_chordNodes[successorIdx].chordId)) {
    // Questo nodo dovrebbe avere il file, ma non ce l'ha
    // Proviamo a inoltrare la richiesta al successore
    cout << "Nodo chordId: " << m_chordId << " non ha trovato il file " << fileId 
         << " che dovrebbe essere in questo intervallo. Inoltra al successore." << endl;
    SendGetFileRequest(fileId, successorIdx);
    return;
  }
  
  // Altrimenti, cerchiamo nella finger table il nodo più vicino ma che non superi il fileId
  bool found = false;
  for (int i = FINGER_TABLE_SIZE - 1; i >= 0; i--) {
    uint32_t fingerIdx = m_chordNodes[currentIdx].fingerTable[i];
    if (isInRange(m_chordNodes[fingerIdx].chordId, m_chordNodes[currentIdx].chordId, fileId)) {
      // Inoltriamo la richiesta al nodo più vicino
      cout << "Nodo chordId: " << m_chordId 
           << " inoltra la richiesta per il file " << fileId 
           << " al finger " << i << " (chordId: " << m_chordNodes[fingerIdx].chordId << ")" << endl;
      SendGetFileRequest(fileId, fingerIdx);
      found = true;
      break;
    }
  }
  
  // Se non troviamo un nodo migliore nella finger table, passiamo al successore
  if (!found) {
    cout << "Nodo chordId: " << m_chordId 
         << " inoltra la richiesta per il file " << fileId 
         << " al successore (chordId: " << m_chordNodes[successorIdx].chordId << ")" << endl;
    SendGetFileRequest(fileId, successorIdx);
  }
}

// Metodo per inviare una risposta di ricerca file
void 
ChordApplication::SendGetFileResponse(uint32_t fileId, uint32_t targetId, bool hasFile) {
  if (!m_running) return;

  // Troviamo l'indice del nodo target
  uint32_t targetIdx = 0;
  for (uint32_t i = 0; i < m_chordNodes.size(); i++) {
    if (m_chordNodes[i].chordId == targetId) {
      targetIdx = i;
      break;
    }
  }

  ChordMessage msg;
  msg.type = GET_FILE_RESPONSE;
  msg.sourceId = m_chordId;
  msg.targetId = targetId;
  msg.key = fileId;
  msg.responseNodeId = hasFile ? m_chordId : 0; // Se abbiamo il file, rispondiamo con il nostro ID
  
  // Debug dettagliato
  if (hasFile) {
    cout << "DEBUG: Nodo chordId: " << m_chordId 
         << " sta rispondendo POSITIVAMENTE alla ricerca del file " << fileId 
         << " dal nodo chordId: " << targetId << endl;
  }

  // Serializziamo il messaggio
  uint8_t buffer[20]; // 5 uint32_t * 4 bytes
  buffer[0] = msg.type;
  buffer[1] = 0; // padding
  buffer[2] = 0; // padding
  buffer[3] = 0; // padding
  *((uint32_t*)(buffer + 4)) = msg.sourceId;
  *((uint32_t*)(buffer + 8)) = msg.targetId;
  *((uint32_t*)(buffer + 12)) = msg.key;
  *((uint32_t*)(buffer + 16)) = msg.responseNodeId;

  Ptr<Packet> packet = Create<Packet>(buffer, 20);
  
  // Inviamo il pacchetto al nodo target
  m_socket->Connect(InetSocketAddress(m_chordNodes[targetIdx].realIp, CHORD_PORT));
  m_socket->Send(packet);
  m_packetsSent++;
  
  cout << "Nodo chordId: " << m_chordId 
       << " ha inviato una risposta di ricerca per il file " << fileId 
       << " al nodo chordId: " << targetId 
       << " - File trovato: " << (hasFile ? "Sì" : "No") << endl;
}

// Metodo per gestire una risposta di ricerca file
void 
ChordApplication::HandleGetFileResponse(ChordMessage msg) {
  cout << "Nodo chordId: " << m_chordId 
       << " ha ricevuto una risposta per la ricerca del file ID " << msg.key;
  
  bool fileFound = (msg.responseNodeId != 0);
  if (fileFound) {
    cout << ": il file è presente nel nodo con chordId: " << msg.responseNodeId << endl;
  } else {
    cout << ": file non trovato." << endl;
  }
  
  // Verifichiamo se questa è una risposta a una nostra ricerca o se è stata inoltrata
  if (m_pendingFileLookups.find(msg.key) != m_pendingFileLookups.end()) {
    // Questa è una risposta a una nostra ricerca diretta
    cout << "Ricerca completata per il file ID " << msg.key << " con risultato: " 
         << (fileFound ? "Trovato" : "Non trovato") << endl;
    
    // Aggiorniamo lo stato della ricerca
    m_pendingFileLookups[msg.key] = fileFound;
    
    // Chiamiamo il callback per segnalare il completamento
    if (m_fileLookupCompletedCallback) {
      m_fileLookupCompletedCallback(msg.key, fileFound);
    } else {
      cout << "ERRORE: Callback per il completamento della ricerca non impostato!" << endl;
    }
  }
}

// Metodo per ottenere la lista dei file memorizzati
vector<uint32_t> 
ChordApplication::GetStoredFiles() {
  return m_chordNodes[m_myIndex].fileIds;
}

void 
ChordApplication::SetFileLookupCompletedCallback(FileLookupCompletedCallback callback) {
  m_fileLookupCompletedCallback = callback;
} 

