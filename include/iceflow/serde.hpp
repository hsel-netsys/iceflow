#ifndef SERDE_HPP
#define SERDE_HPP

#include <iostream>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <vector>

class Serde {
public:
  // Serialize nlohmann::json to CBOR format
  static std::vector<uint8_t> serialize(const nlohmann::json &jsonData);

  // Deserialize CBOR format to nlohmann::json
  static nlohmann::json deserialize(const std::vector<uint8_t> &cborData);
};

#endif // SERDE_HPP
