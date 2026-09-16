#!/usr/bin/env bash
#----------------------------------------------------------------------------#
#  Installatore del plugin Akko 3108 V2 per OpenRGB (Linux)                  #
#  Uso: bash install.sh                                                      #
#  Copia libAkkoKeyboardPlugin.so in ~/.config/OpenRGB/plugins/              #
#----------------------------------------------------------------------------#
set -e

PLUGIN_DIR="$HOME/.config/OpenRGB/plugins"
SO="libAkkoKeyboardPlugin.so"

if [ ! -f "$SO" ]; then
    echo "ERRORE: $SO non trovato accanto a questo script." >&2
    echo "Esegui questo script dalla cartella dove hai estratto il file." >&2
    exit 1
fi

mkdir -p "$PLUGIN_DIR"

# Backup dell'eventuale versione precedente
if [ -f "$PLUGIN_DIR/$SO" ]; then
    BAK="$PLUGIN_DIR/$SO.bak-$(date +%Y%m%d-%H%M%S)"
    cp "$PLUGIN_DIR/$SO" "$BAK"
    echo "Backup della versione precedente salvato in:" 
    echo "  $BAK"
fi

cp "$SO" "$PLUGIN_DIR/$SO"

echo
echo "Installato: $PLUGIN_DIR/$SO"
echo
echo "Prossimi passi:"
echo "  1. Riavvia OpenRGB possibilmente da terminale per vedere i log [AkkoPlugin]:"
echo "       openrgb --startminimized"
echo "  2. Impostazioni -> Plugin -> spunta 'Attivato' sul plugin Akko"
echo "  3. Apri la tab 'Akko Editor' (o seleziona il device 'Akko 3108 V2')"