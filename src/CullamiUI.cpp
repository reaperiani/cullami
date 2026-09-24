#include "DistrhoUI.hpp"
#include "EventHandlers.hpp"
#include "NanoVG.hpp"

#include "CullamiParameters.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>

START_NAMESPACE_DISTRHO

using DGL_NAMESPACE::Color;
using DGL_NAMESPACE::KnobEventHandler;
using DGL_NAMESPACE::NanoSubWidget;

namespace {

struct Rect {
    float x;
    float y;
    float width;
    float height;

    bool contains(const double px, const double py) const
    {
        return px >= x && px <= x + width && py >= y && py <= y + height;
    }
};

constexpr std::array<const char*, kNoiseCount> kNoiseNames = { "1", "2", "3", "4", "CUFFIE" };

} // namespace

class CullamiKnob final : public NanoSubWidget,
                          public KnobEventHandler {
public:
    CullamiKnob(UI* const parent, const char* const label, const char* const valueFormat)
        : NanoSubWidget(parent),
          KnobEventHandler(this),
          fLabel(label),
          fValueFormat(valueFormat),
          fDisplayMultiplier(1.0f)
    {
        setOrientation(Vertical);
        setMouseDeceleration(180.0f);
    }

    void setDisplayMultiplier(const float multiplier) noexcept
    {
        fDisplayMultiplier = multiplier;
    }

protected:
    void onNanoDisplay() override
    {
        const float centerX = static_cast<float>(getWidth()) * 0.5f;
        const float centerY = 62.0f;
        const float radius = 54.0f;
        const float start = 2.35619449f;
        const float angle = start + getNormalizedValue() * 4.71238898f;
        const Color accent(89, 204, 177);

        beginPath();
        circle(centerX, centerY, radius);
        fillColor(Color(22, 27, 33));
        fill();
        strokeColor(Color(69, 79, 90));
        strokeWidth(2.0f);
        stroke();

        if (getNormalizedValue() > 0.0f) {
            beginPath();
            arc(centerX, centerY, radius - 7.0f, start, angle, CW);
            strokeColor(accent);
            strokeWidth(5.0f);
            stroke();
        }

        beginPath();
        circle(centerX + std::cos(angle) * (radius - 18.0f), centerY + std::sin(angle) * (radius - 18.0f), 4.5f);
        fillColor(accent);
        fill();

        char value[32];
        std::snprintf(value, sizeof(value), fValueFormat, getValue() * fDisplayMultiplier);
        fontSize(15.0f);
        textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
        fillColor(Color(227, 232, 238));
        text(centerX, centerY, value, nullptr);
        fontSize(11.0f);
        fillColor(Color(136, 149, 163));
        text(centerX, centerY + radius + 20.0f, fLabel, nullptr);
    }

    bool onMouse(const MouseEvent& event) override
    {
        return mouseEvent(event);
    }

    bool onMotion(const MotionEvent& event) override
    {
        return motionEvent(event);
    }

    bool onScroll(const ScrollEvent& event) override
    {
        return scrollEvent(event);
    }

private:
    const char* const fLabel;
    const char* const fValueFormat;
    float fDisplayMultiplier;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CullamiKnob)
};

class CullamiUI final : public UI,
                        public KnobEventHandler::Callback {
public:
    CullamiUI()
        : UI(DISTRHO_UI_DEFAULT_WIDTH, DISTRHO_UI_DEFAULT_HEIGHT),
          fParameters{ 0.0f, 0.5f, -6.0f, 0.0f, 0.0f, 0.0f, 0.0f },
          fDryWetKnob(this, "DRY / WET", "%.0f %%"),
          fOutputKnob(this, "OUTPUT", "%.1f dB")
    {
       #ifdef DGL_NO_SHARED_RESOURCES
        createFontFromFile("sans", "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf");
       #else
        loadSharedResources();
       #endif
        setGeometryConstraints(DISTRHO_UI_DEFAULT_WIDTH, DISTRHO_UI_DEFAULT_HEIGHT, true);

        fDryWetKnob.setAbsolutePos(114, 83);
        fDryWetKnob.setSize(124, 140);
        fDryWetKnob.setRange(0.0f, 1.0f);
        fDryWetKnob.setDefault(0.5f);
        fDryWetKnob.setValue(0.5f);
        fDryWetKnob.setDisplayMultiplier(100.0f);
        fDryWetKnob.setCallback(this);

        fOutputKnob.setAbsolutePos(336, 83);
        fOutputKnob.setSize(124, 140);
        fOutputKnob.setRange(-24.0f, 6.0f);
        fOutputKnob.setDefault(-6.0f);
        fOutputKnob.setValue(-6.0f);
        fOutputKnob.setCallback(this);
    }

protected:
    void parameterChanged(const uint32_t index, const float value) override
    {
        if (index >= kParameterCount || fParameters[index] == value)
            return;
        fParameters[index] = value;
        if (index == kParameterDryWet)
            fDryWetKnob.setValue(value);
        else if (index == kParameterOutput)
            fOutputKnob.setValue(value);
        repaint();
    }

    void onNanoDisplay() override
    {
        const float width = static_cast<float>(getWidth());
        const float height = static_cast<float>(getHeight());
        const Color background(19, 22, 27);
        const Color panel(29, 34, 41);
        const Color panelBorder(58, 67, 78);
        const Color textColor(227, 232, 238);
        const Color subdued(136, 149, 163);
        const Color accent(89, 204, 177);
        const Color warning(241, 178, 83);

        beginPath();
        rect(0.0f, 0.0f, width, height);
        fillColor(background);
        fill();

        beginPath();
        fontSize(24.0f);
        textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
        fillColor(textColor);
        text(28.0f, 34.0f, "CULLAMI", nullptr);
        fontSize(12.0f);
        fillColor(subdued);
        text(143.0f, 34.0f, "reaperiani  |  mix reality check", nullptr);

        beginPath();
        roundedRect(20.0f, 60.0f, width - 40.0f, 190.0f, 10.0f);
        fillColor(panel);
        fill();
        strokeColor(panelBorder);
        strokeWidth(1.0f);
        stroke();

        fontSize(11.0f);
        textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
        fillColor(subdued);
        text(490.0f, 92.0f, "POST OUTPUT", nullptr);
        drawMeter(500.0f, 108.0f, 38.0f, 105.0f, fParameters[kParameterMeterLeft], "L", accent, warning, textColor);
        drawMeter(550.0f, 108.0f, 38.0f, 105.0f, fParameters[kParameterMeterRight], "R", accent, warning, textColor);

        fontSize(12.0f);
        textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
        fillColor(subdued);
        text(28.0f, 275.0f, "BACKGROUND", nullptr);

        for (uint32_t index = 0; index < kNoiseCount; ++index) {
            const Rect button = noiseButton(index);
            const bool selected = static_cast<uint32_t>(fParameters[kParameterNoise]) == index;
            drawButton(button, kNoiseNames[index], selected, accent, panel, panelBorder, textColor);
        }

        const Rect bypass = bypassButton();
        const Rect safeRender = safeRenderButton();
        drawButton(bypass, "BYPASS NOISE", fParameters[kParameterBypassNoise] >= 0.5f, warning, panel, panelBorder, textColor);
        drawButton(safeRender, "SAFE RENDER", fParameters[kParameterSafeRender] >= 0.5f, accent, panel, panelBorder, textColor);
    }

    bool onMouse(const MouseEvent& event) override
    {
        if (UI::onMouse(event))
            return true;

        if (event.button != 1)
            return false;

        if (!event.press)
            return false;

        const double x = event.pos.getX();
        const double y = event.pos.getY();
        for (uint32_t index = 0; index < kNoiseCount; ++index) {
            if (noiseButton(index).contains(x, y)) {
                setParameter(kParameterNoise, static_cast<float>(index));
                return true;
            }
        }
        if (bypassButton().contains(x, y)) {
            setParameter(kParameterBypassNoise, fParameters[kParameterBypassNoise] >= 0.5f ? 0.0f : 1.0f);
            return true;
        }
        if (safeRenderButton().contains(x, y)) {
            setParameter(kParameterSafeRender, fParameters[kParameterSafeRender] >= 0.5f ? 0.0f : 1.0f);
            return true;
        }
        return false;
    }

    void knobDragStarted(DGL_NAMESPACE::SubWidget* const widget) override
    {
        editParameter(parameterForKnob(widget), true);
    }

    void knobDragFinished(DGL_NAMESPACE::SubWidget* const widget) override
    {
        editParameter(parameterForKnob(widget), false);
    }

    void knobValueChanged(DGL_NAMESPACE::SubWidget* const widget, const float value) override
    {
        const uint32_t parameter = parameterForKnob(widget);
        fParameters[parameter] = value;
        setParameterValue(parameter, value);
    }

private:
    static float clamp(const float value, const float minimum, const float maximum)
    {
        return std::max(minimum, std::min(maximum, value));
    }

    static Rect noiseButton(const uint32_t index)
    {
        return { 128.0f + index * 76.0f, 258.0f, index == 4 ? 102.0f : 66.0f, 34.0f };
    }

    static Rect bypassButton() { return { 20.0f, 312.0f, 180.0f, 30.0f }; }
    static Rect safeRenderButton() { return { 440.0f, 312.0f, 180.0f, 30.0f }; }

    void setParameter(const uint32_t index, const float value)
    {
        editParameter(index, true);
        setParameterValue(index, value);
        editParameter(index, false);
        fParameters[index] = value;
        repaint();
    }

    uint32_t parameterForKnob(const DGL_NAMESPACE::SubWidget* const widget) const noexcept
    {
        return widget == &fDryWetKnob ? kParameterDryWet : kParameterOutput;
    }

    void drawButton(const Rect& bounds, const char* label, const bool active, const Color activeColor, const Color inactiveColor, const Color border, const Color textColor)
    {
        beginPath();
        roundedRect(bounds.x, bounds.y, bounds.width, bounds.height, 5.0f);
        fillColor(active ? activeColor : inactiveColor);
        fill();
        strokeColor(active ? activeColor : border);
        strokeWidth(1.0f);
        stroke();
        fontSize(11.0f);
        textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
        fillColor(active ? Color(12, 23, 24) : textColor);
        text(bounds.x + bounds.width * 0.5f, bounds.y + bounds.height * 0.5f, label, nullptr);
    }

    void drawMeter(const float x, const float y, const float width, const float height, const float value, const char* label, const Color accent, const Color warning, const Color textColor)
    {
        beginPath();
        roundedRect(x, y, width, height, 3.0f);
        fillColor(Color(13, 16, 20));
        fill();
        const float filled = height * clamp(value, 0.0f, 1.0f);
        beginPath();
        roundedRect(x + 3.0f, y + height - filled - 3.0f, width - 6.0f, filled, 2.0f);
        fillColor(value > 0.95f ? warning : accent);
        fill();
        fontSize(11.0f);
        textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
        fillColor(textColor);
        text(x + width * 0.5f, y + height + 14.0f, label, nullptr);
    }

    std::array<float, kParameterCount> fParameters;
    CullamiKnob fDryWetKnob;
    CullamiKnob fOutputKnob;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CullamiUI)
};

UI* createUI()
{
    return new CullamiUI();
}

END_NAMESPACE_DISTRHO
