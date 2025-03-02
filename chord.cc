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
#include <fstream>     // Per la scrittura dei file CSV
#include <iomanip>     // Per std::setprecision
#include <filesystem>  // Per creare directory se necessario
#include <unordered_set>
#include "chord-header.h"
#include "chord-application.h"
#include "chord-helper.h"

using namespace ns3;
using namespace std;

NS_LOG_COMPONENT_DEFINE ("ChordSimulation");

// Struttura per tenere traccia dei tempi di ricerca dei file
struct LookupStats {
  uint32_t fileId;
  Time startTime;
  Time endTime;
  bool completed;
};

// Mappa globale per tenere traccia dei tempi di ricerca
map<uint32_t, LookupStats> g_lookupStats;
// Vettore per tenere traccia degli ID dei file inseriti
vector<uint32_t> g_insertedFileIds;

// Funzione per creare directory se non esistono
void EnsureDirectoryExists(const string& dirPath) {
  try {
    if (!filesystem::exists(dirPath)) {
      filesystem::create_directories(dirPath);
      cout << "Directory creata: " << dirPath << endl;
    }
  } catch (const exception& e) {
    cerr << "Errore nella creazione della directory " << dirPath << ": " << e.what() << endl;
  }
}

// Callback da chiamare quando una ricerca di file è completata
void FileLookupCompleted(uint32_t fileId, bool found) {
  if (g_lookupStats.find(fileId) != g_lookupStats.end()) {
    g_lookupStats[fileId].endTime = Simulator::Now();
    g_lookupStats[fileId].completed = found;
    
    // Calcola il tempo trascorso in millisecondi
    double elapsedTimeMs = (g_lookupStats[fileId].endTime - g_lookupStats[fileId].startTime).GetMilliSeconds();
    
    cout << "Ricerca file ID " << fileId << " completata in " 
         << fixed << setprecision(2) << elapsedTimeMs << " ms. ";
    
    if (found) {
      cout << "File trovato!" << endl;
    } else {
      cout << "File non trovato." << endl;
    }
  }
}

// Funzione per salvare i risultati delle ricerche in un file CSV
void SaveLookupStatsToCsv() {
  string baseDir = "scratch/chord_ns3";
  string csvDir = baseDir + "/csv";
  string graphsDir = baseDir + "/graphs";
  
  EnsureDirectoryExists(csvDir);
  
  string filePath = csvDir + "/lookup_times.csv";
  ofstream csvFile(filePath);
  
  if (!csvFile.is_open()) {
    cerr << "Errore nell'apertura del file CSV: " << filePath << endl;
    return;
  }
  
  // Intestazione del file CSV
  csvFile << "FileID,ElapsedTimeMs,Found\n";
  
  // Scrivi i dati di ogni ricerca
  for (const auto& entry : g_lookupStats) {
    double elapsedTimeMs = (entry.second.endTime - entry.second.startTime).GetMilliSeconds();
    csvFile << entry.first << "," 
            << fixed << setprecision(2) << elapsedTimeMs << "," 
            << (entry.second.completed ? "true" : "false") << "\n";
  }
  
  csvFile.close();
  cout << "Dati di ricerca salvati in: " << filePath << endl;
  
  // Crea anche un file di riepilogo
  string summaryPath = csvDir + "/lookup_summary.csv";
  ofstream summaryFile(summaryPath);
  
  if (!summaryFile.is_open()) {
    cerr << "Errore nell'apertura del file di riepilogo: " << summaryPath << endl;
    return;
  }
  
  // Calcola le statistiche aggregate
  double totalTime = 0.0;
  double minTime = numeric_limits<double>::max();
  double maxTime = 0.0;
  int successCount = 0;
  
  for (const auto& entry : g_lookupStats) {
    double elapsedTimeMs = (entry.second.endTime - entry.second.startTime).GetMilliSeconds();
    totalTime += elapsedTimeMs;
    minTime = min(minTime, elapsedTimeMs);
    maxTime = max(maxTime, elapsedTimeMs);
    if (entry.second.completed) successCount++;
  }
  
  double avgTime = totalTime / g_lookupStats.size();
  double successRate = (double)successCount / g_lookupStats.size() * 100.0;
  
  // Intestazione del file di riepilogo
  summaryFile << "Metric,Value\n";
  summaryFile << "TotalLookups," << g_lookupStats.size() << "\n";
  summaryFile << "SuccessfulLookups," << successCount << "\n";
  summaryFile << "SuccessRate," << fixed << setprecision(2) << successRate << "\n";
  summaryFile << "TotalTimeMs," << fixed << setprecision(2) << totalTime << "\n";
  summaryFile << "AverageTimeMs," << fixed << setprecision(2) << avgTime << "\n";
  summaryFile << "MinTimeMs," << fixed << setprecision(2) << minTime << "\n";
  summaryFile << "MaxTimeMs," << fixed << setprecision(2) << maxTime << "\n";
  
  summaryFile.close();
  cout << "Riepilogo delle ricerche salvato in: " << summaryPath << endl;
}

int main (int argc, char *argv[])
{ 
  // Numero di nodi nella rete Chord
  uint32_t numnodes = 1000;
  // Numero di file da inserire
  uint32_t numFiles = 500;
  // Numero di file da cercare
  uint32_t numLookups = 100;
  // Seed per generatore di numeri casuali
  uint32_t seed = 100;
  uint32_t run = 1000;
  // Tempo di attesa tra l'inserimento dei file e l'inizio delle ricerche (secondi)
  uint32_t waitBeforeLookup = 30;
  
  CommandLine cmd;
  cmd.AddValue("numnodes", "Number of nodes", numnodes);
  cmd.AddValue("numfiles", "Number of files to insert", numFiles);
  cmd.AddValue("numlookups", "Number of files to lookup", numLookups);
  cmd.AddValue("seed", "RNG seed", seed);
  cmd.AddValue("run", "RNG run", run);
  cmd.AddValue("waittime", "Wait time between inserts and lookups (seconds)", waitBeforeLookup);
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponentEnable ("ChordSimulation", LOG_LEVEL_INFO);

  RngSeedManager::SetSeed(seed);
  RngSeedManager::SetRun(run);
  
  // Creiamo le directory per i file CSV e i grafici
  string baseDir = "scratch/chord_ns3";
  EnsureDirectoryExists(baseDir + "/csv");
  EnsureDirectoryExists(baseDir + "/graphs");
  
  cout << "Inizializzazione simulazione Chord con " << numnodes << " nodi" << endl;
  cout << "Verranno inseriti " << numFiles << " file e cercati " << numLookups << " file" << endl;

  NodeContainer nodes;
  nodes.Create (numnodes);
  
  NetDeviceContainer netdev;

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("5ms"));

  Ipv4NixVectorHelper nixRouting;
  InternetStackHelper stackIP;
  stackIP.SetRoutingHelper(nixRouting);
  stackIP.Install(nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.0.0.0", "/30");
 
  // Creiamo una topologia di base con un collegamento tra ogni nodo in sequenza
  for (uint32_t k = 10, i = 0; i < k; ++i)
  { 
    netdev = p2p.Install(nodes.Get(i), nodes.Get((i+1)%k));
    ipv4.NewNetwork();
    ipv4.Assign (netdev);
  }    
  
  // Aggiungiamo alcuni collegamenti random per rendere la rete più connessa
  for (uint32_t i = 10; i < numnodes; ++i)
  { 
    uint32_t minlinks = 3;
    Ptr<UniformRandomVariable> r = CreateObject<UniformRandomVariable> ();
    vector<bool> link(numnodes);
    uint32_t numlinks = 0;
    while (numlinks < minlinks)
    { 
      uint32_t j = r->GetInteger((i-10)/3*2, i-1);
      if ((j == i) || link[j])
        continue;
      link[j] = true;
      netdev = p2p.Install(nodes.Get(i), nodes.Get(j));
      ipv4.NewNetwork();
      ipv4.Assign (netdev); 
      ++numlinks;
    }    
  }

  cout << "Configurazione rete Chord..." << endl;
  // Configuriamo la rete Chord
  vector<Ptr<ChordApplication>> chordApps;
  SetupChordNetwork(nodes, chordApps);
  
  // Registriamo il callback per il completamento delle ricerche di file
  for (auto& app : chordApps) {
    app->SetFileLookupCompletedCallback(&FileLookupCompleted);
  }
  
  // Generiamo numFiles file casuali e li memorizziamo nella rete
  Ptr<UniformRandomVariable> r = CreateObject<UniformRandomVariable>();
  
  cout << "Inserimento di " << numFiles << " file nella rete Chord..." << endl;
  
  // Creiamo un set per assicurarci che gli ID dei file siano tutti univoci
  unordered_set<uint32_t> uniqueFileIds;
  
  // Tempo di inizio per l'inserimento dei file
  double insertStartTime = 20.0;
  // Intervallo tra inserimenti di file
  double insertInterval = 0.05; // Aumentato da 0.01 a 0.05 per dare più tempo
  
  for (uint32_t i = 0; i < numFiles; i++) {
    // Genera un ID univoco
    uint32_t fileId;
    do {
      fileId = r->GetInteger(0, CHORD_SIZE - 1);
    } while (uniqueFileIds.find(fileId) != uniqueFileIds.end());
    
    uniqueFileIds.insert(fileId);
    g_insertedFileIds.push_back(fileId);  // Salviamo l'ID per la ricerca successiva
    
    // Scegliamo un nodo casuale da cui iniziare l'inserimento
    uint32_t startNodeIdx = r->GetInteger(0, chordApps.size() - 1);
    
    // Pianifichiamo l'inserimento del file
    Simulator::Schedule(Seconds(insertStartTime + i * insertInterval), &ChordApplication::StoreFile, 
                      chordApps[startNodeIdx], fileId);
    
    if (i % 50 == 0) {
      cout << "Pianificato inserimento file " << i << "/" << numFiles << endl;
    }
  }
  
  // Tempo totale necessario per l'inserimento di tutti i file
  double totalInsertTime = insertStartTime + numFiles * insertInterval;
  
  // Aggiungiamo un callback per verificare quanti file sono stati effettivamente inseriti
  Simulator::Schedule(Seconds(totalInsertTime + 5.0), [&chordApps]() {
    cout << "\n=== VERIFICA INSERIMENTO FILE ===" << endl;
    uint32_t totalFiles = 0;
    
    // Contiamo quanti file sono presenti in ciascun nodo
    for (uint32_t i = 0; i < chordApps.size(); i++) {
      vector<uint32_t> files = chordApps[i]->GetStoredFiles();
      totalFiles += files.size();
      
      // Stampiamo dettagli solo per alcuni nodi per evitare output eccessivo
      if (i % 100 == 0) {
        cout << "Nodo " << i << " (chordId: " << chordApps[i]->GetChordId() 
             << ") ha " << files.size() << " file" << endl;
      }
    }
    
    cout << "Totale file memorizzati nella rete: " << totalFiles << endl;
    cout << "==============================\n" << endl;
  });
  
  // Scegliamo un nodo casuale da cui effettuare le ricerche
  uint32_t lookupNodeIdx = r->GetInteger(0, chordApps.size() - 1);
  cout << "Nodo scelto per le ricerche: chordId " << chordApps[lookupNodeIdx]->GetChordId() << endl;
  
  // Pianifichiamo le ricerche DOPO un tempo di attesa adeguato
  cout << "Pianificazione di " << numLookups << " ricerche su file esistenti..." << endl;
  
  // Tempo di inizio per le ricerche (dopo l'inserimento dei file + tempo di attesa)
  double lookupStartTime = totalInsertTime + waitBeforeLookup;
  // Intervallo tra ricerche
  double lookupInterval = 0.5; // Aumentato da 0.1 a 0.5 per dare più tempo tra le ricerche
  
  // Assicuriamoci di avere abbastanza file
  if (g_insertedFileIds.size() < numLookups) {
    cout << "ERRORE: Non ci sono abbastanza file inseriti per " << numLookups << " ricerche" << endl;
    return 1;
  }
  
  cout << "Le ricerche inizieranno a " << lookupStartTime << " secondi dall'inizio della simulazione" << endl;
  cout << "Ovvero " << waitBeforeLookup << " secondi dopo il completamento dell'inserimento dei file" << endl;
  
  for (uint32_t i = 0; i < numLookups; i++) {
    // Utilizziamo i primi 100 file che abbiamo sicuramente inserito
    uint32_t fileId = g_insertedFileIds[i];
    
    // Inizializziamo le statistiche di ricerca
    LookupStats stats;
    stats.fileId = fileId;
    stats.startTime = Seconds(lookupStartTime + i * lookupInterval);
    stats.endTime = stats.startTime;  // Verrà aggiornato quando la ricerca è completata
    stats.completed = false;
    g_lookupStats[fileId] = stats;
    
    // Pianifichiamo la ricerca
    Simulator::Schedule(stats.startTime, [lookupNodeIdx, fileId, chordApps]() {
      cout << "Avvio ricerca file ID " << fileId << " dal nodo chordId " 
           << chordApps[lookupNodeIdx]->GetChordId() << endl;
      chordApps[lookupNodeIdx]->GetFile(fileId);
    });
    
    if (i % 10 == 0) {
      cout << "Pianificata ricerca " << i << "/" << numLookups << endl;
    }
  }
  
  // Pianifichiamo il salvataggio dei risultati al termine delle ricerche
  Simulator::Schedule(Seconds(lookupStartTime + numLookups * lookupInterval + 10.0), 
                     &SaveLookupStatsToCsv);
  
  // Aggiungiamo un callback per stampare le statistiche complessive alla fine della simulazione
  Simulator::Schedule(Seconds(lookupStartTime + numLookups * lookupInterval + 15.0), []() {
    cout << "\n\n=== STATISTICHE COMPLESSIVE DELLA SIMULAZIONE CHORD ===" << endl;
    
    // Calcola statistiche aggregate
    double totalTime = 0.0;
    double minTime = numeric_limits<double>::max();
    double maxTime = 0.0;
    int successCount = 0;
    
    for (const auto& entry : g_lookupStats) {
      double elapsedTimeMs = (entry.second.endTime - entry.second.startTime).GetMilliSeconds();
      totalTime += elapsedTimeMs;
      minTime = min(minTime, elapsedTimeMs);
      maxTime = max(maxTime, elapsedTimeMs);
      if (entry.second.completed) successCount++;
    }
    
    double avgTime = totalTime / g_lookupStats.size();
    double successRate = (double)successCount / g_lookupStats.size() * 100.0;
    
    cout << "Ricerche totali: " << g_lookupStats.size() << endl;
    cout << "Ricerche completate con successo: " << successCount << endl;
    cout << "Tasso di successo: " << fixed << setprecision(2) << successRate << "%" << endl;
    cout << "Tempo totale di ricerca: " << fixed << setprecision(2) << totalTime << " ms" << endl;
    cout << "Tempo medio di ricerca: " << fixed << setprecision(2) << avgTime << " ms" << endl;
    cout << "Tempo minimo di ricerca: " << fixed << setprecision(2) << minTime << " ms" << endl;
    cout << "Tempo massimo di ricerca: " << fixed << setprecision(2) << maxTime << " ms" << endl;
    cout << "======================================================\n" << endl;
  });
  
  cout << "Avvio simulazione..." << endl;
  Simulator::Run();
  Simulator::Destroy();
  
  return 0;
}


