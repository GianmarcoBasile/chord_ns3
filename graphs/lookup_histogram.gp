# Script gnuplot per la generazione dell'istogramma dei tempi di ricerca
set terminal png size 800,600
set output "lookup_histogram.png"
set title "Distribuzione dei tempi di ricerca Chord"
set xlabel "Tempo di ricerca (ms)"
set ylabel "Numero di ricerche"
set grid
set style fill solid 0.5
set boxwidth 0.8

# Crea dati di esempio per l'istogramma
set print "sample_histogram.dat"
print "10.5"
print "15.2"
print "8.7"
print "12.3"
print "14.8"
print "9.2"
print "11.7"
print "13.5"
print "10.1"
print "11.9"
unset print

# Parametri dell'istogramma
min = 0
max = 20
binwidth = 2
bin(x) = binwidth * floor(x/binwidth) + binwidth/2
set xrange [0:max]

# Plot dell'istogramma
plot "sample_histogram.dat" using (bin($1)):(1.0) smooth freq with boxes lc rgb "blue" title "Tempi di ricerca" 