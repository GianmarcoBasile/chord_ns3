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
#include <random>
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
  uint32_t hopCount;  // Numero di hop necessari per completare la ricerca
};

// Mappa globale per tenere traccia dei tempi di ricerca
map<uint32_t, LookupStats> g_lookupStats;
// Vettore per tenere traccia degli ID dei file inseriti
vector<uint32_t> g_insertedFileIds;

// Funzione per leggere gli ID dei file dal file
vector<uint32_t> ReadFileIds(const string& filename) {
    vector<uint32_t> fileIds;
    ifstream file(filename);
    if (!file.is_open()) {
        cerr << "Errore nell'apertura del file " << filename << endl;
        return fileIds;
    }

    uint32_t id;
    while (file >> id) {
        fileIds.push_back(id);
    }

    file.close();
    cout << "Letti " << fileIds.size() << " ID di file da " << filename << endl;
    return fileIds;
}

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
void FileLookupCompleted(uint32_t fileId, bool found, uint32_t hopCount) {
  if (g_lookupStats.find(fileId) != g_lookupStats.end()) {
    g_lookupStats[fileId].endTime = Simulator::Now();
    g_lookupStats[fileId].completed = found;
    g_lookupStats[fileId].hopCount = hopCount;
    
    // Calcola il tempo trascorso in millisecondi
    double elapsedTimeMs = (g_lookupStats[fileId].endTime - g_lookupStats[fileId].startTime).GetMilliSeconds();
    
    cout << "Ricerca file ID " << fileId << " completata in " 
         << fixed << setprecision(2) << elapsedTimeMs << " ms. ";
    
    if (found) {
      cout << "File trovato! Hop: " << hopCount << endl;
    } else {
      cout << "File non trovato dopo " << hopCount << " hop." << endl;
      cout << "DETTAGLIO ERRORE: La ricerca del file " << fileId << " è fallita." << endl;
      cout << "Questo non dovrebbe accadere in una rete Chord correttamente funzionante." << endl;
      cout << "Il file dovrebbe essere stato inserito all'inizio della simulazione." << endl;
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
  csvFile << "FileID,ElapsedTimeMs,Found,HopCount\n";
  
  // Scrivi i dati di ogni ricerca
  for (const auto& entry : g_lookupStats) {
    double elapsedTimeMs = (entry.second.endTime - entry.second.startTime).GetMilliSeconds();
    csvFile << entry.first << "," 
            << fixed << setprecision(2) << elapsedTimeMs << "," 
            << (entry.second.completed ? "true" : "false") << "," 
            << entry.second.hopCount << "\n";
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
  uint32_t totalHops = 0;
  uint32_t minHops = numeric_limits<uint32_t>::max();
  uint32_t maxHops = 0;
  
  for (const auto& entry : g_lookupStats) {
    double elapsedTimeMs = (entry.second.endTime - entry.second.startTime).GetMilliSeconds();
    totalTime += elapsedTimeMs;
    minTime = min(minTime, elapsedTimeMs);
    maxTime = max(maxTime, elapsedTimeMs);
    
    totalHops += entry.second.hopCount;
    minHops = min(minHops, entry.second.hopCount);
    maxHops = max(maxHops, entry.second.hopCount);
    
    if (entry.second.completed) successCount++;
  }
  
  double avgTime = totalTime / g_lookupStats.size();
  double successRate = (double)successCount / g_lookupStats.size() * 100.0;
  double avgHops = (double)totalHops / g_lookupStats.size();
  
  // Intestazione del file di riepilogo
  summaryFile << "Metric,Value\n";
  summaryFile << "TotalLookups," << g_lookupStats.size() << "\n";
  summaryFile << "SuccessfulLookups," << successCount << "\n";
  summaryFile << "SuccessRate," << fixed << setprecision(2) << successRate << "\n";
  summaryFile << "TotalTimeMs," << fixed << setprecision(2) << totalTime << "\n";
  summaryFile << "AverageTimeMs," << fixed << setprecision(2) << avgTime << "\n";
  summaryFile << "MinTimeMs," << fixed << setprecision(2) << minTime << "\n";
  summaryFile << "MaxTimeMs," << fixed << setprecision(2) << maxTime << "\n";
  summaryFile << "TotalHops," << totalHops << "\n";
  summaryFile << "AverageHops," << fixed << setprecision(2) << avgHops << "\n";
  summaryFile << "MinHops," << minHops << "\n";
  summaryFile << "MaxHops," << maxHops << "\n";
  
  summaryFile.close();
  cout << "Riepilogo delle ricerche salvato in: " << summaryPath << endl;
}

// Funzione per generare chiavi casuali uniche
vector<uint32_t> GenerateUniqueKeys(uint32_t numKeys, uint32_t maxKey) {
    vector<uint32_t> keys;
    random_device rd;
    mt19937 gen(rd());
    uniform_int_distribution<uint32_t> dis(1, maxKey);
    
    while (keys.size() < numKeys) {
        uint32_t key = dis(gen);
        if (find(keys.begin(), keys.end(), key) == keys.end()) {
            keys.push_back(key);
        }
    }
    
    return keys;
}

int main (int argc, char *argv[])
{ 
  // Numero di nodi nella rete Chord
  uint32_t numnodes = 1000;
  // Numero di file da inserire
  uint32_t numFiles = 100;  // Fissiamo a 100 file
  // Numero di file da cercare
  uint32_t numLookups = 100;  // Cercheremo tutti i file inseriti
  // Seed per generatore di numeri casuali
  uint32_t seed = 19;
  uint32_t run = 1000;
  // Tempo di attesa tra l'inserimento dei file e l'inizio delle ricerche (secondi)
  uint32_t waitBeforeLookup = 5;
  
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

  // Generiamo 100 chiavi uniche tra 1 e 10000
  vector<uint32_t> fileIds = GenerateUniqueKeys(100, 10000);
  cout << "Generate " << fileIds.size() << " chiavi uniche per i file" << endl;

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
  
  // Fase di inserimento dei file
  double insertStartTime = 10.0;
  double insertInterval = 0.05;
  
  Simulator::Schedule(Seconds(insertStartTime), []() {
    cout << "\n=== INIZIO FASE DI INSERIMENTO FILE ===" << endl;
  });
  
  // Inseriamo ogni file da un nodo casuale
  Ptr<UniformRandomVariable> r = CreateObject<UniformRandomVariable>();
  for (uint32_t i = 0; i < fileIds.size(); i++) {
    uint32_t fileId = fileIds[i];
    uint32_t startNodeIdx = r->GetInteger(0, chordApps.size() - 1);
    
    Simulator::Schedule(Seconds(insertStartTime + i * insertInterval), 
                       &ChordApplication::StoreFile, 
                       chordApps[startNodeIdx], 
                       fileId, 
                       1024,  // dimensione fissa di 1KB
                       0);    // hop count iniziale
    
    if (i % 10 == 0) {
      cout << "Pianificato inserimento file " << i << "/" << fileIds.size() << endl;
    }
  }
  
  double totalInsertTime = insertStartTime + fileIds.size() * insertInterval;
  
  // Verifica dei file inseriti
  Simulator::Schedule(Seconds(totalInsertTime + 1.0), [&chordApps]() {
    cout << "\n=== VERIFICA INSERIMENTO FILE ===" << endl;
    uint32_t totalFiles = 0;
    for (uint32_t i = 0; i < chordApps.size(); i++) {
      vector<uint32_t> files = chordApps[i]->GetStoredFiles();
      totalFiles += files.size();
      if (i % 100 == 0) {
        cout << "Nodo " << i << " (chordId: " << chordApps[i]->GetChordId() 
             << ") ha " << files.size() << " file" << endl;
      }
    }
    cout << "Totale file memorizzati nella rete: " << totalFiles << endl;
  });
  
  // Fase di ricerca dei file
  double lookupStartTime = totalInsertTime + waitBeforeLookup;
  double lookupInterval = 0.5;
  
  uint32_t lookupNodeIdx = r->GetInteger(0, chordApps.size() - 1);
  cout << "Nodo scelto per le ricerche: chordId " << chordApps[lookupNodeIdx]->GetChordId() << endl;
  
  Simulator::Schedule(Seconds(lookupStartTime), []() {
    cout << "\n=== INIZIO FASE DI RICERCA FILE ===" << endl;
  });
  
  // Cerchiamo tutti i file che abbiamo inserito
  for (uint32_t i = 0; i < fileIds.size(); i++) {
    uint32_t fileId = fileIds[i];
    
    LookupStats stats;
    stats.fileId = fileId;
    stats.startTime = Seconds(lookupStartTime + i * lookupInterval);
    stats.endTime = stats.startTime;
    stats.completed = false;
    stats.hopCount = 0;
    g_lookupStats[fileId] = stats;
    
    Simulator::Schedule(stats.startTime, [lookupNodeIdx, fileId, chordApps]() {
      cout << "RICERCA: Nodo " << chordApps[lookupNodeIdx]->GetChordId() 
           << " inizia la ricerca del file " << fileId << endl;
      chordApps[lookupNodeIdx]->GetFile(fileId, 0);
    });
  }
  
  // Statistiche finali
  Simulator::Schedule(Seconds(lookupStartTime + fileIds.size() * lookupInterval + 5.0), []() {
    cout << "\n=== STATISTICHE FINALI ===" << endl;
    
    uint32_t successCount = 0;
    uint32_t totalHops = 0;
    vector<uint32_t> failedLookups;
    
    for (const auto& entry : g_lookupStats) {
      if (entry.second.completed) {
        successCount++;
        totalHops += entry.second.hopCount;
      } else {
        failedLookups.push_back(entry.first);
      }
    }
    
    cout << "Ricerche completate con successo: " << successCount << "/" 
         << g_lookupStats.size() << endl;
    if (!failedLookups.empty()) {
      cout << "File non trovati:" << endl;
      for (uint32_t fileId : failedLookups) {
        cout << "  - FileID: " << fileId << endl;
      }
    }
    cout << "Media hop per ricerca: " 
         << (double)totalHops / successCount << endl;
  });
  
  // Pianifichiamo il salvataggio dei risultati al termine delle ricerche
  Simulator::Schedule(Seconds(lookupStartTime + fileIds.size() * lookupInterval + 10.0), 
                     &SaveLookupStatsToCsv);
  
  cout << "Avvio simulazione..." << endl;
  Simulator::Run();
  Simulator::Destroy();
  
  return 0;
}


