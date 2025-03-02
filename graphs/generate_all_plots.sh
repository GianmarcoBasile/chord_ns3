#!/bin/bash
# Script per generare tutti i grafici gnuplot

echo "================================================="
echo "  GENERAZIONE GRAFICI PER SIMULAZIONE CHORD"
echo "================================================="

# Verifica che gnuplot sia installato
if ! command -v gnuplot &> /dev/null; then
    echo "ERRORE: gnuplot non è installato nel sistema."
    echo "Installalo con: sudo apt-get install gnuplot"
    exit 1
fi

# Directory attuale per i grafici
cd "$(dirname "$0")"
GRAPH_DIR="$(pwd)"

echo "Generazione grafici nella directory: $GRAPH_DIR"
echo

# Esegui gli script gnuplot
echo "Generazione grafici in corso..."

echo -n "- Istogramma dei tempi di ricerca... "
gnuplot "$GRAPH_DIR/lookup_histogram.gp" 2>/dev/null
if [ -f "$GRAPH_DIR/lookup_histogram.png" ]; then
    echo "✓ Completato"
else
    echo "✗ Errore"
fi

echo -n "- Grafico a dispersione dei tempi di ricerca... "
gnuplot "$GRAPH_DIR/lookup_scatter.gp" 2>/dev/null
if [ -f "$GRAPH_DIR/lookup_scatter.png" ]; then
    echo "✓ Completato"
else
    echo "✗ Errore"
fi

echo -n "- Grafico delle statistiche... "
gnuplot "$GRAPH_DIR/lookup_stats.gp" 2>/dev/null
if [ -f "$GRAPH_DIR/lookup_stats.png" ]; then
    echo "✓ Completato"
else
    echo "✗ Errore"
fi

echo -n "- Grafico del tasso di successo... "
gnuplot "$GRAPH_DIR/success_rate.gp" 2>/dev/null
if [ -f "$GRAPH_DIR/success_rate.png" ]; then
    echo "✓ Completato"
else
    echo "✗ Errore"
fi

echo "================================================="
echo "Tutti i grafici sono stati generati!"
echo "Le immagini si trovano nella cartella: $GRAPH_DIR"
echo "================================================="

# Pulizia file temporanei
rm -f "$GRAPH_DIR"/*.dat 2>/dev/null

# Visualizza le immagini se possibile
if command -v display &> /dev/null; then
    echo "Vuoi visualizzare i grafici generati? (s/n): "
    read -r answer
    if [[ "$answer" == "s" || "$answer" == "S" ]]; then
        for img in "$GRAPH_DIR"/*.png; do
            display "$img" &
        done
    fi
elif command -v xdg-open &> /dev/null; then
    echo "Vuoi visualizzare i grafici generati? (s/n): "
    read -r answer
    if [[ "$answer" == "s" || "$answer" == "S" ]]; then
        for img in "$GRAPH_DIR"/*.png; do
            xdg-open "$img" &
        done
    fi
elif command -v open &> /dev/null; then
    echo "Vuoi visualizzare i grafici generati? (s/n): "
    read -r answer
    if [[ "$answer" == "s" || "$answer" == "S" ]]; then
        for img in "$GRAPH_DIR"/*.png; do
            open "$img" &
        done
    fi
else
    echo "Per visualizzare i grafici, apri i file .png nella cartella: $GRAPH_DIR"
fi 