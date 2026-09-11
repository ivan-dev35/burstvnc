#pragma once
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <functional>
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include "proto.hpp"

namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = net::ip::tcp;

namespace bst {

class Ses;

class Srv : public std::enable_shared_from_this<Srv> {
public:
    using InpHdlr = std::function<void(const uint8_t* d, size_t sz)>;
    using ConnHdlr = std::function<void(uint64_t id)>;

    Srv(const std::string& host, uint16_t port, const std::string& root);
    ~Srv();

    bool start();
    void stop();

    void setInpHdlr(InpHdlr h) { inpHdlr_ = h; }
    void setConnHdlr(ConnHdlr h) { connHdlr_ = h; }

    void bcastV(const HdrVid& h, const uint8_t* d, size_t sz);
    void bcastA(const HdrAud& h, const uint8_t* d, size_t sz);
    void bcastC(const HdrCur& h, const std::vector<uint8_t>& d);
    void bcastCfg(const HdrCfg& h);

    void sendTo(uint64_t id, std::shared_ptr<std::vector<uint8_t>> msg);
    void reg(uint64_t id, std::shared_ptr<Ses> s);
    void unreg(uint64_t id);
    uint64_t genId() { return nxtId_++; }

    const std::string& getRoot() const { return root_; }
    void onMsg(const uint8_t* d, size_t sz, uint64_t sid);

private:
    void acc();

    std::string host_;
    uint16_t port_;
    std::string root_;
    net::io_context ioc_{4};
    std::unique_ptr<tcp::acceptor> acc_;
    std::vector<std::thread> ths_;

    std::mutex mtx_;
    std::unordered_map<uint64_t, std::shared_ptr<Ses>> ses_;
    std::atomic<uint64_t> nxtId_{1};
    InpHdlr inpHdlr_;
    ConnHdlr connHdlr_;
};

class Ses : public std::enable_shared_from_this<Ses> {
public:
    Ses(tcp::socket&& s, std::shared_ptr<Srv> srv, uint64_t id);
    ~Ses() = default;

    void run(http::request<http::string_body> req);
    void send(std::shared_ptr<std::vector<uint8_t>> msg);
    uint64_t getId() const { return id_; }

private:
    void rd();
    void wrt();

    websocket::stream<beast::tcp_stream> ws_;
    std::shared_ptr<Srv> srv_;
    uint64_t id_ = 0;
    beast::flat_buffer buf_;
    std::vector<std::shared_ptr<std::vector<uint8_t>>> q_;
    std::mutex qMtx_;
    bool wrting_ = false;
    std::atomic<bool> cls_{false};
};

class HttpSes : public std::enable_shared_from_this<HttpSes> {
public:
    HttpSes(tcp::socket&& s, std::shared_ptr<Srv> srv);
    void run();

private:
    void rd();
    void hdl();
    void sndFile(const std::string& path);

    beast::tcp_stream strm_;
    std::shared_ptr<Srv> srv_;
    beast::flat_buffer buf_;
    http::request<http::string_body> req_;
};

}
