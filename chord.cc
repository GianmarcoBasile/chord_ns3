#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/nix-vector-helper.h"

#include <iostream>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <algorithm>
#include <sstream>

using namespace ns3;
using namespace std;

NS_LOG_COMPONENT_DEFINE ("The_net");

//---------------------------------------------------------------------------
//  Struttura dati per l'overlay Chord di ogni nodo
//---------------------------------------------------------------------------
// Ogni nodo memorizza:
// - chordId: un ID Chord assegnato casualmente
// - ip: l'indirizzo IP assegnato dal modello NS‑3 (ottenuto con GetNodeIp)
// - fingerTable: vettore di 3 voci, ciascuna come coppia (ChordID, IP) del nodo successivo
// - fileList: lista dei file gestiti (non utilizzata in questo esempio)
//---------------------------------------------------------------------------
struct ChordNodeInfo
{
  uint32_t chordId;
  Ipv4Address ip;
  vector< pair<uint32_t, Ipv4Address> > fingerTable;
  vector<uint32_t> fileList;
};

// Vettore globale: per ogni nodo NS‑3 viene memorizzata la relativa informazione Chord
static vector<ChordNodeInfo> chordData;

//---------------------------------------------------------------------------
// Restituisce l'indirizzo IP assegnato al nodo (la prima interfaccia non-loopback)
//---------------------------------------------------------------------------
static Ipv4Address
GetNodeIp (Ptr<Node> node)
{
  Ptr<Ipv4> ipv4 = node->GetObject<Ipv4> ();
  return ipv4->GetAddress (1, 0).GetLocal();
}

//---------------------------------------------------------------------------
// Inizializza i nodi Chord: assegna ad ogni nodo un ChordID casuale e prepara la finger table
// (gli IP verranno aggiornati successivamente)
//---------------------------------------------------------------------------
static void
InsertChordInfo (NodeContainer &nodes)
{
  uint32_t numnodes = nodes.GetN ();
  chordData.resize(numnodes);
  srand(time(0));

  for (uint32_t i = 0; i < numnodes; i++)
  {
    chordData[i].chordId = rand() % 10000;
    // IP inizialmente placeholder; verrà aggiornato in BuildChordOverlay()
    chordData[i].ip = Ipv4Address("0.0.0.0");
    // Prepara una finger table di 3 voci (placeholder)
    chordData[i].fingerTable.resize(3, make_pair(999999, Ipv4Address("0.0.0.0")));
    chordData[i].fileList.clear();
  }
  
  cout << "\n=== [Chord Info] Nodi Chord inizializzati ===" << endl;
  for (uint32_t i = 0; i < numnodes; i++)
  {
    cout << "ChordID=" << chordData[i].chordId << endl;
  }
  cout << "=============================================" << endl;
}

//---------------------------------------------------------------------------
// Costruisce l’overlay Chord: aggiorna l’IP dei nodi, ordina per ChordID e calcola la finger table
//--------------------------------------------------------------------------- 
static void
BuildChordOverlay (NodeContainer &nodes)
{
  uint32_t numnodes = nodes.GetN ();
  // Aggiorna l'IP per ogni nodo con l'IP reale assegnato dallo stack NS‑3
  for (uint32_t i = 0; i < numnodes; i++)
  {
    chordData[i].ip = GetNodeIp (nodes.Get(i));
  }
  
  // Crea un vettore di indici ordinati in base al ChordID
  vector<uint32_t> idx(numnodes);
  for (uint32_t i = 0; i < numnodes; i++)
    idx[i] = i;
  sort(idx.begin(), idx.end(), [] (uint32_t a, uint32_t b) {
      return chordData[a].chordId < chordData[b].chordId;
  });

  // (1) Imposta fingerTable[0] come il successore immediato nell’anello (ordinamento circolare)
  for (uint32_t k = 0; k < numnodes; k++)
  {
    uint32_t nextK = (k + 1) % numnodes;
    uint32_t succIndex = idx[nextK];
    chordData[idx[k]].fingerTable[0] = make_pair(chordData[succIndex].chordId, chordData[succIndex].ip);
  }
  
  // (2) Calcola fingerTable[1] e fingerTable[2] usando target = (myChordId + 2^i) mod 10000
  for (uint32_t k = 0; k < numnodes; k++)
  {
    uint32_t myId = chordData[idx[k]].chordId;
    for (uint32_t i = 1; i < 3; i++)
    {
      uint32_t target = (myId + (1 << i)) % 10000;
      uint32_t j = k;
      while (j < numnodes && chordData[idx[j]].chordId < target)
          j++;
      j %= numnodes;
      chordData[idx[k]].fingerTable[i] = make_pair(chordData[idx[j]].chordId, chordData[idx[j]].ip);
    }
  }
  
  // Stampa la "Chord Routing Table": per ogni nodo, mostra ChordID, IP e finger table
  cout << "\n=== [Chord Routing Table] ===" << endl;
  for (uint32_t i = 0; i < numnodes; i++)
  {
    cout << "ChordID=" << chordData[i].chordId << "  IP=" << chordData[i].ip << "  Finger Table: ";
    for (auto &entry : chordData[i].fingerTable)
    {
      cout << "[ID=" << entry.first << " IP=" << entry.second << "] ";
    }
    cout << endl;
  }
  cout << "==============================" << endl;
}

//---------------------------------------------------------------------------
// Aggiorna la route table NS‑3: per ogni nodo aggiunge una route statica che usa
// direttamente l'IP reale del nodo come destinazione
//--------------------------------------------------------------------------- 
static void
UpdateRoutingTable (NodeContainer &nodes)
{
  uint32_t numnodes = nodes.GetN ();
  Ipv4StaticRoutingHelper staticRoutingHelper;
  for (uint32_t i = 0; i < numnodes; i++)
  {
    Ptr<Ipv4> ipv4 = nodes.Get(i)->GetObject<Ipv4>();
    Ptr<Ipv4StaticRouting> staticRouting = staticRoutingHelper.GetStaticRouting(ipv4);
    if (staticRouting == 0)
    {
      NS_FATAL_ERROR("Ipv4StaticRouting non trovato sul nodo " << i);
    }
    // Usa direttamente l'IP reale del nodo come destinazione
    Ipv4Address dest = /* qui, ad esempio, puoi usare l'IP reale o un prefisso basato sull'IP reale */;
    // Per questo esempio, aggiungiamo una route verso l'IP del nodo
    staticRouting->AddNetworkRouteTo(dest, Ipv4Mask("255.255.255.255"), /* next hop */ dest, 1);
  }
}

//---------------------------------------------------------------------------
// MAIN: topologia fisica invariata + overlay Chord integrato
//--------------------------------------------------------------------------- 
int main (int argc, char *argv[])
{ 
  uint32_t numnodes = 100, seed = 100, run = 1000;
  CommandLine cmd;
  cmd.AddValue("numnodes", "Number of nodes", numnodes);
  cmd.AddValue("seed", "RNG seed", seed);
  cmd.AddValue("run", "RNG run", run);
  cmd.Parse (argc, argv);

  Time::SetResolution(Time::NS);
  LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);

  RngSeedManager::SetSeed(seed);
  RngSeedManager::SetRun(run);

  // Creazione della topologia fisica (invariata)
  NodeContainer nodes;
  nodes.Create(numnodes);
  
  NetDeviceContainer netdev;
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("50ms"));

  // Installazione dello stack IP con routing statico
  Ipv4StaticRoutingHelper staticRoutingHelper;
  InternetStackHelper stackIP;
  stackIP.SetRoutingHelper(staticRoutingHelper);
  stackIP.Install(nodes);

  Ipv4AddressHelper ipv4;
  Ipv4Address adr;
  ipv4.SetBase("10.0.0.0", "/30");
 
  // I primi 10 nodi in anello
  for (uint32_t k = 10, i = 0; i < k; ++i)
  { 
    netdev = p2p.Install(nodes.Get(i), nodes.Get((i+1)%k));
    adr = ipv4.NewNetwork();
    ipv4.Assign(netdev);
  }    
  
  // Gli altri nodi collegati in modo random (minimo 3 link per nodo da 10 a numnodes-1)
  for (uint32_t i = 10; i < numnodes; ++i)
  { 
    uint32_t minlinks = 3;
    Ptr<UniformRandomVariable> r = CreateObject<UniformRandomVariable> ();
    vector<bool> link(numnodes, false);
    uint32_t numlinks = 0;
    while (numlinks < minlinks)
    {
      uint32_t j = r->GetInteger((i-10)/3*2, i-1);
      if ((j == i) || link[j])
          continue;
      link[j] = true;
      netdev = p2p.Install(nodes.Get(i), nodes.Get(j));
      adr = ipv4.NewNetwork();
      ipv4.Assign(netdev);
      ++numlinks;
    }
  }
  
  // Scelta di due nodi casuali per UDP echo
  Ptr<UniformRandomVariable> r = CreateObject<UniformRandomVariable> ();
  uint32_t A = r->GetInteger(0, numnodes-1), B;
  do {
     B = r->GetInteger(0, numnodes-1);
  } while (A == B);
  cout << "A= " << A << "  B= " << B << endl;
  
  UdpEchoServerHelper echoServer(9);
  Ptr<Node> srv = nodes.Get(A);
  ApplicationContainer serverApps = echoServer.Install(srv);
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(100.0));
  
  Ipv4Address a = srv->GetObject<Ipv4>()->GetAddress(1,0).GetLocal();
  UdpEchoClientHelper echoClient(a, 9);
  echoClient.SetAttribute("MaxPackets", UintegerValue(2));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(5.0)));
  echoClient.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApps = echoClient.Install(nodes.Get(B));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(100.0));
  
  cout << "Numnodes = " << numnodes << endl;

  // ---- Overlay Chord ----
  InsertChordInfo(nodes);
  BuildChordOverlay(nodes);
  UpdateRoutingTable(nodes);
  
  Simulator::Run();
  Simulator::Destroy();
  
  return 0;
}
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/nix-vector-helper.h"

#include <iostream>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <algorithm>
#include <sstream>

using namespace ns3;
using namespace std;

NS_LOG_COMPONENT_DEFINE ("The_net");

//---------------------------------------------------------------------------
// Struttura dati per l'overlay Chord di ogni nodo
//---------------------------------------------------------------------------
// Ogni nodo memorizza:
// - chordId: un ID Chord assegnato casualmente
// - ip: l'indirizzo IP assegnato dal modello NS‑3 (ottenuto con GetNodeIp)
// - fingerTable: vettore di 3 voci, ciascuna come coppia (ChordID, IP) del nodo successivo
// - fileList: lista dei file gestiti (non utilizzata in questo esempio)
//---------------------------------------------------------------------------
struct ChordNodeInfo
{
  uint32_t chordId;
  Ipv4Address ip;
  vector< pair<uint32_t, Ipv4Address> > fingerTable;
  vector<uint32_t> fileList;
};

// Vettore globale: per ogni nodo NS‑3 viene memorizzata la relativa informazione Chord
static vector<ChordNodeInfo> chordData;

//---------------------------------------------------------------------------
// Restituisce l'indirizzo IP assegnato al nodo (la prima interfaccia non-loopback)
//---------------------------------------------------------------------------
static Ipv4Address
GetNodeIp (Ptr<Node> node)
{
  Ptr<Ipv4> ipv4 = node->GetObject<Ipv4> ();
  return ipv4->GetAddress (1, 0).GetLocal();
}

//---------------------------------------------------------------------------
// Inizializza i nodi Chord: assegna ad ogni nodo un ChordID casuale e prepara la finger table
// (gli IP verranno aggiornati successivamente)
//---------------------------------------------------------------------------
static void
InsertChordInfo (NodeContainer &nodes)
{
  uint32_t numnodes = nodes.GetN ();
  chordData.resize(numnodes);
  srand(time(0));

  for (uint32_t i = 0; i < numnodes; i++)
  {
    chordData[i].chordId = rand() % 10000;
    // IP inizialmente placeholder; verrà aggiornato in BuildChordOverlay()
    chordData[i].ip = Ipv4Address("0.0.0.0");
    // Prepara una finger table di 3 voci (placeholder)
    chordData[i].fingerTable.resize(3, make_pair(999999, Ipv4Address("0.0.0.0")));
    chordData[i].fileList.clear();
  }
  
  cout << "\n=== [Chord Info] Nodi Chord inizializzati ===" << endl;
  for (uint32_t i = 0; i < numnodes; i++)
  {
    cout << "ChordID=" << chordData[i].chordId << endl;
  }
  cout << "=============================================" << endl;
}

//---------------------------------------------------------------------------
// Costruisce l’overlay Chord: aggiorna l’IP dei nodi, ordina per ChordID e calcola la finger table
//--------------------------------------------------------------------------- 
static void
BuildChordOverlay (NodeContainer &nodes)
{
  uint32_t numnodes = nodes.GetN ();
  // Aggiorna l'IP per ogni nodo con l'IP reale assegnato dallo stack NS‑3
  for (uint32_t i = 0; i < numnodes; i++)
  {
    chordData[i].ip = GetNodeIp (nodes.Get(i));
  }
  
  // Crea un vettore di indici ordinati in base al ChordID
  vector<uint32_t> idx(numnodes);
  for (uint32_t i = 0; i < numnodes; i++)
    idx[i] = i;
  sort(idx.begin(), idx.end(), [] (uint32_t a, uint32_t b) {
      return chordData[a].chordId < chordData[b].chordId;
  });

  // (1) Imposta fingerTable[0] come il successore immediato nell’anello (ordinamento circolare)
  for (uint32_t k = 0; k < numnodes; k++)
  {
    uint32_t nextK = (k + 1) % numnodes;
    uint32_t succIndex = idx[nextK];
    chordData[idx[k]].fingerTable[0] = make_pair(chordData[succIndex].chordId, chordData[succIndex].ip);
  }
  
  // (2) Calcola fingerTable[1] e fingerTable[2] usando target = (myChordId + 2^i) mod 10000
  for (uint32_t k = 0; k < numnodes; k++)
  {
    uint32_t myId = chordData[idx[k]].chordId;
    for (uint32_t i = 1; i < 3; i++)
    {
      uint32_t target = (myId + (1 << i)) % 10000;
      uint32_t j = k;
      while (j < numnodes && chordData[idx[j]].chordId < target)
          j++;
      j %= numnodes;
      chordData[idx[k]].fingerTable[i] = make_pair(chordData[idx[j]].chordId, chordData[idx[j]].ip);
    }
  }
  
  // Stampa la "Chord Routing Table": per ogni nodo, mostra ChordID, IP e finger table
  cout << "\n=== [Chord Routing Table] ===" << endl;
  for (uint32_t i = 0; i < numnodes; i++)
  {
    cout << "ChordID=" << chordData[i].chordId << "  IP=" << chordData[i].ip << "  Finger Table: ";
    for (auto &entry : chordData[i].fingerTable)
    {
      cout << "[ID=" << entry.first << " IP=" << entry.second << "] ";
    }
    cout << endl;
  }
  cout << "==============================" << endl;
}

//---------------------------------------------------------------------------
// Aggiorna la route table NS‑3: per ogni nodo aggiunge una route statica che usa
// direttamente l'IP reale del nodo come destinazione
//--------------------------------------------------------------------------- 
static void
UpdateRoutingTable (NodeContainer &nodes)
{
  uint32_t numnodes = nodes.GetN ();
  Ipv4StaticRoutingHelper staticRoutingHelper;
  for (uint32_t i = 0; i < numnodes; i++)
  {
    Ptr<Ipv4> ipv4 = nodes.Get(i)->GetObject<Ipv4>();
    Ptr<Ipv4StaticRouting> staticRouting = staticRoutingHelper.GetStaticRouting(ipv4);
    if (staticRouting == 0)
    {
      NS_FATAL_ERROR("Ipv4StaticRouting non trovato sul nodo " << i);
    }
    // Usa direttamente l'IP reale del nodo come destinazione
    Ipv4Address dest = /* qui, ad esempio, puoi usare l'IP reale o un prefisso basato sull'IP reale */;
    // Per questo esempio, aggiungiamo una route verso l'IP del nodo
    staticRouting->AddNetworkRouteTo(dest, Ipv4Mask("255.255.255.255"), /* next hop */ dest, 1);
  }
}

//---------------------------------------------------------------------------
// MAIN: topologia fisica invariata + overlay Chord integrato
//--------------------------------------------------------------------------- 
int main (int argc, char *argv[])
{ 
  uint32_t numnodes = 100, seed = 100, run = 1000;
  CommandLine cmd;
  cmd.AddValue("numnodes", "Number of nodes", numnodes);
  cmd.AddValue("seed", "RNG seed", seed);
  cmd.AddValue("run", "RNG run", run);
  cmd.Parse (argc, argv);

  Time::SetResolution(Time::NS);
  LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);

  RngSeedManager::SetSeed(seed);
  RngSeedManager::SetRun(run);

  // Creazione della topologia fisica (invariata)
  NodeContainer nodes;
  nodes.Create(numnodes);
  
  NetDeviceContainer netdev;
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("50ms"));

  // Installazione dello stack IP con routing statico
  Ipv4StaticRoutingHelper staticRoutingHelper;
  InternetStackHelper stackIP;
  stackIP.SetRoutingHelper(staticRoutingHelper);
  stackIP.Install(nodes);

  Ipv4AddressHelper ipv4;
  Ipv4Address adr;
  ipv4.SetBase("10.0.0.0", "/30");
 
  // I primi 10 nodi in anello
  for (uint32_t k = 10, i = 0; i < k; ++i)
  { 
    netdev = p2p.Install(nodes.Get(i), nodes.Get((i+1)%k));
    adr = ipv4.NewNetwork();
    ipv4.Assign(netdev);
  }    
  
  // Gli altri nodi collegati in modo random (minimo 3 link per nodo da 10 a numnodes-1)
  for (uint32_t i = 10; i < numnodes; ++i)
  { 
    uint32_t minlinks = 3;
    Ptr<UniformRandomVariable> r = CreateObject<UniformRandomVariable> ();
    vector<bool> link(numnodes, false);
    uint32_t numlinks = 0;
    while (numlinks < minlinks)
    {
      uint32_t j = r->GetInteger((i-10)/3*2, i-1);
      if ((j == i) || link[j])
          continue;
      link[j] = true;
      netdev = p2p.Install(nodes.Get(i), nodes.Get(j));
      adr = ipv4.NewNetwork();
      ipv4.Assign(netdev);
      ++numlinks;
    }
  }
  
  // Scelta di due nodi casuali per UDP echo
  Ptr<UniformRandomVariable> r = CreateObject<UniformRandomVariable> ();
  uint32_t A = r->GetInteger(0, numnodes-1), B;
  do {
     B = r->GetInteger(0, numnodes-1);
  } while (A == B);
  cout << "A= " << A << "  B= " << B << endl;
  
  UdpEchoServerHelper echoServer(9);
  Ptr<Node> srv = nodes.Get(A);
  ApplicationContainer serverApps = echoServer.Install(srv);
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(100.0));
  
  Ipv4Address a = srv->GetObject<Ipv4>()->GetAddress(1,0).GetLocal();
  UdpEchoClientHelper echoClient(a, 9);
  echoClient.SetAttribute("MaxPackets", UintegerValue(2));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(5.0)));
  echoClient.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApps = echoClient.Install(nodes.Get(B));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(100.0));
  
  cout << "Numnodes = " << numnodes << endl;

  // ---- Overlay Chord ----
  InsertChordInfo(nodes);
  BuildChordOverlay(nodes);
  UpdateRoutingTable(nodes);
  
  Simulator::Run();
  Simulator::Destroy();
  
  return 0;
}
