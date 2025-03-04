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

  void broadcast(const std::string &message);

  void set_connection_handler(MessageHandler handler);
  void set_file_handler(MessageHandler handler);

  bool find_file(const std::string &file_name) const;

  std::string get_path_file(const std::string &file_name) const;

  Peer *get_peer();

  bool add_peer(const std::string &address);

  bool remove_peer(const std::string &address);
  void add_file(const std::string &file_name, const std::string &file_path);

  void set_prev_message(const std::string &new_message);

  std::string get_peer_address() const;

  std::string get_prev_message();

  void send_to_peer(const std::string &address, const std::string &message);

private:
  void handle_incoming_message(net::ip::tcp::socket &socket);
  Peer *peer_;
  net::io_context io_context_;

  net::ip::tcp::acceptor acceptor_;

  MessageHandler connection_handler_;
  MessageHandler file_handler_;

  std::string prev_message_;
};

#endif // NETWORK_MANAGER_H