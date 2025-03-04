#include "../include/cli.h"
#include "../include/network_manager.h"
#include "../include/peer.h"
#include <boost/algorithm/string.hpp>
#include <iostream>
#include <vector>

const int TTL = 10;

CLI::CLI(NetworkManager *manager) : manager_(manager) {}

void CLI::start(std::atomic<bool> &running) {
  std::cout << "P2P Client started. Type 'help' for commands list.\n";

  while (running) {
    std::cout << "tor> ";
    std::string input;
    std::getline(std::cin, input);

    if (input.empty())
      continue;

    std::vector<std::string> tokens;
    boost::split(tokens, input, boost::is_any_of(" "),
                 boost::token_compress_on);

    try {
      if (tokens[0] == "exit") {
        running = false;
      } else if (tokens[0] == "list_peers") {
        list_peers();
      } else if (tokens[0] == "add_peer" && tokens.size() > 1) {
        add_peer(tokens[1]);
      } else if (tokens[0] == "remove_peer" && tokens.size() > 1) {
        remove_peer(tokens[1]);
      } else if (tokens[0] == "add_file" && tokens.size() > 1) {
        add_file(input.substr(9));
      } else if (tokens[0] == "find_file" && tokens.size() > 1) {
        find_file(input.substr(10));
      } else if (tokens[0] == "download" && tokens.size() > 2) {
        download(input.substr(8));
      } else if (tokens[0] == "send_message" && tokens.size() > 2) {
        send_message(input.substr(12));
      } else if (tokens[0] == "help") {
        std::cout << "Available commands:\n"
                  << "  add_peer <address>      Add a neighbor\n"
                  << "  remove_peer <address>   Remove a neighbor\n"
                  << "  list_peers              List all neighbors\n"
                  << "  add_file <path> [desc]  Share a file\n"
                  << "  find_file <query>       Search files\n"
                  << "  download <addr> <hash>  Download file\n"
                  << "  send_message <addr> <msg>\n"
                  << "  exit                    Quit\n";
      } else {
        print_error("Unknown command");
      }
    } catch (const std::exception &e) {
      print_error(e.what());
    }
  }
}

void CLI::list_peers() {
  const auto &peers = manager_->get_peer()->get_neighbors();
  if (peers.empty()) {
    std::cout << "No peers found\n";
    return;
  }
  std::cout << "Neighbors (" << peers.size() << "):\n";
  for (const auto &peer : peers) {
    std::cout << "  " << peer << "\n";
  }
}

void CLI::add_peer(const std::string &address) {
  if (manager_->add_peer(address)) {
    print_success("Added peer: " + address);
  } else {
    print_error("Invalid address or peer exists: " + address);
  }
}

void CLI::remove_peer(const std::string &address) {
  if (manager_->remove_peer(address)) {
    print_success("Removed peer: " + address);
  } else {
    print_error("Peer not found: " + address);
  }
}

void CLI::add_file(const std::string &input) {
  size_t space = input.find(' ');
  if (space == std::string::npos) {
    throw std::invalid_argument(
        "Invalid format. Use: add_file <path> [description]");
  }

  std::string path = input.substr(0, space);
  std::string name = (input.size() > space + 1) ? input.substr(space + 1) : "";

  manager_->add_file(path, name);
  print_success("File added: " + path);
}

void CLI::find_file(const std::string &file_name) {
  std::string request_id = generate_unique_string(10);
  std::string message = "FIND_FILE|" + file_name + "|" + std::to_string(TTL) +
                        "|" + request_id + "|" + manager_->get_peer_address();
  manager_->broadcast(message);
  manager_->set_prev_message(request_id);
  std::cout << "QUERY: " << message << "\n";
}

void CLI::download(const std::string &input) {}

void CLI::send_message(const std::string &input) {}

void CLI::print_error(const std::string &message) const {
  std::cerr << "[ERROR] " << message << "\n";
}

void CLI::print_success(const std::string &message) const {
  std::cout << "[OK] " << message << "\n";
}

std::string CLI::generate_unique_string(size_t length) {
  const std::string charset = "0123456789"
                              "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                              "abcdefghijklmnopqrstuvwxyz";

  std::random_device rd;
  std::mt19937 generator(rd());
  std::uniform_int_distribution<size_t> distribution(0, charset.size() - 1);

  std::string result;
  result.reserve(length);

  for (size_t i = 0; i < length; ++i) {
    result += charset[distribution(generator)];
  }

  return result;
}
