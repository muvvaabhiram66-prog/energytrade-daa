# EnergyTrade — Double-Auction Matching Engine for a P2P Energy Market (DAA Project)
Greedy matching of buy bids and sell asks with a Max-Heap and a Min-Heap, benchmarked against a naive O(n²) matcher.

## Structure
- `backend/engine.hpp` — heap matcher, naive matcher, synthetic order generator
- `backend/main.cpp` — REST API (cpp-httplib) + static file server
- `frontend/index.html` — website (HTML/CSS/JS, Chart.js)
- `database/schema.sql` — MySQL schema (users, buy_orders, sell_orders, trades)
- `docs/presentation`, `docs/report` — put slides and report here

## Run
1. Download `httplib.h` from https://github.com/yhirose/cpp-httplib into `backend/`.
2. From the project root: `g++ -O2 -std=c++17 -pthread backend/main.cpp -o energytrade`
3. `./energytrade` then open http://localhost:8080

## API
GET `/api/stats`, `/api/orderbook`, `/api/trades` · POST `/api/orders` (side, party, price, qty), `/api/generate` (n), `/api/clear`, `/api/match`, `/api/reset`, `/api/benchmark`

## Pricing rule
Every trade executes at the **seller's ask price**; quantity = min(remaining bid, remaining ask); partially filled orders are reinserted into their heap.

## Known limitations
Orders and trades are held in server memory; `schema.sql` is provided but the server is not yet wired to MySQL.
