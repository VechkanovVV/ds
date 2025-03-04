#include "../include/cli.h"
#include "../include/network_manager.h"
#include "../include/peer.h"
#include <boost/asio.hpp>
#include <iostream>
#include <thread>

namespace net = boost::asio;
std::atomic<bool> running{true};

const std::string PEER_IP = "127.0.0.1";

// Ctrl+C
void signal_handler(int) { running = false; }

int main(int argc, char *argv[]) {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " <port>\n";
    return 1;
  }

  try {
    Peer my_peer(PEER_IP + ":" + std::string(argv[1]));
    NetworkManager network_manager(&my_peer);
    CLI cli(&network_manager);

    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    std::thread server_thread([&] { network_manager.start_server(running); });
    cli.start(running);

    server_thread.join();
    network_manager.stop_server();

  } catch (const std::exception &e) {
    std::cerr << "Fatal error: " << e.what() << "\n";
    return 2;
  }

  return 0;
}