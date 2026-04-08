#pragma once

#include <vector>
#include <cmath>
#include <algorithm>
#include <cstdint>

namespace lufs {

struct BiquadCoeffs {
    double b0, b1, b2, a1, a2;
};

class Biquad {
public:
    Biquad() : x1(0), x2(0), y1(0), y2(0) {}
    void setCoeffs(const BiquadCoeffs& c) { coeffs = c; }
    double process(double x) {
        double y = coeffs.b0 * x + coeffs.b1 * x1 + coeffs.b2 * x2 - coeffs.a1 * y1 - coeffs.a2 * y2;
        x2 = x1;
        x1 = x;
        y2 = y1;
        y1 = y;
        return y;
    }
    void reset() { x1 = x2 = y1 = y2 = 0; }

private:
    BiquadCoeffs coeffs;
    double x1, x2, y1, y2;
};

class KWeightingFilter {
public:
    KWeightingFilter(double sampleRate);
    double process(double x);
    void reset();

private:
    Biquad stage1;
    Biquad stage2;
};

class TruePeakDetector {
public:
    TruePeakDetector();
    void process(float x);
    void reset();
    double getTruePeak() const { return maxAbs; }

private:
    float maxAbs;
    float history[16]; // Power of 2 for fast wrapping
    uint32_t historyIdx;
};

struct Block {
    std::vector<double> channelMeanSquares;
    double loudness;
};

class LoudnessAnalyzer {
public:
    LoudnessAnalyzer(int numChannels, double sampleRate);
    void process(const float* buffer, size_t numFrames);
    
    double getIntegratedLoudness();
    double getLoudnessRange();
    double getTruePeak();

private:
    void calculateLoudnessForBlock();

    int numChannels;
    double sampleRate;
    size_t blockSizeFrames;
    size_t hopSizeFrames;
    
    std::vector<KWeightingFilter> filters;
    std::vector<TruePeakDetector> tpDetectors;
    std::vector<std::vector<float>> sampleBuffers;
    std::vector<size_t> bufferWriteIndices;

    std::vector<double> channelWeights;
    std::vector<Block> blocks;
};

} // namespace lufs
