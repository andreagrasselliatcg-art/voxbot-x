# VoxBot - X v2 — by Tune Talks

Manipolatore vocale in tempo reale (VST3 / AU / Standalone) da inserire sul canale della voce: pitch e formant shifting granulare, harmonic shifter, grain alternator, effetti, vocoder robotico, controllo via MIDI fino a 4 voci e un sistema di modulazione a 4 slot.

![Interfaccia](screenshot.png)

## Moduli

### Robot · Preset (knob con la faccia)
Il knob grande sceglie uno degli 8 preset di fabbrica. Ogni robot ha la sua faccia e il suo colore. Si cambia preset anche con ◀ ▶ in alto; **INIT** riporta tutto ai valori neutri.

### Pitch Shifter
| Controllo | Funzione |
|---|---|
| **Pitch** (±24 st) | Cambia l'intonazione senza toccare le formanti |
| **Smooth** (micro-slider tra Pitch e Formant) | Riempie i vuoti tra i grani quando si scende di pitch. A 100% i grani si comportano come un oscillatore in sync: Pitch −12 con Formant a 0 suona come l'originale; per il classico "ottava sotto" abbassa Smooth o sposta Formant di almeno un semitono. A 0% suono metallico/robotico. |
| **Formant** (±24 st) | Sposta le formanti (timbro "più grande/più piccolo") senza cambiare la nota |

### Harmonic Shifter
| Controllo | Funzione |
|---|---|
| **Harmonics** (±8) | Sposta tutta la serie armonica di un passo per scatto, mantenendo la nota. I valori intermedi fanno crossfade. Valori negativi spostano verso il basso (la fondamentale può sparire). A 0 è in bypass. |
| **Ratio** (0–2) | Ampiezza del passo rispetto alla fondamentale: 1 = armoniche intere, 0,5 = mezze armoniche (effetto octaver), 0,25 / 0,75 = due ottave. |
| **FM** | Modulazione di frequenza con l'oscillatore interno agganciato al pitch (e a Ratio): poco = grinta, tanto = synth. |

### Grain Alternator
| Controllo | Funzione |
|---|---|
| **Alternator** (0–24 st) | I grani vengono alternativamente spostati sopra e sotto di questo intervallo. 0 = bypass. |
| **Octave** (−8…+8) | Positivo: ogni quanti cicli (2^n) avviene l'alternanza. Negativo: aggiunge sotto-ottave (−1 / −3 ispessisce le voci sottili, valori molto bassi creano movimento lento tipo LFO). |

### Effetti
**Smear** (loop granulari a intervalli, prima degli algoritmi), **Detune** (pitch shifter granulari lenti → chorus), **Stereo** (allargamento pseudo-stereo a fine catena, i bassi restano mono).

### Robot Vocoder
Il vocoder a 20 bande di VoxBot v1 come modulo: **Robot** dosa quanto vocoder entra nel suono, **Type** sceglie la portante (Robot, Android, Choir Bot, Titan, Alien, Ghost, Disco, 8-Bit). Con una nota MIDI tenuta, la portante suona quella nota.

### MIDI
Il plugin accetta MIDI: in Logic va caricato come **MIDI-controlled Effect** (Audio Units → Tune Talks), in Ableton/FL/Reaper basta mandare una traccia MIDI al plugin.

| Modo | Comportamento |
|---|---|
| **Off** | Il MIDI non cambia il pitch (ma arriva comunque ai modulatori MIDI e ADSR) |
| **Mono** | Una nota alla volta: la voce viene intonata sulla nota suonata. Senza note, voce originale. |
| **Poly** | Come Mono ma fino a 4 voci: accordi e armonizzazioni |
| **M.Gated / P.Gated** | Come Mono/Poly, ma senza note il plugin è muto. Appaiono **Attack** e **Release**. |

**Glide**: portamento tra le note. **M** (MIDI Glide): il glide si applica solo passando da una nota tenuta all'altra (legato).

### Master e globali
**Dry/Wet**, **Output**, **Pitch Detect** (LOW 25–700 Hz, MID 40 Hz–1 kHz consigliato, HIGH 80 Hz–4 kHz), **SNAP** (aggancia Pitch, Formant, Alternator ai semitoni, Harmonics ai passi interi, Ratio a multipli di 0,25). L'indicatore in alto a destra mostra livello d'ingresso, nota MIDI attiva e pitch rilevato.

## Modulazione (4 slot)
Ogni slot ha **ON**, la scelta della sorgente e **ASSIGN**.

1. Accendi lo slot e scegli la sorgente.
2. Premi **ASSIGN**: tutti i knob modulabili si illuminano del colore dello slot.
3. Trascina un knob per impostare quanto quello slot lo modula (±100%); doppio clic azzera. Premi di nuovo ASSIGN per uscire.

Gli anelli colorati attorno ai knob mostrano le modulazioni attive; il puntino bianco mostra il valore modulato in tempo reale.

| Sorgente | Controlli |
|---|---|
| **Meta Knob** | Valore, polarità (+ / − / B bipolare), Steps (quantizza a 2…128 valori) — perfetto da automatizzare per muovere più parametri insieme |
| **MIDI** | Note (C3 = 0, 1 per ottava), Velocity, Pitch Wheel, Mod Wheel, After Touch |
| **Sequencer** | 16 step disegnabili (Shift = quarti, Alt = dodicesimi per melodie, Cmd/Ctrl = azzera, doppio clic = azzera step), 100 pattern + **RNDM** (nuovo pattern casuale a ogni giro), **SYNC** al tempo del DAW, **RETRIG** a ogni nota MIDI, Rate, **Shape** (0 curve morbide, 1 linee, 2 gradini, 3 "drop" a ogni step, 4 drop curvi) |
| **Follower** | Segue il volume della voce (Attack / Release) |
| **ADSR** | Inviluppo attivato dalle note MIDI |

**Cross Mod**: i 3 mini-knob in fondo a ogni slot fanno sì che gli altri slot scalino la profondità di questo (modulare un modulatore).

## Note tecniche
- Latenza fissa di 80 ms, compensata automaticamente dal DAW.
- Con tutti gli algoritmi a zero il segnale passa inalterato.
- Circa 9% di un core nel caso peggiore (4 voci + tutti gli effetti).

## Installazione (Mac)
Scarica `VoxBot - X Installer.pkg` dalla pagina **Releases** e aprilo (clic destro → Apri la prima volta: non è firmato da Apple).

## Compilazione
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```
Ogni push su GitHub compila automaticamente Mac (Apple Silicon + Intel) e Windows e pubblica l'installer `.pkg` nella Release.
