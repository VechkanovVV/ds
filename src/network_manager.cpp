#include "../include/network_manager.h"

NetworkManager::NetworkManager(Peer *peer)
    : peer_(peer),
      acceptor_(io_context_,
                net::ip::tcp::endpoint(net::ip::tcp::v4(), peer->get_port())) {
  if (peer->get_port() == 0) {
    throw std::invalid_argument("Invalid port in Peer address");
  }
}

void NetworkManager::start_server(std::atomic<bool> &running) {
  net::ip::tcp::acceptor acceptor(
      io_context_,
      net::ip::tcp::endpoint(net::ip::tcp::v4(), peer_->get_port()));

  while (running) {
    net::ip::tcp::socket socket(io_context_);
    acceptor.accept(socket);

    // Обработка подключения
    std::thread([this, s = std::move(socket)]() mutable {
      boost::asio::streambuf buf;
      read_until(s, buf, '\n');

      std::string message = boost::asio::buffer_cast<const char *>(buf.data());

      if (message.find("CONNECT") == 0) {
        peer_->add_neighbor(message.substr(8));
      }
    }).detach();
  }
}

Peer *NetworkManager::get_peer() { return peer_; }

bool NetworkManager::add_peer(const std::string &address) {
  return peer_->add_neighbor(address);
}

bool NetworkManager::remove_peer(const std::string &address) {
  return peer_->remove_neighbor(address);
}

void NetworkManager::add_file(const std::string &file_name,
                              const std::string &file_path) {
  peer_->add_file(file_name, file_path);
}
