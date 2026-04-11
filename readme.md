# Loudness Leveler

**A C++ audio tool for automated loudness leveling, dynamic range control, and LUFS analysis.**

Loudness Leveler is a desktop application designed to help with vocal volume consistency. Built with podcasts and voiceovers in mind, it combines a **Gain Rider**, a **True Peak Limiter**, and **LUFS analytics** into a simple interface.

**[Download](https://github.com/davidseegert/loudness-leveler/releases/)**

<img width="1604" height="925" alt="screenshot" src="https://github.com/user-attachments/assets/60731a46-7d20-4690-b2e6-c780b3774bbc" />

## Key Features

* **Gain Riding**: Maintains a target loudness level to even out volume differences in spoken word audio.
* **Monitoring**: Real-time LUFS (Integrated/Short-term) and True Peak (TP) meters.
* **Waveform Visualization**: Dual-channel traces with mouse-tracking crosshairs and dB/time readouts.
* **Audio Processing**: Includes filters, a noise gate, and a compressor with parameter smoothing.
* **File Handling**: Drag-and-drop file loading and 32-bit float WAV export.


## Detailed Settings & Controls

Loudness Leveler operates as a sequential DSP chain: **Filters → Noise Gate → Gain Rider → Compressor → Manual Gain → Limiter**.

Controls marked with an asterisk (\*) are available in **Advanced Mode** (Settings > Show Advanced Options).

### Gain Rider
The Gain Rider actively adjusts the volume, boosting quiet sections and taming loud ones.

| Control | Description |
| :--- | :--- |
| **Target** | The desired output loudness level in LUFS. |
| **Range** | The maximum allowed gain boost or reduction (e.g., ±6dB). |
| **Sensitivity** | Adjusts how quickly the rider reacts to speech vs. background noise. |
| **Invert Effect** | Reverses the gain logic. |
| **Attack*** | Speed of gain increases when signal drops below target. |
| **Release*** | Speed of gain decreases when signal exceeds target. |
| **Lookahead*** | Analysis window to anticipate volume changes. |
| **Window*** | The time duration for each loudness calculation step. |

### Noise Gate
Attenuates background noise, breaths, and hum during silent passages.

| Control | Description |
| :--- | :--- |
| **Threshold** | The level below which audio is silenced. |
| **Reduction** | The amount of attenuation applied when the gate is closed. |
| **Attack*** | How quickly the gate opens when speech is detected. |
| **Hold*** | How long the gate stays open after speech stops. |
| **Release*** | How smoothly the gate closes at the end of a phrase. |

### Compressor
Applies standard audio compression to the leveled vocal.

| Control | Description |
| :--- | :--- |
| **Amount** | Scales compression strength from subtle (0) to aggressive (1). |
| **Attack*** | The speed at which the compressor starts reducing volume. |
| **Release*** | The speed at which volume returns to normal. |
| **Ratio*** | The compression ratio (up to 3.5:1 at max 'Amount'). |
| **Threshold*** | The internal threshold used at maximum 'Amount'. |

### Limiter & Output
A peak limiter to prevent digital clipping at the output stage.

| Control | Description |
| :--- | :--- |
| **Ceiling** | The absolute maximum output peak allowed. |
| **Gain** | Manual output gain adjustment (makeup gain). |
| **Lookahead*** | Buffer size used to catch fast transients before they hit the ceiling. |

### EQ & Filters
Basic filtering applied before the signal reaches the gain stages.

* **Low Cut 80Hz**: Removes low-end rumble and "P-pops" (High-pass).
* **Mid Cut 1kHz**: Reduces "boxy" or "nasal" vocal frequencies (Peaking EQ).
* **High Cut 20kHz**: Filters out high-frequency noise (Low-pass).
* **Phase Rotate**: 90° Phase rotation (Hilbert transform) to alter vocal waveform symmetry.

## Visualization

### **Waveform Widget**
* **Dual Traces**: Displays separate waveforms for Left and Right channels in stereo files.
* **Gain Envelope**: Overlays a visual indicator of the gain adjustments being applied.
* **Interactive Crosshair**: Hover to see precise **Time (MM:SS.ms)** and **Amplitude (dB)** at any point.
* **Trace Selection**: Toggle visibility of specific stages (Combined, Gate, Rider, Comp, or Limiter).

### **Loudness Analysis**
The bottom status area provides real-time updates:
* **Integrated LUFS**: Average loudness of the entire file.
* **True Peak (TP)**: Detected using **4x oversampling**.

## Technical Details
* **Language**: **C++17/20**.
* **Audio Threading**: Lock-free real-time processing with atomic synchronization.
* **GUI Framework**: **wxWidgets 3.2+** for the desktop UI.
* **Media Handling**: **FFmpeg** integration for decoding formats like MP3, WAV, and M4A.
* **Smoothing**: Parameter changes are filtered to reduce audio clicks.

## Build & Requirements

### Prerequisites
- **Compiler**: GCC 9+, Clang 10+, or MSVC 2019+.
- **Libraries**: wxWidgets 3.2, ALSA/PulseAudio development headers.
- **Build System**: CMake 3.16+.

### Build Instructions

#### Linux (including ARM)
You can use the provided build script:
```bash
chmod +x build-linux-arm.sh
./build-linux-arm.sh
```

Or build manually:
```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

#### macOS
```bash
chmod +x build-mac.sh
./build-mac.sh
```

## Usage Tip
1. **Load**: Use `File > Load Audio` or drag a file directly into the window.
2. **Configure**: Enable **Effects Active** to hear the processing.
3. **Refine**: Set **Target LUFS** first, then adjust **Compressor Amount**.
4. **Export**: Use `File > Export Audio` to render the processed file as a WAV.


*Loudness Leveler is an open-source side project for processing spoken-word audio.*
