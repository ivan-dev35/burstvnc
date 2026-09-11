#include "srv.hpp"
#include <fstream>
#include <sstream>
#include <chrono>

namespace bst {

static std::string getMime(const std::string& p) {
    auto pos = p.rfind(".");
    if (pos == std::string::npos) return "application/octet-stream";
    std::string e = p.substr(pos);
    if (e == ".html" || e == ".htm") return "text/html";
    if (e == ".css") return "text/css";
    if (e == ".js") return "application/javascript";
    if (e == ".json") return "application/json";
    if (e == ".png") return "image/png";
    if (e == ".svg") return "image/svg+xml";
    if (e == ".ico") return "image/x-icon";
    return "application/octet-stream";
}

Srv::Srv(const std::string& host, uint16_t port, const std::string& root)
    : host_(host), port_(port), root_(root) {}

Srv::~Srv() {
    stop();
}

bool Srv::start() {
    try {
        auto addr = net::ip::make_address(host_);
        acc_ = std::make_unique<tcp::acceptor>(ioc_, tcp::endpoint{addr, port_});
        acc_->set_option(tcp::acceptor::reuse_address(true));
        acc();
        for (int i = 0; i < 4; ++i) {
            ths_.emplace_back([this]() { ioc_.run(); });
        }
        return true;
    } catch (...) {
        return false;
    }
}

void Srv::stop() {
    ioc_.stop();
    for (auto& t : ths_) {
        if (t.joinable()) t.join();
    }
    ths_.clear();
}

void Srv::acc() {
    acc_->async_accept(
        net::make_strand(ioc_),
        [this, self = shared_from_this()](beast::error_code ec, tcp::socket s) {
            if (!ec) {
                s.set_option(tcp::no_delay(true));
                std::make_shared<HttpSes>(std::move(s), self)->run();
            }
            if (acc_->is_open()) acc();
        });
}

void Srv::reg(uint64_t id, std::shared_ptr<Ses> s) {
    {
        std::lock_guard<std::mutex> lk(mtx_);
        ses_[id] = s;
    }
    if (connHdlr_) connHdlr_(id);
}

void Srv::unreg(uint64_t id) {
    std::lock_guard<std::mutex> lk(mtx_);
    ses_.erase(id);
}

void Srv::sendTo(uint64_t id, std::shared_ptr<std::vector<uint8_t>> msg) {
    std::lock_guard<std::mutex> lk(mtx_);
    auto it = ses_.find(id);
    if (it != ses_.end()) {
        it->second->send(msg);
    }
}

void Srv::bcastV(const HdrVid& h, const uint8_t* d, size_t sz) {
    auto m = std::make_shared<std::vector<uint8_t>>(sizeof(HdrVid) + sz);
    std::memcpy(m->data(), &h, sizeof(HdrVid));
    std::memcpy(m->data() + sizeof(HdrVid), d, sz);

    std::lock_guard<std::mutex> lk(mtx_);
    for (auto& pr : ses_) pr.second->send(m);
}

void Srv::bcastA(const HdrAud& h, const uint8_t* d, size_t sz) {
    auto m = std::make_shared<std::vector<uint8_t>>(sizeof(HdrAud) + sz);
    std::memcpy(m->data(), &h, sizeof(HdrAud));
    std::memcpy(m->data() + sizeof(HdrAud), d, sz);

    std::lock_guard<std::mutex> lk(mtx_);
    for (auto& pr : ses_) pr.second->send(m);
}

void Srv::bcastC(const HdrCur& h, const std::vector<uint8_t>& d) {
    auto m = std::make_shared<std::vector<uint8_t>>(sizeof(HdrCur) + d.size());
    std::memcpy(m->data(), &h, sizeof(HdrCur));
    std::memcpy(m->data() + sizeof(HdrCur), d.data(), d.size());

    std::lock_guard<std::mutex> lk(mtx_);
    for (auto& pr : ses_) pr.second->send(m);
}

void Srv::bcastCfg(const HdrCfg& h) {
    auto m = std::make_shared<std::vector<uint8_t>>(sizeof(HdrCfg));
    std::memcpy(m->data(), &h, sizeof(HdrCfg));

    std::lock_guard<std::mutex> lk(mtx_);
    for (auto& pr : ses_) pr.second->send(m);
}

void Srv::onMsg(const uint8_t* d, size_t sz, uint64_t sid) {
    if (sz == 0) return;
    if (d[0] == M_PNG_Q && sz >= sizeof(MsgPng)) {
        const MsgPng* p = reinterpret_cast<const MsgPng*>(d);
        auto now = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch()).count();

        HdrPng pong;
        pong.t = M_PNG;
        pong.c_ts = p->ts;
        pong.s_ts = now;

        auto resp = std::make_shared<std::vector<uint8_t>>(sizeof(HdrPng));
        std::memcpy(resp->data(), &pong, sizeof(HdrPng));
        sendTo(sid, resp);
        return;
    }
    if (inpHdlr_) inpHdlr_(d, sz);
}

Ses::Ses(tcp::socket&& s, std::shared_ptr<Srv> srv, uint64_t id)
    : ws_(std::move(s)), srv_(srv), id_(id) {}

void Ses::run(http::request<http::string_body> req) {
    beast::get_lowest_layer(ws_).socket().set_option(tcp::no_delay(true));
    ws_.binary(true);
    ws_.set_option(websocket::stream_base::timeout::suggested(beast::role_type::server));

    ws_.async_accept(
        req,
        [this, self = shared_from_this()](beast::error_code ec) {
            if (ec) return;
            srv_->reg(id_, self);
            rd();
        });
}

void Ses::send(std::shared_ptr<std::vector<uint8_t>> msg) {
    if (cls_) return;
    std::lock_guard<std::mutex> lk(qMtx_);
    if (q_.size() > 5 && msg->size() > 0 && (*msg)[0] == M_VID) return;

    q_.push_back(msg);
    if (!wrting_) {
        wrting_ = true;
        wrt();
    }
}

void Ses::wrt() {
    if (cls_ || q_.empty()) {
        wrting_ = false;
        return;
    }
    auto m = q_.front();
    ws_.async_write(
        net::buffer(*m),
        [this, self = shared_from_this()](beast::error_code ec, size_t) {
            if (ec) {
                cls_ = true;
                srv_->unreg(id_);
                return;
            }
            std::lock_guard<std::mutex> lk(qMtx_);
            q_.erase(q_.begin());
            wrt();
        });
}

void Ses::rd() {
    if (cls_) return;
    ws_.async_read(
        buf_,
        [this, self = shared_from_this()](beast::error_code ec, size_t bytes) {
            if (ec) {
                cls_ = true;
                srv_->unreg(id_);
                return;
            }
            const uint8_t* p = static_cast<const uint8_t*>(buf_.data().data());
            srv_->onMsg(p, bytes, id_);
            buf_.consume(bytes);
            rd();
        });
}

HttpSes::HttpSes(tcp::socket&& s, std::shared_ptr<Srv> srv)
    : strm_(std::move(s)), srv_(srv) {}

void HttpSes::run() {
    rd();
}

void HttpSes::rd() {
    req_ = {};
    strm_.expires_after(std::chrono::seconds(15));
    http::async_read(strm_, buf_, req_,
        [this, self = shared_from_this()](beast::error_code ec, size_t) {
            if (!ec) hdl();
        });
}

void HttpSes::hdl() {
    if (websocket::is_upgrade(req_)) {
        uint64_t id = srv_->genId();
        std::make_shared<Ses>(strm_.release_socket(), srv_, id)->run(std::move(req_));
        return;
    }
    std::string tgt = std::string(req_.target());
    if (tgt.empty() || tgt == "/") tgt = "/index.html";
    sndFile(srv_->getRoot() + tgt);
}

void HttpSes::sndFile(const std::string& p) {
    std::ifstream f(p, std::ios::binary);
    if (!f.is_open()) {
        http::response<http::string_body> res{http::status::not_found, req_.version()};
        res.set(http::field::server, "BurstVNC");
        res.set(http::field::content_type, "text/plain");
        res.body() = "404 Not Found";
        res.prepare_payload();
        http::write(strm_, res);
        return;
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    std::string b = ss.str();

    http::response<http::string_body> res{http::status::ok, req_.version()};
    res.set(http::field::server, "BurstVNC");
    res.set(http::field::content_type, getMime(p));
    res.set(http::field::cache_control, "no-cache");
    res.body() = std::move(b);
    res.prepare_payload();
    http::write(strm_, res);
}

}
