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
min_time = 20.00
max_time = 110.00
range = max_time - min_time
bin_width = range > 50 ? 5 : (range > 20 ? 2 : 1)
num_bins = ceil(range / bin_width) + 1

# Creiamo l'istogramma per i successi con bin width ottimale
set boxwidth bin_width * 0.8
bin(x) = bin_width * floor(x/bin_width)
set xrange [min_time-bin_width:max_time+bin_width]
set xtics min_time, bin_width * ceil(num_bins/10), max_time

# Stampiamo le statistiche sul grafico
set label 1 sprintf("Ricerche con successo: %d", 50) at graph 0.7, 0.90 font "Arial,9"
set label 2 sprintf("Tempo minimo: %.1f ms", 20.00) at graph 0.7, 0.85 font "Arial,9"
set label 3 sprintf("Tempo massimo: %.1f ms", 110.00) at graph 0.7, 0.80 font "Arial,9"
set label 4 sprintf("Tempo medio: %.1f ms", 44.6) at graph 0.7, 0.75 font "Arial,9"

plot 'grafici/tempi_successi.dat' using (bin($1)):(1.0) smooth freq with boxes lc rgb "#00AA00" title "Tempi di ricerca"
