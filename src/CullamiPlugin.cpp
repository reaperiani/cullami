#include "DistrhoPlugin.hpp"

#include "CullamiParameters.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

#include "CullamiWavData.hpp"

START_NAMESPACE_DISTRHO

namespace {

constexpr double kSourceSampleRate = 44100.0;
constexpr double kFadeSeconds = 2.0;
constexpr float kHalfPi = 1.57079632679489661923f;

uint32_t readLE32(const uint8_t* data)
{
    return uint32_t(data[0]) | (uint32_t(data[1]) << 8) | (uint32_t(data[2]) << 16) | (uint32_t(data[3]) << 24);
}

uint16_t readLE16(const uint8_t* data)
{
    return uint16_t(data[0]) | uint16_t(data[1] << 8);
}

struct StereoSample {
    std::vector<float> left;
    std::vector<float> right;

    bool loadPcm16Stereo(const uint8_t* bytes, const std::size_t size)
    {
        if (size < 44 || std::memcmp(bytes, "RIFF", 4) != 0 || std::memcmp(bytes + 8, "WAVE", 4) != 0)
            return false;

        const uint8_t* format = nullptr;
        const uint8_t* audio = nullptr;
        uint32_t formatSize = 0;
        uint32_t audioSize = 0;

        for (std::size_t offset = 12; offset + 8 <= size;)
        {
            const uint8_t* const chunk = bytes + offset;
            const uint32_t chunkSize = readLE32(chunk + 4);
            const std::size_t next = offset + 8u + chunkSize + (chunkSize & 1u);
            if (next > size)
                return false;

            if (std::memcmp(chunk, "fmt ", 4) == 0) {
                format = chunk + 8;
                formatSize = chunkSize;
            } else if (std::memcmp(chunk, "data", 4) == 0) {
                audio = chunk + 8;
                audioSize = chunkSize;
            }
            offset = next;
        }

        if (format == nullptr || audio == nullptr || formatSize < 16 ||
            readLE16(format) != 1 || readLE16(format + 2) != 2 ||
            readLE32(format + 4) != 44100 || readLE16(format + 14) != 16)
            return false;

        const uint32_t frameCount = audioSize / 4;
        left.resize(frameCount);
        right.resize(frameCount);
        for (uint32_t frame = 0; frame < frameCount; ++frame) {
            const uint8_t* const source = audio + frame * 4;
            const int16_t l = static_cast<int16_t>(readLE16(source));
            const int16_t r = static_cast<int16_t>(readLE16(source + 2));
            left[frame] = static_cast<float>(l) / 32768.0f;
            right[frame] = static_cast<float>(r) / 32768.0f;
        }
        return frameCount > 0;
    }
};

} // namespace

class CullamiPlugin final : public Plugin {
public:
    CullamiPlugin()
        : Plugin(kParameterCount, 0, 0),
          fNoise(0.0f), fDryWet(0.5f), fOutputDb(-6.0f), fBypassNoise(0.0f), fSafeRender(0.0f),
          fMeterLeft(0.0f), fMeterRight(0.0f), fCurrentNoise(0), fPreviousNoise(0),
          fNoiseCrossfade(1.0), fNoiseGate(1.0)
    {
        if (!isDummyInstance())
            loadSamples();
    }

protected:
    const char* getLabel() const override { return "Cullami"; }
    const char* getDescription() const override { return "Audition a mix against looping background noise."; }
    const char* getMaker() const override { return "reaperiani"; }
    const char* getHomePage() const override { return "https://github.com/reaperiani/cullami"; }
    const char* getLicense() const override { return "ISC"; }
    uint32_t getVersion() const override { return d_version(1, 0, 1); }

    void initAudioPort(const bool input, const uint32_t index, AudioPort& port) override
    {
        port.groupId = kPortGroupStereo;
        Plugin::initAudioPort(input, index, port);
    }

    void initParameter(const uint32_t index, Parameter& parameter) override
    {
        switch (index) {
        case kParameterNoise: {
            parameter.hints = kParameterIsAutomatable | kParameterIsInteger;
            parameter.name = "Noise";
            parameter.symbol = "noise";
            parameter.ranges = ParameterRanges(0.0f, 0.0f, 4.0f);
            parameter.enumValues.count = kNoiseCount;
            parameter.enumValues.restrictedMode = true;
            ParameterEnumerationValue* const values = new ParameterEnumerationValue[kNoiseCount];
            parameter.enumValues.values = values;
            values[0] = { 0.0f, "1" };
            values[1] = { 1.0f, "2" };
            values[2] = { 2.0f, "3" };
            values[3] = { 3.0f, "4" };
            values[4] = { 4.0f, "Cuffie" };
            break;
        }
        case kParameterDryWet:
            parameter.hints = kParameterIsAutomatable;
            parameter.name = "Dry/Wet";
            parameter.symbol = "dry_wet";
            parameter.ranges = ParameterRanges(0.5f, 0.0f, 1.0f);
            break;
        case kParameterOutput:
            parameter.hints = kParameterIsAutomatable;
            parameter.name = "Output";
            parameter.symbol = "output";
            parameter.unit = "dB";
            parameter.ranges = ParameterRanges(-6.0f, -24.0f, 6.0f);
            break;
        case kParameterBypassNoise:
            parameter.hints = kParameterIsAutomatable | kParameterIsBoolean;
            parameter.name = "Bypass Noise";
            parameter.symbol = "bypass_noise";
            parameter.ranges = ParameterRanges(0.0f, 0.0f, 1.0f);
            break;
        case kParameterSafeRender:
            parameter.hints = kParameterIsAutomatable | kParameterIsBoolean;
            parameter.name = "Safe Render";
            parameter.symbol = "safe_render";
            parameter.ranges = ParameterRanges(0.0f, 0.0f, 1.0f);
            break;
        case kParameterMeterLeft:
        case kParameterMeterRight:
            parameter.hints = kParameterIsOutput;
            parameter.name = index == kParameterMeterLeft ? "Output Left" : "Output Right";
            parameter.symbol = index == kParameterMeterLeft ? "output_left" : "output_right";
            parameter.ranges = ParameterRanges(0.0f, 0.0f, 1.0f);
            break;
        }
    }

    float getParameterValue(const uint32_t index) const override
    {
        switch (index) {
        case kParameterNoise: return fNoise;
        case kParameterDryWet: return fDryWet;
        case kParameterOutput: return fOutputDb;
        case kParameterBypassNoise: return fBypassNoise;
        case kParameterSafeRender: return fSafeRender;
        case kParameterMeterLeft: return fMeterLeft;
        case kParameterMeterRight: return fMeterRight;
        default: return 0.0f;
        }
    }

    void setParameterValue(const uint32_t index, const float value) override
    {
        switch (index) {
        case kParameterNoise: fNoise = std::round(std::max(0.0f, std::min(4.0f, value))); break;
        case kParameterDryWet: fDryWet = std::max(0.0f, std::min(1.0f, value)); break;
        case kParameterOutput: fOutputDb = std::max(-24.0f, std::min(6.0f, value)); break;
        case kParameterBypassNoise: fBypassNoise = value >= 0.5f ? 1.0f : 0.0f; break;
        case kParameterSafeRender: fSafeRender = value >= 0.5f ? 1.0f : 0.0f; break;
        default: break;
        }
    }

    void run(const float** inputs, float** outputs, const uint32_t frames) override
    {
        const uint32_t selectedNoise = static_cast<uint32_t>(fNoise);
        if (selectedNoise != fCurrentNoise) {
            fPreviousNoise = fCurrentNoise;
            fCurrentNoise = selectedNoise;
            fNoiseCrossfade = 0.0;
        }

        const double sampleRate = getSampleRate();
        const double sourceStep = kSourceSampleRate / sampleRate;
        const double fadeStep = 1.0 / (sampleRate * kFadeSeconds);
        const bool hardMuteNoise = fSafeRender >= 0.5f;
        const double gateTarget = hardMuteNoise ? 0.0 : (fBypassNoise >= 0.5f ? 0.0 : 1.0);
        const bool bypassing = !hardMuteNoise && fBypassNoise >= 0.5f;
        const float dryGain = std::cos(fDryWet * kHalfPi);
        const float wetGain = std::sin(fDryWet * kHalfPi);
        // Render safety returns the dry signal at unity, independent of the audition trim.
        const float outputGain = hardMuteNoise ? 1.0f : std::pow(10.0f, fOutputDb / 20.0f);
        float peakLeft = 0.0f;
        float peakRight = 0.0f;

        for (uint32_t frame = 0; frame < frames; ++frame) {
            if (hardMuteNoise) {
                outputs[0][frame] = inputs[0][frame];
                outputs[1][frame] = inputs[1][frame];
                peakLeft = std::max(peakLeft, std::abs(inputs[0][frame]));
                peakRight = std::max(peakRight, std::abs(inputs[1][frame]));
                continue;
            }

            if (fNoiseGate < gateTarget) {
                fNoiseGate = std::min(gateTarget, fNoiseGate + fadeStep);
            } else if (fNoiseGate > gateTarget) {
                fNoiseGate = bypassing ? std::max(gateTarget, fNoiseGate - fadeStep) : gateTarget;
            }

            float noiseLeft = 0.0f;
            float noiseRight = 0.0f;
            readLoop(fCurrentNoise, sourceStep, noiseLeft, noiseRight);
            if (fNoiseCrossfade < 1.0) {
                float oldLeft = 0.0f;
                float oldRight = 0.0f;
                readLoop(fPreviousNoise, sourceStep, oldLeft, oldRight);
                const float angle = static_cast<float>(fNoiseCrossfade) * kHalfPi;
                noiseLeft = oldLeft * std::cos(angle) + noiseLeft * std::sin(angle);
                noiseRight = oldRight * std::cos(angle) + noiseRight * std::sin(angle);
                fNoiseCrossfade = std::min(1.0, fNoiseCrossfade + fadeStep);
            }

            const float noiseGain = static_cast<float>(fNoiseGate);
            const float outLeft = (inputs[0][frame] * dryGain + noiseLeft * wetGain * noiseGain) * outputGain;
            const float outRight = (inputs[1][frame] * dryGain + noiseRight * wetGain * noiseGain) * outputGain;
            outputs[0][frame] = outLeft;
            outputs[1][frame] = outRight;
            peakLeft = std::max(peakLeft, std::abs(outLeft));
            peakRight = std::max(peakRight, std::abs(outRight));
        }

        fMeterLeft = std::min(1.0f, peakLeft);
        fMeterRight = std::min(1.0f, peakRight);
    }

private:
    void loadSamples()
    {
        fSamples[0].loadPcm16Stereo(CullamiWavData::kNoise1, CullamiWavData::kNoise1Size);
        fSamples[1].loadPcm16Stereo(CullamiWavData::kNoise2, CullamiWavData::kNoise2Size);
        fSamples[2].loadPcm16Stereo(CullamiWavData::kNoise3, CullamiWavData::kNoise3Size);
        fSamples[3].loadPcm16Stereo(CullamiWavData::kNoise4, CullamiWavData::kNoise4Size);
        fSamples[4].loadPcm16Stereo(CullamiWavData::kNoiseCuffie, CullamiWavData::kNoiseCuffieSize);
    }

    void readLoop(const uint32_t sampleIndex, const double step, float& left, float& right)
    {
        StereoSample& sample = fSamples[sampleIndex];
        const uint32_t frames = static_cast<uint32_t>(sample.left.size());
        if (frames == 0) {
            left = right = 0.0f;
            return;
        }

        const double loopCrossfadeFrames = std::min(kSourceSampleRate * kFadeSeconds, static_cast<double>(frames) * 0.5);
        double& position = fPositions[sampleIndex];
        left = readInterpolated(sample.left, position);
        right = readInterpolated(sample.right, position);

        if (position >= static_cast<double>(frames) - loopCrossfadeFrames) {
            const double startPosition = position - (static_cast<double>(frames) - loopCrossfadeFrames);
            const float angle = static_cast<float>(startPosition / loopCrossfadeFrames) * kHalfPi;
            left = left * std::cos(angle) + readInterpolated(sample.left, startPosition) * std::sin(angle);
            right = right * std::cos(angle) + readInterpolated(sample.right, startPosition) * std::sin(angle);
        }

        position += step;
        while (position >= frames)
            position -= frames;
    }

    static float readInterpolated(const std::vector<float>& channel, const double position)
    {
        const uint32_t frames = static_cast<uint32_t>(channel.size());
        const uint32_t first = static_cast<uint32_t>(position);
        const uint32_t second = first + 1u < frames ? first + 1u : 0u;
        const float fraction = static_cast<float>(position - first);
        return channel[first] + (channel[second] - channel[first]) * fraction;
    }

    float fNoise;
    float fDryWet;
    float fOutputDb;
    float fBypassNoise;
    float fSafeRender;
    float fMeterLeft;
    float fMeterRight;
    std::array<StereoSample, kNoiseCount> fSamples;
    std::array<double, kNoiseCount> fPositions{};
    uint32_t fCurrentNoise;
    uint32_t fPreviousNoise;
    double fNoiseCrossfade;
    double fNoiseGate;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CullamiPlugin)
};

Plugin* createPlugin()
{
    return new CullamiPlugin();
}

END_NAMESPACE_DISTRHO
