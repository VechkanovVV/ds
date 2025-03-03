#include "../include/peer.h"

Peer::Peer(const std::string &address) : address_(address) {}

void Peer::add_file(const std::string &file_name,
                    const std::string &file_path) {
  files_[file_name] = file_path;
}

bool Peer::has_file(const std::string &file_name) const {
  return files_.count(file_name) != 0;
}

bool Peer::add_neighbor(const std::string &peer_address) {
  return neighbors_.insert(peer_address).second;
}

bool Peer::remove_neighbor(const std::string &peer_address) {
  return neighbors_.erase(peer_address) > 0;
}
const std::unordered_set<std::string> &Peer::get_neighbors() const {
  return neighbors_;
}

int Peer::get_port() const {
  return std::stoi(address_.substr(address_.find(':') + 1));
}