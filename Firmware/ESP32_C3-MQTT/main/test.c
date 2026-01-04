
// ============ DEMO CÁC TRƯỜNG HỢP ============
void demo_all_cases(void)
{
    uint8_t uart_buffer[256];
    uint16_t length;
    Sensor_Data sensor_data;

    ESP_LOGI(MAIN_TAG, "\n========== DEMO CÁC TRƯỜNG HỢP SỬ DỤNG CỜ ==========\n");

    // TH1: Không có dữ liệu gì cả
    ESP_LOGI(MAIN_TAG, "TH1: Không có dữ liệu (flags=0x00)");
    sensor_data = (Sensor_Data){.flags = SENSOR_FLAG_NONE, .lux = 0, .temp = 0, .humi = 0};
    length = create_uart_data_message(&sensor_data, uart_buffer);
    uart.send.bytes(uart_buffer, length);
    ESP_LOGI(MAIN_TAG, "  => Sent: flags=0x%02X, lux=%d, temp=%d, humi=%d\n",
             sensor_data.flags, sensor_data.lux, sensor_data.temp, sensor_data.humi);
    vTaskDelay(pdMS_TO_TICKS(1000));

    // TH2: Chỉ có lux, không có DHT11
    ESP_LOGI(MAIN_TAG, "TH2: Chỉ có lux (flags=0x01)");
    const char *json_lux_only = "{\"data\":{\"lux\":60}}";
    sensor_data = (Sensor_Data){.flags = SENSOR_FLAG_NONE, .lux = 0, .temp = 0, .humi = 0};
    extract_sensor_data_from_json(json_lux_only, &sensor_data);
    length = create_uart_data_message(&sensor_data, uart_buffer);
    uart.send.bytes(uart_buffer, length);
    ESP_LOGI(MAIN_TAG, "  => Sent: flags=0x%02X, lux=%d, temp=%d, humi=%d\n",
             sensor_data.flags, sensor_data.lux, sensor_data.temp, sensor_data.humi);
    vTaskDelay(pdMS_TO_TICKS(1000));

    // TH3: Chỉ có DHT11 (temp + humi), không có lux
    ESP_LOGI(MAIN_TAG, "TH3: Chỉ có DHT11, không có lux (flags=0x06)");
    const char *json_dht_only = "{\"data\":{\"temp\":24,\"humi\":69}}";
    sensor_data = (Sensor_Data){.flags = SENSOR_FLAG_NONE, .lux = 0, .temp = 0, .humi = 0};
    extract_sensor_data_from_json(json_dht_only, &sensor_data);
    length = create_uart_data_message(&sensor_data, uart_buffer);
    uart.send.bytes(uart_buffer, length);
    ESP_LOGI(MAIN_TAG, "  => Sent: flags=0x%02X, lux=%d, temp=%d, humi=%d\n",
             sensor_data.flags, sensor_data.lux, sensor_data.temp, sensor_data.humi);
    vTaskDelay(pdMS_TO_TICKS(1000));

    // TH4: Chỉ có temp, không có humi và lux
    ESP_LOGI(MAIN_TAG, "TH4: Chỉ có nhiệt độ (flags=0x02)");
    const char *json_temp_only = "{\"data\":{\"temp\":25}}";
    sensor_data = (Sensor_Data){.flags = SENSOR_FLAG_NONE, .lux = 0, .temp = 0, .humi = 0};
    extract_sensor_data_from_json(json_temp_only, &sensor_data);
    length = create_uart_data_message(&sensor_data, uart_buffer);
    uart.send.bytes(uart_buffer, length);
    ESP_LOGI(MAIN_TAG, "  => Sent: flags=0x%02X, lux=%d, temp=%d, humi=%d\n",
             sensor_data.flags, sensor_data.lux, sensor_data.temp, sensor_data.humi);
    vTaskDelay(pdMS_TO_TICKS(1000));

    // TH5: Chỉ có humi, không có temp và lux
    ESP_LOGI(MAIN_TAG, "TH5: Chỉ có độ ẩm (flags=0x04)");
    const char *json_humi_only = "{\"data\":{\"humi\":70}}";
    sensor_data = (Sensor_Data){.flags = SENSOR_FLAG_NONE, .lux = 0, .temp = 0, .humi = 0};
    extract_sensor_data_from_json(json_humi_only, &sensor_data);
    length = create_uart_data_message(&sensor_data, uart_buffer);
    uart.send.bytes(uart_buffer, length);
    ESP_LOGI(MAIN_TAG, "  => Sent: flags=0x%02X, lux=%d, temp=%d, humi=%d\n",
             sensor_data.flags, sensor_data.lux, sensor_data.temp, sensor_data.humi);
    vTaskDelay(pdMS_TO_TICKS(1000));

    // TH6: Có lux và temp, không có humi
    ESP_LOGI(MAIN_TAG, "TH6: Có lux và temp (flags=0x03)");
    const char *json_lux_temp = "{\"data\":{\"lux\":50,\"temp\":26}}";
    sensor_data = (Sensor_Data){.flags = SENSOR_FLAG_NONE, .lux = 0, .temp = 0, .humi = 0};
    extract_sensor_data_from_json(json_lux_temp, &sensor_data);
    length = create_uart_data_message(&sensor_data, uart_buffer);
    uart.send.bytes(uart_buffer, length);
    ESP_LOGI(MAIN_TAG, "  => Sent: flags=0x%02X, lux=%d, temp=%d, humi=%d\n",
             sensor_data.flags, sensor_data.lux, sensor_data.temp, sensor_data.humi);
    vTaskDelay(pdMS_TO_TICKS(1000));

    // TH7: Đầy đủ 3 giá trị
    ESP_LOGI(MAIN_TAG, "TH7: Đầy đủ 3 giá trị (flags=0x07)");
    const char *json_full = "{\"data\":{\"lux\":60,\"temp\":24,\"humi\":69}}";
    sensor_data = (Sensor_Data){.flags = SENSOR_FLAG_NONE, .lux = 0, .temp = 0, .humi = 0};
    extract_sensor_data_from_json(json_full, &sensor_data);
    length = create_uart_data_message(&sensor_data, uart_buffer);
    uart.send.bytes(uart_buffer, length);
    ESP_LOGI(MAIN_TAG, "  => Sent: flags=0x%02X, lux=%d, temp=%d, humi=%d\n",
             sensor_data.flags, sensor_data.lux, sensor_data.temp, sensor_data.humi);
    vTaskDelay(pdMS_TO_TICKS(1000));

    // TH8: Parse từ nhiều JSON (kết hợp lux từ 1 JSON, DHT11 từ JSON khác)
    ESP_LOGI(MAIN_TAG, "TH8: Kết hợp từ 2 JSON messages");
    const char *json1 = "{\"data\":{\"lux\":80}}";
    const char *json2 = "{\"data\":{\"temp\":22,\"humi\":65}}";
    sensor_data = (Sensor_Data){.flags = SENSOR_FLAG_NONE, .lux = 0, .temp = 0, .humi = 0};
    extract_sensor_data_from_json(json1, &sensor_data);
    extract_sensor_data_from_json(json2, &sensor_data);
    length = create_uart_data_message(&sensor_data, uart_buffer);
    uart.send.bytes(uart_buffer, length);
    ESP_LOGI(MAIN_TAG, "  => Sent: flags=0x%02X, lux=%d, temp=%d, humi=%d\n",
             sensor_data.flags, sensor_data.lux, sensor_data.temp, sensor_data.humi);
    vTaskDelay(pdMS_TO_TICKS(2000));

    // Demo điều khiển
    ESP_LOGI(MAIN_TAG, "\n========== DEMO ĐIỀU KHIỂN 3 CÔNG TẮC ==========\n");

    ESP_LOGI(MAIN_TAG, "Control: PLUG_1 = ON");
    length = create_uart_control_message(PLUG_1, STATUS_ON, uart_buffer);
    uart.send.bytes(uart_buffer, length);
    vTaskDelay(pdMS_TO_TICKS(1000));

    ESP_LOGI(MAIN_TAG, "Control: PLUG_2 = OFF");
    length = create_uart_control_message(PLUG_2, STATUS_OFF, uart_buffer);
    uart.send.bytes(uart_buffer, length);
    vTaskDelay(pdMS_TO_TICKS(1000));

    ESP_LOGI(MAIN_TAG, "Control: PLUG_3 = ON");
    length = create_uart_control_message(PLUG_3, STATUS_ON, uart_buffer);
    uart.send.bytes(uart_buffer, length);
    vTaskDelay(pdMS_TO_TICKS(1000));

    ESP_LOGI(MAIN_TAG, "\n========== DEMO HOÀN TẤT ==========\n");
}

/*


AA 55 01 0C 00 00 00 00 00 00 0C 01
AA 55 01 0C 00 01 00 3C 00 00 49 01
AA 55 01 0C 00 06 00 00 18 45 6F 01
AA 55 01 0C 00 02 00 00 19 00 27 01
AA 55 01 0C 00 04 00 00 00 46 56 01
AA 55 01 0C 00 03 00 32 1A 00 5B 01
AA 55 01 0C 00 07 00 3C 18 45 AC 01
AA 55 01 0C 00 07 00 50 16 41 BA 01
AA 55 02 09 00 00 01 0B 01
AA 55 02 09 00 01 00 0B 01
AA 55 02 09 00 02 01 0D 01
AA 55 01 0C 00 00 00 00 00 00 0C 01
AA 55 01 0C 00 01 00 3C 00 00 49 01
AA 55 01 0C 00 06 00 00 18 45 6F 01
AA 55 01 0C 00 02 00 00 19 00 27 01
AA 55 01 0C 00 04 00 00 00 46 56 01
AA 55 01 0C 00 03 00 32 1A 00 5B 01
AA 55 01 0C 00 07 00 3C 18 45 AC 01
AA 55 01 0C 00 07 00 50 16 41 BA 01
AA 55 02 09 00 00 01 0B 01
AA 55 02 09 00 01 00 0B 01
AA 55 02 09 00 02 01 0D 01
AA 55 02 09 00 01 00 0B 01
*/