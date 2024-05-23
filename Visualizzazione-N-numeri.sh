#!/bin/bash

# Rimuovi i file se già esistono
if [ -f "coord_pid.txt" ]; then
    rm coord_pid.txt
fi

if [ -f "vis_pid.txt" ]; then
    rm vis_pid.txt
fi

# Compilazione del Coordinatore
gcc -o outCoordinatore Coordinatore.c -lrt -pthread
# Compilazione del Visualizzatore
gcc -o outVisualizzatore Visualizzatore.c -lrt -pthread

# Messaggio di richiesta inserimento dati
echo "Applicazione: Visualizzazione di N numeri"
echo "Inserisci il numero di visualizzatori (max 10):"
read num_visualizzatori
echo "Inserisci il valore massimo N:"
read N

# Controllo se compilazione OK
if [ $? -eq 0 ]; then
    echo "Compilazione OK!"

    # Esecuzione del programma
    ./outCoordinatore "$num_visualizzatori" "$N"
else
    echo "Errore durante la compilazione del programma."
fi
