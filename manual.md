# Loudness Leveler - User Manual

Welcome to **Loudness Leveler**, a professional-grade tool designed to automate the process of leveling audio volumes, specifically optimized for spoken-word content like podcasts, interviews, and voiceovers.

## 🚀 Quick Start
1.  **Load Audio**: Click the "Load Audio" button and select your file (MP3, WAV, M4A, etc.).
2.  **Adjust Leveling**: Use the **Target** and **Factor** sliders to reach your desired loudness.
3.  **Refine Sound**: Apply the **Compressor** or **Noise Gate** if needed.
4.  **Export**: Click "Export WAV" to save your processed audio.

---

## 🛠 Features & Controls

### 1. Leveling System
The core of the application is an intelligent segment-based leveler.
*   **Target (dB)**: The destination loudness for your audio. The industry standard for podcasts is typically between **-16 dB** and **-19 dB**.
*   **Factor (0.0 - 1.0)**: Determines how "aggressively" the leveler works. A factor of `1.0` moves every segment exactly to the target. Use a lower factor (e.g., `0.7`) to preserve some natural dynamic range.
*   **Sens (Sensitivity)**: Controls how the software detects "segments" of speech. 
    *   High sensitivity creates more, shorter segments.
    *   Low sensitivity creates fewer, longer segments.

> [!TIP]
> If your audio has different speakers with very different volumes, higher sensitivity helps the leveler treat them individually.

### 2. Dynamics Processing
*   **Comp (Compressor)**: A "one-knob" post-processing compressor. It tightens the dynamics *after* leveling, making the voice sound more consistent and "thick."
*   **Gate (Noise Gate)**:
    *   **Gate (Threshold)**: The level below which audio is considered "noise" and silenced.
    *   **Reduc (Reduction)**: How many decibels to turn down the noise once the gate closes. Setting this to `-100 dB` provides total silence.

### 3. Audio Filters
*   **Low Cut 80Hz**: Removes low-frequency "mud" and rumble (like AC hum or desk bumps). Essential for clean voice recordings (Highpass).
*   **Mid Cut 1kHz**: Reduces "boxy" or "nasal" frequencies common in speech (Peaking EQ).
*   **High Cut 20kHz**: Filters out extreme high-end hiss or digital noise that isn't audible (Lowpass).

### 4. Safety & Export
*   **Limiter**: A safety "ceiling" at **-1.0 dB**. It ensures that no matter how much gain you add, the audio will never "clip" or distort.
*   **Export WAV**: Renders the entire file with all processing applied. The export is high-fidelity 32-bit float.

---

## ⚙️ Background Settings
These settings are "under the hood" constants defined in the software configuration to ensure professional results without overwhelming the user.

### Filter Specifications
*   **Q Factor**: `0.707` (Butterworth). This provides the flattest frequency response without resonance at the cutoff point.

### Noise Gate Technicals
*   **Attack**: `2 ms`. The gate opens almost instantly when you start talking.
*   **Hold**: `100 ms`. Prevents the gate from "chattering" or cutting off the ends of words during short pauses.
*   **Release**: `100 ms`. Gently fades the noise out rather than cutting it abruptly.

### Compressor Technicals
*   **Ratio**: Up to `20:1`. At maximum "Comp" setting, it acts as a strong limiter/compressor.
*   **Makeup Gain**: `0.6 factor`. Automatically compensates for the volume lost during compression.

### Segment Analysis
*   **Min Segment Length**: `0.5 seconds`. Prevents the leveler from creating tiny, jarring volume jumps.
*   **Analysis Window**: `100 ms`. The leveler looks at 100ms chunks to calculate the average loudness of a segment.

### User Interface
*   **Zoom Range**: From `5 seconds` (detailed view) up to `5 minutes` (overview).
*   **Label Visibility**: Segment labels (IDs and gain info) automatically hide when zoomed out beyond `60 seconds` to keep the interface clean.

---

## ⌨️ Shortcuts
*   **Spacebar**: Play / Pause
*   **Click Waveform**: Seek to position
*   **Mouse Wheel (on Slider)**: Precision adjustment of settings

> [!IMPORTANT]
> The processing is **non-destructive**. Your original file is never modified; changes are only applied during playback and export.
