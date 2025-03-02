# Script gnuplot per la generazione del grafico a dispersione dei tempi di ricerca
set terminal png size 800,600
set output "lookup_scatter.png"
set title "Tempi di ricerca per ogni file Chord"
set xlabel "ID del file"
set ylabel "Tempo di ricerca (ms)"
set grid
set pointsize 1.5

# Crea dati di esempio per i successi e fallimenti
set print "sample_success.dat"
print "1 10.5"
print "2 15.2"
print "4 12.3"
print "5 9.8"
print "7 11.2"
unset print

set print "sample_failure.dat"
print "3 8.7"
print "6 14.5"
unset print

# Plot con punti colorati diversamente
plot "sample_success.dat" using 1:2 with points pt 7 ps 1.5 lc rgb "green" title "Successo", \
     "sample_failure.dat" using 1:2 with points pt 7 ps 1.5 lc rgb "red" title "Fallimento" 