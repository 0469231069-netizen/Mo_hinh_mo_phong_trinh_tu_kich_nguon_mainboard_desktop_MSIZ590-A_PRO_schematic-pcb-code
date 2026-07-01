/*
 * MÔ HÌNH MÔ PHỎNG MẠCH KÍCH NGUỒN MAINBOARD DESKTOP (MSI Z590)
 * Nền tảng: Arduino + 2 x IC 74HC595N + Module MP3 JQ6500 + LED WS2812B
 * Tính năng: Fade In, Tắt ngược mượt mà, Tích hợp Âm thanh, Hiệu ứng WS2812B
 */

#include <SoftwareSerial.h>
#include <JQ6500_Serial.h>

// BẮT BUỘC: Cho phép FastLED chạy ngầm ngắt để không làm hỏng hiệu ứng PWM của IC 595
#define FASTLED_ALLOW_INTERRUPTS 1
#include <FastLED.h>

// --- KHAI BÁO MODULE MP3 JQ6500 ---
SoftwareSerial mp3Serial(12, A0); 
JQ6500_Serial mp3(mp3Serial);

// --- KHAI BÁO CHÂN IC 74HC595 ---
const int chanChot   = 9;   // Latch Pin
const int chanXung   = 10;  // Clock Pin
const int chanDuLieu = 11;  // Data Pin

// --- KHAI BÁO CHÂN NÚT NHẤN ---
const int nutBatDau     = 2;   // Nút START
const int nutKhoiPhuc   = 3;   // Nút RESET
const int nutNguonChinh = 4;   // Nút POWER SW
const int nutCheDo      = 5;   // Nút MODE

// --- KHAI BÁO DẢI LED WS2812B ---
#define CHAN_WS2812B 6      
#define TONG_SO_LED_WS 27   
CRGB leds[TONG_SO_LED_WS];

// Cấu hình vật lý 8 dải LED
const byte chieuDaiLine[9] = {0, 3, 5, 2, 5, 2, 2, 2, 6};
const byte viTriBatDau[9]  = {0, 0, 3, 8, 10, 15, 17, 19, 21}; 

byte trangThaiWS[9] = {0};      
byte bongHienTaiWS[9] = {0};    
unsigned long thoiGianWSCuoi[9] = {0};

// --- CÁC TRẠNG THÁI CỦA HỆ THỐNG ---
enum TrangThaiHeThong {
  CHO_DOI,             
  CHUOI_KHOI_DONG,     
  CHE_DO_1_TU_DONG,    
  CHE_DO_2_THU_CONG,   
  DANG_TAT_DAN         
};
TrangThaiHeThong trangThaiHienTai = CHO_DOI;

// --- BIẾN ĐIỀU KHIỂN LED CHUNG ---
uint16_t trangThaiLED = 0;       
const int tongSoBuoc = 16;
int buocHienTai = -1;            

// --- BIẾN HIỆU ỨNG PWM ---
bool dangSangDan = false;
int viTriLEDSangDan = -1;      
unsigned long thoiGianBatDauSangDan = 0;
const unsigned long thoiLuongSangDan = 2000;  

bool dangTatDanPWM = false;
int viTriLEDTatDan = -1;
unsigned long thoiGianBatDauTat = 0;
const unsigned long thoiLuongTatDan = 350; 

const unsigned long khoangCachCacBuoc = 200; 
unsigned long thoiGianBuocCuoi = 0;
unsigned long thoiGianTatCuoi = 0;

// --- CẤU HÌNH CHUỖI START ---
const int chuoiKhoiDong[] = {0, 1, 2, 3, 4, 5}; 
const int doDaiChuoiKhoiDong = 6; 
int viTriChuoiKhoiDong = 0;                                                  

// --- BIẾN CHỐNG NHIỄU ---
unsigned long lanNhanBatDauCuoi = 0;
unsigned long lanNhanKhoiPhucCuoi = 0;
unsigned long lanNhanNguonChinhCuoi = 0;

unsigned long thoiGianNhanCheDo = 0;
bool dangNhanCheDo = false;
bool daXuLyCheDo = false;

// Ký hiệu hàm LED để biên dịch
void tatCacLineTu(byte startLine);
void chayHieuUngDayLED();


void setup() {
  // Khởi động module MP3 JQ6500
  mp3Serial.begin(9600);
  mp3.reset();
  mp3.setVolume(25); 
  mp3.setLoopMode(MP3_LOOP_NONE); 
   
  // Khởi tạo WS2812B
  FastLED.addLeds<WS2812B, CHAN_WS2812B, GRB>(leds, TONG_SO_LED_WS);
  FastLED.setBrightness(180);
  FastLED.clear();
  FastLED.show();

  pinMode(chanChot, OUTPUT);
  pinMode(chanXung, OUTPUT);
  pinMode(chanDuLieu, OUTPUT);

  pinMode(nutBatDau, INPUT);
  pinMode(nutKhoiPhuc, INPUT);
  pinMode(nutNguonChinh, INPUT);
  pinMode(nutCheDo, INPUT);

  capNhatPhanCung(0); 
}

void loop() {
  // 1. Quét nút nhấn
  xuLyNutNhan();

  // 2. Chạy logic theo Trạng thái
  switch (trangThaiHienTai) {
    case DANG_TAT_DAN:
      chayTatDanNguoc();
      break;
    case CHUOI_KHOI_DONG:
      chayChuoiKhoiDong();
      break;
    case CHE_DO_1_TU_DONG:
      chayCheDo1TuDong();
      break;
    case CHE_DO_2_THU_CONG:
      break;
    case CHO_DOI:
      break; 
  }

  // 3. Động cơ hiển thị hình ảnh ra LED
  hienThiLED();
  
  // 4. Cập nhật hiệu ứng dải LED WS2812B
  chayHieuUngDayLED();
}

// ==========================================
// CÁC HÀM CON XỬ LÝ LOGIC CHẠY NỀN
// ==========================================

void chayCheDo1TuDong() {
  if (!dangSangDan && buocHienTai < tongSoBuoc - 1) {
    if (millis() - thoiGianBuocCuoi >= khoangCachCacBuoc) {
      buocHienTai++;
      batHieuUngSangDan(buocHienTai);
      phatAmThanh(4); 
      thoiGianBuocCuoi = millis();
    }
  }
}

void chayChuoiKhoiDong() {
  if (!dangSangDan && viTriChuoiKhoiDong < doDaiChuoiKhoiDong - 1) {
    if (millis() - thoiGianBuocCuoi >= khoangCachCacBuoc) {
      viTriChuoiKhoiDong++;
      batHieuUngSangDan(chuoiKhoiDong[viTriChuoiKhoiDong]);
      // phatAmThanh(4); 
      thoiGianBuocCuoi = millis();
    }
  }
}

void chayTatDanNguoc() {
  if (!dangTatDanPWM) { 
    int bitCaoNhat = -1;
    for(int i = tongSoBuoc - 1; i >= 0; i--) {
      if (trangThaiLED & (1UL << i)) {
        bitCaoNhat = i;
        break;
      }
    }
    
    if (bitCaoNhat != -1) {
      batHieuUngTatDan(bitCaoNhat); 
    } else {
      trangThaiHienTai = CHO_DOI;   
    }
  }
}

// ==========================================
// HÀM QUÉT NÚT NHẤN 
// ==========================================

void xuLyNutNhan() {
  // --- Nút RESET ---
  if (digitalRead(nutKhoiPhuc) == HIGH && (millis() - lanNhanKhoiPhucCuoi > 200)) {
    trangThaiHienTai = DANG_TAT_DAN;
    dangSangDan = false;   
    dangTatDanPWM = false; 
    thoiGianTatCuoi = millis();
    lanNhanKhoiPhucCuoi = millis();
    phatAmThanh(5); 
  }

  // --- Nút MODE ---
  bool trangThaiNutCheDo = digitalRead(nutCheDo);
  if (trangThaiNutCheDo == HIGH) {
    if (!dangNhanCheDo) {
      dangNhanCheDo = true;
      thoiGianNhanCheDo = millis();
      daXuLyCheDo = false;
    } else if (millis() - thoiGianNhanCheDo >= 5000 && !daXuLyCheDo) {
      trangThaiHienTai = CHE_DO_2_THU_CONG;
      trangThaiLED = 0; dangSangDan = false; dangTatDanPWM = false; buocHienTai = -1;
      tatCacLineTu(1); // Tắt sạch WS2812B
      daXuLyCheDo = true;
      phatAmThanh(3); 
    }
  } else if (dangNhanCheDo) {
    dangNhanCheDo = false;
    if (!daXuLyCheDo && (millis() - thoiGianNhanCheDo > 50)) { 
      trangThaiHienTai = CHE_DO_1_TU_DONG;
      trangThaiLED = 0; dangSangDan = false; dangTatDanPWM = false; buocHienTai = 0;
      tatCacLineTu(1); // Tắt sạch WS2812B
      batHieuUngSangDan(0); 
      phatAmThanh(2); 
      thoiGianBuocCuoi = millis();
    }
  }

  // --- Nút START ---
  if (digitalRead(nutBatDau) == HIGH && (millis() - lanNhanBatDauCuoi > 200)) {
    if (trangThaiHienTai == CHO_DOI) {
      trangThaiHienTai = CHUOI_KHOI_DONG;
      viTriChuoiKhoiDong = 0;
      batHieuUngSangDan(chuoiKhoiDong[0]);
      phatAmThanh(1);    
      thoiGianBuocCuoi = millis();
    }
    else if (trangThaiHienTai == CHE_DO_2_THU_CONG && !dangSangDan && !dangTatDanPWM && buocHienTai < tongSoBuoc - 1) {
      buocHienTai++;
      batHieuUngSangDan(buocHienTai);
      phatAmThanh(4); 
    }
    lanNhanBatDauCuoi = millis();
  }

  // --- Nút POWER SW ---
  if (digitalRead(nutNguonChinh) == HIGH && (millis() - lanNhanNguonChinhCuoi > 200)) {
    if (trangThaiHienTai == CHE_DO_2_THU_CONG && !dangSangDan && !dangTatDanPWM && buocHienTai >= 0) {
      batHieuUngTatDan(buocHienTai);
      buocHienTai--; 
    }
    lanNhanNguonChinhCuoi = millis();
  }
}

// ==========================================
// HÀM HIỂN THỊ HÌNH ẢNH 
// ==========================================

void hienThiLED() {
  uint16_t duLieuXuat = trangThaiLED;

  if (dangSangDan) {
    unsigned long daTroiQua = millis() - thoiGianBatDauSangDan;
    if (daTroiQua >= thoiLuongSangDan) {
      dangSangDan = false;
      trangThaiLED |= (1UL << viTriLEDSangDan); 
      duLieuXuat = trangThaiLED;
    } else {
      unsigned long tyLe = (daTroiQua * 100UL) / thoiLuongSangDan; 
      unsigned long doSang = (tyLe * tyLe * 5000UL) / 10000UL; 

      if ((micros() % 5000) < doSang) {
        duLieuXuat |= (1UL << viTriLEDSangDan);  
      } else {
        duLieuXuat &= ~(1UL << viTriLEDSangDan); 
      }
    }
  }

  if (dangTatDanPWM) {
    unsigned long daTroiQua = millis() - thoiGianBatDauTat;
    if (daTroiQua >= thoiLuongTatDan) {
      dangTatDanPWM = false;
      trangThaiLED &= ~(1UL << viTriLEDTatDan); 
      duLieuXuat = trangThaiLED;
    } else {
      unsigned long tyLe = (daTroiQua * 100UL) / thoiLuongTatDan;
      unsigned long doSang = ((100 - tyLe) * (100 - tyLe) * 5000UL) / 10000UL;

      if ((micros() % 5000) < doSang) {
        duLieuXuat |= (1UL << viTriLEDTatDan);   
      } else {
        duLieuXuat &= ~(1UL << viTriLEDTatDan);  
      }
    }
  }

  capNhatPhanCung(duLieuXuat);
}

void capNhatPhanCung(uint16_t duLieuXuat) {
  digitalWrite(chanChot, LOW);
  shiftOut(chanDuLieu, chanXung, MSBFIRST, highByte(duLieuXuat));
  shiftOut(chanDuLieu, chanXung, MSBFIRST, lowByte(duLieuXuat));
  digitalWrite(chanChot, HIGH);
}

void phatAmThanh(int thuTuBai) {
  mp3.playFileByIndexNumber(thuTuBai);
}


// ==========================================
// ĐỘNG CƠ XỬ LÝ LED WS2812B & LOGIC KÍCH HOẠT
// ==========================================

void batHieuUngSangDan(int viTriLED) {
  viTriLEDSangDan = viTriLED;
  dangSangDan = true;
  thoiGianBatDauSangDan = millis();

  // --- Logic kích hoạt theo đúng yêu cầu bằng if-else ---
  if (viTriLED == 0) { // Khi có đèn 1 (AC)
    trangThaiWS[1] = 1; bongHienTaiWS[1] = 1; thoiGianWSCuoi[1] = millis();
    tatCacLineTu(2); // Tất cả dải từ thứ 2 trở đi tắt
  }
  else if (viTriLED == 1) { // Khi có đèn 2 (Standby)
    trangThaiWS[2] = 1; bongHienTaiWS[2] = 1; thoiGianWSCuoi[2] = millis();
    tatCacLineTu(3); // Tất cả dải từ thứ 3 trở đi tắt
  }
  else if (viTriLED == 2) { // Khi có đèn 3
    trangThaiWS[3] = 1; bongHienTaiWS[3] = 1; thoiGianWSCuoi[3] = millis();
    tatCacLineTu(4); // Dải từ thứ 4 trở đi tắt
  }
  else if (viTriLED == 5) { // Dải 4 sáng (Đèn thứ 6 - Chuỗi Mode bắt đầu phần Main Power)
    trangThaiWS[4] = 1; bongHienTaiWS[4] = 1; thoiGianWSCuoi[4] = millis();
  }
  else if (viTriLED == 8) { // Đến khi đèn 8 sáng xong (Nhảy sang bước đèn 9)
    trangThaiWS[5] = 1; bongHienTaiWS[5] = 1; thoiGianWSCuoi[5] = millis();
  }
  else if (viTriLED == 10) { // Đến đèn 11 sáng lên
    trangThaiWS[6] = 1; bongHienTaiWS[6] = 1; thoiGianWSCuoi[6] = millis();
  }
  else if (viTriLED == 13) { // Đến đèn 14 sáng lên
    trangThaiWS[7] = 1; bongHienTaiWS[7] = 1; thoiGianWSCuoi[7] = millis();
  }
  else if (viTriLED == 15) { // Led thứ 15 sáng lên sau đó mới chạy dải cuối (Đèn 16)
    trangThaiWS[8] = 1; bongHienTaiWS[8] = 1; thoiGianWSCuoi[8] = millis();
  }
}

void batHieuUngTatDan(int viTriLED) {
  viTriLEDTatDan = viTriLED;
  dangTatDanPWM = true;
  thoiGianBatDauTat = millis();

  // Dập tắt dần các dải WS2812B ngược lại khi hạ nguồn
  if (viTriLED == 15) tatCacLineTu(8);
  else if (viTriLED == 13) tatCacLineTu(7);
  else if (viTriLED == 10) tatCacLineTu(6);
  else if (viTriLED == 8) tatCacLineTu(5);
  else if (viTriLED == 5) tatCacLineTu(4);
  else if (viTriLED == 2) tatCacLineTu(3);
  else if (viTriLED == 1) tatCacLineTu(2);
  else if (viTriLED == 0) tatCacLineTu(1);
}

void tatCacLineTu(byte startLine) {
  for (byte i = startLine; i <= 8; i++) {
    trangThaiWS[i] = 0;
    for (byte j = 0; j < chieuDaiLine[i]; j++) {
      leds[viTriBatDau[i] + j] = CRGB::Black;
    }
  }
  FastLED.show();
}

void chayHieuUngDayLED() {
  bool canCapNhatLED = false;
  
  for (byte i = 1; i <= 8; i++) {
    if (trangThaiWS[i] == 0 || trangThaiWS[i] == 3) continue;
    
    // Tốc độ chạy tia LED: 40ms 1 bóng
    if (millis() - thoiGianWSCuoi[i] >= 40) { 
      thoiGianWSCuoi[i] = millis(); 
      canCapNhatLED = true;
      int vtBong = viTriBatDau[i] + bongHienTaiWS[i] - 1; 
      
      // Chọn màu sắc theo yêu cầu
      CRGB mauChon;
      if (i == 1) mauChon = CRGB::Red;                   // Dải 1: Đỏ
      else if (i == 2) mauChon = CRGB(150, 0, 255);      // Dải 2: Tím
      else if (i == 3) mauChon = CRGB::Green;            // Dải 3: Xanh lá
      else if (i == 4) mauChon = CRGB(100, 150, 255);    // Dải 4: Xanh dương pastel
      else if (i == 5) mauChon = CRGB::Green;            // Dải 5: Xanh lá
      else if (i == 6) mauChon = CRGB::Green;            // Dải 6: Xanh lá
      else if (i == 7) mauChon = CRGB::Green;            // Dải 7: Xanh lá
      else if (i == 8) {                                 // Dải 8: Trộn màu
        if (bongHienTaiWS[i] <= 2) mauChon = CRGB::Red;                   // 2 Đỏ
        else if (bongHienTaiWS[i] <= 4) mauChon = CRGB(255, 120, 0);      // 2 Cam
        else mauChon = CRGB::Yellow;                                      // 2 Vàng
      }

      // Render bóng chạy
      if (trangThaiWS[i] == 1) { 
        if (bongHienTaiWS[i] > 1) {
          leds[vtBong - 1] = CRGB::Black; // Tắt bóng phía sau để tạo hiệu ứng chạy
        }
        leds[vtBong] = mauChon; 
        
        if (bongHienTaiWS[i] >= chieuDaiLine[i]) { 
          trangThaiWS[i] = 2; // Chuyển sang Fill đầy
          bongHienTaiWS[i] = 1; 
          leds[vtBong] = CRGB::Black; 
        } else {
          bongHienTaiWS[i]++;
        }
      }
      else if (trangThaiWS[i] == 2) { 
        leds[vtBong] = mauChon;
        if (bongHienTaiWS[i] >= chieuDaiLine[i]) {
          trangThaiWS[i] = 3; // Kết thúc
        } else {
          bongHienTaiWS[i]++;
        }
      }
    }
  }
  
  if (canCapNhatLED) {
    FastLED.show(); 
  }
}
