#!/bin/bash

# --- SCRIPT BASH PER INVIO SEGNALI A COORDINATORE --- #
# --> SIGUSR1: sospensione coordinatore
# --> SIGUSR2: ripresa coordinatore
# --> SIGINT: ignorato dal coordinatore
# --> SIGTERM: terminazione coordinatore


# Funzione per inviare segnali
invio_segnale() {
    local COORD_PID=$1
    case $2 in
        1)
            kill -SIGUSR1 $COORD_PID
            echo "Inviato SIGUSR1 al processo $COORD_PID (sospendi)"
            ;;
        2)
            kill -SIGUSR2 $COORD_PID
            echo "Inviato SIGUSR2 al processo $COORD_PID (riprendi)"
            ;;
        3)
            kill -SIGINT $COORD_PID
            echo "Inviato SIGINT al processo $COORD_PID (ignora)"
            ;;
        4)
            kill -SIGTERM $COORD_PID
            echo "Inviato SIGTERM al processo $COORD_PID (termina)"
            exit 0
            ;;

        *)
            echo "Scelta non valida. Inserisci 1, 2, 3 o 4."
            ;;
    esac
}

# Trova il PID del processo outCoordinatore
COORD_PID=$(pgrep -f './outCoordinatore')
if [ -z "$COORD_PID" ]; then
    echo "Errore: processo ./outCoordinatore non trovato."
    exit 1
fi

echo "Trovato processo ./outCoordinatore con PID $COORD_PID."

# Loop per inserimento da tastiera
while true; do
    echo "Inserisci il numero del segnale da inviare:"
    echo "1 - Sospendi (SIGUSR1)"
    echo "2 - Riprendi (SIGUSR2)"
    echo "3 - Ignora (SIGINT)"
    echo "4 - Termina (SIGTERM)"
    read -r scelta

    invio_segnale "$COORD_PID" "$scelta"
done
