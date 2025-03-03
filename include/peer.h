#ifndef PEER_H
#define PEER_H

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

  int get_port() const;
  const std::string &get_address() const;

private:
  std::string address_;
  std::unordered_set<std::string> neighbors_;
  std::unordered_map<std::string, std::string> files_;
};

#endif // PEER_H