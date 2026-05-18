# Technisches Referenzblatt: HSTX & PIO Synchronisation (RP2350)

Dieses Dokument beschreibt die Implementierung eines hochpräzisen Trigger-Systems für SDI-Kabeltests unter Verwendung der neuen Hardware-Features des Raspberry Pi Pico 2 (RP2350).

## 1\. Das HSTX\-Modul \(High\-Speed Transmission\)

Das HSTX-Modul ist ein spezialisierter Hardware-Block des RP2350, der Bit-Muster mit Systemtakt (bis zu 150 MHz oder mehr) ohne CPU-Intervention ausgeben kann.

### Warum HSTX für den Trigger?

* **Jitter-frei:** Die Signalausgabe ist direkt an den Takt gekoppelt.

* **Steile Flanken:** Perfekt für die Erzeugung von Impulsen im Nanosekundenbereich.

* **Bit-Ebene:** Wir können die Pulsweite exakt über ein Hex-Muster steuern (z. B. `0x3` für einen 13,3 ns Puls bei 150 MHz).

- - -

## 2\. PIO\-Synchronisations\-Logik

Damit die Zeitmessung (TDR) exakt ist, muss die PIO State Machine (SM) im selben Moment starten, in dem der HSTX-Puls den Pin verlässt.

### Der Mechanismus

1. **HSTX-Ausgang (GPIO 19, D0-):** Sendet den physischen Puls ins Kabel.

2. **PIO-Eingang (GPIO 19):** Die PIO nutzt denselben Pin als internen Trigger (`wait 1 gpio 19`).

3. **Vorteil:** Die Latenz zwischen Senden und Messstart ist konstant und entspricht exakt einem Taktzyklus der Hardware-Logik.

- - -

## 3\. Vollständiger Code\-Entwurf

### A. PIO Programm (`tdr_sync.pio`)

Speichere dies als `tdr_sync.pio`.

<span _ngcontent-ng-c1769569050="" class="ng-tns-c1769569050-180">Code-Snippet</span>

<br>
```
`.program tdr_sync

; PIO Programm für synchronisierte TDR-Messung
; Trigger: GPIO 19 (HSTX Ausgang)
; Echo: GPIO 3 (Komparator Eingang)

public start:
    set x, 0            ; Zähler auf 0 setzen
    pull block          ; Warte auf Signal von der CPU (Arming)

    wait 1 gpio 19      ; Warte auf den HSTX-Puls (Startschuss)

timer_loop:
    jmp pin, found      ; Wenn Echo (GPIO 3) HIGH ist -> Sprung zu found

    ; Inkrement-Logik für den Zähler X
    mov x, !x           ; Invertiere X
    jmp x-- next        ; Dekrementiere (effektiv Inkrement wegen Invertierung)
next:
    mov x, !x           ; Zurück-Invertieren

    jmp timer_loop      ; Weiterzählen bis Echo eintrifft

found:
    in x, 32            ; Schiebe Zählerstand in das FIFO
    push block          ; Sende Daten an Core 1 / CPU
`
```
<br>
<br>
<br>
### B. C++ Implementierung (`main.cpp`)

<span _ngcontent-ng-c1769569050="" class="ng-tns-c1769569050-181">C++</span>

<br>
```
`<span class="hljs-meta">#<span class="hljs-meta-keyword">include</span> <span class="hljs-meta-string">\<stdio.h></span></span><span class="hljs-meta">#<span class="hljs-meta-keyword">include</span> <span class="hljs-meta-string">"pico/stdlib.h"</span></span><span class="hljs-meta">#<span class="hljs-meta-keyword">include</span> <span class="hljs-meta-string">"hardware/pio.h"</span></span><span class="hljs-meta">#<span class="hljs-meta-keyword">include</span> <span class="hljs-meta-string">"hardware/hstx.h"</span></span><span class="hljs-meta">#<span class="hljs-meta-keyword">include</span> <span class="hljs-meta-string">"tdr\_sync.pio.h"</span></span><span class="hljs-comment">// Pin Definitionen</span><span class="hljs-meta">#<span class="hljs-meta-keyword">define</span> TRIGGER\_PIN 2</span><span class="hljs-meta">#<span class="hljs-meta-keyword">define</span> ECHO\_PIN 3</span><span class="hljs-comment">/**
 * Initialisiert den HSTX Block für GPIO 19
 */</span><span class="hljs-function"><span class="hljs-keyword">void</span> <span class="hljs-title">init\_hstx\_trigger</span><span class="hljs-params">()</span> </span>{
    gpio_set_function(TRIGGER_PIN, GPIO_FUNC_HSTX);

     <span class="hljs-comment">// HSTX Konfiguration: Systemtakt nutzen</span>
    hstx_hw->csr = (<span class="hljs-number">0</span>  << HSTX_CSR_CLKDIV_LSB) | HSTX_CSR_EN_BITS;

     <span class="hljs-comment">// Pin 2 mit dem HSTX-FIFO verbinden</span>
    hstx_hw->bit[TRIGGER_PIN] = (HSTX_BIT_SRC_FIFO << HSTX_BIT_SRC_LSB) | (<span class="hljs-number">0</span>  << HSTX_BIT_SEL_LSB);
}

<span class="hljs-comment">/**
 * Führt eine synchronisierte Messung durch
 */</span><span class="hljs-function"><span class="hljs-keyword">void</span> <span class="hljs-title">run\_measurement</span><span class="hljs-params">(PIO pio, uint sm)</span> </span>{
     <span class="hljs-comment">// 1. PIO scharfschalten</span>
    pio_sm_put_blocking(pio, sm,  <span class="hljs-number">0xFFFFFFFF</span>); 

     <span class="hljs-comment">// 2. HSTX Trigger senden (Puls von 2 Bits = 13.3ns bei 150MHz)</span>
    hstx_hw->fifo =  <span class="hljs-number">0x00000003</span>; 

     <span class="hljs-comment">// 3. Ergebnis abholen</span>
     <span class="hljs-keyword">uint32\_t</span>  cycles = pio_sm_get_blocking(pio, sm);

     <span class="hljs-comment">// 4. Distanzberechnung</span>
     <span class="hljs-comment">// v\_p = 0.66 (VOP), c = 299792458 m/s, Takt = 150MHz</span>
     <span class="hljs-comment">// Distanz = (Zyklen \* (1/Takt) \* c \* v\_p) / 2</span>
     <span class="hljs-keyword">float</span>  distance = (cycles *  <span class="hljs-number">4.0f</span>  * (<span class="hljs-number">1.0f</span>  /  <span class="hljs-number">150e6</span>) *  <span class="hljs-number">299792458.0f</span>  *  <span class="hljs-number">0.66f</span>) /  <span class="hljs-number">2.0f</span>;

     <span class="hljs-built_in">printf</span>(<span class="hljs-string">"Messung abgeschlossen: %u Zyklen -> %.2f Meter\n"</span>, cycles, distance);
}

<span class="hljs-function"><span class="hljs-keyword">int</span> <span class="hljs-title">main</span><span class="hljs-params">()</span> </span>{
    stdio_init_all();
    init_hstx_trigger();

     <span class="hljs-comment">// PIO Setup (Standard-Prozedur)</span>
    PIO pio = pio0;
    uint offset = pio_add_program(pio, &tdr_sync_program);
    uint sm = pio_claim_unused_sm(pio,  <span class="hljs-literal">true</span>);
    pio_sm_config c = tdr_sync_program_get_default_config(offset);
    sm_config_set_jmp_pin(&c, ECHO_PIN);
    pio_sm_init(pio, sm, offset, &c);
    pio_sm_set_enabled(pio, sm,  <span class="hljs-literal">true</span>);

     <span class="hljs-keyword">while</span>  (<span class="hljs-literal">true</span>) {
        run_measurement(pio, sm);
        sleep_ms(<span class="hljs-number">1000</span>);
    }
}
`
```
<br>
<br>
<br>
- - -

## 4\. Prämissen & Einschränkungen

1. **Kalibrierung:** Der PIO-Loop benötigt mehrere Takte pro Durchlauf (im Code mit `4.0f` angenommen). Dieser Wert muss durch Messung eines Kabels bekannter Länge exakt kalibriert werden.

2. **Hardware-Pegel:** Der Trigger-Puls (3.3V) sollte über einen 75-Ohm Widerstand eingekoppelt werden.

3. **Kabellänge:** Sehr kurze Kabel (< 2m) liegen eventuell innerhalb der "Blindzone" der PIO-Reaktionszeit.