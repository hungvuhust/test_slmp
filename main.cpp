#include <chrono>
#include <iomanip>  // Required for std::setw and std::fixed
#include <iostream>
#include <random>
#include <spdlog/common.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <test_slmp/plc_client.hpp>
#include <thread>

using namespace std::chrono_literals;

int main(int, char **) {
  // Tạo console sink và file sink
  auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
  auto file_sink =
    std::make_shared<spdlog::sinks::basic_file_sink_mt>("test_slmp.log", true);

  // Thiết lập pattern cho console và file
  console_sink->set_pattern("[%H:%M:%S.%e] [%^%l%$] %v");
  file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");

  // Tạo multi-sink logger
  std::vector<spdlog::sink_ptr> sinks{console_sink, file_sink};
  auto                          logger =
    std::make_shared<spdlog::logger>("test_slmp", sinks.begin(), sinks.end());

  // Đăng ký và set làm default logger
  spdlog::register_logger(logger);
  spdlog::set_default_logger(logger);
  spdlog::set_level(spdlog::level::info);

  std::random_device              rd;
  std::mt19937                    gen(rd());
  std::uniform_int_distribution<> dis(0, 1000);

  spdlog::info("Starting test_slmp");

  plc_slmp::PlcClient plc_client("192.168.6.10", 502, MELCLI_TYPE_TCPIP);

  if (!plc_client.init_plc()) {
    logger->error("Failed to initialize PLC connection");
    return -1;
  }

  logger->info("PLC connection established successfully");
  std::cout << "PLC connected! Starting performance tests...\n";
  while (true) {  // Chuẩn bị dữ liệu test
    std::vector<uint16_t> test_data;
    for (int i = 0; i < 100; i++) {
      test_data.push_back(dis(gen));
    }

    // =================== TEST GHI ===================
    std::cout << "\n=== TESTING WRITE OPERATIONS ===\n";

    // Test 1: Ghi tuần tự từng thanh ghi D1-D100
    auto start_sequential_write = std::chrono::high_resolution_clock::now();

    for (int i = 1; i <= 100; i++) {
      std::string addr = "D" + std::to_string(i);
      if (!plc_client.write_batch_d_register(addr.c_str(), test_data[i - 1])) {
        logger->error("Failed to write to {}", addr);
      }
    }

    auto end_sequential_write = std::chrono::high_resolution_clock::now();
    auto duration_sequential_write =
      std::chrono::duration_cast<std::chrono::microseconds>(
        end_sequential_write - start_sequential_write);

    std::cout << "Sequential write (D1-D100): "
              << duration_sequential_write.count() << " microseconds\n";

    // Chờ một chút trước test tiếp theo
    std::this_thread::sleep_for(100ms);

    // Test 2: Ghi batch D1-D100
    auto start_batch_write = std::chrono::high_resolution_clock::now();

    if (!plc_client.write_batch_d_registers("D1", 100, test_data)) {
      logger->error("Failed to batch write D1-D100");
    }

    auto end_batch_write = std::chrono::high_resolution_clock::now();
    auto duration_batch_write =
      std::chrono::duration_cast<std::chrono::microseconds>(end_batch_write -
                                                            start_batch_write);

    std::cout << "Batch write (D1-D100): " << duration_batch_write.count()
              << " microseconds\n";

    // Tính tỷ lệ cải thiện
    double write_improvement =
      (double)duration_sequential_write.count() / duration_batch_write.count();
    std::cout << "Write speedup: " << std::fixed << std::setprecision(2)
              << write_improvement << "x faster\n";

    // =================== TEST ĐỌC ===================
    std::cout << "\n=== TESTING READ OPERATIONS ===\n";

    // Chờ một chút
    std::this_thread::sleep_for(100ms);

    // Test 3: Đọc tuần tự từng thanh ghi D1-D100
    auto start_sequential_read = std::chrono::high_resolution_clock::now();

    std::vector<uint16_t> sequential_read_data;
    for (int i = 1; i <= 100; i++) {
      std::string addr = "D" + std::to_string(i);
      uint16_t    value;
      if (plc_client.read_batch_d_register(addr.c_str(), value)) {
        sequential_read_data.push_back(value);
      } else {
        logger->error("Failed to read from {}", addr);
        sequential_read_data.push_back(0);
      }
    }

    auto end_sequential_read = std::chrono::high_resolution_clock::now();
    auto duration_sequential_read =
      std::chrono::duration_cast<std::chrono::microseconds>(
        end_sequential_read - start_sequential_read);

    std::cout << "Sequential read (D1-D100): "
              << duration_sequential_read.count() << " microseconds\n";

    // Chờ một chút trước test tiếp theo
    std::this_thread::sleep_for(100ms);

    // Test 4: Đọc batch D1-D100
    auto start_batch_read = std::chrono::high_resolution_clock::now();

    std::vector<uint16_t> batch_read_data;
    if (!plc_client.read_batch_d_registers("D1", 100, batch_read_data)) {
      logger->error("Failed to batch read D1-D100");
    }

    auto end_batch_read = std::chrono::high_resolution_clock::now();
    auto duration_batch_read =
      std::chrono::duration_cast<std::chrono::microseconds>(end_batch_read -
                                                            start_batch_read);

    std::cout << "Batch read (D1-D100): " << duration_batch_read.count()
              << " microseconds\n";

    // Tính tỷ lệ cải thiện
    double read_improvement =
      (double)duration_sequential_read.count() / duration_batch_read.count();
    std::cout << "Read speedup: " << std::fixed << std::setprecision(2)
              << read_improvement << "x faster\n";

    // =================== KIỂM TRA TÍNH CHÍNH XÁC ===================
    std::cout << "\n=== DATA INTEGRITY CHECK ===\n";

    // So sánh dữ liệu đọc được
    bool data_match = true;
    if (sequential_read_data.size() == batch_read_data.size()) {
      for (size_t i = 0; i < sequential_read_data.size(); i++) {
        if (sequential_read_data[i] != batch_read_data[i]) {
          std::cout << "Data mismatch at D" << (i + 1)
                    << ": sequential=" << sequential_read_data[i]
                    << ", batch=" << batch_read_data[i] << "\n";
          data_match = false;
        }
      }
      if (data_match) {
        std::cout << "✓ All data matches between sequential and batch reads\n";
      }
    } else {
      std::cout << "✗ Data size mismatch: sequential="
                << sequential_read_data.size()
                << ", batch=" << batch_read_data.size() << "\n";
      data_match = false;
    }

    // =================== HIỂN THỊ KẾT QUÀ MẪU ===================
    std::cout << "\n=== SAMPLE DATA (First 10 registers) ===\n";
    std::cout << "Register | Written | Sequential | Batch\n";
    std::cout << "---------|---------|------------|-------\n";

    for (int i = 0;
         i < std::min(10,
                      (int)std::min(test_data.size(),
                                    std::min(sequential_read_data.size(),
                                             batch_read_data.size())));
         i++) {
      std::cout << std::setw(8) << ("D" + std::to_string(i + 1)) << " | "
                << std::setw(7) << test_data[i] << " | " << std::setw(10)
                << sequential_read_data[i] << " | " << std::setw(5)
                << batch_read_data[i] << "\n";
    }

    // =================== TỔNG KẾT ===================
    std::cout << "\n=== PERFORMANCE SUMMARY ===\n";
    std::cout << "Write operations:\n";
    std::cout << "  Sequential: " << duration_sequential_write.count()
              << " μs\n";
    std::cout << "  Batch:      " << duration_batch_write.count() << " μs\n";
    std::cout << "  Speedup:    " << write_improvement << "x\n\n";

    std::cout << "Read operations:\n";
    std::cout << "  Sequential: " << duration_sequential_read.count()
              << " μs\n";
    std::cout << "  Batch:      " << duration_batch_read.count() << " μs\n";
    std::cout << "  Speedup:    " << read_improvement << "x\n\n";

    std::cout << "Data integrity: " << (data_match ? "PASSED" : "FAILED")
              << "\n";

    logger->info("Performance test completed");
    logger->info("Write speedup: {}x, Read speedup: {}x",
                 write_improvement,
                 read_improvement);
  }
  return 0;
}
