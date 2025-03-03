#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include "peer.h"
#include <boost/asio.hpp>
#include <functional>
#include <string>

namespace net = boost::asio;

class NetworkManager {
public:
  using MessageHandler =
      std::function<void(const std::string &, const std::string &)>;

  explicit NetworkManager(Peer *peer);

  void start_server(std::atomic<bool> &running);
  void stop_server();

  void send_request(const std::string &to_address, const std::string &message);

  void broadcast(const std::string &message, int ttl = 5);

  void set_connection_handler(MessageHandler handler);
  void set_file_handler(MessageHandler handler);

  Peer *get_peer();

  bool add_peer(const std::string &address);

  bool remove_peer(const std::string &address);

private:
  void handle_incoming_message(net::ip::tcp::socket &socket);
  Peer *peer_;
  net::io_context io_context_;

  net::ip::tcp::acceptor acceptor_;

  MessageHandler connection_handler_;
  MessageHandler file_handler_;
};

#endif // NETWORK_MANAGER_H