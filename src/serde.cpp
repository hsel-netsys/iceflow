#include "serde.hpp"
#include <iostream>

// Serialize nlohmann::json to CBOR format
std::vector<uint8_t> Serde::serialize(const nlohmann::json &jsonData) {
  try {
    // Use the nlohmann::json to_cbor method to serialize the JSON to CBOR
    return nlohmann::json::to_cbor(jsonData);
  } catch (const std::exception &e) {
    throw std::runtime_error("Error during CBOR serialization: " +
                             std::string(e.what()));
  }
}

// Deserialize CBOR format to nlohmann::json
nlohmann::json Serde::deserialize(const std::vector<uint8_t> &cborData) {
  try {
    // Use the nlohmann::json from_cbor method to deserialize the CBOR data
    for (size_t i = 0; i < cborData.size(); ++i) {
      std::cout << static_cast<int>(cborData[i])
                << " "; // Cast to int for proper display
    }
    return nlohmann::json::from_cbor(cborData);
  } catch (const std::exception &e) {
    throw std::runtime_error("Error during CBOR deserialization: " +
                             std::string(e.what()));
  }
}
