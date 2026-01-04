# Luồng Giao Tiếp ESP-NOW Giữa Master và Slave

Tài liệu này mô tả giao thức giao tiếp dựa trên JSON giữa thiết bị Master và các thiết bị Slave qua ESP-NOW.

## 1. Quá Trình Khám Phá Thiết Bị (Discovery)

Quá trình khám phá cho phép Master tìm ra các Slave đang có sẵn trong mạng.

### a. Master Gửi Broadcast

Định kỳ, Master sẽ gửi một bản tin broadcast để tìm kiếm các Slave.

*   **Địa chỉ nhận:** Địa chỉ Broadcast MAC (`FF:FF:FF:FF:FF:FF`)
*   **Định dạng:** JSON
*   **Nội dung bản tin:**
    ```json
    {
        "TYPE": "discovery",
        "ID": "XX:XX:XX:XX:XX:XX"
    }
    ```
    *   `TYPE`: Luôn là `"discovery"`.
    *   `ID`: Địa chỉ MAC của Master gửi yêu cầu.

### b. Slave Phản Hồi

Khi một Slave nhận được bản tin `discovery`, nó phải gửi phản hồi trực tiếp đến địa chỉ MAC của Master.

*   **Địa chỉ nhận:** Địa chỉ MAC của Master (lấy từ trường `ID` của bản tin discovery).
*   **Định dạng:** JSON
*   **Nội dung bản tin:**
    ```json
    {
        "TYPE": "discovery_response",
        "ID": "YY:YY:YY:YY:YY:YY",
        "NAME": "Slave_CamBien_NhietDo_1"
    }
    ```
    *   `TYPE`: Luôn là `"discovery_response"`.
    *   `ID`: Địa chỉ MAC của Slave.
    *   `NAME`: Một chuỗi ký tự để định danh Slave (ví dụ: chức năng hoặc vị trí của nó). Độ dài tối đa là 31 ký tự.

Khi Master nhận được phản hồi này, nó sẽ thêm Slave vào danh sách các thiết bị đã kết nối.

## 2. Quá Trình Yêu Cầu Dữ Liệu (Data Polling)

Master sẽ định kỳ yêu cầu (poll) dữ liệu cảm biến từ các Slave đã được khám phá.

### a. Master Gửi Yêu Cầu

Master gửi một bản tin đến một Slave cụ thể để yêu cầu dữ liệu.

*   **Địa chỉ nhận:** Địa chỉ MAC của Slave cụ thể.
*   **Định dạng:** JSON
*   **Nội dung bản tin:**
    ```json
    {
        "TYPE": "ask_data",
        "ID": "XX:XX:XX:XX:XX:XX"
    }
    ```
    *   `TYPE`: Luôn là `"ask_data"`.
    *   `ID`: Địa chỉ MAC của Master.

### b. Slave Gửi Dữ Liệu Phản Hồi

Khi Slave nhận được yêu cầu `ask_data`, nó sẽ phản hồi bằng bản tin chứa dữ liệu cảm biến của mình.

*   **Địa chỉ nhận:** Địa chỉ MAC của Master.
*   **Định dạng:** JSON
*   **Ví dụ (cho cảm biến DHT11):**
    ```json
    {
        "TYPE": "response_data",
        "ID": "YY:YY:YY:YY:YY:YY",
        "DST": "XX:XX:XX:XX:XX:XX",
        "data": {
            "temp": 25,
            "humi": 60
        }
    }
    ```
*   **Ví dụ (cho cảm biến ánh sáng Lux):**
    ```json
    {
        "TYPE": "response_data",
        "ID": "YY:YY:YY:YY:YY:YY",
        "DST": "XX:XX:XX:XX:XX:XX",
        "data": {
            "lux": 500
        }
    }
    ```
    *   `TYPE`: Luôn là `"response_data"`.
    *   `ID`: Địa chỉ MAC của Slave.
    *   `DST`: Địa chỉ MAC của Master mà Slave đang gửi phản hồi tới.
    *   `data`: Một đối tượng JSON chứa các giá trị đọc được từ cảm biến. Master hiện tại được code để xử lý `temp` và `humi` (cùng nhau), hoặc `lux` (riêng lẻ).
