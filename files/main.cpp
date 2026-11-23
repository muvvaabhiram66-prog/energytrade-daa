// EnergyTrade REST server. Build: see README. Requires httplib.h (cpp-httplib) in this folder.
#include "httplib.h"
#include "engine.hpp"
#include <mutex>
#include <chrono>
#include <cstdio>
#include <cstdlib>
using namespace std;
static vector<Order> buys, sells; static vector<Trade> trades; static int nextId = 1, nextTid = 1; static mutex mx;
static mt19937 rng(random_device{}());
static string esc(const string& s) { string r; for (char c : s) { if (c == '"' || c == '\\') r += '\\'; if ((unsigned char)c >= 32) r += c; } return r; }
static string num(double d) { char b[48]; snprintf(b, 48, "%.2f", d); return b; }
static string oj(const Order& o) { return "{\"id\":" + to_string(o.id) + ",\"party\":\"" + esc(o.party) + "\",\"price\":" + num(o.price) + ",\"qty\":" + to_string(o.qty) + ",\"orig\":" + to_string(o.orig) + "}"; }
static string sortedOpen(const vector<Order>& v, bool bids, size_t lim, bool& any, double& best) {
  vector<const Order*> p; for (auto& o : v) if (o.qty > 0) p.push_back(&o);
  auto cmp = [&](const Order* a, const Order* b) { if (a->price != b->price) return bids ? a->price > b->price : a->price < b->price; return a->id < b->id; };
  size_t k = min(lim, p.size()); partial_sort(p.begin(), p.begin() + k, p.end(), cmp);
  any = !p.empty(); best = any ? p[0]->price : 0; string s = "[";
  for (size_t i = 0; i < k; i++) s += (i ? "," : "") + oj(*p[i]); return s + "]";
}
static string heapJson(const vector<Order>& v, bool bids) {
  auto h = buildHeap(v, bids); string s = "["; for (size_t i = 0; i < min<size_t>(15, h.size()); i++) s += (i ? "," : "") + oj(v[h[i]]); return s + "]";
}
static string stats() {
  double en = 0, val = 0; for (auto& t : trades) { en += t.qty; val += t.qty * t.price; }
  int full = 0, part = 0, none = 0; for (auto* v : {&buys, &sells}) for (auto& o : *v) { if (o.qty == 0) full++; else if (o.qty < o.orig) part++; else none++; }
  return "{\"buys\":" + to_string(buys.size()) + ",\"sells\":" + to_string(sells.size()) + ",\"matched\":" + to_string(full) + ",\"partial\":" + to_string(part) +
    ",\"unmatched\":" + to_string(none) + ",\"trades\":" + to_string(trades.size()) + ",\"energy\":" + num(en) + ",\"value\":" + num(val) + ",\"avgPrice\":" + num(en > 0 ? val / en : 0) + "}";
}
static void json(httplib::Response& r, const string& s, int code = 200) { r.status = code; r.set_content(s, "application/json"); }
static double now() { return chrono::duration<double, milli>(chrono::steady_clock::now().time_since_epoch()).count(); }

int main() {
  httplib::Server svr;
  svr.set_mount_point("/", "./frontend");
  svr.Get("/api/stats", [](const httplib::Request&, httplib::Response& res) { lock_guard<mutex> l(mx); json(res, stats()); });
  svr.Post("/api/orders", [](const httplib::Request& req, httplib::Response& res) {
    string side = req.get_param_value("side"), party = req.get_param_value("party");
    double price = atof(req.get_param_value("price").c_str()); double qd = atof(req.get_param_value("qty").c_str());
    if ((side != "buy" && side != "sell") || party.empty() || party.size() > 30) return json(res, "{\"error\":\"Invalid ID or side\"}", 400);
    if (!(price > 0) || !(qd >= 1) || qd != floor(qd) || qd > 1e6) return json(res, "{\"error\":\"Price must be > 0 and quantity a positive whole number\"}", 400);
    lock_guard<mutex> l(mx); Order o{nextId++, party, price, (int)qd, (int)qd};
    (side == "buy" ? buys : sells).push_back(o); json(res, "{\"ok\":true,\"order\":" + oj(o) + "}");
  });
  svr.Get("/api/orderbook", [](const httplib::Request&, httplib::Response& res) {
    lock_guard<mutex> l(mx); bool a, b; double bb, ba;
    string B = sortedOpen(buys, true, 50, a, bb); string S = sortedOpen(sells, false, 50, b, ba);
    json(res, "{\"buys\":" + B + ",\"sells\":" + S + ",\"bestBid\":" + (a ? num(bb) : "null") + ",\"bestAsk\":" + (b ? num(ba) : "null") + "}");
  });
  svr.Post("/api/generate", [](const httplib::Request& req, httplib::Response& res) {
    int n = atoi(req.get_param_value("n").c_str()); if (n < 2 || n > 100000) return json(res, "{\"error\":\"n must be 2..100000\"}", 400);
    lock_guard<mutex> l(mx); double t = now(); genOrders(buys, n / 2, true, nextId, rng); genOrders(sells, n - n / 2, false, nextId, rng);
    json(res, "{\"ok\":true,\"ms\":" + num(now() - t) + ",\"buys\":" + to_string(n / 2) + ",\"sells\":" + to_string(n - n / 2) + "}");
  });
  svr.Post("/api/clear", [](const httplib::Request&, httplib::Response& res) { lock_guard<mutex> l(mx); buys.clear(); sells.clear(); trades.clear(); json(res, "{\"ok\":true}"); });
  svr.Post("/api/match", [](const httplib::Request&, httplib::Response& res) {
    lock_guard<mutex> l(mx); string hb = heapJson(buys, true), hs = heapJson(sells, false); vector<string> log;
    double t = now(); auto nt = heapMatch(buys, sells, nextTid, &log); double ms = now() - t;
    double en = 0, val = 0; string tl = "["; for (auto& x : nt) { en += x.qty; val += x.qty * x.price; }
    for (size_t i = 0; i < min<size_t>(nt.size(), 30); i++) tl += string(i ? "," : "") + "{\"id\":" + to_string(nt[i].id) + ",\"buyer\":\"" + esc(nt[i].buyer) + "\",\"seller\":\"" + esc(nt[i].seller) + "\",\"price\":" + num(nt[i].price) + ",\"qty\":" + to_string(nt[i].qty) + "}";
    for (auto& x : nt) trades.push_back(x);
    string lg = "["; for (size_t i = 0; i < log.size(); i++) lg += string(i ? "," : "") + "\"" + esc(log[i]) + "\"";
    json(res, "{\"newTrades\":" + to_string(nt.size()) + ",\"energy\":" + num(en) + ",\"value\":" + num(val) + ",\"ms\":" + num(ms) + ",\"heapBuy\":" + hb + ",\"heapSell\":" + hs +
      ",\"sample\":" + tl + "],\"log\":" + lg + "],\"stats\":" + stats() + "}");
  });
  svr.Post("/api/reset", [](const httplib::Request&, httplib::Response& res) { lock_guard<mutex> l(mx); for (auto* v : {&buys, &sells}) for (auto& o : *v) o.qty = o.orig; trades.clear(); nextTid = 1; json(res, "{\"ok\":true}"); });
  svr.Get("/api/trades", [](const httplib::Request&, httplib::Response& res) {
    lock_guard<mutex> l(mx); string s = "{\"stats\":" + stats() + ",\"trades\":["; size_t st = trades.size() > 2000 ? trades.size() - 2000 : 0;
    for (size_t i = st; i < trades.size(); i++) { auto& t = trades[i]; s += string(i > st ? "," : "") + "{\"id\":" + to_string(t.id) + ",\"buyer\":\"" + esc(t.buyer) + "\",\"seller\":\"" + esc(t.seller) + "\",\"price\":" + num(t.price) + ",\"qty\":" + to_string(t.qty) + "}"; }
    json(res, s + "]}");
  });
  svr.Post("/api/benchmark", [](const httplib::Request&, httplib::Response& res) {  // identical datasets for both algorithms
    string s = "["; int first = 1; for (int n : {100, 1000, 10000, 100000}) {
      vector<Order> B, S; int id = 1; mt19937 r(n); genOrders(B, n / 2, true, id, r); genOrders(S, n / 2, false, id, r);
      auto B2 = B, S2 = S; int t1 = 1, t2 = 1;
      double a = now(); auto nv = naiveMatch(B, S, t1); double b = now(); auto hp = heapMatch(B2, S2, t2); double c = now();
      s += string(first ? "" : ",") + "{\"n\":" + to_string(n) + ",\"naiveMs\":" + to_string(b - a) + ",\"heapMs\":" + to_string(c - b) + ",\"trades\":" + to_string(hp.size()) + ",\"agree\":" + (nv.size() == hp.size() ? "true" : "false") + "}"; first = 0;
    }
    json(res, s + "]");
  });
  const char* pe = getenv("PORT"); int port = pe ? atoi(pe) : 8080;
  printf("EnergyTrade running on port %d\n", port); svr.listen("0.0.0.0", port);
}
