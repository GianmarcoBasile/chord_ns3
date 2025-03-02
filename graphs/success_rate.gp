# Script gnuplot per la generazione del grafico del tasso di successo
set terminal png size 800,600
set output "success_rate.png"
set title "Tasso di successo delle ricerche Chord"
set style fill solid 0.5
set ylabel "Percentuale (%)"
set xrange [0:3]
set yrange [0:100]
set grid y
set xtics nomirror
set xtics ("Successo" 1, "Fallimento" 2)

# Dati di esempio per il tasso di successo e fallimento
set print "sample_rates.dat"
print "1 85.0"
print "2 15.0"
unset print

# Crea il grafico a barre
set boxwidth 0.5
set style fill solid 0.5
plot "sample_rates.dat" using 1:2 with boxes lc rgb "green" notitle,\
     "sample_rates.dat" using 1:($2+5):2 with labels 