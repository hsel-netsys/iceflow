#include "serde.hpp"

// Serialize nlohmann::json to CBOR format
std::vector<uint8_t> Serde::serialize(const nlohmann::json &jsonData) {
  try {
    return nlohmann::json::to_cbor(jsonData);
  } catch (const std::exception &e) {
    throw std::runtime_error("Error during CBOR serialization: " +
                             std::string(e.what()));
  }
}

// Deserialize CBOR format to nlohmann::json
nlohmann::json Serde::deserialize(const std::vector<uint8_t> &cborData) {
  try {
    return nlohmann::json::from_cbor(cborData);
  } catch (const std::exception &e) {
    throw std::runtime_error("Error during CBOR deserialization: " +
                             std::string(e.what()));
  }
}