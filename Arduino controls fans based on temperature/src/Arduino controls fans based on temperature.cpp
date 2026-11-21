

#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include "TimerOne.h"


#define Fan_2           10 
#define Fan_1            9 

#define Button           2 

#define Potentiometer   15 
#define LM35            14 

#define Buzzer          13 
#define LED_Warning     12 
#define LED_Stoped       8 
#define LED_Normal       7 



LiquidCrystal_I2C lcd(0x20, 16, 2); // Khởi tạo LCD địa chỉ 0x20, 16 cột 2 dòng

float LM35_update_measure = 0;
float LM35_update_measure_old = 0;
int   LM35_update_measure_for_PWM = 0;

float Potentiometer_update_measure = 0;
float Potentiometer_update_measure_old = 0;
int   Potentiometer_update_measure_for_PWM = 0;

bool Normal_State = true;

// Khai báo volatile cho biến bị thay đổi bên trong hàm ngắt (để tránh trình biên dịch tối ưu hóa sai)
volatile bool Stoped_State = false;

//  ----------------------------------------------------------------------------------------------------
//  Hàm chức năng: Xử lý ngắt khi nhấn nút (StopFan_isr)
//  Tác dụng: Được gọi tự động ngay lập tức khi người dùng nhấn nút. 
//  Nó sẽ đảo trạng thái cờ Stoped_State để bật/tắt Quạt 1 tạm thời và có cơ chế chống dội phím (debounce).
//  ----------------------------------------------------------------------------------------------------
void StopFan_isr() {
    static unsigned long last_interrupt_time = 0;
    unsigned long interrupt_time = millis();
    
    // Nếu khoảng cách giữa 2 lần tín hiệu > 250ms thì mới nhận (chống nhiễu nút nhấn cơ học)
    if (interrupt_time - last_interrupt_time > 250) {
        Stoped_State = !Stoped_State; // Đảo trạng thái
    }
    last_interrupt_time = interrupt_time;
}
int calculateFanPWM(float temperature) {
    int pwm = map((int)temperature, 45, 75, 338, 1023);
    if (pwm < 338) return 338;  // Giới hạn dưới
    if (pwm > 1023) return 1023; // Giới hạn trên
    return pwm;
}




//  ----------------------------------------------------------------------------------------------------
//  Hàm chức năng: Cập nhật các thông số đo lường (Update_measures)
//  Tác dụng: Đọc giá trị từ cảm biến nhiệt độ LM35 và biến trở, sau đó tính toán và xuất xung PWM ra quạt.
//  ----------------------------------------------------------------------------------------------------
void Update_measures() {
    // 1. Đọc và tính toán nhiệt độ từ LM35
    LM35_update_measure = (analogRead(LM35) * 5.0 / 1023.0) / 0.01;
    
    // Quy đổi nhiệt độ (45C - 75C) ra chu kỳ xung PWM (338 - 1023 tương đương 33% - 100%)
    LM35_update_measure_for_PWM = map((int)LM35_update_measure, 45, 75, 338, 1023);
    if (LM35_update_measure_for_PWM < 338) {
        LM35_update_measure_for_PWM = 338; // Giữ tốc độ tối thiểu
    }
    if (LM35_update_measure_for_PWM > 1023) {
        LM35_update_measure_for_PWM = 1023; // Giới hạn tốc độ tối đa
    }

    // 2. Đọc và tính toán giá trị biến trở
    Potentiometer_update_measure = (analogRead(Potentiometer) * 5.0 / 1023.0) * 20.0; // Phần trăm hiển thị
    Potentiometer_update_measure_for_PWM = analogRead(Potentiometer); // Lấy trực tiếp giá trị 0-1023 cho PWM

    // 3. Điều khiển Quạt 2 (Chỉnh thủ công bằng biến trở)
    Timer1.setPwmDuty(Fan_2, Potentiometer_update_measure_for_PWM);

    // 4. Điều khiển Quạt 1 (Chạy tự động theo nhiệt độ kết hợp kiểm tra nút dừng)
    if (Stoped_State == true) {
        Timer1.setPwmDuty(Fan_1, 0); // Dừng quạt 1
        digitalWrite(LED_Stoped, HIGH); // Bật LED báo dừng
    } else {
        Timer1.setPwmDuty(Fan_1, LM35_update_measure_for_PWM); // Chạy theo nhiệt độ
        digitalWrite(LED_Stoped, LOW); // Tắt LED báo dừng
    }

    // Cập nhật trạng thái hệ thống
    if (LM35_update_measure > 75) {
        Normal_State = false;
    } else {
        Normal_State = true;
    }
}

//  ----------------------------------------------------------------------------------------------------
//  Hàm chức năng: Hiển thị nhiệt độ và phần trăm biến trở lên LCD (ShowTemperature)
//  Tác dụng: In thông số ra màn hình. Chỉ xóa và in lại màn hình khi có sự thay đổi giá trị để tránh chớp giật.
//  ----------------------------------------------------------------------------------------------------
void ShowTemperature() {
    if (abs(LM35_update_measure - LM35_update_measure_old) >= 0.5 || 
        abs(Potentiometer_update_measure - Potentiometer_update_measure_old) >= 1.0) {
        
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Temp: ");
        lcd.print(LM35_update_measure, 1); // In 1 số thập phân
        lcd.print("\xDF"); // Mã ký tự hiển thị dấu độ (°)
        lcd.print("C");

        lcd.setCursor(0, 1);
        lcd.print("Pot: ");
        lcd.print((int)Potentiometer_update_measure);
        lcd.print("%");

        LM35_update_measure_old = LM35_update_measure;
        Potentiometer_update_measure_old = Potentiometer_update_measure;
    }
}

//  ----------------------------------------------------------------------------------------------------
//  Hàm chức năng: Cảnh báo và làm mát khẩn cấp (Warning_sr)
//  Tác dụng: Bật còi báo động, nháy LED và ép cả 2 quạt chạy ở tốc độ tối đa (100%).
//  Ghi chú fix lỗi: Sửa `HIGH` thành `1023` để quạt chạy 100% công suất.
//  ----------------------------------------------------------------------------------------------------
void Warning_sr() {
    digitalWrite(LED_Normal, LOW);
    digitalWrite(LED_Warning, HIGH);
    digitalWrite(Buzzer, LOW); 

    Timer1.setPwmDuty(Fan_2, 1023);
    Timer1.setPwmDuty(Fan_1, 1023);

    // Thêm các dòng này để ép LCD in chữ khẩn cấp
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("!!! EMERGENCY!!!");
    lcd.setCursor(0, 1);
    lcd.print("System Overload!");

    LM35_update_measure = (analogRead(LM35) * 5.0 / 1023.0) / 0.01;
    delay(200); 
}

//  ----------------------------------------------------------------------------------------------------
//  Hàm khởi tạo (Setup): Chạy 1 lần duy nhất khi cấp nguồn
#ifndef PIO_UNIT_TESTING  // Khi chạy unit test (pio test), file test/test_man.cpp có setup()/loop() riêng
void setup() {
    // Cài đặt các chân Output
    pinMode(Buzzer, OUTPUT);
    digitalWrite(Buzzer, HIGH); // Tắt còi ban đầu

    pinMode(LED_Normal, OUTPUT);
    pinMode(LED_Warning, OUTPUT);
    pinMode(LED_Stoped, OUTPUT);

    pinMode(Fan_2, OUTPUT);
    pinMode(Fan_1, OUTPUT);

    // Cài đặt các chân Input
    pinMode(Potentiometer, INPUT);
    pinMode(LM35, INPUT);
    
    // Dùng INPUT_PULLUP để bật điện trở kéo lên bên trong chip, giúp nút nhấn ổn định hơn
    pinMode(Button, INPUT_PULLUP); 

    // Gắn ngắt ngoài (External Interrupt) cho nút nhấn tại chân số 2
    attachInterrupt(digitalPinToInterrupt(Button), StopFan_isr, FALLING);

    // Khởi tạo Timer1 với chu kỳ 150 microseconds để băm xung PWM mượt mà
    Timer1.initialize(150); 
    
    // Khởi tạo màn hình LCD
    lcd.begin(16, 2);

    // Khởi động PWM cho 2 quạt ở mức 50% (512/1023)
    Timer1.pwm(Fan_2, 512);
    Timer1.pwm(Fan_1, 512);

    // Bật đèn LED báo trạng thái bình thường
    digitalWrite(LED_Normal, HIGH); 
}

//  ----------------------------------------------------------------------------------------------------
//  Vòng lặp chính (Loop): Chạy liên tục mãi mãi
void loop() {
    // Gọi hàm đo lường và hiển thị ở chế độ bình thường
    Update_measures();
    ShowTemperature();

    // Nếu nhiệt độ vượt quá 75 độ C -> vào chế độ khẩn cấp và GIỮ ở đó (vùng trễ 5 độ C)
    // cho tới khi nhiệt độ giảm xuống mức an toàn 70 độ C.
    // (Bản cũ dùng while (LM35_update_measure > 75) nên thoát ra ngay ở 75 độ C
    //  mà chưa tắt đèn cảnh báo và còi -> báo động không bao giờ tự tắt.)
    if (LM35_update_measure > 75) {
        while (true) {
            Warning_sr(); // Gọi hàm cảnh báo (đọc lại nhiệt độ bên trong)

            // Thoát vòng lặp an toàn khi nhiệt độ giảm xuống (Mức an toàn: 70 độ C)
            if (LM35_update_measure <= 70) {
                Normal_State = true;
                digitalWrite(LED_Normal, HIGH); // Bật lại đèn bình thường
                digitalWrite(LED_Warning, LOW); // Tắt đèn cảnh báo
                digitalWrite(Buzzer, HIGH);     // Tắt còi
                break; // Phá vỡ vòng lặp while để trở về hoạt động bình thường
            }
        }
    }
    
    delay(100); // Trễ nhẹ để vòng lặp loop() không vắt kiệt công suất vi điều khiển
}

#endif // PIO_UNIT_TESTING
