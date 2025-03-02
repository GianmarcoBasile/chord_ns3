# Script gnuplot per la generazione del grafico delle statistiche
set terminal png size 800,600
set output "lookup_stats.png"
set title "Statistiche dei tempi di ricerca Chord"
set style data histogram
set style histogram cluster gap 1
set style fill solid 0.5 border -1
set boxwidth 0.9
set xtic rotate by -45 scale 0
set key top right
set ylabel "Tempo (ms)"

# Crea dati di esempio per le statistiche
set print "sample_stats.dat"
print "AverageTimeMs 12.5"
print "MinTimeMs 5.2"
print "MaxTimeMs 25.7"
unset print

# Plot delle statistiche
plot "sample_stats.dat" using 2:xtic(1) title "Tempi (ms)" with boxes lc rgb "green" 