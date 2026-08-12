#include <gtest/gtest.h>
#include "risk/limits.hpp"

using namespace risk;

TEST(RiskGuard, PositionWithinLimit)
{
    RiskGuard g({100.0, 50.0});
    EXPECT_TRUE(g.check(50.0, -10.0));
}

TEST(RiskGuard, PositionAtLimit)
{
    RiskGuard g({100.0, 50.0});
    EXPECT_TRUE(g.check(100.0, 0.0));
}

TEST(RiskGuard, PositionExceedsLimit)
{
    RiskGuard g({100.0, 50.0});
    EXPECT_FALSE(g.check(150.0, 0.0));
}

TEST(RiskGuard, NegativePositionExceedsLimit)
{
    RiskGuard g({100.0, 50.0});
    EXPECT_FALSE(g.check(-150.0, 0.0));
}

TEST(RiskGuard, DrawdownWithinLimit)
{
    RiskGuard g({100.0, 50.0});
    EXPECT_TRUE(g.check(0.0, -50.0));
}

TEST(RiskGuard, DrawdownExceedsLimit)
{
    RiskGuard g({100.0, 50.0});
    EXPECT_FALSE(g.check(0.0, -60.0));
}

TEST(RiskGuard, BothBreached)
{
    RiskGuard g({100.0, 50.0});
    EXPECT_FALSE(g.check(200.0, -100.0));
}
