// Risk limits and guard for liquid-book
#pragma once

#include <cmath>

namespace risk
{
    struct RiskLimits
    {
        double max_position{0.0}; // absolute position limit
        double max_drawdown{0.0}; // positive value: maximum allowed drawdown
    };

    class RiskGuard
    {
    public:
        explicit RiskGuard(RiskLimits limits) : limits_(limits) {}

        // Returns true if the new state is within limits, false if a limit is breached.
        bool check(double new_position, double current_pnl) const noexcept
        {
            if (limits_.max_position > 0.0)
            {
                if (std::fabs(new_position) > limits_.max_position)
                {
                    return false;
                }
            }
            if (limits_.max_drawdown > 0.0)
            {
                // current_pnl must not fall below -max_drawdown
                if (current_pnl < -limits_.max_drawdown)
                {
                    return false;
                }
            }
            return true;
        }

    private:
        RiskLimits limits_;
    };

} // namespace risk
