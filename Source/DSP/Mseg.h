#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <vector>

#include <juce_core/juce_core.h>

namespace macro_osc
{
inline constexpr int kMsegSlotCount = 3;

struct MsegSyncedDuration
{
    float beats {};
    const char* label {};
};

inline constexpr std::array<MsegSyncedDuration, 21> kMsegSyncedDurations {{
    { 1.0f / 16.0f, "1/64" },
    { 1.0f / 12.0f, "1/32T" },
    { 1.0f / 8.0f, "1/32" },
    { 1.0f / 6.0f, "1/16T" },
    { 3.0f / 16.0f, "1/32D" },
    { 1.0f / 4.0f, "1/16" },
    { 1.0f / 3.0f, "1/8T" },
    { 3.0f / 8.0f, "1/16D" },
    { 1.0f / 2.0f, "1/8" },
    { 2.0f / 3.0f, "1/4T" },
    { 3.0f / 4.0f, "1/8D" },
    { 1.0f, "1/4" },
    { 4.0f / 3.0f, "1/2T" },
    { 3.0f / 2.0f, "1/4D" },
    { 2.0f, "1/2" },
    { 3.0f, "1/2D" },
    { 4.0f, "1 bar" },
    { 6.0f, "1 bar D" },
    { 8.0f, "2 bars" },
    { 12.0f, "2 bars D" },
    { 16.0f, "4 bars" }
}};

[[nodiscard]] inline const MsegSyncedDuration& nearestMsegSyncedDuration (float beats) noexcept
{
    const auto clampedBeats = juce::jlimit (kMsegSyncedDurations.front().beats,
                                           kMsegSyncedDurations.back().beats,
                                           beats);
    const MsegSyncedDuration* nearest = &kMsegSyncedDurations.front();
    float nearestDistance = std::abs (clampedBeats - nearest->beats);

    for (const auto& duration : kMsegSyncedDurations)
    {
        const float distance = std::abs (clampedBeats - duration.beats);
        if (distance < nearestDistance)
        {
            nearest = &duration;
            nearestDistance = distance;
        }
    }

    return *nearest;
}

[[nodiscard]] inline float snapMsegSyncedBeats (float beats) noexcept
{
    return nearestMsegSyncedDuration (beats).beats;
}

[[nodiscard]] inline juce::String msegSyncedDurationLabel (float beats)
{
    return nearestMsegSyncedDuration (beats).label;
}

[[nodiscard]] inline float msegSyncedBeatsToSeconds (float beats, float bpm) noexcept
{
    const float safeBpm = juce::jlimit (20.0f, 300.0f, bpm);
    return juce::jlimit (0.01f, 64.0f, snapMsegSyncedBeats (beats) * 60.0f / safeBpm);
}

struct MsegPoint
{
    float x {};
    float y {};
};

struct MsegShape
{
    std::vector<MsegPoint> points;

    [[nodiscard]] float evaluateUnipolar (float phase) const noexcept
    {
        if (points.empty())
            return 0.5f;

        const float x = juce::jlimit (0.0f, 1.0f, phase);
        if (x <= points.front().x)
            return points.front().y;

        for (size_t i = 1; i < points.size(); ++i)
        {
            const auto& right = points[i];
            if (x <= right.x)
            {
                const auto& left = points[i - 1];
                const float width = juce::jmax (0.0001f, right.x - left.x);
                const float alpha = juce::jlimit (0.0f, 1.0f, (x - left.x) / width);
                return left.y + ((right.y - left.y) * alpha);
            }
        }

        return points.back().y;
    }

    [[nodiscard]] float evaluateBipolar (float phase) const noexcept
    {
        return (evaluateUnipolar (phase) * 2.0f) - 1.0f;
    }

    [[nodiscard]] static std::vector<MsegPoint> defaultPoints()
    {
        return {
            { 0.0f, 0.5f },
            { 0.25f, 1.0f },
            { 0.65f, 0.1f },
            { 1.0f, 0.5f }
        };
    }

    [[nodiscard]] static std::vector<MsegPoint> sanitize (std::vector<MsegPoint> input)
    {
        if (input.empty())
            input = defaultPoints();

        for (auto& point : input)
        {
            point.x = juce::jlimit (0.0f, 1.0f, point.x);
            point.y = juce::jlimit (0.0f, 1.0f, point.y);
        }

        std::stable_sort (input.begin(), input.end(), [] (const auto& a, const auto& b)
        {
            return a.x < b.x;
        });

        std::vector<MsegPoint> result;
        result.reserve (std::min<size_t> (32, input.size() + 2));

        result.push_back ({ 0.0f, input.front().y });
        for (const auto& point : input)
        {
            if (point.x <= 0.0001f || point.x >= 0.9999f)
                continue;

            if (! result.empty()
                && std::abs (point.x - result.back().x) < 0.000001f
                && std::abs (point.y - result.back().y) < 0.000001f)
                result.back() = point;
            else if (result.size() < 31)
                result.push_back (point);
        }
        result.push_back ({ 1.0f, input.back().y });

        return result;
    }

    [[nodiscard]] static juce::String serialise (const std::vector<MsegPoint>& points)
    {
        juce::StringArray encoded;
        for (const auto& point : sanitize (points))
        {
            encoded.add (juce::String (point.x, 6) + "," + juce::String (point.y, 6));
        }
        return encoded.joinIntoString (";");
    }

    [[nodiscard]] static std::vector<MsegPoint> parse (const juce::String& text)
    {
        if (text.isEmpty())
            return defaultPoints();

        std::vector<MsegPoint> parsed;
        juce::StringArray pairs;
        pairs.addTokens (text, ";", {});

        for (const auto& pair : pairs)
        {
            juce::StringArray values;
            values.addTokens (pair, ",", {});
            if (values.size() != 2)
                continue;

            parsed.push_back ({ values[0].getFloatValue(), values[1].getFloatValue() });
        }

        return sanitize (std::move (parsed));
    }
};

enum class MsegDestination : int
{
    off,
    pitch,
    model,
    timbre,
    color,
    modulation,
    fm,
    level
};

[[nodiscard]] inline juce::StringArray msegDestinationNames()
{
    return { "Off", "Pitch", "Model", "Timbre", "Color", "Mod", "FM", "Level" };
}
} // namespace macro_osc
