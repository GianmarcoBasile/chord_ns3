#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/nix-vector-helper.h"
#include <vector>
#include <map>
#include <cmath>
#include <algorithm>
#include <iostream>
#include <fstream>

using namespace ns3;
using namespace std;

NS_LOG_COMPONENT_DEFINE ("ChordProtocol");

// Struttura per i messaggi Chord
struct ChordMessage {
    enum MessageType {
        LOOKUP_REQUEST,
        LOOKUP_RESPONSE,
        STORE_FILE,
        STORE_ACK
    };
    
    MessageType type;
    uint32_t senderId;      // ID del nodo che invia il messaggio
    uint32_t originId;      // ID del nodo che ha avviato il lookup (per inviare la risposta direttamente)
    uint32_t targetId;      // chordId del file o del nodo target
    uint32_t hopCount;
    bool success;
};

// Struttura per i nodi Chord
struct ChordNode {
    uint32_t chordId;
    std::vector<uint32_t> fingerTable;
    std::vector<uint32_t> successorList;
    uint32_t predecessor;
    std::set<uint32_t> storedFiles;
    bool isAlive;
    Ptr<Node> nsNode;     // Riferimento al nodo fisico

    ChordNode(uint32_t id, Ptr<Node> node) : chordId(id), isAlive(true), nsNode(node) {}
};

class ChordApplication : public Application {
private:
    uint32_t chordId;
    uint32_t port;
    Ptr<Socket> socket;
    std::vector<uint32_t> fingerTable;
    std::vector<uint32_t> successorList;
    uint32_t predecessor;
    std::map<uint32_t, Address> nodeAddresses;
    std::set<uint32_t> storedFiles;
    bool isAlive;
    
    struct LookupInfo {
        uint32_t lookupId;
        uint32_t fileId;
        EventId timeoutEvent;
    };
    std::map<uint32_t, LookupInfo> pendingLookups;
    
    typedef Callback<void, bool, uint32_t, uint32_t> StatsCallback;
    StatsCallback statsCallback;

public:
    static TypeId GetTypeId(void) {
        static TypeId tid = TypeId("ChordApplication")
            .SetParent<Application>()
            .AddConstructor<ChordApplication>();
        return tid;
    }
    
    static uint32_t nextLookupId;

    ChordApplication() : port(9), isAlive(true) {
    }

    void Setup(uint32_t id, const std::vector<uint32_t>& ft, 
               const std::vector<uint32_t>& sl, uint32_t pred) {
        chordId = id;
        fingerTable = ft;
        successorList = sl;
        predecessor = pred;
        
        cout << "Node " << chordId << " initialized with " 
             << fingerTable.size() << " finger table entries and "
             << "1 successore (ID: " << (successorList.empty() ? 0 : successorList[0]) << ")" << endl;
    }

    void AddNodeAddress(uint32_t nodeId, const Address& address) {
        nodeAddresses[nodeId] = address;
    }

    void StartApplication() override {
        cout << "ChordApplication::StartApplication - Node " << chordId << endl;
        socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
        InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), port);
        socket->Bind(local);
        socket->SetRecvCallback(MakeCallback(&ChordApplication::HandleRead, this));
        cout << "Node " << chordId << " application started" << endl;
    }

    void StopApplication() override {
        cout << "ChordApplication::StopApplication - Node " << chordId << endl;
        if (socket) {
            socket->Close();
        }
        cout << "Il nodo " << chordId << " è spento" << endl;
    }

    bool IsStarted() const {
        return (socket != nullptr);
    }

    void SetAlive(bool alive) {
        cout << "ChordApplication::SetAlive - Node " << chordId << " alive: " << alive << endl;
        isAlive = alive;
        if (!alive) {
            cout << "Node " << chordId << " è ora spento" << endl;
        }
    }

    void SendMessage(Ptr<Packet> packet, Address targetAddress) {
        if (!isAlive || !socket) {
            cout << "WARN: Il nodo " << chordId << " non può inviare un messaggio: " 
                 << (isAlive ? "socket è nullo" : "il nodo è spento") << endl;
            return;
        }

        ChordMessage msg;
        packet->CopyData((uint8_t*)&msg, sizeof(ChordMessage));
        
        cout << "Il nodo " << chordId << " sta inviando un messaggio " 
             << GetMessageTypeName(msg.type) << " a "
             << InetSocketAddress::ConvertFrom(targetAddress).GetIpv4()
             << " [targetId: " << msg.targetId 
             << ", hops: " << msg.hopCount << "]" << endl;
        
        socket->SendTo(packet, 0, targetAddress);
    }

    uint32_t FindNextHop(uint32_t targetId) {
        cout << "DEBUG: Il nodo " << chordId << " cerca il prossimo nodo per targetId " << targetId << endl;
        cout << "DEBUG: Finger table: ";
        for (size_t i = 0; i < fingerTable.size(); i++) {
            cout << fingerTable[i] << " ";
        }
        cout << endl;
        
        if (targetId == chordId) {
            cout << "DEBUG: Il target ID è uguale al nostro ID, ritorniamo noi stessi" << endl;
            return chordId;
        }
        
        if (IsInRange(targetId, chordId, fingerTable[0])) {
            cout << "DEBUG: Il target " << targetId << " è nel range tra " << chordId << " e " << fingerTable[0] << endl;
            return fingerTable[0];
        }

        for (int i = fingerTable.size() - 1; i >= 0; i--) {
            if (IsInRange(fingerTable[i], chordId, targetId)) {
                cout << "DEBUG: Trovato nodo " << fingerTable[i] << " nel range per target " << targetId << endl;
                return fingerTable[i];
            }
        }

        cout << "DEBUG: Nessun nodo trovato nella finger table, ritorniamo il successore " << fingerTable[0] << endl;
        return fingerTable[0];
    }

    void SetStatsCallback(StatsCallback callback) {
        statsCallback = callback;
    }

    void PerformLookup(uint32_t fileId, Time timeout, uint32_t lookupId) {
        cout << "ChordApplication::PerformLookup - Node " << chordId << " fileId: " << fileId << " lookupId: " << lookupId << endl;
        
        if (!isAlive || !socket) {
            cout << "WARN: Node " << chordId << " cannot perform lookup: " 
                 << (isAlive ? "socket is null" : "node is down") << endl;
            if (!statsCallback.IsNull()) {
                statsCallback(false, 0, lookupId);
            }
            return;
        }
        
        // Stampiamo tutti i file memorizzati da questo nodo
        cout << "DEBUG: Node " << chordId << " ha " << storedFiles.size() << " file memorizzati: ";
        for (const uint32_t fileId : storedFiles) {
            cout << fileId << " ";
        }
        cout << endl;
        
        // Verifica se abbiamo già il file localmente
        if (storedFiles.count(fileId) > 0) {
            cout << "Node " << chordId << " already has file " << fileId << " locally" << endl;
            if (!statsCallback.IsNull()) {
                statsCallback(true, 0, lookupId);
            }
            return;
        }
        
        uint32_t nextHop = FindNextHop(fileId);
        
        if (nextHop == chordId) {
            cout << "WARN: FindNextHop ha restituito il nodo corrente. Possibile errore nella finger table." << endl;
            if (!statsCallback.IsNull()) {
                statsCallback(false, 0, lookupId);
            }
            return;
        }
        
        ChordMessage msg;
        msg.type = ChordMessage::LOOKUP_REQUEST;
        msg.senderId = chordId;
        msg.originId = chordId;  // Il nodo che avvia il lookup è anche l'origine
        msg.targetId = fileId;
        msg.hopCount = 0;
        msg.success = false;
        
        Ptr<Packet> packet = Create<Packet>((uint8_t*)&msg, sizeof(ChordMessage));
        
        if (nodeAddresses.find(nextHop) != nodeAddresses.end()) {
            cout << "Il nodo " << chordId << " sta iniziando una lookup " << lookupId << " per il file " << fileId 
                 << "contattando il nodo " << nextHop << endl;
            
            EventId timeoutEvent = Simulator::Schedule(timeout, &ChordApplication::HandleLookupTimeout, this, fileId, lookupId);
            
            // Salva le informazioni del lookup
            LookupInfo info;
            info.fileId = fileId;
            info.lookupId = lookupId;
            info.timeoutEvent = timeoutEvent;
            pendingLookups[fileId] = info;
            
            SendMessage(packet, nodeAddresses[nextHop]);
        } else {
            cout << "ERROR: Il nodo " << chordId << " non può trovare l'indirizzo del prossimo nodo " << nextHop << endl;
            if (!statsCallback.IsNull()) {
                statsCallback(false, 0, lookupId);
            }
        }
    }

private:
    void HandleRead(Ptr<Socket> socket) {
        if (!isAlive) return;

        Ptr<Packet> packet;
        Address from;
        while ((packet = socket->RecvFrom(from))) {
            ChordMessage msg;
            packet->CopyData((uint8_t*)&msg, sizeof(ChordMessage));

            cout << "Il nodo " << chordId << " ha ricevuto un messaggio di tipo" 
                 << GetMessageTypeName(msg.type) << " dal nodo " 
                 << msg.senderId << " [targetId: " << msg.targetId 
                 << ", hops: " << msg.hopCount << "]" << endl;

            switch (msg.type) {
                case ChordMessage::LOOKUP_REQUEST:
                    HandleLookup(msg, from);
                    break;
                case ChordMessage::LOOKUP_RESPONSE:
                    HandleLookupResponse(msg);
                    break;
                case ChordMessage::STORE_FILE:
                    HandleStore(msg, from);
                    break;
                default:
                    break;
            }
        }
    }

    void HandleLookup(const ChordMessage& msg, const Address& from) {
        cout << "ChordApplication::HandleLookup - Node " << chordId << " targetId: " << msg.targetId << endl;

        if (msg.hopCount > 50) { 
            cout << "WARN: Rilevato possibile ciclo di routing per il file " << msg.targetId 
                 << " dopo " << msg.hopCount << " hop. Interrompo la ricerca." << endl;
            
            ChordMessage response;
            response.type = ChordMessage::LOOKUP_RESPONSE;
            response.senderId = chordId;
            response.originId = msg.originId;
            response.targetId = msg.targetId;
            response.success = false;
            response.hopCount = msg.hopCount;

            if (nodeAddresses.find(msg.originId) != nodeAddresses.end()) {
                Ptr<Packet> packet = Create<Packet>((uint8_t*)&response, sizeof(ChordMessage));
                SendMessage(packet, nodeAddresses[msg.originId]);
                cout << "DEBUG: Inviata risposta negativa direttamente al nodo originale " << msg.originId << endl;
            } else {
                Ptr<Packet> packet = Create<Packet>((uint8_t*)&response, sizeof(ChordMessage));
                SendMessage(packet, from);
                cout << "DEBUG: Inviata risposta negativa al mittente (non trovato indirizzo del nodo originale)" << endl;
            }
            return;
        }

        cout << "DEBUG: Il nodo " << chordId << " ha " << storedFiles.size() << " file memorizzati: ";
        for (const uint32_t fileId : storedFiles) {
            cout << fileId << " ";
        }
        cout << endl;

        if (storedFiles.find(msg.targetId) != storedFiles.end()) {
            cout << "Il nodo " << chordId << " ha il file " << msg.targetId 
                 << ". Inviamo la risposta dopo " << msg.hopCount << " hop" << endl;

            ChordMessage response;
            response.type = ChordMessage::LOOKUP_RESPONSE;
            response.senderId = chordId;
            response.originId = msg.originId;
            response.targetId = msg.targetId;
            response.success = true;
            response.hopCount = msg.hopCount + 1;

            if (nodeAddresses.find(msg.originId) != nodeAddresses.end()) {
                Ptr<Packet> packet = Create<Packet>((uint8_t*)&response, sizeof(ChordMessage));
                SendMessage(packet, nodeAddresses[msg.originId]);
                cout << "DEBUG: Inviata risposta positiva direttamente al nodo originale " << msg.originId << endl;
            } else {
                Ptr<Packet> packet = Create<Packet>((uint8_t*)&response, sizeof(ChordMessage));
                SendMessage(packet, from);
                cout << "DEBUG: Inviata risposta positiva al mittente (non trovato indirizzo del nodo originale)" << endl;
            }
            return;
        } else {
            cout << "DEBUG: Node " << chordId << " NON ha il file " << msg.targetId << endl;
        }

        uint32_t nextHop = FindNextHop(msg.targetId);
        
        if (nextHop == chordId) {
            cout << "WARN: FindNextHop ha restituito il nodo corrente. Possibile errore nella finger table." << endl;
            
            ChordMessage response;
            response.type = ChordMessage::LOOKUP_RESPONSE;
            response.senderId = chordId;
            response.originId = msg.originId;
            response.targetId = msg.targetId;
            response.success = false;
            response.hopCount = msg.hopCount;

            if (nodeAddresses.find(msg.originId) != nodeAddresses.end()) {
                Ptr<Packet> packet = Create<Packet>((uint8_t*)&response, sizeof(ChordMessage));
                SendMessage(packet, nodeAddresses[msg.originId]);
                cout << "DEBUG: Inviata risposta negativa direttamente al nodo originale " << msg.originId << endl;
            } else {
                Ptr<Packet> packet = Create<Packet>((uint8_t*)&response, sizeof(ChordMessage));
                SendMessage(packet, from);
                cout << "DEBUG: Inviata risposta negativa al mittente (non trovato indirizzo del nodo originale)" << endl;
            }
            return;
        }
        
        if (nextHop != chordId) {
            cout << "Il nodo " << chordId << " sta inoltrando una lookup per il file " 
                 << msg.targetId << " al nodo " << nextHop << endl;

            ChordMessage forward = msg;
            forward.senderId = chordId;
            forward.hopCount++;
            Ptr<Packet> packet = Create<Packet>((uint8_t*)&forward, sizeof(ChordMessage));
            
            if (nodeAddresses.find(nextHop) != nodeAddresses.end()) {
                SendMessage(packet, nodeAddresses[nextHop]);
            } else {
                cout << "ERROR: Impossibile trovare l'indirizzo per il nodo " << nextHop << endl;
                
                ChordMessage response;
                response.type = ChordMessage::LOOKUP_RESPONSE;
                response.senderId = chordId;
                response.originId = msg.originId;
                response.targetId = msg.targetId;
                response.success = false;
                response.hopCount = msg.hopCount;

                if (nodeAddresses.find(msg.originId) != nodeAddresses.end()) {
                    Ptr<Packet> packet = Create<Packet>((uint8_t*)&response, sizeof(ChordMessage));
                    SendMessage(packet, nodeAddresses[msg.originId]);
                    cout << "DEBUG: Inviata risposta negativa direttamente al nodo originale " << msg.originId << endl;
                } else {
                    Ptr<Packet> packet = Create<Packet>((uint8_t*)&response, sizeof(ChordMessage));
                    SendMessage(packet, from);
                    cout << "DEBUG: Inviata risposta negativa al mittente (non trovato indirizzo del nodo originale)" << endl;
                }
            }
        }
    }

    void HandleLookupResponse(const ChordMessage& msg) {
        cout << "ChordApplication::HandleLookupResponse - Node " << chordId << " targetId: " << msg.targetId << endl;
        cout << "Il nodo " << chordId << " ha ricevuto una risposta per il file " 
             << msg.targetId << " dopo " << msg.hopCount << " hop" << endl;
        
        if (msg.originId != chordId) {
            cout << "WARN: Il nodo " << chordId << " ha ricevuto una risposta per un lookup che non ha avviato. OriginId: " << msg.originId << endl;
            return;
        }
        
        map<uint32_t, ChordApplication::LookupInfo>::iterator it = pendingLookups.find(msg.targetId);
        if (it != pendingLookups.end()) {
            Simulator::Cancel(it->second.timeoutEvent);
            
            if (!statsCallback.IsNull()) {
                cout << "DEBUG: Il nodo " << chordId << " aggiorna le statistiche per il lookup " << it->second.lookupId 
                     << " (success: " << (msg.success ? "true" : "false") << ", hops: " << msg.hopCount << ")" << endl;
                statsCallback(msg.success, msg.hopCount, it->second.lookupId);
            } else {
                cout << "ERROR: Il nodo " << chordId << " non ha una callback per le statistiche!" << endl;
            }
            
            pendingLookups.erase(it);
        } else {
            cout << "WARN: Il nodo " << chordId << " ha ricevuto una risposta per un lookup non pendente. FileId: " << msg.targetId << endl;
        }
    }

    void HandleStore(const ChordMessage& msg, const Address& from) {
        cout << "ChordApplication::HandleStore - Node " << chordId << " targetId: " << msg.targetId << endl;

        storedFiles.insert(msg.targetId);
        cout << "DEBUG: Node " << chordId << " ha memorizzato il file " << msg.targetId << endl;
        
        cout << "DEBUG: Node " << chordId << " ora ha " << storedFiles.size() << " file memorizzati: ";
        for (const uint32_t fileId : storedFiles) {
            cout << fileId << " ";
        }
        cout << endl;

        ChordMessage ack;
        ack.type = ChordMessage::STORE_ACK;
        ack.senderId = chordId;
        ack.originId = msg.senderId;
        ack.targetId = msg.targetId;
        ack.success = true;
        ack.hopCount = 0;
        Ptr<Packet> packet = Create<Packet>((uint8_t*)&ack, sizeof(ChordMessage));
        SendMessage(packet, from);
    }

    void HandleLookupTimeout(uint32_t fileId, uint32_t lookupId) {
        cout << "ChordApplication::HandleLookupTimeout - Node " << chordId << " fileId: " << fileId << " lookupId: " << lookupId << endl;
        cout << "La ricerca " << lookupId << " del nodo " << chordId << " per il file " << fileId << " ha timeoutato" << endl;
        pendingLookups.erase(fileId);
        
        if (!statsCallback.IsNull()) {
            statsCallback(false, 0, lookupId);
        }
    }

    bool IsInRange(uint32_t id, uint32_t start, uint32_t end) {
        if (start < end) {
            return id > start && id <= end;
        } else {
            return id > start || id <= end;
        }
    }

    std::string GetMessageTypeName(ChordMessage::MessageType type) {
        switch (type) {
            case ChordMessage::LOOKUP_REQUEST: return "LOOKUP_REQUEST";
            case ChordMessage::LOOKUP_RESPONSE: return "LOOKUP_RESPONSE";
            case ChordMessage::STORE_FILE: return "STORE_FILE";
            case ChordMessage::STORE_ACK: return "STORE_ACK";
            default: return "UNKNOWN";
        }
    }
};

uint32_t ChordApplication::nextLookupId = 0;

class ChordNetwork {
private:
    uint32_t m;                      
    uint32_t numNodes;               
    uint32_t numFiles;               
    uint32_t numLookups;          
    uint32_t failingNodes;           
    Time timeoutDuration;            
    
    std::vector<uint32_t> files;
    std::vector<uint32_t> filesForLookup;  
    std::vector<ChordNode> nodes;    
    NodeContainer nsNodes;           
    std::vector<Ptr<ChordApplication>> applications;  
    
    struct Statistics {
        uint32_t totalLookups;
        uint32_t successfulLookups;
        uint32_t averageHops;
        uint32_t failedLookups;
        uint32_t minHops;
        uint32_t maxHops;

        Statistics() : totalLookups(0), successfulLookups(0), averageHops(0), failedLookups(0), 
                       minHops(UINT32_MAX), maxHops(0) {}
    } stats;

    std::set<uint32_t> processedLookups;

public:
    ChordNetwork(uint32_t m_param = 10, 
                uint32_t nodes_param = 100,
                uint32_t files_param = 50,
                uint32_t lookups_param = 25,  
                uint32_t failing_param = 10,
                Time timeout = Seconds(5.0))
        : m(m_param)
        , numNodes(nodes_param)
        , numFiles(files_param)
        , numLookups(lookups_param)
        , failingNodes(failing_param)
        , timeoutDuration(timeout) {
            initializeNetwork();
    }

    void initializeNetwork() {
        createPhysicalNetwork();
        
        for (uint32_t i = 0; i < numNodes; i++) {
            initializeFingerTable(i);
            initializeSuccessorList(i);
        }

        createChordApplications();

        Ptr<UniformRandomVariable> rng = CreateObject<UniformRandomVariable>();
        for (uint32_t i = 0; i < numFiles; i++) {
            uint32_t fileId = rng->GetInteger(0, (1 << m) - 1);
            files.push_back(fileId);
        }
    }

    void StartSimulation() {
        cout << "Startando la simulazione Chord con " << numNodes << " nodi" << endl;
        cout << "Fase 1: Inserimento di " << numFiles << " file" << endl;
        InsertFiles();

        cout << "Fase 2: Simulazione di " << failingNodes << " nodi che falliscono" << endl;
        SimulateNodeFailures();

        PrepareFilesToLookup();

        cout << "Fase 3: Esecuzione di " << numLookups << " lookup" << endl;
        PerformLookups();
    }

    void PrintStatistics() {
        cout << "===========================================" << endl;
        cout << "SIMULAZIONE CHORD - STATISTICHE FINALI" << endl;
        cout << "===========================================" << endl;
        cout << "Parametri di simulazione:" << endl;
        cout << "  Numero di nodi: " << numNodes << endl;
        cout << "  Numero di file inseriti: " << numFiles << endl;
        cout << "  Numero di lookup eseguiti: " << numLookups << endl;
        cout << "  Nodi che hanno fallito: " << failingNodes << " (" << (float)failingNodes/numNodes*100 << "%)" << endl;
        cout << "===========================================" << endl;
        cout << "Risultati dei lookup:" << endl;
        cout << "  Totale lookup eseguiti: " << stats.totalLookups << endl;
        cout << "  Lookup riusciti: " << stats.successfulLookups << " (" << 
            (stats.totalLookups > 0 ? (float)stats.successfulLookups/stats.totalLookups*100 : 0) << "%)" << endl;
        cout << "  Lookup falliti: " << stats.failedLookups << " (" << 
            (stats.totalLookups > 0 ? (float)stats.failedLookups/stats.totalLookups*100 : 0) << "%)" << endl;
        
        if (stats.successfulLookups > 0) {
            cout << "  Media hop per lookup riuscito: " << (float)stats.averageHops / stats.successfulLookups << endl;
            cout << "  Minimo hop per lookup riuscito: " << stats.minHops << endl;
            cout << "  Massimo hop per lookup riuscito: " << stats.maxHops << endl;
            cout << "  Media teorica (log2(N)): " << log2(numNodes) << endl;
        } else {
            cout << "  Media/Min/Max hop: N/A (nessun lookup riuscito)" << endl;
        }
        cout << "===========================================" << endl;
    }
    
    void WriteStatisticsToCSV(const std::string& filename) {
        std::ofstream csvFile;
        csvFile.open(filename);
        
        if (!csvFile.is_open()) {
            cout << "ERRORE: Impossibile aprire il file " << filename << " per la scrittura" << endl;
            return;
        }
        
        csvFile << "NumNodes,NumFiles,NumLookups,FailingNodes,TotalLookups,SuccessfulLookups,FailedLookups,SuccessRate,AverageHops,MinHops,MaxHops,TheoreticalAverage" << endl;
        
        float successRate = stats.totalLookups > 0 ? (float)stats.successfulLookups/stats.totalLookups*100 : 0;
        float averageHops = stats.successfulLookups > 0 ? (float)stats.averageHops / stats.successfulLookups : 0;
        float theoreticalAverage = log2(numNodes);
        
        csvFile << numNodes << ","
                << numFiles << ","
                << numLookups << ","
                << failingNodes << ","
                << stats.totalLookups << ","
                << stats.successfulLookups << ","
                << stats.failedLookups << ","
                << successRate << ","
                << averageHops << ","
                << (stats.successfulLookups > 0 ? stats.minHops : 0) << ","
                << (stats.successfulLookups > 0 ? stats.maxHops : 0) << ","
                << theoreticalAverage << endl;
        
        csvFile.close();
        cout << "Statistiche scritte nel file " << filename << endl;
    }

    void UpdateStats(bool success, uint32_t hops, uint32_t lookupId) {
        if (processedLookups.find(lookupId) != processedLookups.end()) {
            cout << "WARN: Lookup " << lookupId << " already processed, ignoring duplicate" << endl;
            return;
        }
        
        processedLookups.insert(lookupId);
        
        if (success) {
            stats.successfulLookups++;
            stats.averageHops += hops;
            
            if (hops < stats.minHops) {
                stats.minHops = hops;
            }
            if (hops > stats.maxHops) {
                stats.maxHops = hops;
            }
            
            cout << "Lookup " << lookupId << " riuscito dopo " << hops << " hop" << endl;
        } else {
            stats.failedLookups++;
            cout << "Lookup " << lookupId << " fallito" << endl;
        }
        
        cout << "DEBUG: Statistiche aggiornate:" << endl;
        cout << "  Total Lookups: " << stats.totalLookups << endl;
        cout << "  Successful Lookups: " << stats.successfulLookups << endl;
        cout << "  Failed Lookups: " << stats.failedLookups << endl;
        if (stats.successfulLookups > 0) {
            cout << "  Average Hops: " << (float)stats.averageHops / stats.successfulLookups << endl;
            cout << "  Min Hops: " << stats.minHops << endl;
            cout << "  Max Hops: " << stats.maxHops << endl;
        } else {
            cout << "  Average/Min/Max Hops: N/A (no successful lookups)" << endl;
        }
    }

    void PrepareFilesToLookup() {
        filesForLookup.clear();
        
        cout << "Preparazione di " << numLookups << " file da cercare tra i " << files.size() << " inseriti" << endl;
        
        if (numLookups >= numFiles) {
            filesForLookup = files;
            cout << "Cercheremo tutti i " << filesForLookup.size() << " file inseriti" << endl;
            return;
        }
        
        Ptr<UniformRandomVariable> rng = CreateObject<UniformRandomVariable>();
        std::set<uint32_t> selectedIndices;  
        
        while (selectedIndices.size() < numLookups) {
            uint32_t index = rng->GetInteger(0, numFiles - 1);
            if (selectedIndices.find(index) == selectedIndices.end()) {
                selectedIndices.insert(index);
                filesForLookup.push_back(files[index]);
            }
        }
        
        cout << "Selezionati " << filesForLookup.size() << " file da cercare" << endl;
    }

private:
    void createPhysicalNetwork() {
        nsNodes.Create(numNodes);
        
        // Setup del routing
        Ipv4NixVectorHelper nixRouting;
        InternetStackHelper stackIP;
        stackIP.SetRoutingHelper(nixRouting);
        stackIP.Install(nsNodes);

        PointToPointHelper p2p;
        p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
        p2p.SetChannelAttribute ("Delay", StringValue ("50ms"));

        Ipv4AddressHelper ipv4;
        ipv4.SetBase ("10.0.0.0", "/30");
        
        Ptr<UniformRandomVariable> rng = CreateObject<UniformRandomVariable>();
        std::set<uint32_t> usedIds; 
        std::vector<uint32_t> chordIds(numNodes);
        
        cout << "DEBUG: Generazione di " << numNodes << " ChordID nello spazio 0.." << ((1 << m) - 1) << endl;
        
        for (uint32_t i = 0; i < numNodes; i++) {
            uint32_t chordId;
            do {
                chordId = rng->GetInteger(0, (1 << m) - 1);
            } while (usedIds.find(chordId) != usedIds.end());
            
            usedIds.insert(chordId);
            chordIds[i] = chordId;
            cout << "DEBUG: Nodo " << i << " ha ChordID " << chordId << endl;
        }
        
        std::vector<std::pair<uint32_t, uint32_t>> nodeIdPairs;
        for (uint32_t i = 0; i < numNodes; i++) {
            nodeIdPairs.push_back(std::make_pair(chordIds[i], i));
        }
        std::sort(nodeIdPairs.begin(), nodeIdPairs.end());
        
        const uint32_t ringSize = std::min(numNodes, (uint32_t)10);
        for (uint32_t i = 0; i < ringSize; i++) {
            NetDeviceContainer dev = p2p.Install(nsNodes.Get(i), nsNodes.Get((i + 1) % ringSize));
            ipv4.Assign(dev);
            ipv4.NewNetwork();
            
            nodes.push_back(ChordNode(chordIds[i], nsNodes.Get(i)));
        }

        Ptr<UniformRandomVariable> r = CreateObject<UniformRandomVariable>();
        for (uint32_t i = ringSize; i < numNodes; i++) {
            vector<bool> link(numNodes, false);
            uint32_t numLinks = 0;
            uint32_t minLinks = 3;

            nodes.push_back(ChordNode(chordIds[i], nsNodes.Get(i)));

            while (numLinks < minLinks) {
                uint32_t j = r->GetInteger((i - ringSize) / 3 * 2, i - 1);
                if ((j == i) || link[j]) {
                    continue;
                }
                link[j] = true;
                NetDeviceContainer dev = p2p.Install(nsNodes.Get(i), nsNodes.Get(j));
                ipv4.Assign(dev);
                ipv4.NewNetwork();
                numLinks++;
            }
        }
        
        cout << "DEBUG: ChordID generati: ";
        for (uint32_t i = 0; i < nodes.size(); i++) {
            cout << nodes[i].chordId << " ";
        }
        cout << endl;
    }

    void initializeFingerTable(uint32_t nodeIndex) {
        ChordNode& node = nodes[nodeIndex];
        node.fingerTable.clear();
        
        cout << "DEBUG: Inizializzazione finger table per nodo " << nodeIndex << " con ChordID " << node.chordId << endl;
        
        for (uint32_t i = 0; i < m; i++) {
            uint32_t fingerStart = (node.chordId + (1 << i)) % (1 << m);
            uint32_t successor = findSuccessor(fingerStart);
            node.fingerTable.push_back(successor);
            cout << "DEBUG: Finger " << i << " per nodo " << node.chordId << ": start=" << fingerStart << ", successor=" << successor << endl;
        }
    }

    void initializeSuccessorList(uint32_t nodeIndex) {
        ChordNode& node = nodes[nodeIndex];
        node.successorList.clear();
        
        cout << "DEBUG: Inizializzazione successor list per nodo " << nodeIndex << " con ChordID " << node.chordId << endl;
        
        std::vector<uint32_t> sortedChordIds;
        for (const ChordNode& n : nodes) {
            sortedChordIds.push_back(n.chordId);
        }
        std::sort(sortedChordIds.begin(), sortedChordIds.end());
        
        auto it = std::find(sortedChordIds.begin(), sortedChordIds.end(), node.chordId);
        if (it == sortedChordIds.end()) {
            cout << "ERROR: ChordID " << node.chordId << " non trovato nell'array ordinato!" << endl;
            return;
        }
        
        size_t pos = std::distance(sortedChordIds.begin(), it);
        
        size_t successorPos = (pos + 1) % sortedChordIds.size();
        node.successorList.push_back(sortedChordIds[successorPos]);
        cout << "DEBUG: Successore per nodo " << node.chordId << ": " << sortedChordIds[successorPos] << endl;
    }

    uint32_t findSuccessor(uint32_t id) {
        cout << "DEBUG: findSuccessor per id " << id << endl;
        
        if (nodes.empty()) {
            cout << "DEBUG: Nessun nodo nella rete!" << endl;
            return 0;
        }
        
        std::vector<uint32_t> sortedChordIds;
        for (const ChordNode& node : nodes) {
            sortedChordIds.push_back(node.chordId);
        }
        std::sort(sortedChordIds.begin(), sortedChordIds.end());
        
        for (uint32_t chordId : sortedChordIds) {
            if (chordId >= id) {
                cout << "DEBUG: findSuccessor ha trovato il nodo " << chordId << " per id " << id << endl;
                return chordId;
            }
        }
        
        cout << "DEBUG: findSuccessor non ha trovato nodi con id >= " << id << ", ritorna il primo nodo " << sortedChordIds[0] << endl;
        return sortedChordIds[0];
    }

    void createChordApplications() {
        for (uint32_t i = 0; i < numNodes; i++) {
            Ptr<ChordApplication> app = CreateObject<ChordApplication>();
            nsNodes.Get(i)->AddApplication(app);
            app->Setup(nodes[i].chordId, nodes[i].fingerTable, nodes[i].successorList, nodes[i].predecessor);
            
            for (uint32_t j = 0; j < numNodes; j++) {
                app->AddNodeAddress(nodes[j].chordId, getNodeAddress(j));
            }
            
            app->SetStatsCallback(MakeCallback(&ChordNetwork::UpdateStats, this));
            
            applications.push_back(app);
        }
    }

    Address getNodeAddress(uint32_t nodeId) {
        if (nodeId >= nodes.size() || !nodes[nodeId].nsNode) {
            cout << "ERROR: ID nodo non valido o puntatore nodo nullo: " << nodeId << endl;
            return Address();
        }

        Ptr<Ipv4> ipv4 = nodes[nodeId].nsNode->GetObject<Ipv4>();
        if (!ipv4) {
            cout << "ERROR: Nessun stack IPv4 sul nodo: " << nodeId << endl;
            return Address();
        }

        if (ipv4->GetNInterfaces() <= 1) {
            cout << "ERROR: Nessuna interfaccia di rete sul nodo: " << nodeId << endl;
            return Address();
        }

        Ipv4InterfaceAddress iaddr = ipv4->GetAddress(1,0);
        return InetSocketAddress(iaddr.GetLocal(), 9);
    }

    void InsertFiles() {
        Ptr<UniformRandomVariable> rng = CreateObject<UniformRandomVariable>();
        
        Time startTime = Seconds(1.0);
        
        cout << "DEBUG: Inserimento di " << files.size() << " file nella rete" << endl;
        
        std::map<uint32_t, uint32_t> chordIdToIndex;
        for (uint32_t i = 0; i < nodes.size(); i++) {
            chordIdToIndex[nodes[i].chordId] = i;
        }
        
        for (uint32_t i = 0; i < files.size(); i++) {
            uint32_t fileId = files[i];
            uint32_t startNodeIndex = rng->GetInteger(0, numNodes - 1);
            uint32_t responsibleChordId = findSuccessor(fileId);
            
            if (chordIdToIndex.find(responsibleChordId) == chordIdToIndex.end()) {
                cout << "ERROR: ChordID " << responsibleChordId << " non trovato nella mappa!" << endl;
                continue;
            }
            uint32_t responsibleNodeIndex = chordIdToIndex[responsibleChordId];
            
            cout << "DEBUG: File " << fileId << " - Nodo di partenza: " << startNodeIndex 
                 << " (ChordID: " << nodes[startNodeIndex].chordId << ")"
                 << " - Nodo responsabile: " << responsibleNodeIndex 
                 << " (ChordID: " << responsibleChordId << ")" << endl;
            
            if (startNodeIndex >= applications.size() || responsibleNodeIndex >= nodes.size()) {
                cout << "ERROR: Indice nodo non valido in InsertFiles" << endl;
                continue;
            }

            Address targetAddr = getNodeAddress(responsibleNodeIndex);
            if (targetAddr.IsInvalid()) {
                cout << "ERROR: Indirizzo target non valido per il nodo " << responsibleNodeIndex << endl;
                continue;
            }

            uint32_t sNodeIndex = startNodeIndex;
            uint32_t fId = fileId;
            Address tAddr = targetAddr;

            auto sendStoreMessage = [this, sNodeIndex, fId, tAddr]() {
                if (sNodeIndex >= applications.size() || !applications[sNodeIndex]) {
                    cout << "ERROR: Puntatore applicazione non valido" << endl;
                    return;
                }
                
                ChordMessage msg;
                msg.type = ChordMessage::STORE_FILE;
                msg.senderId = nodes[sNodeIndex].chordId;
                msg.originId = nodes[sNodeIndex].chordId;   
                msg.targetId = fId;
                msg.hopCount = 0;
                msg.success = false;

                Ptr<Packet> packet = Create<Packet>((uint8_t*)&msg, sizeof(ChordMessage));
                if (packet && applications[sNodeIndex]->IsStarted()) {
                    applications[sNodeIndex]->SendMessage(packet, tAddr);
                    cout << "DEBUG: Inviato messaggio STORE_FILE per file " << fId 
                         << " dal nodo " << sNodeIndex << " (ChordID: " << nodes[sNodeIndex].chordId << ")"
                         << " al nodo responsabile" << endl;
                } else {
                    cout << "ERROR: Failed to create packet or application not started" << endl;
                }
            };

            Simulator::Schedule(startTime + Seconds(0.1 * i), sendStoreMessage);
        }
    }

    void SimulateNodeFailures() {
        Ptr<UniformRandomVariable> rng = CreateObject<UniformRandomVariable>();
        
        Time startTime = Seconds(5.0);
        
        std::set<uint32_t> failedNodes;
        
        for (uint32_t i = 0; i < failingNodes; i++) {
            uint32_t nodeIndex;
            bool validNode = false;

            while (!validNode) {
                nodeIndex = rng->GetInteger(0, numNodes - 1);
                if (nodes[nodeIndex].isAlive && failedNodes.find(nodeIndex) == failedNodes.end()) {
                    validNode = true;
                    failedNodes.insert(nodeIndex);
                }
            }
            
            uint32_t nIndex = nodeIndex;
            
            auto setNodeDown = [this, nIndex]() {
                if (nIndex < nodes.size() && nIndex < applications.size()) {
                    nodes[nIndex].isAlive = false;
                    applications[nIndex]->SetAlive(false);
                    cout << "INFO: Simulando fallimento del nodo " << nIndex << endl;
                }
            };
            
            Time failureTime = startTime + Seconds(rng->GetValue(0.0, 5.0));
            Simulator::Schedule(failureTime, setNodeDown);
        }
    }

    void PerformLookups() {
        Ptr<UniformRandomVariable> rng = CreateObject<UniformRandomVariable>();
        
        Time startTime = Seconds(10.0);
        
        uint32_t totalAttempts = 0;
        uint32_t skippedDeadNodes = 0;
        
        std::map<uint32_t, uint32_t> chordIdToIndex;
        for (uint32_t i = 0; i < nodes.size(); i++) {
            chordIdToIndex[nodes[i].chordId] = i;
        }
        
        for (uint32_t i = 0; i < filesForLookup.size(); i++) {
            uint32_t fileId = filesForLookup[i];
            uint32_t startNodeIndex = rng->GetInteger(0, numNodes - 1);
            
            totalAttempts++;
            
            if (startNodeIndex >= applications.size() || !applications[startNodeIndex]) {
                cout << "ERROR: Indice nodo non valido in PerformLookups: " << startNodeIndex << endl;
                continue;
            }
            
            if (!nodes[startNodeIndex].isAlive) {
                cout << "INFO: Skippo una lookup da un nodo down " << startNodeIndex 
                     << " (ChordID: " << nodes[startNodeIndex].chordId << ")"
                     << " per il file " << fileId << endl;
                skippedDeadNodes++;
                continue;
            }
            
            uint32_t sNodeIndex = startNodeIndex;
            uint32_t fId = fileId;
            uint32_t lookupId = ChordApplication::nextLookupId++;

            auto startLookup = [this, sNodeIndex, fId, lookupId]() {
                if (sNodeIndex < applications.size() && applications[sNodeIndex] && applications[sNodeIndex]->IsStarted()) {
                    cout << "INFO: Inizio lookup " << lookupId << " dal nodo " << sNodeIndex 
                         << " (ChordID: " << nodes[sNodeIndex].chordId << ")"
                         << " per il file " << fId << endl;
                    applications[sNodeIndex]->PerformLookup(fId, timeoutDuration, lookupId);
                    stats.totalLookups++;
                }
            };

            Simulator::Schedule(startTime + Seconds(0.1 * i), startLookup);
        }
        
        cout << "INFO: Statistiche lookup:" << endl;
        cout << "  Tentativi totali: " << totalAttempts << endl;
        cout << "  Tentativi saltati a causa di nodi down: " << skippedDeadNodes << endl;
    }
};

int main(int argc, char *argv[]) {
    uint32_t m = 14; 
    uint32_t numNodes = 10;     
    uint32_t numFiles = 5;
    uint32_t numLookups = 3; 
    uint32_t failingNodes = 0;
    uint32_t seed = 1;
    std::string csvFilename = "chord_stats.csv";  

    CommandLine cmd;
    cmd.AddValue("m", "Numero di bit per lo spazio degli ID", m);
    cmd.AddValue("nodes", "Numero di nodi", numNodes);
    cmd.AddValue("files", "Numero di file da inserire", numFiles);
    cmd.AddValue("lookups", "Numero di lookup da eseguire", numLookups);
    cmd.AddValue("failing", "Numero di nodi che falliranno", failingNodes);
    cmd.AddValue("seed", "Seed for random number generator", seed);
    cmd.AddValue("csv", "Nome del file CSV per le statistiche", csvFilename);
    cmd.Parse(argc, argv);

    RngSeedManager::SetSeed(seed);
    
    ChordNetwork network(m, numNodes, numFiles, numLookups, failingNodes);
    network.StartSimulation();

    Simulator::Run();
    network.PrintStatistics();
    network.WriteStatisticsToCSV(csvFilename);
    Simulator::Destroy();

    return 0;
} 