# Limit Order Book Simulator
A high-performance limit order book (LOB) simulator that models market microstructure dynamics.  
Implements price–time priority, matching engine logic, and stochastic market event generation.

## Features
- **Core order book engine**  
  - Price–time priority queues  
  - FIFO order matching  
  - Limit/market orders, cancels, partial fills

- **Market dynamics**  
  - Event-driven simulation  
  - Poisson arrivals for order flow  
  - Configurable low/high-volatility regimes (Markov switching)  
  - Price offsets drawn from geometric/Laplace distributions

- **Metrics & analysis**  
  - Trade volume, spreads, slippage  
  - Limit order fill ratios (by distance from mid)  
  - Customizable logging and snapshots

## Build and Run
```bash
git clone https://github.com/YOUR_USERNAME/lob-sim.git
cd lob-sim
cmake -S . -B build
cmake --build build -j
./build/lob_sim
