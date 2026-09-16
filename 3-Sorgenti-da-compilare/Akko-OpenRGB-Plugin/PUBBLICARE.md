# 📦 Come pubblicare il plugin — SOLO dal sito web di GitHub (senza terminale)

Guida passo-passo, tutto dal browser. Nessun comando da digitare.

---

## Cosa ti serve già pronto

La cartella **`DA-CREARE-Release/`** fuori dal repo contiene tutto:

```
DA-CREARE-Release/
├── 1-Linux/          → Akko-OpenRGB-Plugin-linux-x86_64.zip   (già pronto)
├── 2-Windows/        → Akko-OpenRGB-Plugin-windows-x64.zip    (template: la .dll la crea la CI)
└── 3-Sorgenti-da-compilare/
    ├── Akko-OpenRGB-Plugin/                                   ← È QUESTA la cartella da caricare su GitHub
    └── Akko-OpenRGB-Plugin-sorgenti.zip                       (da allegare alla Release)
```

La cartella `Akko-OpenRGB-Plugin/` dentro `3-Sorgenti-da-compilare/` contiene
**solo i sorgenti** (niente `build/`, niente `.zip`): è esattamente quello che
va caricato su GitHub.

---

## FASE 1 — Crea il repository (2 minuti)

1. Vai su **github.com** e fai il login.
2. In alto a destra: icona **+** → **New repository**.
3. **Repository name**: `Akko-OpenRGB-Plugin`
4. **Description** (facoltativa): `OpenRGB plugin – Akko 3108 V2 driver + per-key editor`
5. Scegli **Public**.
6. IMPORTANTE: **NON** spuntare *Add a README file* / *Add .gitignore* /
   *Choose a license* (li hai già nei sorgenti).
7. Clicca **Create repository**.

> Ora vedi una pagina vuota con scritto *"Quick setup"*. Non chiuderla:
> la useremo alla Fase 2.

---

## FASE 2 — Carica i sorgenti (trascina e rilascia)

1. Apri la cartella sul tuo PC:
   `Scrivania/Reverse Engineering per LED Akko su Linux/DA-CREARE-Release/3-Sorgenti-da-compilare/`
2. Entra nella cartella **`Akko-OpenRGB-Plugin`**.
3. Premi **Ctrl+H** nel file manager per mostrare i file nascosti
   (devi vedere anche `.github` e `.gitignore`).
4. Seleziona **TUTTO** il contenuto (tutto quello che vedi, compresi i
   nascosti): più semplice: **Ctrl+A** dentro quella cartella.
5. Trascina il tutto dentro la pagina di GitHub, nel riquadro
   **"uploading an existing file"** (oppure usa **Add file → Upload files**).
6. Aspetta che GitHub mostri la lista dei file con le cartelle
   `src/`, `sdk/`, `packaging/`, `.github/`… (controlla che ci sia tutto).
7. Sotto, nel riquadro *Commit changes*, scrivi un messaggio:
   `Import sorgenti v1.0.0`
8. Lascia *Commit directly to the main branch*.
9. Clicca **Commit changes**.

> Verifica: apri la tab **Code** → devono esserci `CMakeLists.txt`,
> `README.md`, `LICENSE`, `PUBBLICARE.md` e le cartelle `src/` e `sdk/`.

---

## FASE 3 — Crea la Release (questo crea anche il tag → fa partire la CI)

1. Nella pagina del repo, tab **Releases** (a destra).
2. Clicca **Draft a new release**.
3. **Choose a tag**: digita `v1.0.0` e seleziona la voce
   **"Create new tag: v1.0.0 on publish"** (sì: il tag si crea da qui,
   niente terminale).
4. **Target**: lascia `main`.
5. **Release title**: `v1.0.0`
6. **Describe this release**: incolla il testo pronto qui sotto.
7. In basso, **Publish release**.

Testo pronto da incollare nella descrizione:

```
## Akko 3108 V2 – OpenRGB Plugin v1.0.0

Adds a full HID driver + a per-key visual editor ("Akko Editor") for the
Akko 3108 V2 keyboard (VID 0x0C45, PID 0x762B). Works on **any**
OpenRGB 1.0 build, even stock ones without the Akko core driver.

- 108 keys, 6×22 matrix, 19 effects (Custom = per-key)
- Bundled driver: auto-detects the keyboard via HID (interface 1,
  usage page 0xFF1C, EVision protocol)
- No core modification: registers as a virtual RGB controller

### Install
| Platform | Archive | Destination |
|---|---|---|
| Linux | Akko-OpenRGB-Plugin-linux-x86_64.zip | `~/.config/OpenRGB/plugins/` — run `bash install.sh` |
| Windows | Akko-OpenRGB-Plugin-windows-x64.zip | `%APPDATA%\OpenRGB\plugins\` |
| From source | Akko-OpenRGB-Plugin-sorgenti.zip | see README.md |

Requirements: OpenRGB 1.0 built with Qt6. Linux: glibc >= 2.35
(Ubuntu 22.04+, Debian 12+, Fedora 36+, Arch/CachyOS/Manjaro).

⚠️ Windows build is produced automatically by CI (MSVC + Qt6) and is
considered **beta** until tested on real Windows hardware.
```

---

## FASE 4 — Aspetta la CI (automatica)

Appena pubblichi la Release, parte da solo il workflow **Release builds**
(tab **Actions**). Tre lavori: `linux`, `windows`, `release`. ~8-10 minuti.

- I due ZIP (`…-linux-x86_64.zip` e `…-windows-x64.zip`) verranno
  **allegati da soli** alla Release (il terzo lavoro li aggancia).
- Quando `Actions` mostra i quadratini verdi, vai su **Releases**:
  trovi la release con i 2 ZIP già attachati. Puoi scaricarli e provare
  quello Linux su CachyOS.

---

## FASE 5 — Allega a mano lo ZIP dei sorgenti

1. Tab **Releases** → sulla tua release clicca la **matita** (Edit).
2. Nel riquadro *Attach binaries…* trascina:
   `DA-CREARE-Release/3-Sorgenti-da-compilare/Akko-OpenRGB-Plugin-sorgenti.zip`
3. Clicca **Update release**.

---

## FASE 6 — Checklist finale

- [ ] Repository `Akko-OpenRGB-Plugin` pubblico, con i sorgenti
- [ ] In `Code` c'è `.github/workflows/release-builds.yml`
- [ ] Release `v1.0.0` pubblicata (il tag si è creato da solo)
- [ ] Tab `Actions`: 3 lavori verdi
- [ ] Nella Release: ZIP Linux + ZIP Windows (automatici) + ZIP sorgenti (a mano)
- [ ] Prova su CachyOS: estrai lo ZIP Linux, `bash install.sh`, riavvia
      OpenRGB, la tastiera appare e la tab Akko Editor colora i tasti

---

## Se qualcosa va storto

| Problema | Soluzione |
|---|---|
| `Actions` non parte dopo la Release | Il tag non è stato creato: modifica la Release (Edit), nel campo *Choose a tag* ridigita `v1.0.0` e scegli *Create new tag on publish* |
| Un lavoro della CI è rosso | Tab Actions → apri il job → clicca lo step rosso: il log dice perché. |
| Nella Release non ci sono gli ZIP | Riapri la Release con Edit: se la CI è finita verde, salva di nuovo con *Update release* e i file si agganciano |
| Non vedi `.github` dopo il drag&drop | Apri in `Code` la cartella `.github/workflows/`: se manca, creala con **Add file → Create new file**, scrivi il path `release-builds.yml` dentro `.github/workflows/`... e incolla il contenuto del file che hai in `DA-CREARE-Release/3-Sorgenti-da-compilare/Akko-OpenRGB-Plugin/.github/workflows/release-builds.yml` |

---

## (Appendice, facoltativa) Se un giorno vuoi usare il terminale

Solo per completezza — NON necessario:

```bash
cd Akko-OpenRGB-Plugin
git init && git add . && git commit -m "v1.0.0"
git branch -M main
git remote add origin https://github.com/TUO-UTENTE/Akko-OpenRGB-Plugin.git
git push -u origin main
git tag v1.0.0 && git push origin v1.0.0
```