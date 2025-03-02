#!/bin/bash
# Script per generare e visualizzare grafici significativi per la simulazione Chord

echo "====================================================="
echo "  GENERAZIONE GRAFICI CHORD OTTIMIZZATI"
echo "====================================================="

# Verifica che gnuplot sia installato
if ! command -v gnuplot &> /dev/null; then
    echo "ERRORE: gnuplot non è installato nel sistema."
    echo "Installalo con: sudo apt-get install gnuplot"
    exit 1
fi

# Creazione directory per i grafici se non esiste
mkdir -p grafici

# Pulizia di eventuali grafici precedenti
rm -f grafici/*.png grafici/*.dat

# Estrazione dati dai file CSV
echo "Elaborazione dati dalle statistiche Chord..."

# GRAFICO 1: Conteggio successi/fallimenti semplificato
total_searches=$(wc -l < csv/lookup_times.csv)
((total_searches--))  # Rimuoviamo l'intestazione
successful=$(grep "true" csv/lookup_times.csv | wc -l)
failed=$((total_searches - successful))
success_rate=$(echo "scale=2; $successful * 100 / $total_searches" | bc)
failure_rate=$(echo "scale=2; 100 - $success_rate" | bc)

# Analisi dei tempi medi
avg_success_time=$(grep "true" csv/lookup_times.csv | awk -F, '{sum+=$2; count++} END {print sum/count}')
avg_failure_time=$(grep "false" csv/lookup_times.csv | awk -F, '{sum+=$2; count++} END {print sum/count}')

# Creazione del grafico 1 - Versione estremamente semplificata
cat << EOF > grafici/grafico1.gnuplot
set terminal pngcairo enhanced font "Arial,12" size 800,600
set output "grafici/rapporto_successi_fallimenti.png"
set title "Risultati delle ricerche nella rete Chord" font "Arial,14"
set style fill solid 0.8
set boxwidth 0.5

# Dati direttamente inclusi nello script
\$data << EOD
0 $successful "Successi"
1 $failed "Fallimenti"
EOD

# Configurazione semplice
set border 3  # Mostra solo i bordi inferiore e sinistro
set ylabel "Numero di ricerche" font "Arial,11"
set yrange [0:$total_searches*1.2]
set xrange [-0.5:1.5]
unset xtics  # Rimuove tutti i tick
set xtics nomirror ("Successi" 0, "Fallimenti" 1) font "Arial,11"  # Aggiunge solo i tick inferiori
set ytics nomirror font "Arial,10"  # Aggiunge il nomirror anche all'asse y
set grid y
set key off

# Definizione dei colori senza mostrare la colorbox
set palette defined (0 "#00AA00", 1 "#CC0000")
set cbrange [0:1]
unset colorbox  # Nasconde la barra dei colori

# Plot semplice con colori espliciti
plot \$data using 1:2:(0.5):(column(1)) with boxes palette notitle, \
     \$data using 1:2:(sprintf("%d (%.1f%%)", \$2, \$1 == 0 ? $success_rate : $failure_rate)) with labels offset 0,1 font "Arial,10" notitle

EOF

# GRAFICO 2: Distribuzione dei tempi di ricerca (solo successi)
echo "Generazione grafico distribuzione dei tempi di ricerca..."

# Prepariamo dati per i successi
echo "Preparazione dati per l'istogramma..."
grep "true" csv/lookup_times.csv | cut -d, -f2 > grafici/tempi_successi.dat

# Analisi statistiche
min_success=$(sort -n grafici/tempi_successi.dat | head -1)
max_success=$(sort -n grafici/tempi_successi.dat | tail -1)
avg_success=$avg_success_time

cat << EOF > grafici/grafico2.gnuplot
set terminal pngcairo enhanced font "Arial,12" size 800,600
set output "grafici/distribuzione_tempi.png"
set title "Distribuzione dei tempi di ricerca delle ricerche con successo" font "Arial,14"
set style fill solid 0.8 border -1
set grid y
set border 3
set tics nomirror
set xlabel "Tempo di ricerca (ms)"
set ylabel "Numero di ricerche"
set key top right

# Determiniamo il bin ottimale per i successi
min_time = $min_success
max_time = $max_success
range = max_time - min_time
bin_width = range > 50 ? 5 : (range > 20 ? 2 : 1)
num_bins = ceil(range / bin_width) + 1

# Creiamo l'istogramma per i successi con bin width ottimale
set boxwidth bin_width * 0.8
bin(x) = bin_width * floor(x/bin_width)
set xrange [min_time-bin_width:max_time+bin_width]
set xtics min_time, bin_width * ceil(num_bins/10), max_time

# Stampiamo le statistiche sul grafico
set label 1 sprintf("Ricerche con successo: %d", $successful) at graph 0.7, 0.90 font "Arial,9"
set label 2 sprintf("Tempo minimo: %.1f ms", $min_success) at graph 0.7, 0.85 font "Arial,9"
set label 3 sprintf("Tempo massimo: %.1f ms", $max_success) at graph 0.7, 0.80 font "Arial,9"
set label 4 sprintf("Tempo medio: %.1f ms", $avg_success) at graph 0.7, 0.75 font "Arial,9"

plot 'grafici/tempi_successi.dat' using (bin(\$1)):(1.0) smooth freq with boxes lc rgb "#00AA00" title "Tempi di ricerca"
EOF

# Esegui gnuplot per generare i grafici
echo "Generazione dei grafici in corso..."
gnuplot grafici/grafico1.gnuplot
gnuplot grafici/grafico2.gnuplot

# Visualizza i grafici
echo "Apertura dei grafici generati..."

# Determina il metodo di visualizzazione in base all'ambiente
if grep -q Microsoft /proc/version; then
    # Siamo in WSL
    echo "Rilevato ambiente WSL, apertura con visualizzatore di Windows..."
    explorer.exe "$(wslpath -w "$(pwd)/grafici/rapporto_successi_fallimenti.png")"
    explorer.exe "$(wslpath -w "$(pwd)/grafici/distribuzione_tempi.png")"
elif command -v xdg-open &> /dev/null; then
    # Linux con xdg-open
    xdg-open "grafici/rapporto_successi_fallimenti.png"
    xdg-open "grafici/distribuzione_tempi.png"
else
    echo "I grafici sono stati generati in: $(pwd)/grafici/"
    echo "Aprili con un visualizzatore di immagini."
fi

echo "====================================================="
echo "Generazione grafici completata!"
echo "=====================================================" 