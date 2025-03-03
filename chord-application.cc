#include "chord-application.h"
#include "chord-helper.h"
#include <iostream>

using namespace ns3;
using namespace std;

NS_LOG_COMPONENT_DEFINE ("ChordApplication");

// Funzione di base per il routing Chord
int closestPrecedingFinger(int current, int key, const vector<int>& fingerTable, int modulus) {
    // Iteriamo dalla fine della finger table (distanze più grandi)
    for (int i = fingerTable.size() - 1; i >= 0; --i) {
        int finger = fingerTable[i];
        
        // Ignoriamo il nodo corrente
        if (finger == current) continue;
        
        // Caso di wrap-around: key < current
        if (key < current) {
            // Cerchiamo un nodo che sia:
            // 1. Maggiore del nodo corrente (per andare in avanti nella circonferenza) OPPURE
            // 2. Minore o uguale alla chiave (per il wrap-around)
            if (finger > current || finger <= key) {
                return finger;
            }
        }
        // Caso normale: key > current
        else {
            // Cerchiamo un nodo che sia:
            // 1. Maggiore del nodo corrente E
            // 2. Minore o uguale alla chiave
            if (finger > current && finger <= key) {
                return finger;
            }
        }
    }
    
    // Se non troviamo un nodo adatto nella finger table,
    // restituiamo il nodo corrente
    return current;
}

TypeId 
ChordApplication::GetTypeId(void) {
  static TypeId tid = TypeId("ChordApplication")
    .SetParent<Application>()
    .SetGroupName("Applications")
    .AddConstructor<ChordApplication>();
  return tid;
}

ChordApplication::ChordApplication() :
  m_socket(0),
  m_chordId(0),
  m_myIndex(0),
  m_running(false),
  m_packetsSent(0),
  m_packetsReceived(0),
  m_totalFilesStored(0),
  m_totalBytesStored(0),
  m_applicationPort(9000)
{
  // Inizializziamo il callback come una funzione vuota
  m_fileLookupCompletedCallback = [](uint32_t, bool, uint32_t) {};
}

ChordApplication::~ChordApplication() {
}

void 
ChordApplication::Setup(uint32_t chordId, const vector<ChordInfo>& chordNodes) {
  m_chordId = chordId;
  m_chordNodes = chordNodes;
  m_applicationPort = 9000;
  m_totalFilesStored = 0;
  m_totalBytesStored = 0;
  
  // Troviamo il nostro indice nella lista dei nodi
  for (uint32_t i = 0; i < m_chordNodes.size(); i++) {
    if (m_chordNodes[i].chordId == m_chordId) {
      m_myIndex = i;
      break;
    }
  }
  
  cout << "Nodo " << m_chordId << " configurato" << endl;
}

void
ChordApplication::SendLookupRequest(uint32_t key, uint32_t targetIdx) {
  if (targetIdx >= m_chordNodes.size()) {
    cout << "ERRORE: Indice del nodo target non valido" << endl;
    return;
  }
  
  cout << "Nodo " << m_chordId << " invia richiesta di lookup per chiave " << key 
       << " al nodo " << m_chordNodes[targetIdx].chordId << endl;
  
  Ptr<Packet> packet = Create<Packet>();
  ChordMessage chordMessage;
  chordMessage.type = LOOKUP_REQUEST;
  chordMessage.sourceId = m_chordId;
  chordMessage.destinationId = m_chordNodes[targetIdx].chordId;
  chordMessage.fileId = key;
  
  packet->AddHeader(chordMessage);
  
  m_socket->SendTo(packet, 0, InetSocketAddress(m_chordNodes[targetIdx].realIp, m_applicationPort));
}

void 
ChordApplication::LookupKey(uint32_t key) {
  if (!m_running) return;
  
  cout << "Nodo " << m_chordId << " inizia lookup per chiave " << key << endl;
  
  // Otteniamo l'indice del nostro successore immediato
  uint32_t successorIdx = m_chordNodes[m_myIndex].fingerTable[0];
  
  // Verifichiamo se la chiave è tra noi e il nostro successore
  if (isInRange(key, m_chordId, m_chordNodes[successorIdx].chordId)) {
    // Se la chiave è tra noi e il nostro successore, il successore è responsabile
    cout << "Nodo " << m_chordId << " ha identificato che la chiave " << key 
         << " è gestita dal successore " << m_chordNodes[successorIdx].chordId << endl;
    return;
  }
  
  // Utilizziamo il metodo FindFarthestPrecedingNode per trovare il nodo più lontano
  // che precede la chiave
  uint32_t farthestPrecedingIdx = FindFarthestPrecedingNode(key);
  
  // Se abbiamo trovato un nodo migliore, inviamo la richiesta a quel nodo
  if (farthestPrecedingIdx != m_myIndex) {
    cout << "Nodo " << m_chordId << " inoltra la richiesta al nodo più lontano che precede " 
         << m_chordNodes[farthestPrecedingIdx].chordId << " per la chiave " << key << endl;
    SendLookupRequest(key, farthestPrecedingIdx);
  } else {
    // Se non abbiamo trovato un nodo migliore, inviamo al successore
    cout << "Nodo " << m_chordId << " non ha trovato un nodo migliore, "
         << "inoltra al successore " << m_chordNodes[successorIdx].chordId << endl;
    SendLookupRequest(key, successorIdx);
  }
}

void 
ChordApplication::HandleLookupRequest(ChordMessage msg, Address from) {
  uint32_t key = msg.fileId;
  uint32_t sourceId = msg.sourceId;
  
  cout << "Nodo " << m_chordId << " ha ricevuto richiesta di lookup per chiave " << key 
       << " dal nodo " << sourceId << endl;
  
  // Otteniamo l'indice del nostro successore immediato
  uint32_t successorIdx = m_chordNodes[m_myIndex].fingerTable[0];
  
  // Verifichiamo se la chiave è tra noi e il nostro successore
  if (isInRange(key, m_chordId, m_chordNodes[successorIdx].chordId)) {
    // Se la chiave è tra noi e il nostro successore, il successore è responsabile
    cout << "Nodo " << m_chordId << " ha identificato che la chiave " << key 
         << " è gestita dal successore " << m_chordNodes[successorIdx].chordId << endl;
    
    // Inviamo la risposta al nodo che ha fatto la richiesta
    SendLookupResponse(key, m_chordNodes[successorIdx].chordId, m_chordNodes[successorIdx].chordId);
    return;
  }
  
  // Utilizziamo il metodo FindFarthestPrecedingNode per trovare il nodo più lontano
  // che precede la chiave
  uint32_t farthestPrecedingIdx = FindFarthestPrecedingNode(key);
  
  // Se abbiamo trovato un nodo migliore, inoltriamo la richiesta a quel nodo
  if (farthestPrecedingIdx != m_myIndex) {
    cout << "Nodo " << m_chordId << " inoltra la richiesta al nodo più lontano che precede " 
         << m_chordNodes[farthestPrecedingIdx].chordId << " per la chiave " << key << endl;
    
    // Creiamo un nuovo messaggio di lookup
    Ptr<Packet> packet = Create<Packet>();
    ChordMessage chordMessage;
    chordMessage.type = LOOKUP_REQUEST;
    chordMessage.sourceId = sourceId; // Manteniamo il nodo sorgente originale
    chordMessage.destinationId = m_chordNodes[farthestPrecedingIdx].chordId;
    chordMessage.fileId = key;
    
    packet->AddHeader(chordMessage);
    
    // Inviamo il pacchetto al nodo più lontano
    m_socket->SendTo(packet, 0, InetSocketAddress(m_chordNodes[farthestPrecedingIdx].realIp, m_applicationPort));
  } else {
    // Se non abbiamo trovato un nodo migliore, inoltriamo al successore
    cout << "Nodo " << m_chordId << " non ha trovato un nodo migliore, "
         << "inoltra al successore " << m_chordNodes[successorIdx].chordId << endl;
    
    // Creiamo un nuovo messaggio di lookup
    Ptr<Packet> packet = Create<Packet>();
    ChordMessage chordMessage;
    chordMessage.type = LOOKUP_REQUEST;
    chordMessage.sourceId = sourceId; // Manteniamo il nodo sorgente originale
    chordMessage.destinationId = m_chordNodes[successorIdx].chordId;
    chordMessage.fileId = key;
    
    packet->AddHeader(chordMessage);
    
    // Inviamo il pacchetto al successore
    m_socket->SendTo(packet, 0, InetSocketAddress(m_chordNodes[successorIdx].realIp, m_applicationPort));
  }
}

void 
ChordApplication::SendLookupResponse(uint32_t key, uint32_t targetId, uint32_t responseNodeId) {
  // Troviamo l'indice del nodo target
  uint32_t targetIdx = 0;
  for (uint32_t i = 0; i < m_chordNodes.size(); i++) {
    if (m_chordNodes[i].chordId == targetId) {
      targetIdx = i;
      break;
    }
  }
  
  cout << "Nodo " << m_chordId << " invia risposta di lookup per chiave " << key 
       << " al nodo " << targetId << " - Nodo responsabile: " << responseNodeId << endl;
  
  Ptr<Packet> packet = Create<Packet>();
  ChordMessage chordMessage;
  chordMessage.type = LOOKUP_RESPONSE;
  chordMessage.sourceId = m_chordId;
  chordMessage.destinationId = responseNodeId;
  chordMessage.fileId = key;
  
  packet->AddHeader(chordMessage);
  
  m_socket->SendTo(packet, 0, InetSocketAddress(m_chordNodes[targetIdx].realIp, m_applicationPort));
}

void 
ChordApplication::HandleLookupResponse(ChordMessage msg) {
  uint32_t key = msg.fileId;
  uint32_t responseNodeId = msg.destinationId;
  
  cout << "Nodo " << m_chordId << " ha ricevuto risposta di lookup per chiave " << key 
       << " - Nodo responsabile: " << responseNodeId << endl;
}

void 
ChordApplication::PrintStats() {
  cout << "Statistiche del nodo " << m_chordId << ":" << endl;
  cout << "  - File memorizzati: " << m_totalFilesStored << endl;
  cout << "  - Bytes memorizzati: " << m_totalBytesStored << endl;
}

void 
ChordApplication::StartApplication(void) {
  m_running = true;
  
  // Creiamo il socket
  if (!m_socket) {
    m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
    m_socket->Bind(InetSocketAddress(Ipv4Address::GetAny(), m_applicationPort));
    m_socket->SetRecvCallback(MakeCallback(&ChordApplication::HandleRead, this));
  }
  
  cout << "Nodo " << m_chordId << " avviato" << endl;
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
    ChordMessage msg;
    packet->RemoveHeader(msg);
    
    switch (msg.type) {
      case LOOKUP_REQUEST:
        HandleLookupRequest(msg, from);
        break;
      case LOOKUP_RESPONSE:
        HandleLookupResponse(msg);
        break;
      case STORE_FILE_REQUEST:
        HandleStoreFileRequest(msg, from);
        break;
      case STORE_FILE_RESPONSE:
        HandleStoreFileResponse(msg);
        break;
      case GET_FILE_REQUEST:
        HandleGetFileRequest(msg, from);
        break;
      case GET_FILE_RESPONSE:
        HandleGetFileResponse(msg);
        break;
      case FORWARDING_RESPONSE:
        HandleForwardingResponse(msg);
        break;
      default:
        cout << "ERRORE: Tipo di messaggio sconosciuto: " << (int)msg.type << endl;
        break;
    }
  }
}

// Metodo per aggiungere un file localmente
void 
ChordApplication::AddFile(uint32_t fileId) {
  // Verifichiamo se il file è già presente
  if (HasFile(fileId)) {
    cout << "ARCHIVIAZIONE: Nodo " << m_chordId << " - File " << fileId << " già presente" << endl;
    return;
  }
  
  // Aggiungiamo il file
  FileData newFile;
  newFile.fileId = fileId;
  newFile.fileSize = 1024; // Dimensione di default
  m_storedFiles.push_back(newFile);
  
  // Aggiorniamo le statistiche
  m_totalFilesStored++;
  m_totalBytesStored += newFile.fileSize;
  
  cout << "ARCHIVIAZIONE: Nodo " << m_chordId << " ha memorizzato il file " << fileId << endl;
}

// Metodo per verificare se un file è presente localmente
bool 
ChordApplication::HasFile(uint32_t fileId) {
  for (const auto& file : m_storedFiles) {
    if (file.fileId == fileId) {
      return true;
    }
  }
  return false;
}

// Metodo per memorizzare un file nella rete Chord
void 
ChordApplication::StoreFile(uint32_t fileId, uint32_t fileSize, uint32_t hopCount) {
  // Verifica se il file deve essere memorizzato in questo nodo
  if (IsResponsibleForKey(fileId)) {
    // Questo nodo è responsabile per il file
    cout << "ARCHIVIAZIONE: Nodo " << m_chordId << " memorizza il file " << fileId << " (dimensione: " << fileSize << " bytes)" << endl;
    
    // Memorizza il file
    FileData newFile;
    newFile.fileId = fileId;
    newFile.fileSize = fileSize;
    m_storedFiles.push_back(newFile);
    
    // Aggiorna le statistiche
    m_totalFilesStored++;
    m_totalBytesStored += fileSize;
    
    // Stampa le statistiche di archiviazione
    cout << "STATISTICHE ARCHIVIAZIONE: Nodo " << m_chordId << " ha memorizzato " << m_totalFilesStored 
         << " file per un totale di " << m_totalBytesStored << " bytes" << endl;
  } else {
    // Questo nodo non è responsabile per il file, inoltra la richiesta
    cout << "ARCHIVIAZIONE: Nodo " << m_chordId << " inoltra la richiesta di archiviazione per il file " << fileId << endl;
    
    // Trova il nodo successivo a cui inoltrare la richiesta utilizzando il metodo unificato
    uint32_t nextIdx = FindFarthestPrecedingNode(fileId);
    
    if (nextIdx != m_myIndex) {
      // Inoltra la richiesta al nodo trovato
      uint32_t nextId = m_chordNodes[nextIdx].chordId;
      cout << "ARCHIVIAZIONE: Nodo " << m_chordId << " inoltra a nodo " << nextId << " la richiesta di archiviazione per il file " << fileId << endl;
      
      // Crea e invia il messaggio di archiviazione
      Ptr<Packet> storeFilePacket = Create<Packet>();
      ChordMessage chordMessage = CreateStoreFileMessage(fileId, fileSize, hopCount + 1);
      storeFilePacket->AddHeader(chordMessage);
      
      m_socket->SendTo(storeFilePacket, 0, InetSocketAddress(m_chordNodes[nextIdx].realIp, m_applicationPort));
    } else {
      // Non è stato trovato un nodo migliore, questo è un caso anomalo
      cout << "ERRORE: Nodo " << m_chordId << " non ha trovato un nodo a cui inoltrare la richiesta di archiviazione per il file " << fileId << endl;
    }
  }
}

// Metodo per inviare una richiesta di memorizzazione file
void 
ChordApplication::SendStoreFileRequest(uint32_t fileId, uint32_t fileSize, uint32_t targetIdx) {
  if (targetIdx >= m_chordNodes.size()) {
    cout << "ERRORE: Indice del nodo target non valido" << endl;
    return;
  }
  
  cout << "ARCHIVIAZIONE: Nodo " << m_chordId << " invia richiesta di archiviazione per file " << fileId 
       << " al nodo " << m_chordNodes[targetIdx].chordId << endl;
  
  Ptr<Packet> packet = Create<Packet>();
  ChordMessage chordMessage = CreateStoreFileMessage(fileId, fileSize, 1); // Inizia con hop count = 1
  packet->AddHeader(chordMessage);
  
  m_socket->SendTo(packet, 0, InetSocketAddress(m_chordNodes[targetIdx].realIp, m_applicationPort));
}

// Metodo per gestire una richiesta di memorizzazione file
void 
ChordApplication::HandleStoreFileRequest(ChordMessage message, Address sourceAddress) {
  uint32_t fileId = message.fileId;
  uint32_t fileSize = message.fileSize;
  uint32_t hopCount = message.hopCount;
  
  cout << "ARCHIVIAZIONE: Nodo " << m_chordId << " ha ricevuto richiesta di archiviazione per file " << fileId 
       << " (hop: " << hopCount << ")" << endl;
  
  // Inoltra la richiesta al metodo StoreFile che gestirà l'archiviazione o l'inoltro
  StoreFile(fileId, fileSize, hopCount);
}

// Metodo per inviare una risposta di memorizzazione file
void 
ChordApplication::SendStoreFileResponse(uint32_t fileId, uint32_t targetId, bool success) {
  // Troviamo l'indice del nodo target
  uint32_t targetIdx = 0;
  for (uint32_t i = 0; i < m_chordNodes.size(); i++) {
    if (m_chordNodes[i].chordId == targetId) {
      targetIdx = i;
      break;
    }
  }
  
  cout << "ARCHIVIAZIONE: Nodo " << m_chordId << " invia risposta " 
       << (success ? "POSITIVA" : "NEGATIVA") << " per file " << fileId 
       << " al nodo " << targetId << endl;
  
  Ptr<Packet> packet = Create<Packet>();
  ChordMessage chordMessage;
  chordMessage.type = STORE_FILE_RESPONSE;
  chordMessage.sourceId = m_chordId;
  chordMessage.destinationId = targetId;
  chordMessage.fileId = fileId;
  chordMessage.success = success;
  
  packet->AddHeader(chordMessage);
  
  m_socket->SendTo(packet, 0, InetSocketAddress(m_chordNodes[targetIdx].realIp, m_applicationPort));
}

// Metodo per gestire una risposta di memorizzazione file
void 
ChordApplication::HandleStoreFileResponse(ChordMessage msg) {
  uint32_t fileId = msg.fileId;
  bool success = msg.success;
  
  if (success) {
    cout << "ARCHIVIAZIONE COMPLETATA: Nodo " << m_chordId << " ha ricevuto conferma che il file " 
         << fileId << " è stato memorizzato con successo" << endl;
  } else {
    cout << "ARCHIVIAZIONE FALLITA: Nodo " << m_chordId << " ha ricevuto notifica che il file " 
         << fileId << " non è stato memorizzato" << endl;
  }
}

// Metodo per cercare un file nella rete Chord
void
ChordApplication::GetFile(uint32_t fileId, uint32_t hopCount) {
    // Controllo anti-loop avanzato:
    // 1. Verifica se abbiamo superato il numero di nodi nella rete
    if (hopCount >= m_chordNodes.size()) {
        cout << "ERRORE: Rilevato possibile loop nel routing per file " << fileId 
             << " dopo " << hopCount << " hop" << endl;
        if (m_fileLookupCompletedCallback) {
            m_fileLookupCompletedCallback(fileId, false, hopCount);
        }
        return;
    }
    
    // 2. Verifica se abbiamo già visitato questo nodo per questa ricerca
    if (m_visitedNodes.find(fileId) != m_visitedNodes.end()) {
        // Se questo nodo è già stato visitato per questa ricerca, abbiamo un loop
        if (m_visitedNodes[fileId].find(m_chordId) != m_visitedNodes[fileId].end()) {
            cout << "ERRORE: Rilevato loop nel routing per file " << fileId 
                 << " - nodo " << m_chordId << " già visitato" << endl;
            if (m_fileLookupCompletedCallback) {
                m_fileLookupCompletedCallback(fileId, false, hopCount);
            }
            return;
        }
        
        // Aggiungiamo questo nodo alla lista dei nodi visitati
        m_visitedNodes[fileId].insert(m_chordId);
    } else {
        // Inizializziamo la lista dei nodi visitati per questa ricerca
        m_visitedNodes[fileId] = {m_chordId};
    }

    cout << "RICERCA: Nodo " << m_chordId << " cerca file " << fileId 
         << " (hop: " << hopCount << ")" << endl;
    
    // 1. Verifica se il file è memorizzato localmente
    for (const auto& file : m_storedFiles) {
        if (file.fileId == fileId) {
            cout << "RICERCA COMPLETATA: Nodo " << m_chordId 
                 << " ha trovato il file " << fileId << " localmente" << endl;
            
            // Pulizia della mappa dei nodi visitati
            m_visitedNodes.erase(fileId);
            
            if (m_fileLookupCompletedCallback) {
                m_fileLookupCompletedCallback(fileId, true, hopCount);
            }
            return;
        }
    }
    
    // 2. Verifica se siamo responsabili per questa chiave
    if (IsResponsibleForKey(fileId)) {
        cout << "RICERCA COMPLETATA: Nodo " << m_chordId 
             << " è responsabile per il file " << fileId 
             << " ma non lo possiede. Ricerca fallita." << endl;
        
        // Pulizia della mappa dei nodi visitati
        m_visitedNodes.erase(fileId);
        
        if (m_fileLookupCompletedCallback) {
            m_fileLookupCompletedCallback(fileId, false, hopCount);
        }
        return;
    }
    
    // 3. Prepara la finger table per il routing
    vector<int> fingerIds;
    for (uint32_t idx : m_chordNodes[m_myIndex].fingerTable) {
        fingerIds.push_back(static_cast<int>(m_chordNodes[idx].chordId));
    }
    
    // 4. Trova il nodo più vicino alla chiave
    int nextNodeId = closestPrecedingFinger(
        static_cast<int>(m_chordId),
        static_cast<int>(fileId),
        fingerIds,
        static_cast<int>(CHORD_SIZE)
    );
    
    // 5. Trova l'indice del nodo successivo
    uint32_t nextIdx = m_myIndex;  // Default al nostro indice
    for (uint32_t i = 0; i < m_chordNodes.size(); i++) {
        if (m_chordNodes[i].chordId == static_cast<uint32_t>(nextNodeId)) {
            nextIdx = i;
            break;
        }
    }
    
    // 6. Verifica se il routing è sensato
    if (nextIdx == m_myIndex) {
        // Non abbiamo trovato un nodo migliore, inoltriamo al successore
        nextIdx = m_chordNodes[m_myIndex].fingerTable[0];
        cout << "RICERCA: Nodo " << m_chordId << " non ha trovato un nodo migliore, "
             << "inoltra al successore " << m_chordNodes[nextIdx].chordId << endl;
    } else {
        cout << "RICERCA: Nodo " << m_chordId << " inoltra al nodo " 
             << m_chordNodes[nextIdx].chordId << endl;
    }
    
    // 7. Verifica se il nodo successivo è già stato visitato
    if (m_visitedNodes[fileId].find(m_chordNodes[nextIdx].chordId) != m_visitedNodes[fileId].end()) {
        cout << "ERRORE: Rilevato loop nel routing per file " << fileId 
             << " - nodo " << m_chordNodes[nextIdx].chordId << " già visitato" << endl;
        
        // Pulizia della mappa dei nodi visitati
        m_visitedNodes.erase(fileId);
        
        if (m_fileLookupCompletedCallback) {
            m_fileLookupCompletedCallback(fileId, false, hopCount);
        }
        return;
    }
    
    // 8. Invia la richiesta al nodo successivo
    SendGetFileRequestWithHopCount(fileId, nextIdx, hopCount + 1);
}

// Metodo per gestire una richiesta di ricerca file
void 
ChordApplication::HandleGetFileRequest(ChordMessage message, Address sourceAddress) {
  uint32_t fileId = message.fileId;
  uint32_t sourceId = message.sourceId;
  uint32_t hopCount = message.hopCount;
  
  // Otteniamo l'indirizzo IP del mittente effettivo
  InetSocketAddress inetSourceAddr = InetSocketAddress::ConvertFrom(sourceAddress);
  Ipv4Address senderIp = inetSourceAddr.GetIpv4();
  
  // Troviamo il nodo che ha effettivamente inviato la richiesta
  uint32_t senderIdx = m_myIndex; // Default al nostro indice
  uint32_t senderId = m_chordId; // Default al nostro ID
  for (uint32_t i = 0; i < m_chordNodes.size(); i++) {
    if (m_chordNodes[i].realIp == senderIp) {
      senderIdx = i;
      senderId = m_chordNodes[i].chordId;
      break;
    }
  }
  
  // Log che mostra sia il mittente effettivo che il nodo sorgente originale
  if (senderId != sourceId) {
    cout << "RICERCA: Nodo " << m_chordId << " ha ricevuto richiesta per file " << fileId 
         << " inoltrata dal nodo " << senderId << ", originata dal nodo " << sourceId 
         << " (hop: " << hopCount << ")" << endl;
  } else {
    cout << "RICERCA: Nodo " << m_chordId << " ha ricevuto richiesta per file " << fileId 
         << " dal nodo " << sourceId << " (hop: " << hopCount << ")" << endl;
  }
  
  // Controllo anti-loop avanzato:
  // 1. Verifica se abbiamo superato il numero di nodi nella rete
  if (hopCount >= m_chordNodes.size()) {
    cout << "ERRORE: Rilevato possibile loop nel routing per file " << fileId 
         << " dopo " << hopCount << " hop" << endl;
    
    // Trova l'indice del nodo sorgente
    uint32_t sourceIdx = 0;
    for (uint32_t i = 0; i < m_chordNodes.size(); i++) {
      if (m_chordNodes[i].chordId == sourceId) {
        sourceIdx = i;
        break;
      }
    }
    
    // Invia la risposta negativa al nodo sorgente
    SendGetFileResponse(fileId, sourceIdx, false, hopCount);
    return;
  }
  
  // 2. Verifica se abbiamo già visitato questo nodo per questa ricerca
  if (m_visitedNodes.find(fileId) != m_visitedNodes.end()) {
    // Se questo nodo è già stato visitato per questa ricerca, abbiamo un loop
    if (m_visitedNodes[fileId].find(m_chordId) != m_visitedNodes[fileId].end()) {
      cout << "ERRORE: Rilevato loop nel routing per file " << fileId 
           << " - nodo " << m_chordId << " già visitato" << endl;
      
      // Trova l'indice del nodo sorgente
      uint32_t sourceIdx = 0;
      for (uint32_t i = 0; i < m_chordNodes.size(); i++) {
        if (m_chordNodes[i].chordId == sourceId) {
          sourceIdx = i;
          break;
        }
      }
      
      // Invia la risposta negativa al nodo sorgente
      SendGetFileResponse(fileId, sourceIdx, false, hopCount);
      return;
    }
    
    // Aggiungiamo questo nodo alla lista dei nodi visitati
    m_visitedNodes[fileId].insert(m_chordId);
  } else {
    // Inizializziamo la lista dei nodi visitati per questa ricerca
    m_visitedNodes[fileId] = {m_chordId};
  }
  
  // Verifica se il file è memorizzato localmente
  for (const auto& file : m_storedFiles) {
    if (file.fileId == fileId) {
      cout << "RICERCA: Nodo " << m_chordId << " ha trovato il file " << fileId << " localmente" << endl;
      
      // Pulizia della mappa dei nodi visitati
      m_visitedNodes.erase(fileId);
      
      // Trova l'indice del nodo sorgente
      uint32_t sourceIdx = 0;
      for (uint32_t i = 0; i < m_chordNodes.size(); i++) {
        if (m_chordNodes[i].chordId == sourceId) {
          sourceIdx = i;
          break;
        }
      }
      
      // Invia la risposta positiva al nodo sorgente
      SendGetFileResponse(fileId, sourceIdx, true, hopCount);
      return;
    }
  }
  
  // Se siamo responsabili per questa chiave ma non abbiamo il file, la ricerca fallisce
  if (IsResponsibleForKey(fileId)) {
    cout << "RICERCA: Nodo " << m_chordId << " è responsabile per il file " << fileId 
         << " ma non lo possiede. Risposta negativa." << endl;
    
    // Pulizia della mappa dei nodi visitati
    m_visitedNodes.erase(fileId);
    
    // Trova l'indice del nodo sorgente
    uint32_t sourceIdx = 0;
    for (uint32_t i = 0; i < m_chordNodes.size(); i++) {
      if (m_chordNodes[i].chordId == sourceId) {
        sourceIdx = i;
        break;
      }
    }
    
    // Invia la risposta negativa al nodo sorgente
    SendGetFileResponse(fileId, sourceIdx, false, hopCount);
    return;
  }
  
  // Altrimenti, dobbiamo inoltrare la richiesta
  // Utilizziamo il metodo unificato per trovare il nodo a cui inoltrare
  uint32_t nextIdx = FindFarthestPrecedingNode(fileId);
  
  // Verifica se il nodo successivo è già stato visitato
  if (m_visitedNodes[fileId].find(m_chordNodes[nextIdx].chordId) != m_visitedNodes[fileId].end()) {
    cout << "ERRORE: Rilevato loop nel routing per file " << fileId 
         << " - nodo " << m_chordNodes[nextIdx].chordId << " già visitato" << endl;
    
    // Pulizia della mappa dei nodi visitati
    m_visitedNodes.erase(fileId);
    
    // Trova l'indice del nodo sorgente
    uint32_t sourceIdx = 0;
    for (uint32_t i = 0; i < m_chordNodes.size(); i++) {
      if (m_chordNodes[i].chordId == sourceId) {
        sourceIdx = i;
        break;
      }
    }
    
    // Invia la risposta negativa al nodo sorgente
    SendGetFileResponse(fileId, sourceIdx, false, hopCount);
    return;
  }
  
  if (nextIdx != m_myIndex) {
    // Abbiamo trovato un nodo a cui inoltrare
    uint32_t nextId = m_chordNodes[nextIdx].chordId;
    cout << "RICERCA: Nodo " << m_chordId << " inoltra richiesta per file " << fileId 
         << " al nodo " << nextId << endl;
    
    // Invia la richiesta al nodo successivo, mantenendo il nodo sorgente originale
    SendForwardingRequest(fileId, nextIdx, sourceId, hopCount + 1);
  } else {
    // Non abbiamo trovato un nodo migliore, questo è un caso anomalo
    cout << "ERRORE: Nodo " << m_chordId << " non ha trovato un nodo a cui inoltrare la richiesta per il file " 
         << fileId << endl;
    
    // Pulizia della mappa dei nodi visitati
    m_visitedNodes.erase(fileId);
    
    // Trova l'indice del nodo sorgente
    uint32_t sourceIdx = 0;
    for (uint32_t i = 0; i < m_chordNodes.size(); i++) {
      if (m_chordNodes[i].chordId == sourceId) {
        sourceIdx = i;
        break;
      }
    }
    
    // Invia la risposta negativa al nodo sorgente
    SendGetFileResponse(fileId, sourceIdx, false, hopCount);
  }
}

// Nuovo metodo per inviare una risposta di inoltro
void 
ChordApplication::SendForwardingResponse(uint32_t fileId, uint32_t targetId, uint32_t forwardedToNodeId, uint32_t hopCount) {
  // Troviamo l'indice del nodo target
  uint32_t targetIdx = 0;
  for (uint32_t i = 0; i < m_chordNodes.size(); i++) {
    if (m_chordNodes[i].chordId == targetId) {
      targetIdx = i;
      break;
    }
  }
  
  cout << "RICERCA: Nodo " << m_chordId << " notifica al nodo " << targetId 
       << " che la richiesta per il file " << fileId << " è stata inoltrata al nodo " 
       << forwardedToNodeId << " (hop: " << hopCount << ")" << endl;
  
  Ptr<Packet> packet = Create<Packet>();
  ChordMessage chordMessage;
  chordMessage.type = FORWARDING_RESPONSE;
  chordMessage.sourceId = m_chordId;
  chordMessage.destinationId = targetId;
  chordMessage.fileId = fileId;
  chordMessage.hopCount = hopCount;
  chordMessage.forwardedToNodeId = forwardedToNodeId;
  
  packet->AddHeader(chordMessage);
  
  m_socket->SendTo(packet, 0, InetSocketAddress(m_chordNodes[targetIdx].realIp, m_applicationPort));
}

// Metodo per inviare una risposta di ricerca file
void 
ChordApplication::SendGetFileResponse(uint32_t fileId, uint32_t targetIdx, bool hasFile, uint32_t hopCount) {
  if (targetIdx >= m_chordNodes.size()) {
    cout << "ERRORE: Indice del nodo target non valido" << endl;
    return;
  }
  
  cout << "RICERCA: Nodo " << m_chordId << " invia risposta " 
       << (hasFile ? "POSITIVA" : "NEGATIVA") << " per file " << fileId 
       << " al nodo " << m_chordNodes[targetIdx].chordId << " (hop: " << hopCount << ")" << endl;
  
  Ptr<Packet> packet = Create<Packet>();
  ChordMessage chordMessage;
  chordMessage.type = GET_FILE_RESPONSE;
  chordMessage.sourceId = m_chordId;
  chordMessage.destinationId = m_chordNodes[targetIdx].chordId;
  chordMessage.fileId = fileId;
  chordMessage.hopCount = hopCount;
  chordMessage.success = hasFile;
  
  packet->AddHeader(chordMessage);
  
  m_socket->SendTo(packet, 0, InetSocketAddress(m_chordNodes[targetIdx].realIp, m_applicationPort));
}

// Modifica la gestione delle risposte per gestire sia le risposte definitive che le notifiche di inoltro
void 
ChordApplication::HandleGetFileResponse(ChordMessage msg) {
  uint32_t fileId = msg.fileId;
  bool hasFile = msg.success;
  uint32_t hopCount = msg.hopCount;
  
  if (hasFile) {
    cout << "RICERCA COMPLETATA: Nodo " << m_chordId << " ha ricevuto risposta POSITIVA per file " 
         << fileId << " (hop: " << hopCount << ")" << endl;
  } else {
    cout << "RICERCA COMPLETATA: Nodo " << m_chordId << " ha ricevuto risposta NEGATIVA per file " 
         << fileId << " (hop: " << hopCount << ")" << endl;
  }
  
  // Pulizia della mappa dei nodi visitati
  if (m_visitedNodes.find(fileId) != m_visitedNodes.end()) {
    m_visitedNodes.erase(fileId);
  }
  
  // Chiamiamo il callback per notificare che la ricerca è completata
  if (m_fileLookupCompletedCallback) {
    m_fileLookupCompletedCallback(fileId, hasFile, hopCount);
  }
}

// Metodo per ottenere la lista dei file memorizzati
vector<uint32_t> 
ChordApplication::GetStoredFiles() {
  vector<uint32_t> fileIds;
  for (const auto& file : m_storedFiles) {
    fileIds.push_back(file.fileId);
  }
  return fileIds;
}

void 
ChordApplication::SetFileLookupCompletedCallback(FileLookupCompletedCallback callback) {
  m_fileLookupCompletedCallback = callback;
}

// Metodo per inviare una richiesta di ricerca file con un conteggio di hop specifico
void 
ChordApplication::SendGetFileRequestWithHopCount(uint32_t fileId, uint32_t targetIdx, uint32_t hopCount) {
  if (targetIdx >= m_chordNodes.size()) {
    cout << "ERRORE: Indice del nodo target non valido" << endl;
    return;
  }
  
  cout << "RICERCA: Nodo " << m_chordId << " invia richiesta per file " << fileId 
       << " al nodo " << m_chordNodes[targetIdx].chordId << " (hop: " << hopCount << ")" << endl;
  
  Ptr<Packet> packet = Create<Packet>();
  ChordMessage chordMessage;
  chordMessage.type = GET_FILE_REQUEST;
  chordMessage.sourceId = m_chordId;
  chordMessage.destinationId = m_chordNodes[targetIdx].chordId;
  chordMessage.fileId = fileId;
  chordMessage.hopCount = hopCount;
  
  packet->AddHeader(chordMessage);
  
  m_socket->SendTo(packet, 0, InetSocketAddress(m_chordNodes[targetIdx].realIp, m_applicationPort));
}

ChordMessage
ChordApplication::CreateStoreFileMessage(uint32_t fileId, uint32_t fileSize, uint32_t hopCount) {
  ChordMessage msg;
  msg.type = STORE_FILE_REQUEST;
  msg.sourceId = m_chordId;
  msg.destinationId = 0; // Non specificato, sarà determinato dal routing
  msg.fileId = fileId;
  msg.fileSize = fileSize;
  msg.hopCount = hopCount;
  return msg;
}

void
ChordApplication::InitiateStoreFile(uint32_t fileId, uint32_t fileSize) {
  if (!m_running) return;
  
  cout << "ARCHIVIAZIONE: Nodo " << m_chordId << " inizia processo di archiviazione per file " << fileId 
       << " (dimensione: " << fileSize << " bytes)" << endl;
  
  // Avvia il processo di archiviazione con hop count iniziale a 0
  StoreFile(fileId, fileSize, 0);
}

bool
ChordApplication::IsResponsibleForKey(uint32_t key) {
    // Se c'è un solo nodo nella rete, è responsabile per tutte le chiavi
    if (m_chordNodes.size() == 1) {
        return true;
    }
    
    // Otteniamo l'ID del nostro predecessore
    uint32_t predecessorIdx = m_chordNodes[m_myIndex].predecessor;
    uint32_t predecessorId = m_chordNodes[predecessorIdx].chordId;
    
    // Se il nostro ID è uguale alla chiave, siamo responsabili
    if (m_chordId == key) {
        return true;
    }
    
    // Secondo la specifica di Chord, un nodo è responsabile per le chiavi che sono
    // maggiori dell'ID del suo predecessore e minori o uguali al suo ID
    // Utilizziamo la funzione globale isInRange per verificare se la chiave è nell'intervallo
    // tra il predecessore (escluso) e noi (incluso)
    return isInRange(key, predecessorId, m_chordId);
}

void
ChordApplication::SendForwardingRequest(uint32_t fileId, uint32_t targetIdx, uint32_t originalSourceId, uint32_t hopCount) {
  if (targetIdx >= m_chordNodes.size()) {
    cout << "ERRORE: Indice del nodo target non valido" << endl;
    return;
  }
  
  cout << "RICERCA: Nodo " << m_chordId << " inoltra richiesta per file " << fileId 
       << " al nodo " << m_chordNodes[targetIdx].chordId << " (hop: " << hopCount << ")" << endl;
  
  Ptr<Packet> packet = Create<Packet>();
  ChordMessage chordMessage;
  chordMessage.type = GET_FILE_REQUEST;
  chordMessage.sourceId = originalSourceId; // Manteniamo il nodo sorgente originale
  chordMessage.destinationId = m_chordNodes[targetIdx].chordId;
  chordMessage.fileId = fileId;
  chordMessage.hopCount = hopCount;
  
  packet->AddHeader(chordMessage);
  
  m_socket->SendTo(packet, 0, InetSocketAddress(m_chordNodes[targetIdx].realIp, m_applicationPort));
}

void
ChordApplication::InitiateGetFile(uint32_t fileId) {
  if (!m_running) return;
  
  cout << "RICERCA: Nodo " << m_chordId << " inizia ricerca per file " << fileId << endl;
  
  // Inizializza la mappa dei nodi visitati per questa ricerca
  m_visitedNodes[fileId] = {m_chordId};
  
  // Avvia il processo di ricerca con hop count iniziale a 0
  GetFile(fileId, 0);
}

void
ChordApplication::HandleForwardingResponse(ChordMessage msg) {
  uint32_t fileId = msg.fileId;
  uint32_t forwardedToNodeId = msg.forwardedToNodeId;
  uint32_t hopCount = msg.hopCount;
  
  cout << "RICERCA: Nodo " << m_chordId << " ha ricevuto notifica che la richiesta per il file " 
       << fileId << " è stata inoltrata al nodo " << forwardedToNodeId 
       << " (hop: " << hopCount << ")" << endl;
}

// Metodo unificato per trovare il nodo precedente più lontano dalla chiave
uint32_t 
ChordApplication::FindFarthestPrecedingNode(uint32_t key) {
    // Se la finger table è vuota, restituiamo il nostro indice
    if (m_chordNodes[m_myIndex].fingerTable.empty()) {
        return m_myIndex;
    }

    // Convertiamo la finger table in un formato più facile da manipolare
    vector<pair<uint32_t, uint32_t>> nodes; // (chordId, index)
    for (uint32_t idx : m_chordNodes[m_myIndex].fingerTable) {
        if (idx != m_myIndex) { // Escludiamo noi stessi
            nodes.push_back(make_pair(m_chordNodes[idx].chordId, idx));
        }
    }

    // Caso speciale: se la chiave è uguale al nostro ID, restituiamo il nostro successore
    if (key == m_chordId && !nodes.empty()) {
        return m_chordNodes[m_myIndex].fingerTable[0]; // Il primo elemento è il successore
    }

    // Caso 1: Cerchiamo un nodo che sia nell'intervallo (m_chordId, key]
    // Questo è il caso normale senza wrap-around
    if (m_chordId < key) {
        uint32_t bestIdx = m_myIndex;
        uint32_t bestId = m_chordId;
        
        for (const auto& node : nodes) {
            if (node.first > m_chordId && node.first <= key && node.first > bestId) {
                bestId = node.first;
                bestIdx = node.second;
            }
        }
        
        if (bestIdx != m_myIndex) {
            cout << "ROUTING: Nodo " << m_chordId << " trova nodo precedente " << bestId 
                 << " per chiave " << key << " (caso normale)" << endl;
            return bestIdx;
        }
    }
    
    // Caso 2: Cerchiamo un nodo che sia nell'intervallo (m_chordId, CHORD_SIZE) o [0, key]
    // Questo è il caso con wrap-around
    else {
        // Prima cerchiamo nella parte superiore dell'anello (m_chordId, CHORD_SIZE)
        uint32_t bestIdUpper = m_chordId;
        uint32_t bestIdxUpper = m_myIndex;
        
        for (const auto& node : nodes) {
            if (node.first > m_chordId && node.first > bestIdUpper) {
                bestIdUpper = node.first;
                bestIdxUpper = node.second;
            }
        }
        
        if (bestIdxUpper != m_myIndex) {
            cout << "ROUTING: Nodo " << m_chordId << " trova nodo precedente " << bestIdUpper 
                 << " per chiave " << key << " (parte superiore dell'anello)" << endl;
            return bestIdxUpper;
        }
        
        // Se non troviamo nulla nella parte superiore, cerchiamo nella parte inferiore [0, key]
        uint32_t bestIdLower = 0;
        uint32_t bestIdxLower = m_myIndex;
        bool foundLower = false;
        
        for (const auto& node : nodes) {
            if (node.first <= key && (node.first > bestIdLower || !foundLower)) {
                bestIdLower = node.first;
                bestIdxLower = node.second;
                foundLower = true;
            }
        }
        
        if (foundLower) {
            cout << "ROUTING: Nodo " << m_chordId << " trova nodo precedente " << bestIdLower 
                 << " per chiave " << key << " (parte inferiore dell'anello)" << endl;
            return bestIdxLower;
        }
    }
    
    // Se non troviamo un nodo migliore, restituiamo il nostro successore
    if (!nodes.empty()) {
        uint32_t successorIdx = m_chordNodes[m_myIndex].fingerTable[0];
        cout << "ROUTING: Nodo " << m_chordId << " non ha trovato un nodo migliore, "
             << "utilizza il successore " << m_chordNodes[successorIdx].chordId << endl;
        return successorIdx;
    }
    
    // Se non abbiamo nemmeno un successore, restituiamo noi stessi
    return m_myIndex;
}




