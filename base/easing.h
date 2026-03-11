// Copyright (C) 2020-2026 Sami Väisänen
// Copyright (C) 2020-2026 Ensisoft http://www.ensisoft.com
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include "config.h"

#include <cmath>

#include "base/assert.h"
#include "base/math.h"

// Easing curves, CSS inspired.
// https://easings.net/
// https://codeplea.com/simple-interpolation

namespace easing {
    inline float step_start(float t) noexcept
    { return t > 0.0f ? 1.0f : 0.0f; }

    inline float step_end(float t) noexcept
    { return t >= 1.0f ? 1.0f : 0.0f; }

    inline float step(float t) noexcept
    { return (t < 0.5f) ? 0.0f : 1.0f; }

    inline float cosine(float t) noexcept
    { return -std::cos(math::Pi * t) * 0.5f + 0.5f; }

    inline float smooth_step(float t) noexcept
    { return 3.0f*t*t - 2.0f*t*t*t; }

    inline float acceleration(float t) noexcept
    { return t*t; }

    inline float deceleration(float t) noexcept
    { return 1.0f - ((1.0f-t)*(1.0f-t)); }

    inline float ease_in_sine(float t) noexcept
    { return 1.0f - std::cos((t * math::Pi) / 2.0); }

    inline float ease_out_sine(float t) noexcept
    { return std::sin((t * math::Pi) / 2.0); }

    inline float ease_in_out_sine(float t) noexcept
    { return -(std::cos(math::Pi * t) - 1.0) / 2.0;}

    inline float ease_in_quadratic(float t) noexcept
    { return t*t; }

    inline float ease_out_quadratic(float t) noexcept
    { return 1.0f - (1.0f - t) * (1.0f - t);}

    inline float ease_in_out_quadratic(float t) noexcept
    {
        return t < 0.5f
               ? 2.0f * t * t
               : 1.0f - std::pow(-2.0f * t + 2.0f, 2.0f) / 2.0f;
    }

    inline float ease_in_cubic(float t) noexcept
    { return t * t * t; }

    inline float ease_out_cubic(float t) noexcept
    { return 1.0f - std::pow(1.0f - t, 3.0f); }

    inline float ease_in_out_cubic(float t) noexcept
    {
        return t < 0.5f
             ? 4.0f  * t * t * t
             : 1.0f - std::pow(-2.0f * t + 2.0f, 3.0f) / 2.0f;
    }

    inline float ease_in_back(float t) noexcept
    {
        const auto c1 = 1.70158f;
        const auto c3 = c1 + 1.0f;
        return c3 * t * t * t - c1 * t * t;
    }

    inline float ease_out_back(float t) noexcept
    {
        const auto c1 = 1.70158f;
        const auto c3 = c1 + 1.0f;
        return 1.0f + c3 * std::pow(t - 1.0f, 3.0f) + c1 * std::pow(t - 1.0f, 2.0f);
    }

    inline float ease_in_out_back(float t) noexcept
    {
        const auto c1 = 1.70158f;
        const auto c2 = c1 * 1.525f;

        return t < 0.5f
               ? (std::pow(2.0f * t, 2.0f) * ((c2 + 1.0f) * 2.0f * t - c2)) / 2.0f
               : (std::pow(2.0f * t - 2.0f, 2.0f) * ((c2 + 1.0f) * (t * 2.0f - 2.0f) + c2) + 2.0f) / 2.0f;
    }

    inline float ease_in_elastic(float t) noexcept
    {
        t = math::clamp(0.0f, 1.0f, t);
        if (t == 0.0f)
            return 0.0f;
        if (t == 1.0f)
            return 1.0f;

        const auto c4 = (2.0f * math::Pi) / 3.0f;

        return  -std::pow(2.0f, 10.0f * t - 10.0f) * std::sin((t * 10.0f - 10.75f) * c4);
    }

    inline float ease_out_elastic(float t) noexcept
    {
        t = math::clamp(0.0f, 1.0f, t);
        if (t == 0.0f)
            return 0.0f;
        if (t == 1.0f)
            return 1.0f;

        const auto c4 = (2.0f * math::Pi) / 3.0f;

        return std::pow(2.0f, -10.0f * t) * std::sin((t * 10.0f - 0.75f) * c4) + 1.0f;
    }

    inline float ease_in_out_elastic(float t) noexcept
    {
        t = math::clamp(0.0f, 1.0f, t);
        if (t == 0.0f)
            return 0.0f;
        if (t == 1.0f)
            return 1.0f;

        const auto c5 = (2.0f * math::Pi) / 4.5f;

        return  t < 0.5f
                ? -(std::pow(2.0f,  20.0f * t - 10.0f) * std::sin((20.0f * t - 11.125f) * c5)) / 2.0f
                :  (std::pow(2.0f, -20.0f * t + 10.0f) * std::sin((20.0f * t - 11.125f) * c5)) / 2.0f + 1.0f;
    }

    inline float ease_out_bounce(float t) noexcept
    {
        const auto n1 = 7.5625f;
        const auto d1 = 2.75f;
        if (t < 1.0f / d1)
            return n1 * t * t;
        if (t < 2.0f / d1)
        {
            const auto x = t - 1.5f/d1;
            return n1 * x * x + 0.75f;
        }
        if (t < 2.5f / d1)
        {
            const auto x = t - 2.25f/d1;
            return n1 * x * x + 0.9375f;
        }
        const auto x = t - 2.625f/d1;
        return n1 * x * x + 0.984375f;
    }
    inline float ease_in_bounce(float t) noexcept
    {
        return 1.0f - ease_out_bounce(1.0f - t);
    }
    inline float ease_in_out_bounce(float t) noexcept
    {
        return t < 0.5f
               ? (1.0f - ease_out_bounce(1.0f - 2.0f * t)) / 2.0f
               : (1.0f + ease_out_bounce(2.0f * t - 1.0f)) / 2.0f;
    }

    enum class Curve {
        // No interpolation, a discrete jump from y0 to y1 when t > 0.0
        StepStart,
        // No interpolation, a discrete jump from y0 to y1 when t is >= 0.5.
        Step,
        // No interpolation, a discrete jump from y0 to y1 when t is >= 1.0
        StepEnd,
        // Linear interpolation also known as "lerp". Take a linear mix
        // of y0 and y1 in exact proportions per t.
        Linear,
        // Use cosine function to smooth t before doing linear interpolation.
        Cosine,
        // Use a polynomial function to smooth t before doing linear interpolation.
        SmoothStep,
        // Accelerate increase in y1 value when t approaches 1.0f
        Acceleration,
        // Decelerate increase in y1 value when t approaches 1.0f
        Deceleration,
        // Gentle ease in using a sine curve. Starts slow, finishes at full speed.
        EaseInSine,
        // Gentle ease out using a sine curve. Starts at full speed, finishes slow.
        EaseOutSine,
        // Gentle ease in-out using a sine curve. Slow at both ends, fastest in the middle.
        EaseInOutSine,
        // Ease in using a quadratic (t²) curve. Starts slow, accelerates.
        EaseInQuadratic,
        // Ease out using a quadratic curve. Decelerates into the end value.
        EaseOutQuadratic,
        // Ease in-out using a quadratic curve. Slow at both ends, faster than sine in the middle.
        EaseInOutQuadratic,
        // Ease in using a cubic (t³) curve. Stronger acceleration than quadratic.
        EaseInCubic,
        // Ease out using a cubic curve. Stronger deceleration than quadratic.
        EaseOutCubic,
        // Ease in-out using a cubic curve. More pronounced slow/fast contrast than quadratic.
        EaseInOutCubic,
        // Ease in with a brief backward pull before accelerating forward (anticipation).
        EaseInBack,
        // Ease out with a slight overshoot past the target before settling (follow-through).
        EaseOutBack,
        // Ease in-out with backward pull at the start and overshoot at the end.
        EaseInOutBack,
        // Spring-like oscillation at the beginning before moving toward the target.
        EaseInElastic,
        // Spring-like oscillation around the target value at the end.
        EaseOutElastic,
        // Spring-like oscillation at both the start and end of the transition.
        EaseInOutElastic,
        // Bouncing motion at the start, as if the value bounces off the origin.
        EaseInBounce,
        // Bouncing motion at the end, like a ball landing and bouncing to rest.
        EaseOutBounce,
        // Bouncing motion at both the start and end of the transition.
        EaseInOutBounce
    };

    inline float Ease(const float t, const Curve curve) noexcept
    {
        if (curve == Curve::StepStart)
            return step_start(t);
        if (curve == Curve::Step)
            return step(t);
        if (curve == Curve::StepEnd)
            return step_end(t);
        if (curve == Curve::Linear)
            return math::clamp(0.0f, 1.0f, t);
        if (curve == Curve::Cosine)
            return cosine(t);
        if (curve == Curve::SmoothStep)
            return smooth_step(t);
        if (curve == Curve::Acceleration)
            return acceleration(t);
        if (curve == Curve::Deceleration)
            return deceleration(t);
        if (curve == Curve::EaseInSine)
            return ease_in_sine(t);
        if (curve == Curve::EaseOutSine)
            return ease_out_sine(t);
        if (curve == Curve::EaseInOutSine)
            return ease_in_out_sine(t);
        if (curve == Curve::EaseInQuadratic)
            return ease_in_quadratic(t);
        if (curve == Curve::EaseOutQuadratic)
            return ease_out_quadratic(t);
        if (curve == Curve::EaseInOutQuadratic)
            return ease_in_out_quadratic(t);
        if (curve == Curve::EaseInCubic)
            return ease_in_cubic(t);
        if (curve == Curve::EaseOutCubic)
            return ease_out_cubic(t);
        if (curve == Curve::EaseInOutCubic)
            return ease_in_out_cubic(t);
        if (curve == Curve::EaseInBack)
            return ease_in_back(t);
        if (curve == Curve::EaseOutBack)
            return ease_out_back(t);
        if (curve == Curve::EaseInOutBack)
            return ease_in_out_back(t);
        if (curve == Curve::EaseInElastic)
            return ease_in_elastic(t);
        if (curve == Curve::EaseOutElastic)
            return ease_out_elastic(t);
        if (curve == Curve::EaseInOutElastic)
            return ease_in_out_elastic(t);
        if (curve == Curve::EaseInBounce)
            return ease_in_bounce(t);
        if (curve == Curve::EaseOutBounce)
            return ease_out_bounce(t);
        if (curve == Curve::EaseInOutBounce)
            return ease_in_out_bounce(t);

        BUG("No such easing curve.");
        return t;
    }

    template<typename T>
    T Lerp(const T& y0, const T& y1, const float t, const Curve curve) noexcept
    {
        return math::lerp(y0, y1, Ease(t, curve));
    }

} // namespace
