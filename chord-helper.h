#ifndef CHORD_HELPER_H
#define CHORD_HELPER_H

#include "ns3/node-container.h"
#include "chord-application.h"

using namespace ns3;
using namespace std;

// Funzione per configurare la rete Chord e costruire le tabelle di routing
void SetupChordNetwork(NodeContainer &nodes, vector<Ptr<ChordApplication>>& chordApps);

// Funzione per pianificare operazioni di lookup casuali
void ScheduleRandomLookups(vector<Ptr<ChordApplication>>& chordApps, int numLookups);

// Funzione per pianificare operazioni di memorizzazione e ricerca di file
void ScheduleFileOperations(vector<Ptr<ChordApplication>>& chordApps, int numFiles);

// Dichiarazione della funzione findSuccessor
uint32_t findSuccessor(uint32_t id, const std::vector<ChordInfo>& chordNodes);

#endif // CHORD_HELPER_H