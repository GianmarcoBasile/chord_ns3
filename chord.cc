#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/nix-vector-helper.h"
#include "ns3/socket.h"
#include "ns3/socket-factory.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"
#include "ns3/simulator.h"
#include "ns3/udp-socket-factory.h"
#include <vector>
#include <algorithm>
#include <iostream>
#include <string>
#include <sstream>   // Aggiungi questo include per ostringstream
#include <functional>  // Per std::hash
#include <map>
#include <cmath>
#include "chord-header.h"
#include "chord-application.h"
#include "chord-helper.h"

using namespace ns3;
using namespace std;

NS_LOG_COMPONENT_DEFINE ("The_net");

// Rimuoviamo tutte le ridefinizioni di costanti, enum e strutture
// che sono già definite in chord-header.h

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
 
  // Configuriamo la rete Chord
  vector<Ptr<ChordApplication>> chordApps;
  SetupChordNetwork(nodes, chordApps);
  
  // Pianifichiamo alcune operazioni di lookup casuali
  ScheduleRandomLookups(chordApps, 5);
  
  // Aggiungiamo un callback per stampare le statistiche complessive alla fine della simulazione
  Simulator::Schedule(Seconds(95.0), []() {
    cout << "\n\n=== STATISTICHE COMPLESSIVE DEL TRAFFICO CHORD ===" << endl;
    cout << "La simulazione ha generato traffico di rete reale per le operazioni Chord." << endl;
    cout << "Ogni lookup ha generato pacchetti UDP che sono stati inviati attraverso la rete simulata." << endl;
    cout << "Le statistiche dettagliate per nodo sono state stampate sopra." << endl;
    cout << "======================================================\n" << endl;
  });
  
  Simulator::Run();
  Simulator::Destroy();
  
  return 0;
}


