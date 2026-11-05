# Akko 3108 V2 — protocollo LED e mappa dei tasti

Documento di riferimento: come il plugin parla con la tastiera.
Tutto è stato **verificato su hardware reale** (Akko 3108 V2 / Ducky 3108S RGB,
chip Sonix, USB `0C45:762B`).

## 1. Interfaccia USB

| Campo         | Valore                                              |
|---------------|-----------------------------------------------------|
| VID : PID     | `0x0C45 : 0x762B`                                   |
| Interfaccia   | `1` (controllo LED)                                 |
| Usage page    | `0xFF1C` (stessa del detector EVision di OpenRGB)   |
| Nodo Linux    | `/dev/hidraw*` (di solito 0666, nessun permesso extra) |

Il plugin enumera l'HID da solo (`hid_enumerate`), sceglie l'interfaccia
migliore e registra un controller RGB virtuale "Akko 3108 V2".

## 2. Pacchetto HID (64 byte, Report ID `0x04`)

```
byte 0     0x04            Report ID
byte 1-2   checksum        somma dei byte 3..63, little-endian
byte 3     comando
byte 4     dimensione dati
byte 5-6   parametro / offset (little-endian)
byte 8..   dati
```

Il checksum va calcolato **su ogni pacchetto**, anche BEGIN e END.

| Comando              | Codice |
|----------------------|--------|
| BEGIN                | `0x01` |
| END                  | `0x02` |
| SET_PARAM            | `0x06` |
| WRITE_COLOR (custom) | `0x11` |

## 3. Cambio effetto (sequenza verificata)

```
BEGIN
SET_PARAM(0x00 Modo)        1 byte
SET_PARAM(0x01 Luminosità)  1 byte  0x00 (spento) .. 0x04 (max)
SET_PARAM(0x02 Velocità)    1 byte  0x00 veloce .. 0x05 lento
SET_PARAM(0x03 Direzione)   1 byte  0 sx, 1 dx, 2 su, 3 giù
SET_PARAM(0x04 Casuale)     1 byte  0/1
SET_PARAM(0x05 Colore)      3 byte  R,G,B
END
```

Senza BEGIN/END, o senza il parametro colore, alcuni effetti si spengono o
partono solo al secondo clic.

### Modi

| Modo | ID | Modo | ID |
|---|---|---|---|
| Off | `0x00` | Reactive Line | `0x09` |
| Color Wave (corta) | `0x01` | Starlight | `0x0A` |
| Color Wave | `0x02` | Blooming | `0x0B` |
| Color Wheel | `0x03` | Vertical Rainbow | `0x0C` |
| Spectrum Cycle | `0x04` | Hurricane | `0x0D` |
| Breathing | `0x05` | Accumulate | `0x0E` |
| Static | `0x06` | Starlight (lento) | `0x0F` |
| Reactive | `0x07` | Visor | `0x10` |
| Reactive Ripple | `0x08` | Rainbow Circle | `0x12` |
| **Custom (per tasto)** | `0x14` | | |

## 4. Colori per tasto (modo Custom `0x14`)

Stessa sequenza con Modo = `0x14`, poi 8 pacchetti `WRITE_COLOR`
(18 LED = 54 byte ciascuno) che riempiono l'intero buffer firmware di
**133 slot** (399 byte), poi END. Gli slot senza tasto restano neri.

## 5. Mappa slot → tasto

Il firmware usa una matrice di **21 colonne**: `slot = riga × 21 + colonna`.
Colonne 0-13 blocco principale, 14-16 blocco di navigazione, 17-20 tastierino.

| Riga | Slot | Tasti |
|------|------|-------|
| 0 | 0–20 | Esc, F1–F12, (13 vuoto), PrtSc, ScrLk, Pause, Calc, Vol-, Vol+, Mute |
| 1 | 21–41 | `` ` ``, 1–0, -, =, Backspace, Ins, Home, PgUp, NumLk, /, *, - |
| 2 | 42–62 | Tab, Q–P, [, ], \, Del, End, PgDn, 7, 8, 9, + |
| 3 | 63–83 | Caps, A–L, ;, ', (75 vuoto), Enter (76), 4, 5, 6 (80–82) |
| 4 | 84–104 | LShift (84), Z–/ (86–95), RShift (97), **Up (99)**, 1, 2, 3 (101–103), **Num Enter (104)** |
| 5 | 105–125 | LCtrl 105, LWin 106, LAlt 107, Space 108, **RAlt 112, RWin 113, Fn 114, RCtrl 115**, **Left 119, Down 120, Right 121**, Num0 **122+123** (due LED), Num. 124 |

Le voci in grassetto sono state corrette il 2026-11-05 (prima erano spostate:
Fn e Ctrl destro non si accendevano). Verifica fatta accendendo gli slot con
colori diversi e fotografando la tastiera.

## 6. Come si ricostruisce la mappa su un'altra tastiera

1. Entra in modo Custom e accendi **un solo slot** (`test_led_probe.py single N`).
2. Oppure accendi più slot insieme, ognuno di un colore diverso, e fotografa.
3. Applica la regola `slot = riga × 21 + colonna` e controlla le colonne vuote.
