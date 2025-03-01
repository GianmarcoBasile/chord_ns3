#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/nix-vector-helper.h"
#include <vector>
#include <algorithm>
#include <iostream>
#include <string>
#include <sstream>   // Aggiungi questo include per ostringstream
#include <functional>  // Per std::hash
#include <map>
#include <cmath>

using namespace ns3;
using namespace std;

NS_LOG_COMPONENT_DEFINE ("The_net");

// Costanti per la rete Chord
const uint32_t CHORD_BITS = 14;  // 2^14 = 16384 possibili ID
const uint32_t CHORD_SIZE = 1 << CHORD_BITS;
const uint32_t FINGER_TABLE_SIZE = CHORD_BITS;

// Struttura per memorizzare le informazioni di ogni nodo nel network Chord
struct ChordInfo {
  uint32_t chordId;
  Ipv4Address realIp;
  Ptr<Node> node;
  vector<uint32_t> fingerTable;  // Indici dei nodi nella finger table
  uint32_t predecessor;          // Indice del predecessore
};

// Funzione per verificare se id è nell'intervallo (start, end) nel ring Chord
bool isInRange(uint32_t id, uint32_t start, uint32_t end) {
  if (start < end) {
    return (id > start && id <= end);
  } else {  // L'intervallo attraversa lo zero
    return (id > start || id <= end);
  }
}

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

// Funzione per cercare la chiave nella rete Chord
uint32_t lookupKey(uint32_t key, uint32_t startNodeIdx, const vector<ChordInfo>& chordNodes) {
  cout << "Lookup per chiave " << key << " partendo dal nodo " << chordNodes[startNodeIdx].node->GetId() << endl;
  
  uint32_t currentIdx = startNodeIdx;
  uint32_t hops = 0;
  
  while (true) {
    hops++;
    
    // Se la chiave è tra il nodo corrente e il suo successore, il successore è responsabile
    uint32_t successorIdx = chordNodes[currentIdx].fingerTable[0];
    if (isInRange(key, chordNodes[currentIdx].chordId, chordNodes[successorIdx].chordId)) {
      cout << "Trovato! La chiave " << key << " è gestita dal nodo " << chordNodes[successorIdx].node->GetId() 
           << " (chordId: " << chordNodes[successorIdx].chordId << ") dopo " << hops << " hop" << endl;
      return successorIdx;
    }
    
    // Altrimenti, cerchiamo nella finger table il nodo più vicino ma che non superi la chiave
    bool found = false;
    for (int i = FINGER_TABLE_SIZE - 1; i >= 0; i--) {
      uint32_t fingerIdx = chordNodes[currentIdx].fingerTable[i];
      if (isInRange(chordNodes[fingerIdx].chordId, chordNodes[currentIdx].chordId, key)) {
        currentIdx = fingerIdx;
        found = true;
        cout << "Hop a nodo " << chordNodes[currentIdx].node->GetId() 
             << " (chordId: " << chordNodes[currentIdx].chordId << ")" << endl;
        break;
      }
    }
    
    // Se non troviamo un nodo migliore nella finger table, passiamo al successore
    if (!found) {
      currentIdx = successorIdx;
      cout << "Nessun nodo migliore trovato nella finger table, passiamo al successore " 
           << chordNodes[currentIdx].node->GetId() << endl;
    }
    
    // Limitiamo il numero di hop per evitare loop infiniti
    if (hops > chordNodes.size()) {
      cout << "Troppi hop, possibile loop. Restituiamo il nodo corrente." << endl;
      return currentIdx;
    }
  }
}

// Metodo per configurare la rete Chord e costruire le tabelle di routing
void SetupChordNetwork(NodeContainer &nodes) {
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

  // Simuliamo alcune operazioni di lookup
  Ptr<UniformRandomVariable> r = CreateObject<UniformRandomVariable> ();
  
  // Eseguiamo 5 lookup casuali
  for (int i = 0; i < 5; i++) {
    uint32_t randomKey = r->GetInteger(0, CHORD_SIZE - 1);
    uint32_t startNodeIdx = r->GetInteger(0, chordNodes.size() - 1);
    
    cout << "\nLookup #" << (i+1) << ":" << endl;
    uint32_t resultIdx = lookupKey(randomKey, startNodeIdx, chordNodes);
    
    cout << "Risultato: La chiave " << randomKey << " è gestita dal nodo " 
         << chordNodes[resultIdx].node->GetId() << " (chordId: " 
         << chordNodes[resultIdx].chordId << ")" << endl;
  }
}

int
main (int argc, char *argv[])
{ 
  uint32_t numnodes = 100, seed = 100, run = 1000;
  CommandLine cmd;
  cmd.AddValue("numnodes", "Number of nodes", numnodes);
  cmd.AddValue("seed", "RNG seed", seed);
  cmd.AddValue("run", "RNG run", run);
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);

  RngSeedManager::SetSeed(seed);
  RngSeedManager::SetRun(run);

  NodeContainer nodes;
  nodes.Create (numnodes);
  
  NetDeviceContainer netdev;

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("50ms"));

  Ipv4NixVectorHelper nixRouting;
  InternetStackHelper stackIP;
  stackIP.SetRoutingHelper(nixRouting);
  stackIP.Install(nodes);

  Ipv4AddressHelper ipv4;
  Ipv4Address adr;
  ipv4.SetBase ("10.0.0.0", "/30");
 
  for (uint32_t k = 10, i = 0; i < k; ++i)
  { 
    netdev = p2p.Install(nodes.Get(i), nodes.Get((i+1)%k));
    adr = ipv4.NewNetwork();
    // cout << i <<" - " << (i+1)%k << "\t" << adr << endl;
    ipv4.Assign (netdev);
  }    
  
  for (uint32_t i = 10; i < numnodes; ++i)
  { 
    uint32_t minlinks = 3;
    Ptr<UniformRandomVariable> r = CreateObject<UniformRandomVariable> ();
    vector<bool> link(numnodes);
    uint32_t numlinks = 0;
    while (numlinks < minlinks)
    { 
      uint32_t j = r->GetInteger((i-10)/3*2, i-1);
      // cout << "r= " << j << " imin= " << (i-10)/3*2 << " i max = " << i-1 << endl;
      if ((j == i) || link[j])
        continue;
      link[j] = true;
      netdev = p2p.Install(nodes.Get(i), nodes.Get(j));
      adr = ipv4.NewNetwork();
      // cout << i << " - " << j <<"\t" << adr <<" \n";
      ipv4.Assign (netdev); 
      ++numlinks;
    }    
  }

  Ptr<UniformRandomVariable> r = CreateObject<UniformRandomVariable> ();
  uint32_t A = r->GetInteger(0, numnodes-1), B;
  do { 
     B = r->GetInteger(0, numnodes-1);
  } while (A == B);
  cout << "A= " << A << " B= " << B << endl;

  UdpEchoServerHelper echoServer(9);
  Ptr<Node> srv = nodes.Get(A);
  ApplicationContainer serverApps = echoServer.Install(srv);
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (100.0));
  Ipv4Address a = srv->GetObject<Ipv4>()->GetAddress(1,0).GetLocal();
  
  UdpEchoClientHelper echoClient (a, 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (2));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (5.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientApps = echoClient.Install(nodes.Get(B));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (100.0));

  cout << "Numnodes = " << numnodes << endl;
 
  // Chiamata al nuovo metodo per configurare la rete Chord e costruire le tabelle di routing
  SetupChordNetwork(nodes);
  
  Simulator::Run();
  Simulator::Destroy();
  
  return 0;
}


