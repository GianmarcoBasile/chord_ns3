#!/bin/bash

# Script per eseguire esperimenti con il simulatore Chord
# Uso: ./run_chord_experiments.sh [numero_ripetizioni]

# Numero di ripetizioni predefinito
NUM_REPETITIONS=3

# Se viene fornito un argomento, usa quello come numero di ripetizioni
if [ $# -eq 1 ]; then
    NUM_REPETITIONS=$1
fi

# Percorso del programma Chord
CHORD_PATH="scratch/new_chord/chord.cc"  

# Percorso di ns-3 (relativo alla home dell'utente)
NS3_DIR="ns-allinone-3.43/ns-3.43"

# Verifica che il programma Chord esista
cd ~/$NS3_DIR
if [ ! -f "$CHORD_PATH" ]; then
    echo "ERRORE: Il file $CHORD_PATH non esiste!"
    echo "Assicurati che il file chord.cc sia nella directory scratch/new_chord/"
    exit 1
fi

# Torna alla directory originale
cd - > /dev/null

# Crea directory per i risultati
RESULTS_DIR="chord_results"
mkdir -p $RESULTS_DIR

# File CSV per i risultati finali
FINAL_RESULTS="$RESULTS_DIR/final_results.csv"

# Intestazione del file CSV finale
echo "NumNodes,NumFiles,NumLookups,FailingNodes,TotalLookups,SuccessfulLookups,FailedLookups,SuccessRate,AverageHops,MinHops,MaxHops,TheoreticalAverage" > $FINAL_RESULTS

# Array dei parametri
NODES_ARRAY=(400 1600 3200)
LOOKUPS_ARRAY=(10 50 100 200)
FAILING_ARRAY=(10 20 40 160)

# Funzione per calcolare la media di una colonna in un file CSV
calculate_average() {
    local file=$1
    local column=$2
    local skip_header=$3
    
    if [ ! -f "$file" ]; then
        echo "0"
        return
    fi
    
    if [ "$skip_header" = "true" ]; then
        awk -F, -v col="$column" 'NR>1 {sum+=$col; count++} END {if(count>0) print sum/count; else print "0"}' "$file"
    else
        awk -F, -v col="$column" '{sum+=$col; count++} END {if(count>0) print sum/count; else print "0"}' "$file"
    fi
}

# Ciclo principale per tutte le combinazioni di parametri
for NODES in "${NODES_ARRAY[@]}"; do
    for LOOKUPS in "${LOOKUPS_ARRAY[@]}"; do
        for FAILING in "${FAILING_ARRAY[@]}"; do
            # Verifica che il numero di nodi che falliscono non superi il numero totale di nodi
            if [ $FAILING -ge $NODES ]; then
                echo "Skipping configuration: Nodes=$NODES, Lookups=$LOOKUPS, Failing=$FAILING (too many failing nodes)"
                continue
            fi
            
            echo "Running configuration: Nodes=$NODES, Lookups=$LOOKUPS, Failing=$FAILING"
            
            # Crea directory per questa configurazione
            CONFIG_DIR="$RESULTS_DIR/n${NODES}_l${LOOKUPS}_f${FAILING}"
            mkdir -p $CONFIG_DIR
            
            # File CSV per i risultati di questa configurazione
            CONFIG_RESULTS="$CONFIG_DIR/results.csv"
            
            # Intestazione del file CSV di configurazione
            echo "NumNodes,NumFiles,NumLookups,FailingNodes,TotalLookups,SuccessfulLookups,FailedLookups,SuccessRate,AverageHops,MinHops,MaxHops,TheoreticalAverage" > $CONFIG_RESULTS
            
            # Esegui la simulazione NUM_REPETITIONS volte
            for (( i=1; i<=$NUM_REPETITIONS; i++ )); do
                echo "  Repetition $i/$NUM_REPETITIONS"
                
                # Nome del file CSV per questa ripetizione
                REPETITION_CSV="$CONFIG_DIR/rep${i}.csv"
                
                # Percorso assoluto del file CSV
                ABSOLUTE_REPETITION_CSV="$(pwd)/$REPETITION_CSV"
                
                # Esegui la simulazione
                cd ~/$NS3_DIR
                
                # Poi eseguilo con il percorso assoluto del file CSV
                ./ns3 run "scratch/new_chord/chord --m=14 --nodes=$NODES --files=$LOOKUPS --lookups=$LOOKUPS --failing=$FAILING --seed=$i --csv=$ABSOLUTE_REPETITION_CSV"
                
                # Torna alla directory originale
                cd - > /dev/null
                
                # Aggiungi i risultati al file CSV di configurazione
                if [ -f "$REPETITION_CSV" ]; then
                    echo "  File CSV trovato: $REPETITION_CSV"
                    tail -n 1 "$REPETITION_CSV" >> $CONFIG_RESULTS
                else
                    echo "Warning: $REPETITION_CSV not found"
                    echo "  Verificando percorso alternativo..."
                    
                    # Controlla se il file esiste nella directory di ns-3 (usando path relativo)
                    NS3_CSV="~/$NS3_DIR/$REPETITION_CSV"
                    if [ -f "$NS3_CSV" ]; then
                        echo "  File trovato in: $NS3_CSV"
                        tail -n 1 "$NS3_CSV" >> $CONFIG_RESULTS
                    else
                        # Controlla se il file è stato scritto nella directory corrente di ns-3
                        NS3_CURRENT_CSV="~/$NS3_DIR/rep${i}.csv"
                        if [ -f "$NS3_CURRENT_CSV" ]; then
                            echo "  File trovato in: $NS3_CURRENT_CSV"
                            tail -n 1 "$NS3_CURRENT_CSV" >> $CONFIG_RESULTS
                            # Sposta il file nella posizione corretta
                            mkdir -p "$(dirname "$REPETITION_CSV")"
                            cp "$NS3_CURRENT_CSV" "$REPETITION_CSV"
                            echo "  File copiato in: $REPETITION_CSV"
                        else
                            echo "  File non trovato neanche in: $NS3_CURRENT_CSV"
                            # Aggiungi una riga vuota per mantenere la struttura
                            echo "$NODES,$LOOKUPS,$LOOKUPS,$FAILING,0,0,0,0,0,0,0,0" >> $CONFIG_RESULTS
                        fi
                    fi
                fi
            done
            
            # Verifica che il file dei risultati esista e non sia vuoto
            if [ ! -f "$CONFIG_RESULTS" ] || [ ! -s "$CONFIG_RESULTS" ]; then
                echo "Warning: No results for this configuration. Adding empty row."
                echo "$NODES,$LOOKUPS,$LOOKUPS,$FAILING,0,0,0,0,0,0,0,0" >> $FINAL_RESULTS
                continue
            fi
            
            # Calcola le medie per questa configurazione
            AVG_TOTAL_LOOKUPS=$(calculate_average "$CONFIG_RESULTS" 5 true)
            AVG_SUCCESSFUL_LOOKUPS=$(calculate_average "$CONFIG_RESULTS" 6 true)
            AVG_FAILED_LOOKUPS=$(calculate_average "$CONFIG_RESULTS" 7 true)
            AVG_SUCCESS_RATE=$(calculate_average "$CONFIG_RESULTS" 8 true)
            AVG_AVERAGE_HOPS=$(calculate_average "$CONFIG_RESULTS" 9 true)
            AVG_MIN_HOPS=$(calculate_average "$CONFIG_RESULTS" 10 true)
            AVG_MAX_HOPS=$(calculate_average "$CONFIG_RESULTS" 11 true)
            AVG_THEORETICAL=$(calculate_average "$CONFIG_RESULTS" 12 true)
            
            # Aggiungi le medie al file CSV finale
            echo "$NODES,$LOOKUPS,$LOOKUPS,$FAILING,$AVG_TOTAL_LOOKUPS,$AVG_SUCCESSFUL_LOOKUPS,$AVG_FAILED_LOOKUPS,$AVG_SUCCESS_RATE,$AVG_AVERAGE_HOPS,$AVG_MIN_HOPS,$AVG_MAX_HOPS,$AVG_THEORETICAL" >> $FINAL_RESULTS
            
            echo "  Results saved to $CONFIG_DIR"
        done
    done
done

echo "All experiments completed. Final results saved to $FINAL_RESULTS"

# Crea un file di riepilogo leggibile
SUMMARY="$RESULTS_DIR/summary.txt"
echo "CHORD SIMULATION SUMMARY" > $SUMMARY
echo "=======================" >> $SUMMARY
echo "Number of repetitions per configuration: $NUM_REPETITIONS" >> $SUMMARY
echo "" >> $SUMMARY
echo "RESULTS:" >> $SUMMARY
echo "--------" >> $SUMMARY

# Formatta i risultati in modo leggibile
awk -F, 'NR==1 {print "Nodes\tFiles\tLookups\tFailing\tSuccess%\tAvgHops\tMinHops\tMaxHops\tTheory"} 
         NR>1 {printf "%d\t%d\t%d\t%d\t%.2f%%\t%.2f\t%.2f\t%.2f\t%.2f\n", $1, $2, $3, $4, $8, $9, $10, $11, $12}' $FINAL_RESULTS >> $SUMMARY

echo "" >> $SUMMARY
echo "Full results available in $FINAL_RESULTS" >> $SUMMARY
echo "" >> $SUMMARY
echo "=======================" >> $SUMMARY

echo "Summary created at $SUMMARY" 