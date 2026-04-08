#include "LufsAnalyzer.h"
#include <iostream>
#include <numeric>
#include <cmath>
#include <algorithm>
#include <vector>
#include <cstring>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace lufs {

KWeightingFilter::KWeightingFilter(double sampleRate) {
    BiquadCoeffs s1, s2;
    if (std::abs(sampleRate - 48000.0) < 1.0) {
        s1 = {1.53512485958697, -2.69169618940638, 1.19839281085285, -1.69065929318241, 0.73248077421585};
        s2 = {1.0, -2.0, 1.0, -1.99004745483398, 0.99007225036621};
    } else if (std::abs(sampleRate - 44100.0) < 1.0) {
        s1 = {1.5309095994509831, -2.651771422231242, 1.1690820038815147, -1.6637221141740625, 0.7125301614643149};
        s2 = {1.0, -2.0, 1.0, -1.990444327070211, 0.9904722389417079};
    } else {
        s1 = {1.53512485958697, -2.69169618940638, 1.19839281085285, -1.69065929318241, 0.73248077421585};
        s2 = {1.0, -2.0, 1.0, -1.99004745483398, 0.99007225036621};
    }
    stage1.setCoeffs(s1);
    stage2.setCoeffs(s2);
}

double KWeightingFilter::process(double x) {
    return stage2.process(stage1.process(x));
}

void KWeightingFilter::reset() {
    stage1.reset();
    stage2.reset();
}

static const double TP_COEFFS[3][12] = {
    { 0.0017, -0.0129,  0.0434, -0.0984,  0.1983,  0.9205, -0.0984,  0.0434, -0.0129,  0.0017,  0.0000,  0.0000 },
    { 0.0028, -0.0210,  0.0682, -0.1601,  0.6101,  0.6101, -0.1601,  0.0682, -0.0210,  0.0028,  0.0000,  0.0000 },
    { 0.0017, -0.0129,  0.0434, -0.0984,  0.9205,  0.1983, -0.0984,  0.0434, -0.0129,  0.0017,  0.0000,  0.0000 }
};

TruePeakDetector::TruePeakDetector() : maxAbs(0), historyIdx(0) {
    std::fill(history, history + 16, 0.0f);
}

void TruePeakDetector::process(float x) {
    history[historyIdx] = x;
    float a = std::abs(x);
    if (a > maxAbs) maxAbs = a;
    
    // Unrolled 4x oversampling filter (12-tap)
    // We access history indices using (historyIdx + offset) & 15
    for (int p = 0; p < 3; ++p) {
        float interp = history[(historyIdx - 11) & 15] * (float)TP_COEFFS[p][0] +
                       history[(historyIdx - 10) & 15] * (float)TP_COEFFS[p][1] +
                       history[(historyIdx - 9) & 15] * (float)TP_COEFFS[p][2] +
                       history[(historyIdx - 8) & 15] * (float)TP_COEFFS[p][3] +
                       history[(historyIdx - 7) & 15] * (float)TP_COEFFS[p][4] +
                       history[(historyIdx - 6) & 15] * (float)TP_COEFFS[p][5] +
                       history[(historyIdx - 5) & 15] * (float)TP_COEFFS[p][6] +
                       history[(historyIdx - 4) & 15] * (float)TP_COEFFS[p][7] +
                       history[(historyIdx - 3) & 15] * (float)TP_COEFFS[p][8] +
                       history[(historyIdx - 2) & 15] * (float)TP_COEFFS[p][9] +
                       history[(historyIdx - 1) & 15] * (float)TP_COEFFS[p][10] +
                       history[historyIdx] * (float)TP_COEFFS[p][11];
        float ai = std::abs(interp);
        if (ai > maxAbs) maxAbs = ai;
    }
    historyIdx = (historyIdx + 1) & 15; // Wrap forward
}

void TruePeakDetector::reset() {
    maxAbs = 0;
    std::fill(history, history + 16, 0.0f);
}

LoudnessAnalyzer::LoudnessAnalyzer(int numChannels, double sampleRate) 
    : numChannels(numChannels), sampleRate(sampleRate), 
      tpDetectors(numChannels), sampleBuffers(numChannels), bufferWriteIndices(numChannels, 0) {
    blockSizeFrames = static_cast<size_t>(0.4 * sampleRate);
    hopSizeFrames = static_cast<size_t>(0.1 * sampleRate); 
    
    for (int i = 0; i < numChannels; ++i) {
        filters.emplace_back(sampleRate);
        sampleBuffers[i].resize(blockSizeFrames + 8192); // Extra room for overflow
    }

    channelWeights.resize(numChannels, 1.0);
    if (numChannels == 6) {
        channelWeights[3] = 0.0;  // LFE
        channelWeights[4] = 1.41; // Ls
        channelWeights[5] = 1.41; // Rs
    }
}

void LoudnessAnalyzer::process(const float* buffer, size_t numFrames) {
    for (size_t f = 0; f < numFrames; ++f) {
        for (int c = 0; c < numChannels; ++c) {
            float rawSample = buffer[f * numChannels + c];
            tpDetectors[c].process(rawSample);
            float filtered = (float)filters[c].process((double)rawSample);
            sampleBuffers[c][bufferWriteIndices[c]++] = filtered;
        }

        if (bufferWriteIndices[0] >= blockSizeFrames) {
            calculateLoudnessForBlock();
            
            // Slide buffer for the next hop (hopSize overlap)
            size_t overlap = blockSizeFrames - hopSizeFrames;
            for (int c = 0; c < numChannels; ++c) {
                std::memmove(sampleBuffers[c].data(), sampleBuffers[c].data() + hopSizeFrames, overlap * sizeof(float));
                bufferWriteIndices[c] = overlap;
            }
        }
    }
}

void LoudnessAnalyzer::calculateLoudnessForBlock() {
    Block b;
    b.channelMeanSquares.resize(numChannels);
    double weightedSum = 0;
    for (int c = 0; c < numChannels; ++c) {
        double ms = 0;
        const float* data = sampleBuffers[c].data();
        for (size_t i = 0; i < blockSizeFrames; ++i) ms += (double)data[i] * data[i];
        ms /= blockSizeFrames;
        b.channelMeanSquares[c] = ms;
        weightedSum += channelWeights[c] * ms;
    }
    b.loudness = (weightedSum > 1e-12) ? (-0.691 + 10.0 * std::log10(weightedSum)) : -1000.0;
    blocks.push_back(b);
}

double LoudnessAnalyzer::getIntegratedLoudness() {
    if (blocks.empty()) return -1000.0;
    std::vector<size_t> absGated;
    for (size_t j = 0; j < blocks.size(); ++j) if (blocks[j].loudness > -70.0) absGated.push_back(j);
    if (absGated.empty()) return -70.0;
    std::vector<double> msAbs(numChannels, 0.0);
    for (size_t idx : absGated) for (int c = 0; c < numChannels; ++c) msAbs[c] += blocks[idx].channelMeanSquares[c];
    double sumAbs = 0;
    for (int c = 0; c < numChannels; ++c) sumAbs += channelWeights[c] * (msAbs[c] / absGated.size());
    double gammaR = -0.691 + 10.0 * std::log10(sumAbs) - 10.0;
    std::vector<size_t> finalGated;
    for (size_t idx : absGated) if (blocks[idx].loudness > gammaR) finalGated.push_back(idx);
    if (finalGated.empty()) return -0.691 + 10.0 * std::log10(sumAbs);
    std::vector<double> msFinal(numChannels, 0.0);
    for (size_t idx : finalGated) for (int c = 0; c < numChannels; ++c) msFinal[c] += blocks[idx].channelMeanSquares[c];
    double sumFinal = 0;
    for (int c = 0; c < numChannels; ++c) sumFinal += channelWeights[c] * (msFinal[c] / finalGated.size());
    return -0.691 + 10.0 * std::log10(sumFinal);
}

double LoudnessAnalyzer::getLoudnessRange() {
    int blocksPer3s = 30; 
    if (blocks.size() < static_cast<size_t>(blocksPer3s)) return 0.0;
    std::vector<double> stLoudness;
    for (size_t i = 0; i <= blocks.size() - blocksPer3s; ++i) {
        std::vector<double> chMS(numChannels, 0.0);
        for (int j = 0; j < blocksPer3s; ++j) for (int c = 0; c < numChannels; ++c) chMS[c] += blocks[i + j].channelMeanSquares[c];
        double sum = 0;
        for (int c = 0; c < numChannels; ++c) sum += channelWeights[c] * (chMS[c] / blocksPer3s);
        if (sum > 1e-12) stLoudness.push_back(-0.691 + 10.0 * std::log10(sum));
    }
    std::vector<double> absGated;
    for (double l : stLoudness) if (l > -70.0) absGated.push_back(l);
    if (absGated.empty()) return 0.0;
    double sumLIN = 0;
    for (double l : absGated) sumLIN += std::pow(10.0, l / 10.0);
    double gammaR = 10.0 * std::log10(sumLIN / absGated.size()) - 20.0;
    std::vector<double> finalGated;
    for (double l : absGated) if (l > gammaR) finalGated.push_back(l);
    if (finalGated.size() < 2) return 0.0;
    std::sort(finalGated.begin(), finalGated.end());
    return finalGated[size_t(0.95 * (finalGated.size()-1))] - finalGated[size_t(0.1 * (finalGated.size()-1))];
}

double LoudnessAnalyzer::getTruePeak() {
    double overallMax = 0;
    for (int c = 0; c < numChannels; ++c) overallMax = std::max(overallMax, tpDetectors[c].getTruePeak());
    return (overallMax <= 0) ? -100.0 : 20.0 * std::log10(overallMax);
}

} // namespace lufs
