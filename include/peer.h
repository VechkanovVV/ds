#ifndef PEER_H
#define PEER_H

#include <boost/asio.hpp>
#include <string>
#include <unordered_map>
#include <unordered_set>

class Peer {
public:
  explicit Peer(const std::string &address);

  bool add_neighbor(const std::string &peer_address);
  bool remove_neighbor(const std::string &peer_address);
  const std::unordered_set<std::string> &get_neighbors() const;

  void add_file(const std::string &file_name, const std::string &file_path);
  bool has_file(const std::string &file_name) const;

  std::string get_file_path(const std::string &file_name) const;

  uint16_t get_port() const;
  std::pair<std::string, uint16_t>
  parse_address(const std::string &neighbor_address) const;
  std::string get_address() const;

private:
  std::string address_;
  std::unordered_set<std::string> neighbors_;
  std::unordered_map<std::string, std::string> files_;
};

#endif // PEER_H