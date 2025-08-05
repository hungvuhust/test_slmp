#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>
#include <mutex>
#include <regex>
#include <string>
#include <sys/types.h>
#include <thread>
#include <vector>

#include <libmelcli/melcli.h>
#include <libmelcli/melclidef.h>

namespace plc_slmp {

// Enum cho các loại thanh ghi PLC
enum class RegisterType {
  D_REGISTER,   // Data register (D0, D100, D1000, ...)
  X_REGISTER,   // Input register (X0, X1, X10, X100, ...)
  Y_REGISTER,   // Output register (Y0, Y1, Y10, Y100, ...)
  M_REGISTER,   // Memory register (M0, M1, M100, ...)
  B_REGISTER,   // Link register (B0, B1, B10, ...)
  SD_REGISTER,  // SD register (SD0, SD1, SD10, SD100, ...)
  UNKNOWN
};

class PlcClient {
private:
  melcli_ctx_t *g_ctx_   = NULL;
  int           ctxtype_ = MELCLI_TYPE_TCPIP;

  char             local_ip_addr_[64] = "0.0.0.0";
  int              local_port_        = 0;
  // Parameters
  std::string      target_ip_addr_{"192.168.5.125"};
  int              target_port_{2001};
  melcli_station_t target_station_ = MELCLI_CONNECTED_STATION;
  melcli_timeout_t timeout_        = MELCLI_TIMEOUT_DEFAULT;
  //   Mutex
  std::mutex       mutex_;

  // Regex patterns cho validation
  std::regex d_register_pattern_{R"(^D\d+$)"};           // D0, D100, D1000, ...
  std::regex x_register_pattern_{R"(^X[0-9A-Fa-f]+$)"};  // X0, X1, XA, XFF, ...
  std::regex y_register_pattern_{R"(^Y[0-9A-Fa-f]+$)"};  // Y0, Y1, YA, YFF, ...
  std::regex m_register_pattern_{R"(^M\d+$)"};           // M0, M1, M100, ...
  std::regex b_register_pattern_{R"(^B[0-9A-Fa-f]+$)"};  // B0, B1, BA, BFF, ...
  std::regex sd_register_pattern_{
    R"(^SD[0-9A-Fa-f]+$)"};  // SD0, SD1, SD10, SD100, ...

  // Validate địa chỉ thanh ghi
  inline bool validate_register_address(const char *addr) {
    if (addr == nullptr || strlen(addr) == 0) {
      return false;
    }

    std::string  address(addr);
    RegisterType type = get_register_type(address);

    if (type == RegisterType::UNKNOWN) {
      std::cerr << "Invalid register address format: " << address << std::endl;
      return false;
    }

    // std::cout << "Valid register address: " << address
    //           << " (Type: " << static_cast<int>(type) << ")" << std::endl;
    return true;
  }

  // Xác định loại thanh ghi
  inline RegisterType get_register_type(const std::string &addr) {
    if (std::regex_match(addr, d_register_pattern_)) {
      return RegisterType::D_REGISTER;
    } else if (std::regex_match(addr, x_register_pattern_)) {
      return RegisterType::X_REGISTER;
    } else if (std::regex_match(addr, y_register_pattern_)) {
      return RegisterType::Y_REGISTER;
    } else if (std::regex_match(addr, m_register_pattern_)) {
      return RegisterType::M_REGISTER;
    } else if (std::regex_match(addr, b_register_pattern_)) {
      return RegisterType::B_REGISTER;
    } else if (std::regex_match(addr, sd_register_pattern_)) {
      return RegisterType::SD_REGISTER;
    }
    return RegisterType::UNKNOWN;
  }

  // Lấy tên loại thanh ghi dưới dạng string
  inline std::string get_register_type_name(RegisterType type) {
    switch (type) {
      case RegisterType::D_REGISTER:
        return "D Register";
      case RegisterType::X_REGISTER:
        return "X Register";
      case RegisterType::Y_REGISTER:
        return "Y Register";
      case RegisterType::M_REGISTER:
        return "M Register";
      case RegisterType::B_REGISTER:
        return "B Register";
      case RegisterType::SD_REGISTER:
        return "SD Register";
      default:
        return "Unknown";
    }
  }

public:
  PlcClient(std::string target_ip_addr, int target_port, int type_protocol)
    : ctxtype_(type_protocol),
      target_ip_addr_(target_ip_addr),
      target_port_(target_port) {
    std::cout << "PLC Client has been created." << std::endl;
    std::cout << "PLC IP: " << target_ip_addr_ << std::endl;
    std::cout << "PLC Port: " << target_port_ << std::endl;
    if (type_protocol == MELCLI_TYPE_TCPIP) {
      std::cout << "PLC Protocol: TCP" << std::endl;
    } else if (type_protocol == MELCLI_TYPE_UDPIP) {
      std::cout << "PLC Protocol: UDP" << std::endl;
    } else {
      std::cerr << "Invalid Protocol Type: " << type_protocol << std::endl;
    }
  }
  ~PlcClient() {
    if (g_ctx_ != NULL) {
      melcli_disconnect(g_ctx_);
      melcli_free_context(g_ctx_);
    }
  }

  inline bool init_plc() {
    try {
      if (g_ctx_ != NULL) {
        melcli_disconnect(g_ctx_);
        melcli_free_context(g_ctx_);
      }

      g_ctx_ = melcli_new_context(ctxtype_,
                                  target_ip_addr_.c_str(),
                                  target_port_,
                                  local_ip_addr_,
                                  local_port_,
                                  &target_station_,
                                  &timeout_);
      if (g_ctx_ == NULL) {
        return false;
      }

      if (melcli_connect(g_ctx_) != 0) {
        return false;
      }

    } catch (...) {
      return false;
    }
    return true;
  }
  inline bool disconnect() {
    if (g_ctx_ != NULL) {
      melcli_disconnect(g_ctx_);
      melcli_free_context(g_ctx_);
    }
    return true;
  }

  inline bool read_batch_d_register(const char *addr, uint16_t &data) {
    // Validate địa chỉ thanh ghi trước khi đọc
    if (!validate_register_address(addr)) {
      return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    uint16_t                   *rd_words;
    if (melcli_batch_read(g_ctx_, NULL, addr, 1, (char **)(&rd_words), NULL) !=
        0) {
      std::cerr << "Failed to batch read from address: " << addr << std::endl;
      return false;
    }
    data = rd_words[0];
    melcli_free(rd_words);

    // std::cout << "Successfully read from " << addr << ": " << data <<
    // std::endl;
    return true;
  }

  inline bool read_batch_d_registers(const char            *addr,
                                     int                    num,
                                     std::vector<uint16_t> &data) {
    // Validate địa chỉ thanh ghi trước khi đọc
    if (!validate_register_address(addr)) {
      return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    uint16_t                   *rd_words;
    if (melcli_batch_read(
          g_ctx_, NULL, addr, num, (char **)(&rd_words), NULL) != 0) {
      std::cerr << "Failed to batch read " << num
                << " registers from address: " << addr << std::endl;
      return false;
    }

    data.resize(num);
    for (size_t i = 0; i < num; i++) {
      data[i] = rd_words[i];
    }
    melcli_free(rd_words);

    // std::cout << "Successfully read " << num << " registers from " << addr
    //           << std::endl;
    return true;
  }

  inline bool write_batch_d_register(const char *addr, uint16_t data) {
    // Validate địa chỉ thanh ghi trước khi ghi
    if (!validate_register_address(addr)) {
      return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    if (melcli_batch_write(g_ctx_, NULL, addr, 1, (char *)(&data)) != 0) {
      std::cerr << "Failed to batch write to address: " << addr << std::endl;
      return false;
    }

    // std::cout << "Successfully wrote to " << addr << ": " << data <<
    // std::endl;
    return true;
  }

  inline bool write_batch_d_registers(const char                  *addr,
                                      int                          num,
                                      const std::vector<uint16_t> &data) {
    // Validate địa chỉ thanh ghi trước khi ghi
    if (!validate_register_address(addr)) {
      return false;
    }

    if (data.size() < num) {
      std::cerr << "Data vector size (" << data.size()
                << ") is smaller than requested write count (" << num << ")"
                << std::endl;
      return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<uint16_t>       write_data(data.begin(), data.begin() + num);

    if (melcli_batch_write(
          g_ctx_, NULL, addr, num, (char *)(write_data.data())) != 0) {
      std::cerr << "Failed to batch write " << num
                << " registers to address: " << addr << std::endl;
      return false;
    }

    // std::cout << "Successfully wrote " << num << " registers to " << addr
    //           << std::endl;
    return true;
  }

  // Thêm method public để validate address từ bên ngoài
  inline bool is_valid_register_address(const char *addr) {
    return validate_register_address(addr);
  }

  // Thêm method public để lấy loại thanh ghi
  inline RegisterType get_address_type(const char *addr) {
    if (addr == nullptr)
      return RegisterType::UNKNOWN;
    return get_register_type(std::string(addr));
  }

  // Thêm method public để lấy tên loại thanh ghi
  inline std::string get_address_type_name(const char *addr) {
    RegisterType type = get_address_type(addr);
    return get_register_type_name(type);
  }
};

}  // namespace plc_slmp
