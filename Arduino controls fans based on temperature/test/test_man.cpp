#include <Arduino.h>
#include <unity.h>

// Khai báo lại hàm cần test để file này nhận diện được
int calculateFanPWM(float temperature);

// Unity yêu cầu 2 hàm này: chạy trước/sau mỗi test (ở đây không cần làm gì)
void setUp(void) {}
void tearDown(void) {}

// Kịch bản 1: Nhiệt độ thấp (dưới 45 độ) -> Quạt phải chạy ở mức tối thiểu (338)
void test_temperature_below_minimum(void) {
    TEST_ASSERT_EQUAL(338, calculateFanPWM(30.0));
}

// Kịch bản 2: Nhiệt độ cao (trên 75 độ) -> Quạt phải chạy ở mức tối đa (1023)
void test_temperature_above_maximum(void) {
    TEST_ASSERT_EQUAL(1023, calculateFanPWM(85.5));
}

// Kịch bản 3: Nhiệt độ nằm ở giữa (ví dụ 60 độ) -> Quạt phải chạy ở mức trung bình
void test_temperature_in_middle(void) {
    // Theo hàm map(): 60 độ nằm giữa 45 và 75, thì PWM sẽ là 680
    TEST_ASSERT_EQUAL(680, calculateFanPWM(60.0)); 
}

void setup() {
    delay(2000); // Chờ 2 giây để mạch ổn định
    UNITY_BEGIN(); // Bắt đầu quá trình test

    // Chạy các kịch bản test
    RUN_TEST(test_temperature_below_minimum);
    RUN_TEST(test_temperature_above_maximum);
    RUN_TEST(test_temperature_in_middle);

    UNITY_END(); // Kết thúc test
}

void loop() {
    // Không cần làm gì trong loop khi đang chạy test
}