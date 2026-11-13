# 🔥 HỆ THỐNG GIÁM SÁT VÀ CẢNH BÁO CHÁY SỬ DỤNG ESP32

**Sinh viên thực hiện:** Trần Ngọc Duy Anh  
**Mã sinh viên:** 231432172  
**Tên bài tập:** LAB3 - DE3: Mạch báo cháy gia đình

**Link mô phỏng Wokwi:** [Xem mô hình hệ thống](https://wokwi.com/projects/480384754066633729)

---

## 1. Tổng quan đề tài

### 1.1. Giới thiệu

Đề tài xây dựng một hệ thống giám sát và cảnh báo cháy trong gia đình dựa trên nền tảng vi điều khiển ESP32 kết hợp với công nghệ IoT.

Hệ thống sử dụng cảm biến MQ-2 để phát hiện sự thay đổi của khí gas và khói trong môi trường, đồng thời sử dụng cảm biến DHT22 để theo dõi nhiệt độ và độ ẩm. Dữ liệu sau khi thu thập được ESP32 xử lý thông qua cơ chế đa nhiệm FreeRTOS.

Bên cạnh chức năng cảnh báo tại chỗ bằng còi buzzer và điều khiển quạt, máy bơm, hệ thống còn tích hợp MQTT để truyền thông tin lên Internet. Người dùng có thể theo dõi các thông số cảm biến và gửi lệnh điều khiển thiết bị thông qua giao diện Web Dashboard.

### 1.2. Mục tiêu thực hiện

- Xây dựng mô hình phát hiện sớm các dấu hiệu bất thường liên quan đến khí gas, khói và nhiệt độ.
- Thu thập, xử lý và cập nhật dữ liệu cảm biến theo chu kỳ.
- Tổ chức chương trình theo mô hình đa nhiệm bằng FreeRTOS.
- Trao đổi dữ liệu giữa các Task thông qua Queue.
- Kết nối Wi-Fi và sử dụng MQTT để truyền nhận dữ liệu.
- Thiết kế giao diện Web phục vụ việc giám sát và điều khiển từ xa.

## 2. Mô hình và kiến trúc tổng thể

Hệ thống được chia thành ba phần chính:

1. **Khối đầu vào:** Bao gồm cảm biến MQ-2 và DHT22, thực hiện nhiệm vụ thu thập thông tin môi trường.
2. **Khối xử lý trung tâm:** ESP32 tiếp nhận dữ liệu, thực hiện các Task FreeRTOS, kiểm tra điều kiện cảnh báo và điều khiển thiết bị.
3. **Khối giao tiếp và đầu ra:** Bao gồm MQTT Broker, Web Dashboard, relay và buzzer.

### Sơ đồ nguyên lý hoạt động

```text
       ┌──────────────────┐
       │   CẢM BIẾN MQ-2  │
       │   Khí Gas / Khói │
       └────────┬─────────┘
                │
                ▼
       ┌──────────────────┐       ┌──────────────┐
       │                  │       │    DHT22     │
       │       ESP32      │◄──────│ Nhiệt độ/Độ  │
       │                  │       │     ẩm       │
       │    FreeRTOS      │       └──────────────┘
       │                  │
       │  ┌────────────┐  │
       │  │  TaskRead  │  │
       │  └─────┬──────┘  │
       │        │         │
       │      Queue       │
       │        │         │
       │  ┌─────▼──────┐  │
       │  │ TaskAlert  │──┼────► Relay / Buzzer
       │  └────────────┘  │
       │                  │
       │  ┌────────────┐  │
       │  │TaskTransmit│  │
       │  └─────┬──────┘  │
       └────────┼─────────┘
                │ Wi-Fi
                ▼
       ┌──────────────────┐
       │   MQTT BROKER    │
       │   EMQX Server    │
       └────────┬─────────┘
                │
                ▼
       ┌──────────────────┐
       │   WEB DASHBOARD  │
       │ Giám sát và điều │
       │ khiển thiết bị   │
       └──────────────────┘
```

## 3. Danh sách phần cứng

Các linh kiện được sử dụng trong mô hình:

| STT | Linh kiện | Nhiệm vụ |
|---|---|---|
| 1 | ESP32 DevKit V1 | Xử lý dữ liệu và quản lý toàn bộ hệ thống |
| 2 | MQ-2 | Phát hiện khí gas và khói |
| 3 | DHT22 | Đo nhiệt độ và độ ẩm |
| 4 | Relay quạt | Đóng/ngắt thiết bị quạt |
| 5 | Relay máy bơm | Điều khiển máy bơm |
| 6 | Buzzer | Phát tín hiệu âm thanh cảnh báo |

### 3.1. Bảng phân bố chân kết nối

| Thiết bị | Chân ESP32 | Chức năng |
|---|---|---|
| MQ-2 (AOUT) | GPIO 34 | Đọc tín hiệu analog |
| DHT22 (DATA) | GPIO 15 | Nhận dữ liệu cảm biến |
| Relay quạt | GPIO 26 | Điều khiển quạt |
| Relay máy bơm | GPIO 27 | Điều khiển máy bơm |
| Buzzer | GPIO 25 | Phát âm thanh cảnh báo |

## 4. Xây dựng chương trình với FreeRTOS

Để tránh việc xử lý cảm biến, cảnh báo và truyền thông mạng phụ thuộc hoàn toàn vào một luồng chương trình, hệ thống được tổ chức thành ba Task riêng biệt.

Mỗi Task đảm nhiệm một chức năng cụ thể và được phân bố trên hai nhân xử lý của ESP32.

### 4.1. TaskRead – Thu thập dữ liệu

**Core sử dụng:** Core 1  
**Độ ưu tiên:** 2  
**Chu kỳ thực hiện:** 2 giây

Nhiệm vụ:

- Lấy giá trị ADC từ cảm biến MQ-2.
- Đọc nhiệt độ và độ ẩm thông qua DHT22.
- Kiểm tra tính hợp lệ của dữ liệu cảm biến.
- Xuất thông số lên Serial Monitor.
- Chuyển dữ liệu đến hai Queue để các Task khác tiếp tục xử lý.

Task này đóng vai trò là nguồn cung cấp dữ liệu đầu vào cho toàn bộ hệ thống.

### 4.2. TaskAlert – Giám sát và điều khiển

**Core sử dụng:** Core 1  
**Độ ưu tiên:** 3

Đây là Task chịu trách nhiệm xử lý các điều kiện cảnh báo và điều khiển thiết bị.

Các công việc chính:

- Nhận thông tin cảm biến thông qua `dataAlert`.
- Đối chiếu giá trị Gas và nhiệt độ với ngưỡng quy định.
- Xác định trạng thái hoạt động của quạt và máy bơm.
- Điều khiển buzzer khi xuất hiện điều kiện cảnh báo.
- Tiếp nhận yêu cầu điều khiển thủ công từ `commandQueue`.
- Chuyển đổi giữa chế độ tự động và điều khiển bằng Web.

### 4.3. TaskTransmit – Truyền thông dữ liệu

**Core sử dụng:** Core 0  
**Độ ưu tiên:** 2

Task này phụ trách kết nối mạng và trao đổi dữ liệu với máy chủ MQTT.

Quy trình thực hiện:

- Khởi tạo kết nối Wi-Fi.
- Thiết lập kết nối đến MQTT Broker.
- Theo dõi trạng thái kết nối và thực hiện kết nối lại khi cần.
- Lấy dữ liệu cảm biến từ `dataQueue`.
- Chuyển thông tin thành chuỗi JSON.
- Gửi dữ liệu lên MQTT Broker bằng lệnh Publish.
- Đăng ký các Topic điều khiển bằng lệnh Subscribe.
- Chuyển yêu cầu điều khiển nhận được vào Queue tương ứng.

### 4.4. Trao đổi dữ liệu giữa các Task

Chương trình sử dụng ba Queue để tổ chức quá trình truyền dữ liệu:

| Queue | Nơi gửi | Nơi nhận | Công dụng |
|---|---|---|---|
| `dataQueue` | TaskRead | TaskTransmit | Chuyển thông số cảm biến để gửi MQTT |
| `dataAlert` | TaskRead | TaskAlert | Cung cấp dữ liệu phục vụ kiểm tra cảnh báo |
| `commandQueue` | MQTT Callback | TaskAlert | Truyền yêu cầu điều khiển thiết bị |

Nhờ cơ chế Queue, các Task có thể trao đổi thông tin mà không cần gọi trực tiếp các hàm xử lý của nhau.

## 5. Nguyên lý vận hành và xử lý cảnh báo

Sau khi hệ thống được cấp nguồn, ESP32 khởi tạo cảm biến, các chân điều khiển và ba Queue. Tiếp theo, ba Task được tạo và hoạt động theo chức năng đã thiết lập.

### 5.1. Giám sát môi trường

MQ-2 cung cấp giá trị ADC trong khoảng 0–4095 đối với cấu hình đọc analog của ESP32. DHT22 cung cấp hai thông số là nhiệt độ và độ ẩm.

Dữ liệu được cập nhật theo chu kỳ 2 giây và chuyển đến các Task xử lý.

### 5.2. Điều kiện cảnh báo

Các ngưỡng được thiết lập trong chương trình:

| Đại lượng | Ngưỡng thiết lập | Điều kiện kích hoạt |
|---|---:|---|
| Khí Gas (ADC) | 2000 | Lớn hơn 2000 |
| Nhiệt độ | 50°C | Lớn hơn 50°C |

Cơ chế phản ứng:

- Khi giá trị Gas vượt ngưỡng: quạt được kích hoạt và buzzer phát cảnh báo.
- Khi nhiệt độ vượt 50°C: máy bơm được kích hoạt và buzzer phát cảnh báo.
- Khi cả hai điều kiện cùng xuất hiện: quạt, máy bơm và buzzer cùng hoạt động theo logic điều khiển.

**Lưu ý:** Giá trị 2000 của MQ-2 là ngưỡng ADC sử dụng trong mô hình, không phải giá trị nồng độ khí đo theo đơn vị ppm.

### 5.3. Các chế độ điều khiển

Hệ thống cung cấp ba lựa chọn cho từng thiết bị:

| Lệnh | Ý nghĩa | Hoạt động |
|---|---|---|
| `ON` | Bật thủ công | Bật thiết bị được chọn |
| `OFF` | Tắt thủ công | Tắt thiết bị được chọn |
| `AUTO` | Tự động | Trả quyền điều khiển về TaskAlert |

Chế độ thủ công của quạt và máy bơm được quản lý độc lập, không phụ thuộc vào nhau.

## 6. Tích hợp giao thức MQTT

MQTT được sử dụng làm phương thức trao đổi thông tin giữa ESP32 và giao diện Web.

### 6.1. Thông số kết nối

| Thuộc tính | Giá trị |
|---|---|
| MQTT Broker | broker.emqx.io |
| Giao thức | MQTT |
| ESP32 Port | 1883 |
| WebSocket Port | 8084 |
| Kết nối Web | MQTT over WebSocket |

### 6.2. Hệ thống Topic

| Topic | Hướng truyền | Mục đích |
|---|---|---|
| `hoang/fire/sensor` | ESP32 → Web | Cập nhật dữ liệu cảm biến |
| `hoang/fire/cmd/fan` | Web → ESP32 | Điều khiển quạt |
| `hoang/fire/cmd/pump` | Web → ESP32 | Điều khiển máy bơm |

### 6.3. Cấu trúc dữ liệu truyền nhận

Dữ liệu cảm biến được đóng gói theo định dạng JSON:

```json
{
    "gas": 1850,
    "temp": 32.5,
    "hum": 70.2
}
```

Ý nghĩa các trường:

- `gas`: Giá trị ADC thu được từ MQ-2.
- `temp`: Nhiệt độ đo được từ DHT22.
- `hum`: Độ ẩm đo được từ DHT22.

Đối với chiều điều khiển, Web gửi các chuỗi `ON`, `OFF` hoặc `AUTO` đến Topic của thiết bị tương ứng.

## 7. Thiết kế giao diện giám sát Web

Giao diện Web được xây dựng bằng HTML, CSS và JavaScript nhằm giúp người dùng theo dõi tình trạng hoạt động của hệ thống mà không cần truy cập trực tiếp vào Serial Monitor.

### 7.1. Các chức năng chính

- Hiển thị giá trị khí Gas từ cảm biến MQ-2.
- Theo dõi nhiệt độ và độ ẩm môi trường.
- Sử dụng biểu đồ để thể hiện sự thay đổi của dữ liệu theo thời gian.
- Hiển thị trạng thái cảnh báo khi các thông số vượt ngưỡng.
- Cung cấp nút điều khiển quạt và máy bơm.
- Hỗ trợ chuyển đổi chế độ AUTO và điều khiển thủ công.

### 7.2. Công nghệ giao diện

| Thành phần | Vai trò |
|---|---|
| HTML | Xây dựng cấu trúc trang Web |
| CSS | Thiết kế bố cục và giao diện |
| JavaScript | Xử lý dữ liệu và sự kiện |
| MQTT.js | Giao tiếp với MQTT Broker |
| Chart.js | Tạo biểu đồ giám sát |

Dữ liệu được cập nhật khi Web nhận được bản tin từ Topic cảm biến. Các thao tác trên nút điều khiển sẽ tạo bản tin MQTT gửi ngược về ESP32.

## 8. Công nghệ và môi trường phát triển

| Công nghệ / Công cụ | Mục đích sử dụng |
|---|---|
| ESP32 | Nền tảng phần cứng chính |
| C/C++ | Xây dựng chương trình điều khiển |
| Arduino Framework | Hỗ trợ lập trình ESP32 |
| FreeRTOS | Quản lý các tác vụ đồng thời |
| Wi-Fi | Kết nối Internet |
| MQTT | Trao đổi dữ liệu IoT |
| HTML/CSS/JavaScript | Phát triển Web Dashboard |
| PlatformIO | Biên dịch và quản lý chương trình |
| Wokwi | Mô phỏng mạch điện và hoạt động của hệ thống |

## 9. Thư viện sử dụng

Các thư viện cần thiết trong môi trường PlatformIO:

| Tên thư viện | Chức năng |
|---|---|
| WiFi.h | Kết nối Wi-Fi |
| PubSubClient | Giao tiếp MQTT |
| DHT sensor library | Đọc dữ liệu DHT22 |
| Adafruit Unified Sensor | Thư viện hỗ trợ cảm biến |

Ngoài ra, các chức năng FreeRTOS được sử dụng thông qua ESP32 Arduino Core.

## 10. Kết quả và đánh giá

### 10.1. Các chức năng đã xây dựng

- Thu thập dữ liệu từ hai loại cảm biến MQ-2 và DHT22.
- Xử lý dữ liệu theo mô hình ba Task FreeRTOS.
- Sử dụng Queue để trao đổi thông tin giữa các tác vụ.
- Phát cảnh báo khi điều kiện môi trường vượt ngưỡng cài đặt.
- Điều khiển quạt và máy bơm theo chế độ tự động hoặc thủ công.
- Truyền thông tin cảm biến lên MQTT Broker.
- Xây dựng Web Dashboard để theo dõi và gửi lệnh điều khiển.

### 10.2. Hướng phát triển

Trong các phiên bản tiếp theo, hệ thống có thể được cải tiến bằng cách:

- Bổ sung lưu trữ lịch sử dữ liệu cảm biến.
- Thêm chức năng thông báo cảnh báo đến điện thoại.
- Xây dựng cơ chế xác thực và bảo mật kết nối MQTT.
- Bổ sung các mức cảnh báo khác nhau.
- Kiểm tra và hiệu chỉnh ngưỡng cảm biến dựa trên điều kiện thực tế.

---

**Sinh viên thực hiện: Trần Ngọc Duy Anh**

**Mã sinh viên: 231432172**

**Đề tài: Mạch báo cháy gia đình sử dụng ESP32 và công nghệ IoT.**
