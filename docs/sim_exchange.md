# Sim Exchange & Market Replay

This document captures the Phase 3 design for the deterministic replay pipeline and the matching logic used to turn a spread-crossing order book into a trade log.

## Replay pipeline

`MarketReplay` reads a CSV file in timestamp order and replays each event into the same `OrderBook` implementation used by the earlier phases. The key design property is determinism: the same CSV file always creates the same book state and the same trade stream. This matters because exchange bugs are reproducible when the input sequence is stable, and debugging becomes a matter of replaying the exact same event set instead of chasing a race.

The CSV format is intentionally minimal:

```csv
timestamp_ns,side,price,qty,action
1700000000000,BID,100.00,50,INSERT
1700000000100,ASK,100.10,30,INSERT
1700000000200,BID,100.00,0,DELETE
```

The replay loop is intentionally sequential. We are not trying to simulate an exchange in parallel yet; we are validating the matching model and the event-to-book pipeline first.

## Crossing logic

A crossing condition is defined by the book state itself:

- for a valid book, a bid is crossing if it is at or above the best ask
- an ask is crossing if it is at or below the best bid

When the replay sees a crossing, it asks `SimExchange` to resolve it. The exchange matches against the resting side at the best price and consumes the full level in the aggregate-model v1. This is intentionally simpler than per-order queue matching, but it preserves the essential property: queue priority is respected at the level, and the trade is emitted at the resting price.

In other words, the engine is still price-priority aware because it always consumes the best resting level first; the only limitation is that we do not yet represent individual order IDs, fill timestamps, or per-order queue sequencing.

## Why aggregate-level matching is the right Phase 3 cut

The repository intentionally separates the book from the exchange. The order book tracks the state of price levels as aggregate quantity snapshots; the simulator owns the decision of what a crossing event means. This is the correct architecture because it keeps the book data structure simple and cache-friendly while deferring all matching semantics to a higher layer.

This makes future extensions straightforward:

- add order IDs and queue arrays for true price-time priority
- add partial fills and aggressor quantity caps
- add synthetic market replay from historical L2 event streams

## Lifecycle of one fill

1. The replay loads a CSV event into the book.
2. The book state is checked for crossing.
3. If a crossing exists, `match_crossing` resolves against the best resting level.
4. The matching side's level is drained or reduced to zero.
5. A `Trade` record is appended to the trade log with the aggressor side and resting price.

This is small, testable, and deterministic — exactly the shape we want before adding more complexity.
