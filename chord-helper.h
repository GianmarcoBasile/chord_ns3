#ifndef CHORD_HELPER_H
#define CHORD_HELPER_H

#include "chord-header.h"
#include "chord-application.h"

using namespace ns3;
using namespace std;

// Funzione per configurare la rete Chord e costruire le tabelle di routing
void SetupChordNetwork(NodeContainer &nodes, vector<Ptr<ChordApplication>>& chordApps);

// Funzione per pianificare operazioni di lookup casuali
void ScheduleRandomLookups(vector<Ptr<ChordApplication>>& chordApps, int numLookups);

#endif // CHORD_HELPER_H 