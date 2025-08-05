#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <spdlog/common.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <sstream>
#include <test_slmp/plc_client.hpp>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

struct RegisterGroup {
  int         start_addr;
  int         count;
  std::string name;

  RegisterGroup(int start, int cnt, const std::string &nm)
    : start_addr(start), count(cnt), name(nm) {
  }

  std::string get_address() const {
    return "D" + std::to_string(start_addr);
  }
};

// Hàm để ghi CSV header
void write_csv_header(const std::string &filename) {
  std::ofstream csv_file(filename, std::ios::trunc);
  if (csv_file.is_open()) {
    csv_file
      << "Timestamp,Cycle,Write_Scattered_us,Write_Sequential_us,Write_Ratio,"
      << "Read_Scattered_us,Read_Sequential_us,Read_Ratio,Data_Integrity,"
      << "Total_Scattered_us,Total_Sequential_us,Total_Ratio\n";
    csv_file.close();
  }
}

// Hàm để ghi dữ liệu CSV
void write_csv_data(const std::string &filename,
                    int                cycle,
                    long               write_scattered,
                    long               write_sequential,
                    double             write_ratio,
                    long               read_scattered,
                    long               read_sequential,
                    double             read_ratio,
                    bool               data_integrity) {
  std::ofstream csv_file(filename, std::ios::app);
  if (csv_file.is_open()) {
    // Lấy timestamp hiện tại
    auto now    = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms     = std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch()) %
              1000;

    // Format timestamp
    std::stringstream timestamp;
    timestamp << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    timestamp << "." << std::setfill('0') << std::setw(3) << ms.count();

    // Tính tổng thời gian
    long   total_scattered  = write_scattered + read_scattered;
    long   total_sequential = write_sequential + read_sequential;
    double total_ratio      = (double)total_scattered / total_sequential;

    csv_file << timestamp.str() << "," << cycle << "," << write_scattered << ","
             << write_sequential << "," << write_ratio << "," << read_scattered
             << "," << read_sequential << "," << read_ratio << ","
             << (data_integrity ? "PASS" : "FAIL") << "," << total_scattered
             << "," << total_sequential << "," << total_ratio << "\n";

    csv_file.close();
  }
}

int main(int, char **) {
  // Cấu hình logging
  auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
  auto file_sink =
    std::make_shared<spdlog::sinks::basic_file_sink_mt>("scattered_test.log",
                                                        true);

  console_sink->set_pattern("[%H:%M:%S.%e] [%^%l%$] %v");
  file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");

  std::vector<spdlog::sink_ptr> sinks{console_sink, file_sink};
  auto logger = std::make_shared<spdlog::logger>("scattered_test",
                                                 sinks.begin(),
                                                 sinks.end());

  spdlog::register_logger(logger);
  spdlog::set_default_logger(logger);
  spdlog::set_level(spdlog::level::info);

  // Khởi tạo random generator
  std::random_device              rd;
  std::mt19937                    gen(rd());
  std::uniform_int_distribution<> dis(0, 65535);

  logger->info("Starting Scattered Memory Access Test");
  logger->info(
    "Test comparison: Scattered(10x100 batches) vs Sequential(1000x1 "
    "single)");

  // Kết nối PLC
  plc_slmp::PlcClient plc_client("192.168.6.10", 502, MELCLI_TYPE_TCPIP);

  if (!plc_client.init_plc()) {
    logger->error("Failed to initialize PLC connection");
    return -1;
  }

  logger->info("PLC connection established successfully");

  // Định nghĩa 10 cụm thanh ghi (mỗi cụm 100 thanh ghi)
  std::vector<RegisterGroup> register_groups = {
    RegisterGroup(1, 100, "D1-D100"),      // Cụm 1
    RegisterGroup(101, 100, "D101-D200"),  // Cụm 2
    RegisterGroup(201, 100, "D201-D300"),  // Cụm 3
    RegisterGroup(301, 100, "D301-D400"),  // Cụm 4
    RegisterGroup(401, 100, "D401-D500"),  // Cụm 5
    RegisterGroup(501, 100, "D501-D600"),  // Cụm 6
    RegisterGroup(601, 100, "D601-D700"),  // Cụm 7
    RegisterGroup(701, 100, "D701-D800"),  // Cụm 8
    RegisterGroup(801, 100, "D801-D900"),  // Cụm 9
    RegisterGroup(901, 100, "D901-D1000")  // Cụm 10
  };

  // Kiểm tra tính hợp lệ của address
  for (const auto &group : register_groups) {
    if (!plc_client.is_valid_register_address(group.get_address().c_str())) {
      logger->error("Invalid register address: {}", group.get_address());
      return -1;
    }
  }

  logger->info("All register addresses validated successfully");

  // Pattern đọc cách xa nhau: 1->6->2->7->3->8->4->9->5->10
  std::vector<int> scattered_pattern = {0, 5, 1, 6, 2, 7, 3, 8, 4, 9};

  logger->info(
    "Testing scattered access pattern: {}",
    "D1-D100 -> D501-D600 -> D101-D200 -> D601-D700 -> D201-D300 -> "
    "D701-D800 -> D301-D400 -> D801-D900 -> D401-D500 -> D901-D1000");

  // Khởi tạo file CSV
  std::string csv_filename = "performance_results.csv";
  write_csv_header(csv_filename);
  std::cout << "CSV file created: " << csv_filename << std::endl;

  int cycle_count = 0;

  while (true) {
    cycle_count++;
    std::cout << "\n" << std::string(80, '=') << std::endl;
    std::cout << "Starting test cycle #" << cycle_count << "..." << std::endl;

    // Chuẩn bị dữ liệu test (1000 thanh ghi)
    std::vector<uint16_t> test_data;
    for (int i = 0; i < 1000; i++) {
      test_data.push_back(dis(gen));
    }

    // =================== TEST GHI SCATTERED ===================
    std::cout << "\n=== TESTING SCATTERED WRITE OPERATIONS ===\n";

    auto start_scattered_write = std::chrono::high_resolution_clock::now();

    for (int pattern_idx : scattered_pattern) {
      const RegisterGroup  &group = register_groups[pattern_idx];
      // Sửa lỗi: sử dụng pattern_idx để lấy data tương ứng với group đó
      std::vector<uint16_t> group_data(test_data.begin() + pattern_idx * 100,
                                       test_data.begin() +
                                         (pattern_idx + 1) * 100);

      if (!plc_client.write_batch_d_registers(group.get_address().c_str(),
                                              100,
                                              group_data)) {
        logger->error("Failed to write to group: {}", group.name);
      } else {
        logger->debug("Successfully wrote to group: {}", group.name);
      }

      std::cout << "Wrote " << group.name << std::endl;
    }

    auto end_scattered_write = std::chrono::high_resolution_clock::now();
    auto duration_scattered_write =
      std::chrono::duration_cast<std::chrono::microseconds>(
        end_scattered_write - start_scattered_write);

    // =================== TEST GHI SEQUENTIAL ===================
    std::cout << "\n=== TESTING SEQUENTIAL WRITE OPERATIONS ===\n";

    std::this_thread::sleep_for(100ms);

    auto start_sequential_write = std::chrono::high_resolution_clock::now();

    // Ghi tuần tự từng thanh ghi từ D1 đến D1000 (1000 lần ghi đơn lẻ)
    for (int i = 1; i <= 1000; i++) {
      std::string addr = "D" + std::to_string(i);
      if (!plc_client.write_batch_d_register(addr.c_str(), test_data[i - 1])) {
        logger->error("Failed to write to register: {}", addr);
      }

      if (i % 100 == 0) {
        std::cout << "Wrote up to " << addr << " (" << i << "/1000)"
                  << std::endl;
      }
    }

    std::cout << "Completed writing D1-D1000 (1000 individual writes)"
              << std::endl;

    auto end_sequential_write = std::chrono::high_resolution_clock::now();
    auto duration_sequential_write =
      std::chrono::duration_cast<std::chrono::microseconds>(
        end_sequential_write - start_sequential_write);

    // =================== TEST ĐỌC SCATTERED ===================
    std::cout << "\n=== TESTING SCATTERED READ OPERATIONS ===\n";

    std::this_thread::sleep_for(100ms);

    auto start_scattered_read = std::chrono::high_resolution_clock::now();

    // Khởi tạo vector với size phù hợp
    std::vector<std::vector<uint16_t>> scattered_read_data(10);
    for (int pattern_idx : scattered_pattern) {
      const RegisterGroup &group = register_groups[pattern_idx];

      if (!plc_client.read_batch_d_registers(
            group.get_address().c_str(),
            100,
            scattered_read_data[pattern_idx])) {
        logger->error("Failed to read from group: {}", group.name);
        scattered_read_data[pattern_idx].resize(
          100, 0);  // Đảm bảo có đủ size nếu read fail
      } else {
        logger->debug("Successfully read from group: {}", group.name);
      }

      std::cout << "Read " << group.name << std::endl;
    }

    auto end_scattered_read = std::chrono::high_resolution_clock::now();
    auto duration_scattered_read =
      std::chrono::duration_cast<std::chrono::microseconds>(
        end_scattered_read - start_scattered_read);

    // =================== TEST ĐỌC SEQUENTIAL ===================
    std::cout << "\n=== TESTING SEQUENTIAL READ OPERATIONS ===\n";

    std::this_thread::sleep_for(100ms);

    auto start_sequential_read = std::chrono::high_resolution_clock::now();

    // Đọc tuần tự từng thanh ghi từ D1 đến D1000 (1000 lần đọc đơn lẻ)
    std::vector<uint16_t> sequential_read_data;
    sequential_read_data.reserve(1000);

    for (int i = 1; i <= 1000; i++) {
      std::string addr = "D" + std::to_string(i);
      uint16_t    value;

      if (plc_client.read_batch_d_register(addr.c_str(), value)) {
        sequential_read_data.push_back(value);
      } else {
        logger->error("Failed to read from register: {}", addr);
        sequential_read_data.push_back(0);  // Default value nếu fail
      }

      if (i % 100 == 0) {
        std::cout << "Read up to " << addr << " (" << i << "/1000)"
                  << std::endl;
      }
    }

    std::cout << "Completed reading D1-D1000 (1000 individual reads)"
              << std::endl;

    auto end_sequential_read = std::chrono::high_resolution_clock::now();
    auto duration_sequential_read =
      std::chrono::duration_cast<std::chrono::microseconds>(
        end_sequential_read - start_sequential_read);

    // =================== HIỂN THỊ KẾT QUẢ MẪU ===================
    std::cout << "\n=== SAMPLE DATA (First 10 registers from each scattered "
                 "group) ===\n";
    std::cout << "Group    | Register | Written | Scattered | Sequential\n";
    std::cout << "---------|----------|---------|-----------|------------\n";

    for (int i = 0; i < 10; i++) {
      if (scattered_read_data[i].size() > 0 &&
          (i * 100) < sequential_read_data.size()) {
        int sequential_index = i * 100;
        std::cout << std::setw(8) << register_groups[i].name << " | "
                  << std::setw(8) << register_groups[i].get_address() << " | "
                  << std::setw(7) << test_data[i * 100] << " | " << std::setw(9)
                  << scattered_read_data[i][0] << " | " << std::setw(10)
                  << sequential_read_data[sequential_index] << "\n";
      }
    }

    // =================== KIỂM TRA DATA INTEGRITY ===================
    std::cout << "=== DATA INTEGRITY CHECK ===\n";

    bool integrity_ok = true;

    // Kiểm tra size của sequential data
    if (sequential_read_data.size() != 1000) {
      std::cout << "✗ Sequential data size mismatch: expected 1000, got "
                << sequential_read_data.size() << std::endl;
      integrity_ok = false;
    }

    // So sánh từng group scattered với phần tương ứng trong sequential
    for (int i = 0; i < 10; i++) {
      if (scattered_read_data[i].size() != 100) {
        std::cout << "✗ Size mismatch in scattered group "
                  << register_groups[i].name << std::endl;
        integrity_ok = false;
        continue;
      }

      // So sánh group i của scattered với offset tương ứng trong sequential
      int sequential_offset = i * 100;
      for (int j = 0; j < 100; j++) {
        int sequential_index = sequential_offset + j;

        if (sequential_index >= sequential_read_data.size()) {
          std::cout << "✗ Sequential data index out of bounds at "
                    << sequential_index << std::endl;
          integrity_ok = false;
          break;
        }

        if (scattered_read_data[i][j] !=
            sequential_read_data[sequential_index]) {
          std::cout << "✗ Data mismatch in " << register_groups[i].name
                    << " at offset " << j
                    << ": scattered=" << scattered_read_data[i][j]
                    << ", sequential=" << sequential_read_data[sequential_index]
                    << std::endl;
          integrity_ok = false;
          break;
        }
      }
    }

    if (integrity_ok) {
      std::cout << "✓ All data integrity checks passed\n";
    }

    // =================== HIỂN THỊ KẾT QUẢ PERFORMANCE ===================
    std::cout << "\n=== PERFORMANCE COMPARISON ===\n";

    double write_ratio = (double)duration_scattered_write.count() /
                         duration_sequential_write.count();
    double read_ratio = (double)duration_scattered_read.count() /
                        duration_sequential_read.count();

    std::cout << "Write Operations:\n";
    std::cout << "  Scattered (10 batches):   "
              << duration_scattered_write.count() << " μs\n";
    std::cout << "  Sequential (1000 single): "
              << duration_sequential_write.count() << " μs\n";
    std::cout << "  Ratio (Scattered/Sequential): " << std::fixed
              << std::setprecision(2) << write_ratio << "x\n\n";

    std::cout << "Read Operations:\n";
    std::cout << "  Scattered (10 batches):   "
              << duration_scattered_read.count() << " μs\n";
    std::cout << "  Sequential (1000 single): "
              << duration_sequential_read.count() << " μs\n";
    std::cout << "  Ratio (Scattered/Sequential): " << std::fixed
              << std::setprecision(2) << read_ratio << "x\n\n";

    // Log kết quả
    logger->info(
      "Cycle completed - Write: Scattered(10 batches)={}μs, Sequential(1000 "
      "single)={}μs, "
      "Ratio={:.2f}x",
      duration_scattered_write.count(),
      duration_sequential_write.count(),
      write_ratio);
    logger->info(
      "Cycle completed - Read: Scattered(10 batches)={}μs, Sequential(1000 "
      "single)={}μs, "
      "Ratio={:.2f}x",
      duration_scattered_read.count(),
      duration_sequential_read.count(),
      read_ratio);
    logger->info("Data integrity: {}", integrity_ok ? "PASSED" : "FAILED");

    // Ghi dữ liệu vào CSV
    write_csv_data(csv_filename,
                   cycle_count,
                   duration_scattered_write.count(),
                   duration_sequential_write.count(),
                   write_ratio,
                   duration_scattered_read.count(),
                   duration_sequential_read.count(),
                   read_ratio,
                   integrity_ok);

    std::cout << "Data logged to CSV: " << csv_filename << std::endl;

    // Chờ trước khi chạy cycle tiếp theo
    std::cout << "\nWaiting 2 seconds before next cycle...\n";
    std::this_thread::sleep_for(2s);
  }

  plc_client.disconnect();
  return 0;
}