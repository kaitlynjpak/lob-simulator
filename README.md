# LOB Simulator (C++)

Event-driven limit order book simulator with:
- Matching engine (price-time priority)
- Strategies (market maker, momentum, pairs)
- Metrics (PnL, inventory, Sharpe, drawdown)

## Build
```bash
mkdir -p build && cd build
cmake ..
cmake --build .
```

## To run default demo
```bash
./lob_sim
```

## To run event-driven simulator
```bash
./lob_sim --run-sim --events N --seed 42
//replace N with some number
```
