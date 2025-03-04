#ifndef CLI_H
#define CLI_H

#include "network_manager.h"
#include <functional>
#include <random>
#include <string>

class CLI {
public:
  explicit CLI(NetworkManager *manager);
  void start(std::atomic<bool> &running);

private:
  void list_peers();
  void add_peer(const std::string &address);
  void remove_peer(const std::string &address);
  void add_file(const std::string &input);
  void find_file(const std::string &query);
  void download(const std::string &input);
  void send_message(const std::string &input);

  std::string generate_unique_string(size_t length);

  NetworkManager *manager_;

  // Форматирование вывода
  void print_error(const std::string &message) const;
  void print_success(const std::string &message) const;
};

#endif // CLI_H