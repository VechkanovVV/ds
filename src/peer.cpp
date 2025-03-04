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

uint16_t Peer::get_port() const {
  size_t colon_pos = address_.find(':');
  if (colon_pos == std::string::npos) {
    throw std::runtime_error("Invalid address format");
  }
  return static_cast<uint16_t>(std::stoi(address_.substr(colon_pos + 1)));
}

std::pair<std::string, uint16_t>
Peer::parse_address(const std::string &neighbor_address) const {
  size_t colon_pos = neighbor_address.find(':');
  std::string ip = neighbor_address.substr(0, colon_pos);
  uint16_t port = std::stoi(neighbor_address.substr(colon_pos + 1));
  return {ip, port};
}

std::string Peer::get_file_path(const std::string &file_name) const {
  return files_.at(file_name);
}

std::string Peer::get_address() const { return address_; }