#include "chord-header.h"
#include "chord-application.h"
#include <iostream>
#include <algorithm>
#include <sstream>
#include <functional>

using namespace ns3;
using namespace std;

// Funzione per trovare il successore di un ID nella rete Chord
uint32_t findSuccessor(uint32_t id, const vector<ChordInfo>& chordNodes) {
  // Caso base: un solo nodo
  if (chordNodes.size() == 1) {
    return 0;
  }
  
  // Cerchiamo il primo nodo con ID >= id
  for (uint32_t i = 0; i < chordNodes.size(); i++) {
    uint32_t nextIdx = (i + 1) % chordNodes.size();
    if (isInRange(id, chordNodes[i].chordId, chordNodes[nextIdx].chordId)) {
      return nextIdx;
    }
  }
  
  // Se non troviamo nulla, restituiamo il primo nodo
  return 0;
}

// Funzione per configurare la rete Chord e costruire le tabelle di routing
void SetupChordNetwork(NodeContainer &nodes, vector<Ptr<ChordApplication>>& chordApps) {
  vector<ChordInfo> chordNodes;
  uint32_t numNodes = nodes.GetN();

  // Per ogni nodo estraiamo l'IP reale (prendiamo la prima interfaccia non loopback) 
  for (uint32_t i = 0; i < numNodes; ++i) {
    Ptr<Node> node = nodes.Get(i);
    Ipv4Address ip;
    Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
    if (ipv4->GetNInterfaces() > 1) {
      ip = ipv4->GetAddress(1, 0).GetLocal();
    } else {
      ip = Ipv4Address("0.0.0.0");
    }

    std::ostringstream oss;
    oss << ip;
    std::string key = oss.str() + "_" + std::to_string(node->GetId());
    std::hash<std::string> hasher;
    uint32_t chordId = hasher(key) % CHORD_SIZE;
    ChordInfo info;
    info.chordId = chordId;
    info.realIp = ip;
    info.node = node;
    chordNodes.push_back(info);
  }

  // Ordina la struttura dati in base al chordId
  sort(chordNodes.begin(), chordNodes.end(), [](const ChordInfo &a, const ChordInfo &b) {
    return a.chordId < b.chordId;
  });

  // Visualizziamo il ring ordinato
  cout << "Chord ring ordinato per chordId:" << endl;
  for (uint32_t i = 0; i < chordNodes.size(); ++i) {
    cout << "Nodo " << chordNodes[i].node->GetId()
         << " - chordId: " << chordNodes[i].chordId
         << " - IP reale: " << chordNodes[i].realIp << endl;
  }
  cout << endl;

  // Costruiamo le finger table per ogni nodo
  for (uint32_t i = 0; i < chordNodes.size(); ++i) {
    // Inizializziamo la finger table
    chordNodes[i].fingerTable.resize(FINGER_TABLE_SIZE);
    
    // Calcoliamo il predecessore
    chordNodes[i].predecessor = (i == 0) ? chordNodes.size() - 1 : i - 1;
    
    // Calcoliamo le finger table
    for (uint32_t j = 0; j < FINGER_TABLE_SIZE; ++j) {
      // Calcoliamo l'ID del j-esimo finger: (n + 2^j) mod 2^m
      uint32_t fingerID = (chordNodes[i].chordId + (1 << j)) % CHORD_SIZE;
      
      // Troviamo il successore di questo ID
      uint32_t successorIdx = i;
      for (uint32_t k = 0; k < chordNodes.size(); ++k) {
        uint32_t nextIdx = (i + k) % chordNodes.size();
        if (chordNodes[nextIdx].chordId >= fingerID) {
          successorIdx = nextIdx;
          break;
        }
      }
      
      // Se non abbiamo trovato un successore, prendiamo il primo nodo (wrap-around)
      if (successorIdx == i && chordNodes[i].chordId >= fingerID) {
        successorIdx = (i + 1) % chordNodes.size();
      }
      
      chordNodes[i].fingerTable[j] = successorIdx;
    }
  }

  // Visualizziamo le finger table
  cout << "Finger tables:" << endl;
  for (uint32_t i = 0; i < chordNodes.size(); ++i) {
    cout << "Nodo " << chordNodes[i].node->GetId() 
         << " (chordId " << chordNodes[i].chordId << "):" << endl;
    
    cout << "  Predecessore: Nodo " << chordNodes[chordNodes[i].predecessor].node->GetId() 
         << " (chordId " << chordNodes[chordNodes[i].predecessor].chordId << ")" << endl;
    
    cout << "  Finger Table:" << endl;
    for (uint32_t j = 0; j < min(FINGER_TABLE_SIZE, (uint32_t)5); ++j) {  // Mostriamo solo i primi 5 finger per brevità
      uint32_t fingerIdx = chordNodes[i].fingerTable[j];
      cout << "    Finger[" << j << "] (+" << (1 << j) << "): Nodo " 
           << chordNodes[fingerIdx].node->GetId()
           << " (chordId " << chordNodes[fingerIdx].chordId << ")" << endl;
    }
    cout << endl;
  }

  // Installiamo l'applicazione Chord su ogni nodo
  chordApps.clear();
  for (uint32_t i = 0; i < chordNodes.size(); ++i) {
    Ptr<ChordApplication> app = CreateObject<ChordApplication>();
    chordNodes[i].node->AddApplication(app);
    app->Setup(chordNodes[i].chordId, chordNodes);
    app->SetStartTime(Seconds(1.0));
    app->SetStopTime(Seconds(100.0));
    chordApps.push_back(app);
  }
}

// Funzione per pianificare operazioni di lookup casuali
void ScheduleRandomLookups(vector<Ptr<ChordApplication>>& chordApps, int numLookups) {
  Ptr<UniformRandomVariable> r = CreateObject<UniformRandomVariable>();
  
  // Eseguiamo numLookups lookup casuali con traffico di rete reale
  for (int i = 0; i < numLookups; i++) {
    uint32_t randomKey = r->GetInteger(0, CHORD_SIZE - 1);
    uint32_t startNodeIdx = r->GetInteger(0, chordApps.size() - 1);
    
    // Pianifichiamo il lookup dopo 5 secondi + 1 secondo per ogni lookup
    Simulator::Schedule(Seconds(5.0 + i * 1.0), &ChordApplication::LookupKey, 
                        chordApps[startNodeIdx], randomKey);
    
    cout << "Pianificato lookup #" << (i+1) << " per chiave " << randomKey 
         << " dal nodo " << chordApps[startNodeIdx]->GetNode()->GetId() << endl;
  }
} 