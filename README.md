# Hướng dẫn sử dụng thư viện PLC Client

## Tổng quan

Thư viện `plc_client.hpp` cung cấp một interface C++ để giao tiếp với PLC Mitsubishi thông qua giao thức SLMP (Seamless Message Protocol). Thư viện hỗ trợ cả kết nối TCP và UDP.

## Tính năng chính

- Kết nối TCP/UDP với PLC Mitsubishi hỗ trợ SLMP
- Đọc/ghi thanh ghi D (Data Register)
- Đọc/ghi nhiều thanh ghi cùng lúc
- Validation địa chỉ thanh ghi
- Thread-safe với mutex
- Hỗ trợ các loại thanh ghi: D, X, Y, M, B, SD

## Cài đặt và dependencies

### Yêu cầu
- C++11 trở lên
- Thư viện `libmelcli`, `libslmp` (Mitsubishi Electric Communication Library)
- CMake (để build project)

### Cài đặt libmelcli
...

## Cách sử dụng

### 1. Include thư viện

```cpp
#include "plc_client.hpp"

using namespace plc_slmp;
```

### 2. Khởi tạo PLC Client

```cpp
// Kết nối TCP
PlcClient plc("192.168.1.100", 2001, MELCLI_TYPE_TCPIP);

// Kết nối UDP
PlcClient plc("192.168.1.100", 2001, MELCLI_TYPE_UDPIP);
```

### 3. Kết nối đến PLC

```cpp
if (!plc.init_plc()) {
    std::cerr << "Không thể kết nối đến PLC" << std::endl;
    return -1;
}
std::cout << "Đã kết nối thành công đến PLC" << std::endl;
```

### 4. Đọc thanh ghi

#### Đọc một thanh ghi D
```cpp
uint16_t value;
if (plc.read_batch_d_register("D100", value)) {
    std::cout << "Giá trị tại D100: " << value << std::endl;
} else {
    std::cerr << "Lỗi khi đọc D100" << std::endl;
}
```

#### Đọc nhiều thanh ghi D
```cpp
std::vector<uint16_t> values;
if (plc.read_batch_d_registers("D100", 5, values)) {
    std::cout << "Đã đọc " << values.size() << " thanh ghi:" << std::endl;
    for (size_t i = 0; i < values.size(); i++) {
        std::cout << "D" << (100 + i) << ": " << values[i] << std::endl;
    }
} else {
    std::cerr << "Lỗi khi đọc thanh ghi" << std::endl;
}
```

### 5. Ghi thanh ghi

#### Ghi một thanh ghi D
```cpp
uint16_t value = 12345;
if (plc.write_batch_d_register("D200", value)) {
    std::cout << "Đã ghi giá trị " << value << " vào D200" << std::endl;
} else {
    std::cerr << "Lỗi khi ghi D200" << std::endl;
}
```

#### Ghi nhiều thanh ghi D
```cpp
std::vector<uint16_t> values = {100, 200, 300, 400, 500};
if (plc.write_batch_d_registers("D300", 5, values)) {
    std::cout << "Đã ghi " << values.size() << " giá trị vào thanh ghi" << std::endl;
} else {
    std::cerr << "Lỗi khi ghi thanh ghi" << std::endl;
}
```

### 6. Validation địa chỉ thanh ghi

```cpp
// Kiểm tra địa chỉ hợp lệ
if (plc.is_valid_register_address("D100")) {
    std::cout << "D100 là địa chỉ hợp lệ" << std::endl;
}

if (plc.is_valid_register_address("X10")) {
    std::cout << "X10 là địa chỉ hợp lệ" << std::endl;
}

// Lấy loại thanh ghi
RegisterType type = plc.get_address_type("D100");
std::string type_name = plc.get_address_type_name("D100");
std::cout << "D100 là " << type_name << std::endl;
```

### 7. Ngắt kết nối

```cpp
plc.disconnect();
```

## Các loại thanh ghi được hỗ trợ

| Loại | Định dạng | Ví dụ | Mô tả |
|------|-----------|-------|-------|
| D Register | `D\d+` | D0, D100, D1000 | Data Register |
| X Register | `X[0-9A-Fa-f]+` | X0, X1, XA, XFF | Input Register |
| Y Register | `Y[0-9A-Fa-f]+` | Y0, Y1, YA, YFF | Output Register |
| M Register | `M\d+` | M0, M1, M100 | Memory Register |
| B Register | `B[0-9A-Fa-f]+` | B0, B1, BA, BFF | Link Register |
| SD Register | `SD[0-9A-Fa-f]+` | SD0, SD1, SD10 | SD Register |

## Ví dụ hoàn chỉnh

```cpp
#include "test_slmp/plc_client.hpp"
#include <iostream>

using namespace test_slmp;

int main() {
    // Khởi tạo PLC Client với kết nối TCP
    PlcClient plc("192.168.1.100", 2001, MELCLI_TYPE_TCPIP);
    
    // Kết nối đến PLC
    if (!plc.init_plc()) {
        std::cerr << "Không thể kết nối đến PLC" << std::endl;
        return -1;
    }
    
    // Đọc một thanh ghi
    uint16_t value;
    if (plc.read_batch_d_register("D100", value)) {
        std::cout << "Giá trị tại D100: " << value << std::endl;
    }
    
    // Ghi một thanh ghi
    if (plc.write_batch_d_register("D200", 12345)) {
        std::cout << "Đã ghi giá trị vào D200" << std::endl;
    }
    
    // Đọc nhiều thanh ghi
    std::vector<uint16_t> values;
    if (plc.read_batch_d_registers("D300", 5, values)) {
        for (size_t i = 0; i < values.size(); i++) {
            std::cout << "D" << (300 + i) << ": " << values[i] << std::endl;
        }
    }
    
    // Ngắt kết nối
    plc.disconnect();
    
    return 0;
}
```

## Lưu ý quan trọng

1. **Thread Safety**: Thư viện sử dụng mutex để đảm bảo thread-safe khi đọc/ghi thanh ghi.

2. **Validation**: Tất cả địa chỉ thanh ghi đều được validate trước khi thực hiện thao tác.

3. **Error Handling**: Các method trả về `bool` để chỉ ra thành công hay thất bại.

4. **Memory Management**: Thư viện tự động quản lý memory cho các thao tác đọc/ghi.

5. **Timeout**: Sử dụng timeout mặc định của libmelcli. Có thể điều chỉnh trong constructor nếu cần.

## Troubleshooting

### Lỗi kết nối
- Kiểm tra địa chỉ IP và port của PLC
- Đảm bảo PLC đang chạy và có thể truy cập từ máy tính
- Kiểm tra firewall và network settings

### Lỗi đọc/ghi thanh ghi
- Kiểm tra địa chỉ thanh ghi có hợp lệ không
- Đảm bảo thanh ghi tồn tại trong PLC
- Kiểm tra quyền truy cập đến thanh ghi

### Lỗi build
- Đảm bảo đã cài đặt libmelcli
- Kiểm tra CMakeLists.txt có link đúng thư viện
- Đảm bảo sử dụng C++11 trở lên

## API Reference

### Constructor
```cpp
PlcClient(std::string target_ip_addr, int target_port, int type_protocol)
```

### Methods chính
- `bool init_plc()`: Kết nối đến PLC
- `bool disconnect()`: Ngắt kết nối
- `bool read_batch_d_register(const char* addr, uint16_t& data)`: Đọc một thanh ghi D
- `bool read_batch_d_registers(const char* addr, int num, std::vector<uint16_t>& data)`: Đọc nhiều thanh ghi D
- `bool write_batch_d_register(const char* addr, uint16_t data)`: Ghi một thanh ghi D
- `bool write_batch_d_registers(const char* addr, int num, const std::vector<uint16_t>& data)`: Ghi nhiều thanh ghi D
- `bool is_valid_register_address(const char* addr)`: Kiểm tra địa chỉ hợp lệ
- `RegisterType get_address_type(const char* addr)`: Lấy loại thanh ghi
- `std::string get_address_type_name(const char* addr)`: Lấy tên loại thanh ghi 