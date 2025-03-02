set terminal pngcairo enhanced font "Arial,12" size 800,600
set output "grafici/rapporto_successi_fallimenti.png"
set title "Risultati delle ricerche nella rete Chord" font "Arial,14"
set style fill solid 0.8
set boxwidth 0.5

# Dati direttamente inclusi nello script
$data << EOD
0 50 "Successi"
1 50 "Fallimenti"
EOD

# Configurazione semplice
set border 3  # Mostra solo i bordi inferiore e sinistro
set ylabel "Numero di ricerche" font "Arial,11"
set yrange [0:100*1.2]
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
plot $data using 1:2:(0.5):(column(1)) with boxes palette notitle,      $data using 1:2:(sprintf("%d (%.1f%%)", $2, $1 == 0 ? 50.00 : 50.00)) with labels offset 0,1 font "Arial,10" notitle

