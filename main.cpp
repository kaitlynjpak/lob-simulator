#include <iostream>
#include <stdexcept>
#include "order_book.hpp"
#include "matching_engine.hpp"
#include "sim.hpp"
#include <cstring>

// Simple pretty-printers for quick sanity checks
static void dump_side(const char* name,
                      const std::map<Price, LevelQueue, std::less<Price>>& asks) {
  std::cout << name << " (low→high):\n";
  for (const auto& [px, q] : asks) {
    std::cout << "  " << px << " : [";
    for (size_t i = 0; i < q.size(); ++i) {
      std::cout << q[i].id << ":" << q[i].qty << (i + 1 < q.size() ? ", " : "");
    }
    std::cout << "]\n";
  }
}

static void dump_side(const char* name,
                      const std::map<Price, LevelQueue, std::greater<Price>>& bids) {
  std::cout << name << " (high→low):\n";
  for (const auto& [px, q] : bids) {
    std::cout << "  " << px << " : [";
    for (size_t i = 0; i < q.size(); ++i) {
      std::cout << q[i].id << ":" << q[i].qty << (i + 1 < q.size() ? ", " : "");
    }
    std::cout << "]\n";
  }
}

static void dump_book(const OrderBook& ob) {
  std::cout << "================ BOOK ================\n";
  dump_side("ASKS", ob.asks);
  dump_side("BIDS", ob.bids);
  std::cout << "best_bid=" << ob.best_bid()
            << " best_ask=" << ob.best_ask()
            << " mid="      << ob.mid() << "\n";
  std::cout << "======================================\n";
}

static void dump_fills(const std::vector<Fill>& fills) {
  for (const auto& f : fills) {
    std::cout << "TRADE taker=" << f.taker_id
              << " maker=" << f.maker_id
              << " side="  << (f.taker_side == Side::Buy ? "B" : "S")
              << " px="    << f.price
              << " qty="   << f.qty
              << " t="     << f.ts << "\n";
  }
  if (fills.empty()) std::cout << "(no trades)\n";
}

int main(int argc, char** argv) {
    // --- CLI flags ---
  bool run_sim = false;
  size_t max_events = 200000;
  uint64_t seed = 42;

  for (int i = 1; i < argc; ++i) {
    if (!std::strcmp(argv[i], "--run-sim")) run_sim = true;
    else if (!std::strcmp(argv[i], "--events") && i + 1 < argc) max_events = std::stoull(argv[++i]);
    else if (!std::strcmp(argv[i], "--seed") && i + 1 < argc) seed = std::stoull(argv[++i]);
  }

  if (run_sim) {
    // ---- Milestone 3 simulation run ----
    SimConfig sc;
    sc.seed            = 42;
    sc.max_events      = 200000;
    sc.snapshot_every  = 0;      // heartbeat off
    sc.log_trades      = false;  // leave off for speed

    // regime switching
    sc.regime.p_LL     = 0.995;  // stay-low probability
    sc.regime.p_HH     = 0.990;  // stay-high probability
    sc.regime.low.lambda  = 800.0;
    sc.regime.high.lambda = 2000.0;

    // low regime
    sc.regime.low.mix.p_limit_buy  = 0.35;
    sc.regime.low.mix.p_limit_sell = 0.35;
    sc.regime.low.mix.p_mkt_buy    = 0.10;
    sc.regime.low.mix.p_mkt_sell   = 0.10;

    // high regime
    sc.regime.high.mix.p_limit_buy  = 0.28;
    sc.regime.high.mix.p_limit_sell = 0.28;
    sc.regime.high.mix.p_mkt_buy    = 0.18;
    sc.regime.high.mix.p_mkt_sell   = 0.18;

    // qty distribution means
    sc.mean_limit_qty  = 50.0;
    sc.mean_market_qty = 50.0;

    // price model
    sc.initial_mid_ticks = 10000;
    sc.min_price_ticks   = 1;
    sc.max_offset_ticks  = 50;
    sc.geolap_alpha      = 0.15; 
    sc.keep_cross_prob   = 0.15;

    // run M3
    Simulator sim(sc);
    sim.run();
    std::cout << "=== SIM DONE ===\n";
  }
  
  OrderBook ob;

  auto make_order = [](OrderId id, Side side, Price px, Qty qty, TimePoint ts) {
    Order o;
    o.id = id;
    o.side = side;
    o.type = OrdType::Limit;
    o.limit_price = px;
    o.qty = qty;
    o.ts = ts;
    return o;
  };

  // Add some bids/asks
  ob.add_limit(make_order(101, Side::Buy, 100, 5, 0.10));
  ob.add_limit(make_order(102, Side::Buy, 100, 3, 0.20));
  ob.add_limit(make_order(103, Side::Buy,  99, 7, 0.30));
  ob.add_limit(make_order(201, Side::Sell, 102, 4, 0.15));
  ob.add_limit(make_order(202, Side::Sell, 103, 6, 0.25));
  ob.add_limit(make_order(203, Side::Sell, 102, 2, 0.35));

  if (!ob.self_check()) { std::cerr << "self_check failed after adds!\n"; return 1; } // <---

  std::cout << "After adds:\n";
  dump_book(ob);

  // Cancels
  ob.cancel(102);
  ob.cancel(201);

  if (!ob.self_check()) { std::cerr << "self_check failed after cancels!\n"; return 1; } // <---

  std::cout << "\nAfter cancels (102, 201):\n";
  dump_book(ob);

  // Cancel non-existent
  ob.cancel(999);

  if (!ob.self_check()) { std::cerr << "self_check failed after cancel(999)!\n"; return 1; } // <---

  std::cout << "\nAfter cancel(999) (no-op):\n";
  dump_book(ob);

  // Duplicate ID (should throw)
try {
  ob.add_limit(make_order(101, Side::Buy, 100, 1, 0.5)); // duplicate id
  std::cerr << "Expected duplicate ID exception!\n";
} catch (const std::invalid_argument&) { /* ok */ }

// Cancel non-existent (no-op)
ob.cancel(424242); 
if (!ob.self_check()) return 1;

// Cancel the last remaining order at a level (level should disappear)
ob.add_limit(make_order(300, Side::Sell, 105, 2, 1.0));
ob.cancel(300);
if (ob.asks.count(105) != 0) { std::cerr << "level not erased\n"; return 1; }

std::cout << "\n===== M2: Matching Engine Demo =====\n";
MatchingEngine me(ob);

  // Seed book with some resting orders
  OrderId id1 = 1;
  ob.add_limit({id1++, Side::Sell, OrdType::Limit, 101, 5, 0.1});
  ob.add_limit({id1++, Side::Sell, OrdType::Limit, 102, 3, 0.2});
  ob.add_limit({id1++, Side::Buy,  OrdType::Limit,  99, 4, 0.3});
  ob.add_limit({id1++, Side::Buy,  OrdType::Limit, 100, 6, 0.4});

  std::cout << "Initial book:\n";
  dump_book(ob);

  // Crossing BUY limit @ 102 for 8 units
  std::vector<Fill> fills1;
  me.submit_limit(Side::Buy, 102, 8, 1.0, fills1);

  std::cout << "\nAfter BUY limit @102 x8:\n";
  dump_fills(fills1);
  dump_book(ob);

  // Market SELL for 7 units
  std::vector<Fill> fills2;
  me.submit_market(Side::Sell, 7, 2.0, fills2);

  std::cout << "\nAfter MARKET SELL x7:\n";
  dump_fills(fills2);
  dump_book(ob);


 // ----- M3: Stochastic Simulator -----
  // {
  //   std::cout << "\n===== M3: Market Dynamics (stochastic) =====\n";

  //   SimConfig sc{};
  //   sc.seed             = 42;
  //   sc.max_events       = 300;
  //   sc.snapshot_every   = 50;

  //   sc.regime.p_LL      = 0.995;
  //   sc.regime.p_HH      = 0.990;

  //   sc.regime.low.lambda  = 800.0;
  //   sc.regime.high.lambda = 2000.0;

  //   // low regime event probabilities
  //   sc.regime.low.mix.p_limit_buy  = 0.35;
  //   sc.regime.low.mix.p_limit_sell = 0.35;
  //   sc.regime.low.mix.p_mkt_buy    = 0.10;
  //   sc.regime.low.mix.p_mkt_sell   = 0.10;

  //   // high regime event probabilities
  //   sc.regime.high.mix.p_limit_buy  = 0.28;
  //   sc.regime.high.mix.p_limit_sell = 0.28;
  //   sc.regime.high.mix.p_mkt_buy    = 0.18;
  //   sc.regime.high.mix.p_mkt_sell   = 0.18;

  //   sc.mean_limit_qty   = 50.0;
  //   sc.mean_market_qty  = 50.0;

  //   sc.initial_mid_ticks= 10000;
  //   sc.min_price_ticks  = 1;
  //   sc.max_offset_ticks = 20;
  //   sc.geolap_alpha     = 0.15;
  //   sc.keep_cross_prob  = 0.15;

  //   sc.log_trades = false;
    
  //   Simulator sim(sc);
  //   sim.run();
  // }

  {
  std::cout << "\n===== M3 sweeps =====\n";
  auto base = [&](){
    SimConfig sc{};
    sc.seed             = 42;
    sc.max_events       = 50000;      // keep small for quick sweeps
    sc.snapshot_every   = 0;          // disable snapshots for speed
    sc.regime.p_LL      = 0.995;
    sc.regime.p_HH      = 0.990;
    sc.regime.low.lambda  = 800.0;
    sc.regime.high.lambda = 2000.0;
    sc.regime.low.mix.p_limit_buy  = 0.35;
    sc.regime.low.mix.p_limit_sell = 0.35;
    sc.regime.low.mix.p_mkt_buy    = 0.10;
    sc.regime.low.mix.p_mkt_sell   = 0.10;
    sc.regime.high.mix.p_limit_buy  = 0.28;
    sc.regime.high.mix.p_limit_sell = 0.28;
    sc.regime.high.mix.p_mkt_buy    = 0.18;
    sc.regime.high.mix.p_mkt_sell   = 0.18;
    sc.mean_limit_qty   = 50.0;
    sc.mean_market_qty  = 50.0;
    sc.initial_mid_ticks= 10000;
    sc.min_price_ticks  = 1;
    sc.max_offset_ticks = 20;
    sc.geolap_alpha     = 0.15;
    sc.keep_cross_prob  = 0.15;
    sc.log_trades       = false;
    return sc;
  };

  auto run = [&](const char* label, SimConfig sc){
    std::cout << "\n--- " << label << " ---\n";
    Simulator sim(sc);
    sim.run();
    // Your Simulator::run() already prints summary. If you want a single-line summary:
    // (Optional) Add a method in Simulator to expose metrics and print here.
  };

  // Sweep geolap_alpha (spread tightness)
  for (double a : {0.08, 0.15, 0.30}) {
    auto sc = base();
    sc.geolap_alpha = a;
    run((std::string("alpha=") + std::to_string(a)).c_str(), sc);
  }

  // Sweep keep_cross_prob (aggressiveness)
  for (double p : {0.05, 0.15, 0.35}) {
    auto sc = base();
    sc.keep_cross_prob = p;
    run((std::string("keep_cross_prob=") + std::to_string(p)).c_str(), sc);
  }

  // Sweep max_offset_ticks (tail width)
  for (int m : {5, 20, 50}) {
    auto sc = base();
    sc.max_offset_ticks = m;
    run((std::string("max_offset_ticks=") + std::to_string(m)).c_str(), sc);
  }
}

  return 0;
}