#include <boost/asio/detail/service_registry.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/strand.hpp>
#include <boost/system/detail/error_code.hpp>
#include <cstdlib>
#include <format>
#include <memory>
#include <print>
#include <boost/asio.hpp>
#include <vector>
#include <algorithm>

constexpr int BUF_SIZE = 1312;

struct client {
    int id;
    boost::asio::ip::tcp::socket socket;
    char buf[BUF_SIZE];

    client(int id_, boost::asio::io_context &io_ctx)
        : id(id_), socket(io_ctx) {}
};

class chat {
    boost::asio::io_context &io_ctx_;
    std::unique_ptr<boost::asio::ip::tcp::acceptor> socket_;
    boost::asio::strand<boost::asio::io_context::executor_type> strand_;
    std::vector<std::shared_ptr<client>> clients_;

  public:
    chat(const char *ip, unsigned short port, boost::asio::io_context &ctx)
        : io_ctx_(ctx), strand_(boost::asio::make_strand(io_ctx_)) {
        socket_ = std::make_unique<boost::asio::ip::tcp::acceptor>(
          io_ctx_,
          boost::asio::ip::tcp::endpoint{boost::asio::ip::tcp::v4(), port});
        socket_->set_option(
          boost::asio::ip::tcp::acceptor::reuse_address(true));
        socket_->listen();
        start_accept();
    }
    void start_accept() {
        auto cl = std::make_shared<client>(rand() % 10000, io_ctx_);
        socket_->async_accept(
          cl->socket, boost::asio::bind_executor(
                        strand_, std::bind(&chat::on_accept, this,
                                           std::placeholders::_1, cl)));
    }
    void on_accept(const boost::system::error_code &ec,
                   std::shared_ptr<client> peer) {
        if(!ec) {
            clients_.push_back(peer);
            peer->socket.async_read_some(
              boost::asio::buffer(peer->buf, BUF_SIZE),
              boost::asio::bind_executor(
                strand_, std::bind(&chat::on_read, this, std::placeholders::_1,
                                   std::placeholders::_2, peer)));
        }
        start_accept();
    }
    void on_read(const boost::system::error_code &error,
                 std::size_t bytes_transferred, std::shared_ptr<client> peer) {
        if(!error) {
            {
                for(auto &client : clients_) {
                    if(client->id != peer->id) {
                        std::string msg = std::format(
                          "[{}]: {}", peer->id,
                          std::string{peer->buf, bytes_transferred});
                        client->socket.async_write_some(
                          boost::asio::buffer(msg.c_str(), msg.length()),
                          boost::asio::bind_executor(
                            strand_,
                            [](boost::system::error_code ec, std::size_t bt) {
                                if(ec) {
                                    std::println("Could not write: {}",
                                                 ec.message());
                                }
                            }));
                    }
                }
            }
            peer->socket.async_read_some(
              boost::asio::buffer(peer->buf, BUF_SIZE),
              boost::asio::bind_executor(
                strand_, std::bind(&chat::on_read, this, std::placeholders::_1,
                                   std::placeholders::_2, peer)));
        } else {
            close_peer(peer);
        }
    }
    void close_peer(std::shared_ptr<client> peer) {
        std::println("Closed");
        try {
            peer->socket.close();
            auto r = std::remove_if(clients_.begin(), clients_.end(),
                                    [&peer](const auto &element) {
                                        return element->id == peer->id;
                                    });
            clients_.erase(r, clients_.end());
        } catch(const boost::system::system_error &ex) {
            std::println("Failed to close: {}", ex.what());
        }
    }
};

int main() {
    boost::asio::io_context io_ctx;
    chat chat{"0.0.0.0", 13333, io_ctx};
    io_ctx.run();
    return 0;
}
