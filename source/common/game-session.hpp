#pragma once

#include <algorithm>

namespace our
{
    enum class EndingOutcome
    {
        None,
        Win,
        Lose
    };

    struct GameSession
    {
        static inline EndingOutcome endingOutcome = EndingOutcome::None;
        static inline float finalExposure = 1.0f;

        static void clear()
        {
            endingOutcome = EndingOutcome::None;
            finalExposure = 1.0f;
        }

        static void setEndingResult(EndingOutcome outcome, float exposure)
        {
            endingOutcome = outcome;
            finalExposure = std::clamp(exposure, 0.0f, 2.0f);
        }
    };
}
