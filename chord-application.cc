#include "chord-application.h"
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

void 
ChordApplication::SendLookupRequest(uint32_t key, uint32_t targetNodeIdx) {
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
  
  cout << "Nodo " << GetNode()->GetId() << " (chordId: " << m_chordId 
       << ") ha inviato una richiesta di lookup per la chiave " << key 
       << " al nodo " << m_chordNodes[targetNodeIdx].node->GetId() 
       << " (chordId: " << m_chordNodes[targetNodeIdx].chordId << ")" << endl;
}

void 
ChordApplication::LookupKey(uint32_t key) {
  cout << "Nodo " << GetNode()->GetId() << " (chordId: " << m_chordId 
       << ") inizia lookup per chiave " << key << endl;
  
  uint32_t currentIdx = m_myIndex;
  
  // Se la chiave è tra il nodo corrente e il suo successore, il successore è responsabile
  uint32_t successorIdx = m_chordNodes[currentIdx].fingerTable[0];
  if (isInRange(key, m_chordNodes[currentIdx].chordId, m_chordNodes[successorIdx].chordId)) {
    cout << "Trovato localmente! La chiave " << key << " è gestita dal nodo " 
         << m_chordNodes[successorIdx].node->GetId() << " (chordId: " 
         << m_chordNodes[successorIdx].chordId << ")" << endl;
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
  cout << "Nodo " << GetNode()->GetId() << " (chordId: " << m_chordId 
       << ") ha ricevuto una richiesta di lookup per la chiave " << msg.key 
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
  
  cout << "Nodo " << GetNode()->GetId() << " (chordId: " << m_chordId 
       << ") ha inviato una risposta di lookup per la chiave " << key 
       << " al nodo " << m_chordNodes[targetIdx].node->GetId() 
       << " (chordId: " << targetId << ")" << endl;
}

void 
ChordApplication::HandleLookupResponse(ChordMessage msg) {
  cout << "Nodo " << GetNode()->GetId() << " (chordId: " << m_chordId 
       << ") ha ricevuto una risposta di lookup per la chiave " << msg.key 
       << ": la chiave è gestita dal nodo con chordId: " << msg.responseNodeId << endl;
}

void 
ChordApplication::PrintStats() {
  cout << "Nodo " << GetNode()->GetId() << " (chordId: " << m_chordId 
       << ") - Pacchetti inviati: " << m_packetsSent 
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
    
    if (msg.type == LOOKUP_REQUEST) {
      HandleLookupRequest(msg, from);
    } else if (msg.type == LOOKUP_RESPONSE) {
      HandleLookupResponse(msg);
    }
  }
} 