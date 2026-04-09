# Loudness Leveler

**A lightweight C++ audio post-production tool for automated loudness leveling, dynamic range control, and industry-standard LUFS analysis.**

Loudness Leveler is a high-performance desktop application designed for creators who need broadcast-quality vocal consistency without the "squashed" sound of traditional heavy compression. Optimized for podcasts, interviews, and voiceovers, it combines an intelligent **Gain Rider**, a **True Peak Limiter**, and **ITU-R BS.1770-5 compliant analytics** into a streamlined, intuitive interface.

<img width="1604" height="925" alt="screenshot" src="https://github.com/user-attachments/assets/0fc58e8d-ae84-49d4-b1c0-f42f31b21827" />


---

## Key Features

* **Intelligent Gain Riding**: Automatically maintains a target loudness level while preserving natural performance dynamics.
* **Precision Meters**: Real-time ITU-R BS.1770-5 LUFS (Integrated/Short-term) and True Peak (TP) monitoring.
* **Stereo Waveform Visualization**: Dual-channel traces with interactive mouse-tracking crosshairs and real-time dB/time readouts.
* **Transparent DSP**: High-quality filters, noise gate, and compressor with zero-artifact one-pole smoothing.
* **Professional Workflow**: Drag-and-drop file loading, system menubar for settings management, and high-fidelity 32-bit float export.

---

## Detailed Settings & Controls

Loudness Leveler operates as a sequential DSP chain: **Filters → Noise Gate → Gain Rider → Compressor → Manual Gain → Limiter**.

### Gain Rider (The Core Engine)
The Gain Rider actively "rides" the volume, boosting quiet sections and taming loud ones.

| Control | Description |
| :--- | :--- |
| **Target** | The desired output loudness level in LUFS. |
| **Range** | The maximum allowed gain boost or reduction (e.g., ±6dB). |
| **Sensitivity** | Adjusts how quickly the rider reacts to speech vs. background noise. |
| **Invert Effect** | Reverses the gain logic (useful for creative sound design). |
| **Attack*** | Speed of gain increases when signal drops below target. |
| **Release*** | Speed of gain decreases when signal exceeds target. |
| **Lookahead*** | Analysis window to anticipate volume changes (prevents overshoots). |
| **Window*** | The time duration for each loudness calculation step. |

### Noise Gate
Eliminates background noise, breaths, and hum during silent passages.

| Control | Description |
| :--- | :--- |
| **Threshold** | The level below which audio is silenced. |
| **Reduction** | The amount of attenuation applied when the gate is closed. |
| **Attack*** | How quickly the gate opens when speech is detected. |
| **Hold*** | How long the gate stays open after speech stops. |
| **Release*** | How smoothly the gate closes at the end of a phrase. |

### Compressor
Adds "punch" and consistency to the leveled vocal.

| Control | Description |
| :--- | :--- |
| **Amount** | Scales compression strength from subtle (0) to aggressive (1). |
| **Attack*** | The speed at which the compressor starts reducing volume. |
| **Release*** | The speed at which volume returns to normal. |
| **Ratio*** | The compression ratio (up to 3.5:1 at max 'Amount'). |
| **Threshold*** | The internal threshold used at maximum 'Amount'. |

### Limiter & Output
The final safety stage to prevent digital clipping and ensure 0dBFS compliance.

| Control | Description |
| :--- | :--- |
| **Ceiling** | The absolute maximum output peak allowed. |
| **Gain** | Manual output gain adjustment (makeup gain). |
| **Lookahead*** | Buffer size used to catch fast transients before they hit the ceiling. |

### EQ & Filters
Pre-processing filters to clean up the signal before it hits the gain stages.

* **Low Cut 80Hz**: Removes low-end rumble and "P-pops" (High-pass).
* **Mid Cut 1kHz**: Reduces "boxy" or "nasal" vocal frequencies (Peaking EQ).
* **High Cut 20kHz**: Filters out ultrasonic hiss or digital noise (Low-pass).
* **Phase Rotate**: 90° Phase rotation (Hilbert transform) to improve vocal symmetry.

---

## Visual Monitoring

### **Waveform Widget**
* **Dual Traces**: Displays separate waveforms for Left and Right channels in stereo files.
* **Gain Envelope**: Overlays a visual indicator of the gain adjustments being applied.
* **Interactive Crosshair**: Hover to see precise **Time (MM:SS.ms)** and **Amplitude (dB)** at any point.
* **Trace Selection**: Toggle visibility of specific stages (Combined, Gate, Rider, Comp, or Limiter).

### **Loudness Analysis**
The bottom status area provides real-time updates:
* **Integrated LUFS**: Average loudness of the entire file.
* **True Peak (TP)**: Detected using **4x oversampling** to ensure inter-sample accuracy.

---

## Technical Highlights
* **Language**: Modern **C++17/20** for high-performance DSP.
* **DSP Architecture**: Lock-free real-time processing with atomic synchronization.
* **GUI Framework**: **wxWidgets 3.2+** for a native, responsive desktop experience.
* **Engine**: **FFmpeg** integration for robust decoding of MP3, WAV, M4A, and more.
* **Smoothing**: Every parameter change is filtered to prevent clicks or "zipper" noise.

---

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

---

## Usage Tip
1. **Load**: Use `File > Load Audio` or drag a file directly into the window.
2. **Configure**: Enable **Effects Active** to hear the processing.
3. **Refine**: Set **Target LUFS** first, then adjust **Compressor Amount** for thickness.
4. **Export**: Use `File > Export Audio` to render the processed file as a high-fidelity WAV.

*\*Controls marked with an asterisk (\*) are available in **Advanced Mode** (Settings > Show Advanced Options).*

---

*Loudness Leveler is open-source and designed for professional audio engineers and podcasters alike.*
