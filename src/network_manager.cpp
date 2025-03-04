#include "../include/network_manager.h"
#include <iostream>
#include <mutex>

std::mutex cout_mutex;

NetworkManager::NetworkManager(Peer *peer)
    : peer_(peer),
      acceptor_(io_context_,
                net::ip::tcp::endpoint(net::ip::tcp::v4(), peer->get_port())) {
  acceptor_.set_option(net::ip::tcp::acceptor::reuse_address(true));
  if (peer->get_port() == 0) {
    throw std::invalid_argument("Invalid port in Peer address");
  }
}

void NetworkManager::start_server(std::atomic<bool> &running) {
  std::cout << "[SERVER] Listening on port " << peer_->get_port() << std::endl
            << "tor> ";

  while (running) {
    net::ip::tcp::socket socket(io_context_);
    acceptor_.accept(socket);
    std::cout << "[SERVER] Accepted connection from "
              << socket.remote_endpoint().address().to_string() << std::endl;

    std::thread([this, s = std::move(socket)]() mutable {
      try {
        boost::asio::streambuf buf;
        boost::system::error_code ec;

        read_until(s, buf, '\n', ec);

        if (ec) {
          if (ec == boost::asio::error::eof) {
            std::cout << "[INFO] Client disconnected gracefully\n";
            return;
          }
          throw boost::system::system_error(ec);
        }

        std::istream is(&buf);
        std::string message;
        std::getline(is, message);

        std::cout << "[RECEIVED] " << message << std::endl;

        if (message.empty() || message.find('|') == std::string::npos) {
          throw std::invalid_argument("Invalid message format");
        }

        if (message.find("FIND_FILE") == 0) {
          size_t pos1 = message.find('|', 0) + 1;
          size_t pos2 = message.find('|', pos1);
          size_t pos3 = message.find('|', pos2 + 1);
          size_t pos4 = message.find('|', pos3 + 1);

          if (pos1 == std::string::npos || pos2 == std::string::npos ||
              pos3 == std::string::npos || pos4 == std::string::npos) {
            throw std::invalid_argument("Invalid FIND_FILE format");
          }

          std::string file_name = message.substr(pos1, pos2 - pos1);
          int ttl = std::stoi(message.substr(pos2 + 1, pos3 - pos2 - 1));
          std::string request_id = message.substr(pos3 + 1, pos4 - pos3 - 1);
          std::string sender_ip = message.substr(pos4 + 1);

          std::cout << "[ACTION] TTL: " << ttl << " from " << sender_ip
                    << std::endl;

          if (ttl > 0 && request_id != get_prev_message()) {
            set_prev_message(request_id);
            ttl--;

            if (find_file(file_name)) {
              std::cout << "[OK] File exists at " << peer_->get_address()
                        << std::endl;
              std::string response = "FOUND_FILE|" + file_name + "|" +
                                     request_id + "|" + sender_ip;
              send_to_peer(sender_ip, response);
            } else if (ttl > 0) {
              std::string new_message = "FIND_FILE|" + file_name + "|" +
                                        std::to_string(ttl) + "|" + request_id +
                                        "|" + sender_ip;
              broadcast(new_message);
            }
          }
        }
      } catch (const boost::system::system_error &e) {
        if (e.code() == boost::asio::error::eof) {
          std::cout << "[INFO] Client disconnected\n";
        } else {
          std::cerr << "[ERROR] Handling connection: " << e.what() << std::endl;
        }
      } catch (const std::exception &e) {
        std::cerr << "[ERROR] Handling connection: " << e.what() << std::endl;
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

void NetworkManager::set_prev_message(const std::string &new_message) {
  prev_message_ = new_message;
}

std::string NetworkManager::get_prev_message() { return prev_message_; }

void NetworkManager::broadcast(const std::string &message) {
  using namespace boost::asio;
  for (const auto &peer : peer_->get_neighbors()) {
    try {
      const auto address = peer_->parse_address(peer);
      ip::tcp::endpoint endpoint(ip::address::from_string(address.first),
                                 address.second);
      ip::tcp::socket socket(io_context_);
      socket.connect(endpoint);
      write(socket, buffer(message + "\n"));
      std::cout << "[SENT] To " << peer << ": " << message << std::endl;
    } catch (const std::exception &e) {
      std::cerr << "[ERROR] Failed to send to " << peer << ": " << e.what()
                << "\n";
    }
  }
}

void NetworkManager::stop_server() {
  acceptor_.close();
  io_context_.stop();
}

bool NetworkManager::find_file(const std::string &file_name) const {
  return peer_->has_file(file_name);
}

std::string NetworkManager::get_path_file(const std::string &file_name) const {
  return peer_->get_file_path(file_name);
}

std::string NetworkManager::get_peer_address() const {
  return peer_->get_address();
}

void NetworkManager::send_to_peer(const std::string &address,
                                  const std::string &message) {
  using namespace boost::asio;
  try {
    auto [ip, port] = peer_->parse_address(address);
    ip::tcp::endpoint endpoint(ip::address::from_string(ip), port);
    ip::tcp::socket socket(io_context_);

    socket.async_connect(endpoint, [](const boost::system::error_code &ec) {
      if (ec)
        throw boost::system::system_error(ec);
    });
    io_context_.run_for(std::chrono::seconds(2));

    std::string full_message = message + "\n";
    write(socket, buffer(full_message));

    socket.shutdown(ip::tcp::socket::shutdown_both);
    socket.close();

  } catch (const std::exception &e) {
    std::lock_guard<std::mutex> lock(cout_mutex);
    std::cerr << "[ERROR] Failed to send to " << address << ": " << e.what()
              << std::endl;
  }
}