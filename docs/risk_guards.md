# Risk Guards & Exchange Safety Controls

This document captures the design and implementation of the Phase 4 exchange-side safety controls in `liquid-book`.

## Overview

In automated trading systems and electronic exchanges, risk guards act as the final line of defense against catastrophic runaway algorithms, fat-finger orders, or extreme market volatility. `liquid-book` implements lightweight, $O(1)$ exchange-side risk controls using the `RiskGuard` class.

## Core Controls

The `RiskGuard` monitors two primary metrics:
1. **Max Net Position (`max_position`)**: Absolute net inventory limit ($\vert \text{position} \vert \le \text{max\_pos}$). If a trade or sequence of trades causes cumulative net position to exceed this limit (either long or short), the guard triggers.
2. **Max Drawdown (`max_drawdown`)**: Maximum tolerable cumulative realized loss. If realized PnL drops below $-\text{max\_drawdown}$, the guard triggers.

## Performance Requirements

- **$O(1)$ Time Complexity**: Risk checks occur directly in the event processing pathway. Checks involve primitive numeric comparisons only.
- **Zero Allocation**: No heap allocations or dynamic allocations are performed during risk evaluation.

## Interlocking Kill Switch

When a risk threshold is breached:
1. `RiskGuard::check(...)` returns `false`.
2. The caller sets an atomic `kill_switch` flag (`kill_switch.store(true)`).
3. The event producer (e.g. `MarketReplay`) checks `kill_switch` before processing subsequent updates and immediately halts execution.
4. Active consumer threads finish processing queued events and exit cleanly.
