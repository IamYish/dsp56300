# DSP56300 Sampler Architecture Plan

## Overview

Build a 90s-era hardware sampler where **all audio processing runs as DSP56300 assembly
inside the emulator**. The C++ host handles only non-audio tasks: file I/O, MIDI,
sample loading, and GUI. This mirrors the two-processor architecture of real hardware
(MCU + DSP), with our C++ host replacing the MCU.

No production sampler ever shipped on the DSP56300 (it was a VA synth chip), so this is
a new firmware design using the existing emulator infrastructure.

---

## Phase 1: Toolchain & Hello World

**Goal:** Get DSP56300 assembly compiling and running inside the emulator, outputting a
sine wave through ESAI.

### Step 1.1: Opcode Assembler Infrastructure

We cannot rely on the proprietary Motorola `asm56300` assembler. Instead, build a
lightweight **compile-time opcode builder** in C++ that:

- Uses the existing `OpcodeInfo` and `FieldInfo` infrastructure to encode instructions
- Provides a fluent API: `asm.mac(x0, y0, a)`, `asm.move_to_esai(a, TX0)`, etc.
- Writes encoded 24-bit words directly to `Memory::set(MemArea_P, addr, opcode)`
- Uses the existing `Disassembler` class to verify generated code

This avoids external toolchain dependencies while leveraging the opcode tables already
in the codebase (390+ instructions fully defined with bit patterns).

**Alternative:** If an open-source assembler (`asm56k` from GitHub) works, we can write
`.asm` files and load via `OMFLoader`. We should evaluate both paths.

### Step 1.2: Minimal Boot Program

Write a DSP program that:

1. Boots at P:$000000
2. Sets up interrupt vector table (ESAI TX at vector $38, HDI08 RX at $60)
3. Initializes ESAI for stereo I2S output at 48kHz (TX0=left, TX1=right)
4. Generates a 440Hz sine wave from a 256-entry lookup table in Y memory
5. Outputs samples via ESAI TX interrupt handler
6. Main loop: idle, waiting for interrupts

### Step 1.3: Host Integration

C++ host code that:

1. Creates `Memory` + `Peripherals56362` + `DSP` instances
2. Loads the boot program into P memory
3. Runs `dsp.exec()` on a dedicated thread
4. Reads stereo audio from ESAI TX ring buffer
5. Writes audio to the system audio output (JUCE AudioCallback)

**Deliverable:** A working JUCE plugin that outputs a sine wave generated entirely by
DSP56300 assembly running inside the emulator.

---

## Phase 2: Sample Playback Engine (DSP Assembly)

**Goal:** Play back samples from DSP memory with pitch control and interpolation.

### Step 2.1: Sample Memory Layout

```
X Memory Map:
  $000000-$0000FF  Voice state table (256 words, 8 voices × 32 words each)
  $000100-$0001FF  Global parameters (HDI08 command buffer, mixer state)
  $000200-$01FFFF  Sample data bank 0 (128K words = ~2.7 seconds mono @ 48kHz)

Y Memory Map:
  $000000-$0000FF  Lookup tables (sine, exponential, log)
  $000100-$0001FF  Filter coefficients (per-voice)
  $000200-$01FFFF  Sample data bank 1 (for stereo or more samples)

P Memory Map:
  $000000-$0000FF  Interrupt vectors + boot code
  $000100-$003FFF  Main program (~16K instructions)
```

### Step 2.2: Per-Voice State Block (32 words in X memory)

```
Offset  Name              Description
0       SAMPLE_BASE       Base address of sample data in X/Y memory
1       SAMPLE_END        End address (or length)
2       LOOP_START        Loop start address
3       LOOP_END          Loop end address
4       PHASE_INT         Integer part of playback position
5       PHASE_FRAC        Fractional part (24-bit, sub-sample)
6       PHASE_INC_INT     Integer part of playback increment
7       PHASE_INC_FRAC    Fractional part of increment (pitch)
8       AMP_EG_VALUE      Current amplitude envelope value
9       AMP_EG_RATE       Current envelope rate (attack/decay/release)
10      AMP_EG_TARGET     Envelope target for current stage
11      AMP_EG_STAGE      Stage: 0=idle,1=attack,2=decay,3=sustain,4=release
12      FILT_B0           Filter coefficient b0
13      FILT_B1           Filter coefficient b1
14      FILT_B2           Filter coefficient b2
15      FILT_A1           Filter coefficient a1
16      FILT_A2           Filter coefficient a2
17      FILT_Z1           Filter state z1
18      FILT_Z2           Filter state z2
19      GAIN_L            Left channel gain (pan * volume)
20      GAIN_R            Right channel gain
21      VOICE_FLAGS       Bit flags: active, looping, loop_type, stereo
22      LOOP_TYPE         0=no loop, 1=forward, 2=backward, 3=pingpong
23      DIRECTION         Playback direction (for pingpong)
24-31   RESERVED          Future: LFO state, mod matrix, etc.
```

### Step 2.3: Audio Processing Loop (ESAI TX ISR)

Per audio frame (called at 48kHz via ESAI TX interrupt):

```
esai_tx_isr:
    ; Clear output accumulators
    clr   a                         ; a = left channel sum
    clr   b                         ; b = right channel sum

    ; Point to voice table base
    move  #VOICE_TABLE,r0
    move  #VOICE_STRIDE,n0          ; 32 words per voice

    ; Process all voices
    do    #MAX_VOICES,voice_loop_end

        ; Check if voice is active
        move  x:(r0+VOICE_FLAGS),x0
        jclr  #0,x0,skip_voice      ; bit 0 = active flag

        ; --- Sample Playback with Linear Interpolation ---
        ; Load phase accumulator
        move  x:(r0+PHASE_INT),r1      ; integer position -> address reg
        move  x:(r0+PHASE_FRAC),y0     ; fractional position

        ; Read two adjacent samples
        move  x:(r1),x0                ; sample[n]
        move  x:(r1+1),x1              ; sample[n+1]

        ; Interpolate: out = s[n] + frac * (s[n+1] - s[n])
        sub   x0,x1                    ; x1 = s[n+1] - s[n]
        move  x1,y1
        mac   y0,y1,x0                 ; x0 = s[n] + frac * diff

        ; --- Advance phase ---
        move  x:(r0+PHASE_INC_FRAC),y1
        add   y1,y0                    ; frac += inc_frac
        ; Handle overflow (carry into integer part)
        jcc   no_carry
        move  x:(r0+PHASE_INT),x1
        move  x:(r0+PHASE_INC_INT),y1
        add   y1,x1
        move  #1,y1
        add   y1,x1                   ; int += inc_int + 1
        move  x1,x:(r0+PHASE_INT)
        jmp   phase_done
    no_carry:
        move  x:(r0+PHASE_INT),x1
        move  x:(r0+PHASE_INC_INT),y1
        add   y1,x1                   ; int += inc_int
        move  x1,x:(r0+PHASE_INT)
    phase_done:
        move  y0,x:(r0+PHASE_FRAC)    ; store updated frac

        ; --- Check loop/end ---
        ; (loop handling code here)

        ; --- Biquad Filter (Transposed Direct Form II) ---
        ; in = x0 (from interpolation above)
        move  x:(r0+FILT_B0),y0
        move  x:(r0+FILT_Z1),y1
        mac   y0,x0,y1                ; out = b0*in + z1
        ; (store out, compute new z1, z2)

        ; --- Amplitude Envelope ---
        move  x:(r0+AMP_EG_VALUE),y0
        ; (advance envelope per stage)

        ; --- Mix to output ---
        move  x:(r0+GAIN_L),y1
        mac   y0,y1,a                 ; left += sample * envelope * gain_L
        move  x:(r0+GAIN_R),y1
        mac   y0,y1,b                 ; right += sample * envelope * gain_R

    skip_voice:
        lua   (r0)+n0,r0              ; advance to next voice block

    voice_loop_end:

    ; --- Write stereo output to ESAI ---
    movep a,x:<<M_TX0               ; Left channel
    movep b,x:<<M_TX1               ; Right channel

    rti
```

### Step 2.4: Host-to-DSP Communication (HDI08 Protocol)

Define a simple command protocol over HDI08:

```
Command Format: [CMD_WORD] [DATA_WORD_0] [DATA_WORD_1] ...

Commands:
  $010000  NOTE_ON    voice# key velocity
  $020000  NOTE_OFF   voice# key velocity
  $030000  CC_CHANGE  cc# value
  $040000  PITCH_BEND value
  $050000  LOAD_SAMPLE voice# base_addr length loop_start loop_end
  $060000  SET_PARAM  voice# param_offset value
```

The DSP's HDI08 RX interrupt handler parses commands and updates voice state.

---

## Phase 3: Envelopes, Filters & Modulation (DSP Assembly)

### Step 3.1: ADSR Envelope Generator

Per-voice, computed every sample in the ESAI ISR:

```
envelope_process:
    move  x:(r0+AMP_EG_STAGE),x0    ; load stage
    move  x:(r0+AMP_EG_VALUE),a     ; load current value
    move  x:(r0+AMP_EG_RATE),y0     ; load rate
    move  x:(r0+AMP_EG_TARGET),y1   ; load target

    ; Stage dispatch (Attack=1, Decay=2, Sustain=3, Release=4)
    cmp   #1,x0
    jeq   env_attack
    cmp   #2,x0
    jeq   env_decay
    ; ... etc

env_attack:
    add   y0,a                       ; value += attack_rate
    cmp   y1,a                       ; reached target (1.0)?
    jlt   env_store
    move  y1,a                       ; clamp to 1.0
    move  #2,x0                      ; advance to decay stage
    move  x0,x:(r0+AMP_EG_STAGE)
    ; load decay rate and target...
    jmp   env_store

env_store:
    move  a,x:(r0+AMP_EG_VALUE)
```

### Step 3.2: Biquad Filter (Full Implementation)

Transposed Direct Form II, single biquad section:

```
; Input in x0, coefficients and state at voice offset
; out = b0*in + z1
; z1  = b1*in - a1*out + z2
; z2  = b2*in - a2*out

biquad:
    move  x:(r0+FILT_B0),y0
    move  x:(r0+FILT_Z1),a
    mac   x0,y0,a                    ; a = b0*in + z1 (this is output)

    ; Save output for later
    move  a,x1                       ; x1 = output

    ; z1 = b1*in - a1*out + z2
    move  x:(r0+FILT_Z2),a          ; a = z2
    move  x:(r0+FILT_B1),y0
    mac   x0,y0,a                    ; a = z2 + b1*in
    move  x:(r0+FILT_A1),y0
    macr  -x1,y0,a                   ; a = z2 + b1*in - a1*out
    move  a,x:(r0+FILT_Z1)          ; store new z1

    ; z2 = b2*in - a2*out
    move  x:(r0+FILT_B2),y0
    mpy   x0,y0,a                    ; a = b2*in
    move  x:(r0+FILT_A2),y0
    macr  -x1,y0,a                   ; a = b2*in - a2*out
    move  a,x:(r0+FILT_Z2)          ; store new z2

    move  x1,x0                      ; output in x0
```

Filter coefficient calculation happens on the C++ host side (requires trig functions)
and is sent to DSP via HDI08 `SET_PARAM` commands.

### Step 3.3: LFO (Triangle Wave)

Simple phase-accumulator triangle LFO per voice:

```
lfo_process:
    move  x:(r0+LFO_PHASE),a
    move  x:(r0+LFO_RATE),y0
    add   y0,a                       ; phase += rate
    ; Wrap at 1.0 (represented as $7FFFFF)
    ; Triangle: output = |4*phase - 2| - 1
    ; (Using absolute value and scaling)
```

### Step 3.4: Modulation Routing

Filter cutoff modulation (envelope + LFO → cutoff):

```
; Compute modulated cutoff
move  x:(r0+FILT_BASE_CUTOFF),a     ; base cutoff
move  x:(r0+FILT_EG_VALUE),y0       ; filter envelope
move  x:(r0+FILT_EG_DEPTH),y1       ; envelope depth
mac   y0,y1,a                        ; cutoff += eg * depth
move  x:(r0+LFO_OUTPUT),y0          ; LFO value
move  x:(r0+FILT_LFO_DEPTH),y1      ; LFO depth
mac   y0,y1,a                        ; cutoff += lfo * depth
; Send updated cutoff to host for coefficient recalculation
; OR use a lookup table for fast coefficient approximation
```

---

## Phase 4: Effects Processing (DSP Assembly)

### Step 4.1: Reverb (8 Comb + 4 Allpass, Freeverb-style)

Uses modulo addressing for circular buffers:

```
; Set up comb filter circular buffer
move  #COMB0_BASE,r4
move  #COMB0_SIZE-1,m4               ; modulo addressing

; Comb filter with damping
move  x:(r4),y0                       ; read delayed sample
move  x:(r4+COMB0_DAMP_STORE),y1     ; previous filtered
mpy   y0,#damp2,a                     ; a = delayed * (1-damp)
mac   y1,#damp1,a                     ; a += prev * damp
move  a,x:(r4+COMB0_DAMP_STORE)      ; store filtered
mac   a,#feedback,input               ; input += filtered * feedback
move  input,x:(r4)+                   ; write to buffer, advance
```

### Step 4.2: Delay Line

Simple feedback delay with modulo buffer.

### Step 4.3: Master EQ

3-band parametric EQ using biquad sections (same biquad code as voice filter).

---

## Phase 5: C++ Host Integration

### Step 5.1: SFZ Parser (existing code, refactored)

Keep existing SFZ parsing in C++. Convert parsed regions to DSP-compatible format:
- Convert sample data from float to 24-bit fixed-point ($7FFFFF = +1.0)
- Upload sample data to DSP X/Y memory via `memory.set()` or HDI08 DMA
- Convert filter/envelope parameters to DSP fixed-point format

### Step 5.2: MIDI → HDI08 Bridge

Translate MIDI events to HDI08 command words:
- Note On → `NOTE_ON voice# key velocity`
- Note Off → `NOTE_OFF voice# key velocity`
- CC → `CC_CHANGE cc# value`
- Pitch Bend → `PITCH_BEND value`

### Step 5.3: Voice Allocator (C++ Host)

Voice allocation stays on the host side:
- Track which DSP voices are active
- Assign incoming notes to free voices
- Handle voice stealing (send NOTE_OFF + NOTE_ON)
- Group management for SFZ groups/exclusive classes

### Step 5.4: Audio Output Bridge

Read stereo audio from ESAI TX buffer → deliver to JUCE audio callback:
- `esai.readTX()` returns 24-bit samples
- Convert to float: `sample / 8388608.0f`
- Deliver to DAW via `AudioProcessorEditor`

---

## Phase 6: Lookup Tables & Math

### Step 6.1: Sine Table (256 entries in Y memory)

Pre-computed at startup, loaded into Y:$000000-$0000FF:
```
For i = 0..255:
    Y[i] = sin(2π * i / 256) * $7FFFFF   ; 24-bit fixed-point
```

Used by LFO sine generation and pan law.

### Step 6.2: Pitch Table

Pre-computed semitone-to-rate conversion:
```
For semitone = -128..+127:
    table[semitone+128] = 2^(semitone/12) * $400000  ; fixed-point rate
```

Eliminates the need for `pow(2, x)` on the DSP.

### Step 6.3: Exponential/Log Tables

For envelope curves and dB-to-linear conversion.

---

## File Structure

```
source/
  dsp56kSampler/           # NEW: The sampler project
    CMakeLists.txt
    dspProgram/
      assembler.h          # C++ opcode builder using FieldInfo system
      assembler.cpp
      samplerProgram.h     # The DSP assembly program (built programmatically)
      samplerProgram.cpp
      lookupTables.h       # Sine, pitch, exp tables
      lookupTables.cpp
    host/
      dspHost.h            # Creates DSP + Memory + Peripherals, runs exec loop
      dspHost.cpp
      hdi08Protocol.h      # Command definitions for host<->DSP communication
      midiToDsp.h          # MIDI event → HDI08 command translation
      midiToDsp.cpp
      voiceAllocator.h     # Voice assignment on host side
      voiceAllocator.cpp
      sampleLoader.h       # WAV/SFZ → DSP memory upload
      sampleLoader.cpp
    plugin/
      pluginProcessor.h    # JUCE AudioProcessor (reads ESAI output)
      pluginProcessor.cpp
      pluginEditor.h       # GUI
      pluginEditor.cpp
```

---

## Implementation Order

1. **Phase 1** - Toolchain + sine wave output (proves the pipeline works)
2. **Phase 2** - Sample playback with pitch + interpolation (core sampler function)
3. **Phase 3** - Envelopes + filters (makes it sound like a real sampler)
4. **Phase 4** - Effects (reverb, delay)
5. **Phase 5** - Full SFZ integration with host
6. **Phase 6** - Lookup tables and optimization

Each phase produces a working, testable artifact. Phase 1 is the critical proof-of-concept.

---

## Key Design Decisions

1. **Opcode builder vs external assembler:** Start with C++ opcode builder (no external
   deps), evaluate external assembler later if maintaining raw opcodes becomes unwieldy.

2. **Fixed-point arithmetic:** All DSP math uses 24-bit fractional format
   ($7FFFFF = +0.999999, $800000 = -1.0). The 56-bit accumulator prevents overflow
   during mixing.

3. **Filter coefficients computed on host:** Trig functions (sin, cos, sqrt) needed for
   biquad coefficients are too expensive on DSP. Host computes them and sends via HDI08.

4. **Voice count target:** 32 voices with filter + envelope at 48kHz. The DSP56362 at
   120MHz gives ~2500 cycles per frame. At ~30 cycles per voice, 32 voices uses ~960
   cycles, leaving ~1500 for effects and overhead.

5. **Sample memory:** Use bridged X/Y memory for samples. At 24-bit words, 128K words
   = ~2.7 seconds of mono audio at 48kHz per bank. For longer samples, use DMA
   streaming from extended P memory.
