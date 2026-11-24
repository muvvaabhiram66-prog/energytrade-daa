// Matching engines: heap-based greedy (O(n log n)) and naive (O(n^2)). Pricing rule: trade at SELLER's ask.
#pragma once
#include <vector>
#include <string>
#include <algorithm>
#include <random>
#include <cmath>
#include <chrono>
struct Order { int id; std::string party; double price; int qty; int orig; };
struct Trade { int id; std::string buyer, seller; double price; int qty; };
// Max-heap on bids (highest price, then earliest id); Min-heap on asks (lowest price, then earliest id)
struct BidCmp { const std::vector<Order>* v; bool operator()(int a, int b) const {
  const Order &x = (*v)[a], &y = (*v)[b]; return x.price < y.price || (x.price == y.price && x.id > y.id); } };
struct AskCmp { const std::vector<Order>* v; bool operator()(int a, int b) const {
  const Order &x = (*v)[a], &y = (*v)[b]; return x.price > y.price || (x.price == y.price && x.id > y.id); } };

inline std::vector<int> buildHeap(const std::vector<Order>& v, bool bids) {
  std::vector<int> h; for (size_t i = 0; i < v.size(); i++) if (v[i].qty > 0) h.push_back((int)i);
  if (bids) std::make_heap(h.begin(), h.end(), BidCmp{&v}); else std::make_heap(h.begin(), h.end(), AskCmp{&v});
  return h;
}
// Greedy double auction. B,S are updated in place (remaining qty). Optional step log.
inline std::vector<Trade> heapMatch(std::vector<Order>& B, std::vector<Order>& S, int& tid, std::vector<std::string>* log = nullptr) {
  std::vector<Trade> out; BidCmp bc{&B}; AskCmp ac{&S};
  auto bh = buildHeap(B, true); auto sh = buildHeap(S, false);
  while (!bh.empty() && !sh.empty()) {
    int bi = bh.front(), si = sh.front();                      // highest bid, lowest ask (heap roots)
    bool ok = B[bi].price >= S[si].price;
    if (log && log->size() < 40) log->push_back("bid " + std::to_string(B[bi].price).substr(0, 5) + " >= ask " + std::to_string(S[si].price).substr(0, 5) + " ? " + (ok ? "YES -> trade" : "NO -> stop"));
    if (!ok) break;
    std::pop_heap(bh.begin(), bh.end(), bc); bh.pop_back();
    std::pop_heap(sh.begin(), sh.end(), ac); sh.pop_back();
    int q = std::min(B[bi].qty, S[si].qty);
    out.push_back({tid++, B[bi].party, S[si].party, S[si].price, q});
    B[bi].qty -= q; S[si].qty -= q;
    if (B[bi].qty > 0) { bh.push_back(bi); std::push_heap(bh.begin(), bh.end(), bc); }   // reinsert partial fill
    if (S[si].qty > 0) { sh.push_back(si); std::push_heap(sh.begin(), sh.end(), ac); }
  }
  return out;
}
// Naive: every iteration linearly scans ALL orders for best bid and best ask -> O(n) per trade, O(n^2) total.
// (Scans compact arrays for cache efficiency; the asymptotic behaviour is unchanged.)
inline std::vector<Trade> naiveMatch(std::vector<Order>& B, std::vector<Order>& S, int& tid, double maxMs = 0, bool* capped = nullptr) {
  auto t0 = std::chrono::steady_clock::now(); std::vector<Trade> out; size_t nb = B.size(), ns = S.size();
  std::vector<double> bp(nb), sp(ns); std::vector<int> bq(nb), sq(ns), bi_(nb), si_(ns);
  for (size_t i = 0; i < nb; i++) { bp[i] = B[i].price; bq[i] = B[i].qty; bi_[i] = B[i].id; }
  for (size_t i = 0; i < ns; i++) { sp[i] = S[i].price; sq[i] = S[i].qty; si_[i] = S[i].id; }
  while (true) {
    if (maxMs > 0 && (out.size() & 63) == 0 && std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count() > maxMs) { if (capped) *capped = true; break; }
    int bi = -1, si = -1;
    for (size_t i = 0; i < nb; i++) if (bq[i] > 0 && (bi < 0 || bp[i] > bp[bi] || (bp[i] == bp[bi] && bi_[i] < bi_[bi]))) bi = (int)i;
    for (size_t i = 0; i < ns; i++) if (sq[i] > 0 && (si < 0 || sp[i] < sp[si] || (sp[i] == sp[si] && si_[i] < si_[si]))) si = (int)i;
    if (bi < 0 || si < 0 || bp[bi] < sp[si]) break;
    int q = std::min(bq[bi], sq[si]);
    out.push_back({tid++, B[bi].party, S[si].party, sp[si], q});
    bq[bi] -= q; sq[si] -= q;
  }
  for (size_t i = 0; i < nb; i++) B[i].qty = bq[i];
  for (size_t i = 0; i < ns; i++) S[i].qty = sq[i];
  return out;
}
// Synthetic order generator: prices ~ N(6,1) for bids, N(5,1) for asks (Rs/kWh); qty uniform 1..50 kWh
inline void genOrders(std::vector<Order>& v, int n, bool buy, int& id, std::mt19937& rng) {
  std::normal_distribution<double> pd(buy ? 6.0 : 5.0, 1.0); std::uniform_int_distribution<int> qd(1, 50);
  v.reserve(v.size() + n);
  for (int i = 0; i < n; i++) { double p = std::max(1.0, std::round(pd(rng) * 100) / 100); int q = qd(rng);
    v.push_back({id, std::string(buy ? "C" : "P") + std::to_string(id), p, q, q}); id++; }
}
