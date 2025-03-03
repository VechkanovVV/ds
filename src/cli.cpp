#include "../include/cli.h"
#include "../include/network_manager.h"
#include "../include/peer.h"
#include <boost/algorithm/string.hpp>
#include <iostream>
#include <vector>

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
      } else if (tokens[0] == "broadcast" && tokens.size() > 1) {
        broadcast(input.substr(9));
      } else if (tokens[0] == "status") {
        show_status();
      } else if (tokens[0] == "help") {
        std::cout << "Available commands:\n"
                  << "  add_peer <address>      Add a neighbor\n"
                  << "  remove_peer <address>   Remove a neighbor\n"
                  << "  list_peers              List all neighbors\n"
                  << "  add_file <path> [desc]  Share a file\n"
                  << "  find_file <query>       Search files\n"
                  << "  download <addr> <hash>  Download file\n"
                  << "  send_message <addr> <msg>\n"
                  << "  broadcast <msg>         Send to all neighbors\n"
                  << "  status                  Show node status\n"
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
  std::string desc = (input.size() > space + 1) ? input.substr(space + 1) : "";

  manager_.get_peer().add_file(path, desc);
  print_success("File added: " + path);
}

void CLI::find_file(const std::string &query) {
  auto results = manager_.search_files(query);
  if (results.empty()) {
    std::cout << "No files found\n";
    return;
  }
  std::cout << "Search results (" << results.size() << "):\n";
  for (const auto &[hash, meta] : results) {
    std::cout << "  " << hash << " | " << meta.description << " | "
              << meta.peer_address << "\n";
  }
}

void CLI::download(const std::string &input) {
  std::vector<std::string> parts;
  boost::split(parts, input, boost::is_any_of(" "));
  if (parts.size() != 2) {
    throw std::invalid_argument(
        "Invalid format. Use: download <address> <hash>");
  }

  if (manager_.download_file(parts[0], parts[1])) {
    print_success("Download started");
  } else {
    print_error("Download failed");
  }
}

void CLI::send_message(const std::string &input) {
  size_t space = input.find(' ');
  if (space == std::string::npos) {
    throw std::invalid_argument(
        "Invalid format. Use: send_message <address> <message>");
  }

  std::string address = input.substr(0, space);
  std::string message = input.substr(space + 1);

  manager_.send_request(address, "MSG " + message);
  print_success("Message sent to " + address);
}

void CLI::broadcast(const std::string &message) {
  manager_.broadcast("BCAST " + message);
  print_success("Broadcasted to all peers");
}

void CLI::show_status() const {
  const auto &peer = manager_.get_peer();
  std::cout << "Address:    " << peer.get_address() << "\n"
            << "Neighbors:  " << peer.get_neighbors().size() << "\n"
            << "Shared files: " << peer.get_files().size() << "\n";
}

void CLI::print_error(const std::string &message) const {
  std::cerr << "[ERROR] " << message << "\n";
}

void CLI::print_success(const std::string &message) const {
  std::cout << "[OK] " << message << "\n";
}