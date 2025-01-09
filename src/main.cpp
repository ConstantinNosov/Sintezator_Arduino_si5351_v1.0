#include <Rotary.h>
#include <si5351.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#define FREQUENCY_MIN 3500000L  // 3.5000_Мгц
#define FREQUENCY_MAX 14350000L // 14.3500_Мгц            
#define ENCODER_A    3                      
#define ENCODER_B    2   
#define ENCODER_BTN  4

LiquidCrystal_I2C lcd(0x27,16,2); 
Si5351 si5351; 
Rotary encoder = Rotary(ENCODER_A, ENCODER_B); 

volatile uint32_t variable_frequency_output = 710000000ULL / SI5351_FREQ_MULT;  // Частота ГПД
volatile uint32_t reference_frequency_output = 886723800ULL; // частота опорного генератора, при старте вкл.верхняя боковая
volatile uint32_t LSB = 886723800ULL; // частота ОГ для "нижней" боковой. Настр. на ниж. скат КФ.
volatile uint32_t USB = 886723800ULL; // частота ОГ для "верхней" боковой. Настр. на вверхн. скат КФ.
volatile uint32_t step_frequency = 100000;  // шаг перестройки, по умолчанию, при старте = 100 кГц
boolean is_frequency_changed = 0; // Флаг изменения частоты
String LSB_USB = "LSB"; // Переменная для отображения верхней или нижней боковой

//------------------ Установка дополнительных параметров здесь  ---------------------------//
// Удалить коммент (//) для применения нужного варианта. Задействовать только одно.
#define IF_Offset// Показание на ЖКИ плюс(минус) на значение ПЧ
// #define Direct_conversion // чатота на выходе как на ЖКИ. Прямой выход. Генератор.
// #define FreqX4  // частота на выходе, умноженная на четыре ...
// #define FreqX2  // частота на выходе, умноженная на два ...
//---------------------------------------------------------------------------------------//

//----------------------------- S-метр----------------------------------------------------//
const int signalPin = A0; // вход АЦП
const int maxSignalValue = 1023; // максимальное значение сигнала (для 10-битного АЦП — 1023)
const int smoothingWindowSize = 10; // сглаживание
int smoothingBuffer[smoothingWindowSize]; // Массив для хранения последних измерений
int bufferIndex = 0;

// Создание символов отображения шкалы
byte leftHalf[8] = {
  0b00000,
  0b11000,
  0b11000,
  0b11000,
  0b11000,
  0b11000,
  0b11000,
  0b00000
};
byte rightHalf[8] = {
  0b00000,
  0b00011,
  0b00011,
  0b00011,
  0b00011,
  0b00011,
  0b00011,
  0b00000
};
byte fullBlock[8] = {
  0b00000,
  0b11011,
  0b11011,
  0b11011,
  0b11011,
  0b11011,
  0b11011,
  0b00000
};

//------------------------Функции----------------------------------------------//

void set_frequency(short direction_frequency)
// Функция ввода частоты
{
  if (direction_frequency == 1)
    variable_frequency_output += step_frequency;
  if (direction_frequency== -1)
    variable_frequency_output -= step_frequency;
  if (variable_frequency_output > FREQUENCY_MAX)
    variable_frequency_output = FREQUENCY_MAX;
  if (variable_frequency_output < FREQUENCY_MIN)
    variable_frequency_output = FREQUENCY_MIN;
  is_frequency_changed = 1;
}

ISR(PCINT2_vect) 
// Функция изменения частоты по сигналу энкодера через прерывание
{
  unsigned char encoder_direction = encoder.process();
  if (encoder_direction == DIR_CW)
    set_frequency(1);
  else if (encoder_direction == DIR_CCW)
    set_frequency(-1);
}

boolean get_button()
//Функция чтения кнопки энкодера,возвращает true если нажата
{
  if (!digitalRead(ENCODER_BTN))
  {
    delay(20);
    if (!digitalRead(ENCODER_BTN))
    {
      while (!digitalRead(ENCODER_BTN));
      return 1;
    }
  }
  return 0;
}

void display_frequency()
 //Функция вывода значения частоты на дисплей
{
  uint16_t frequency;
  lcd.setCursor(3, 0);
  frequency = variable_frequency_output / 1000000;
    lcd.print(' ');
  lcd.print(frequency);
  lcd.print('.');
  frequency = (variable_frequency_output % 1000000) / 1000;
  if (frequency < 100)
    lcd.print('0');
  if (frequency < 10)
    lcd.print('0');
  lcd.print(frequency);
  lcd.print('.');
  frequency = variable_frequency_output % 1000;
  if (frequency < 100)
    lcd.print('0');
  if (frequency < 10)
    lcd.print('0');
  lcd.print(frequency);
  lcd.print("Hz ");
  lcd.setCursor(0, 0);
  lcd.print(LSB_USB);

}

void display_step()
//Функция отображения шага частоты
{
  lcd.setCursor(9, 1);
  switch (step_frequency)
  {
    case 1:
      lcd.print("    1");
      break;
    case 10:
      lcd.print("   10");
      break;
    case 100:
      lcd.print("  100");
      break;
    case 1000:
      lcd.print("   1k");
      break;
    case 10000:
      lcd.print("  10k");
      break;
    case 100000:
      //LiquidCrystal_I2C .setCursor(10, 1);
      lcd.print(" 100k");
      break;
    case 1000000:
      //LiquidCrystal_I2C .setCursor(9, 1);
      lcd.print("   1MHz"); //1MHz increments
      break;
  }
  lcd.print("Hz");

}

int getSmoothedSignal()
// Функция сглаживания для s-метра
 {
  int rawValue = analogRead(signalPin);
  smoothingBuffer[bufferIndex] = rawValue;
  bufferIndex = (bufferIndex + 1) % smoothingWindowSize;
  long sum = 0;
  for (int i = 0; i < smoothingWindowSize; i++) {
    sum += smoothingBuffer[i];
  }
  return sum / smoothingWindowSize;

}

void setup()
{
  lcd.init();
  lcd.backlight();
  PCICR |= (1 << PCIE2);
  PCMSK2 |= (1 << PCINT18) | (1 << PCINT19);
  sei();
  Serial.begin(19200);
  lcd.begin(16, 2);   
  lcd.clear();
  Wire.begin();
  int32_t correction = 100000; // Значение коррекции частоты синтезатора
  si5351.set_correction(correction, SI5351_PLL_INPUT_XO);
  si5351.init(SI5351_CRYSTAL_LOAD_8PF, 25000000, 0);  // 8pF для кристалла, 25 МГц частота, 0 коррекция
  si5351.set_pll(SI5351_PLL_FIXED, SI5351_PLLA);

  // Установка выходной частоты в соответствии с доп.настройами (см.выше)
  #ifdef IF_Offset
    si5351.set_freq((variable_frequency_output* SI5351_FREQ_MULT) + reference_frequency_output, SI5351_CLK0);
    si5351.set_freq( reference_frequency_output,SI5351_CLK2);
    
  #endif

  #ifdef Direct_conversion
    si5351.set_freq((variable_frequency_output* SI5351_FREQ_MULT),SI5351_CLK0);
  #endif

  #ifdef FreqX4
    si5351.set_freq((variable_frequency_output * SI5351_FREQ_MULT) * 4,SI5351_CLK0);
  #endif

  #ifdef FreqX2
    si5351.set_freq((variable_frequency_output * SI5351_FREQ_MULT) * 2, SI5351_CLK0);
  #endif

  pinMode(ENCODER_BTN, INPUT_PULLUP);
  display_frequency(); 
  display_step();
  pinMode(12, OUTPUT); // для кнопки УВЧ = A2
  pinMode(13, OUTPUT); //  для кнопки АТТ = A1
  // Порты, пины управл. напр. для дешифр. CD4028
  pinMode(14, OUTPUT); // b0 для A
  pinMode(15, OUTPUT); // b1 для B
  pinMode(16, OUTPUT); // b2 для D
  pinMode(17, OUTPUT); // b3 для C


//-----------------S-метр------------------//
   // Загрузка пользовательских символов
  lcd.createChar(0, leftHalf);
  lcd.createChar(1, rightHalf);
  lcd.createChar(2, fullBlock);
  lcd.setCursor(0, 1);
  lcd.print("S-");
  // Инициализация сглаживающего буфера
  for (int i = 0; i < smoothingWindowSize; i++) {
    smoothingBuffer[i] = 0;
  }

}

void loop()
  // ------------------------------ГЛАВНЫЙ ЦИКЛ------------------------------//
{
  //---------------------S-метр----------------------------------------------//
  int smoothedSignal = getSmoothedSignal();
  int filledHalves = map(smoothedSignal, 0, maxSignalValue, 0, 16); //кол-во элементов 
  lcd.setCursor(2, 1); 
  for (int i = 0; i < 8; i++) {
    if (filledHalves >= 2) {
      lcd.write(byte(2)); // Полностью заполненный символ
      filledHalves -= 2;
    } else if (filledHalves == 1) {
      lcd.write(byte(0)); // Только левая половина
      filledHalves -= 1;
    } else {
      lcd.write(' '); // Пустое пространство
    }
  }

  // --------Установка выходных частот синтезатора и отображение режима LSB/USB----------//
  if (is_frequency_changed)
  {
    display_frequency();
    #ifdef IF_Offset
      si5351.set_freq((variable_frequency_output * SI5351_FREQ_MULT) + reference_frequency_output, SI5351_CLK0);

      if (variable_frequency_output >= 10000000ULL && LSB_USB != "USB")
        {
          reference_frequency_output = USB;
          LSB_USB = "USB";
          si5351.set_freq( reference_frequency_output, SI5351_CLK2);
        }
      else if (variable_frequency_output < 10000000ULL && LSB_USB != "LSB")
        {
          reference_frequency_output = LSB;
          LSB_USB = "LSB";
          si5351.set_freq( reference_frequency_output,SI5351_CLK2);
        }
    #endif

    #ifdef Direct_conversion
        si5351.set_freq((variable_frequency_output * SI5351_FREQ_MULT), SI5351_CLK0);
        LSB_USB = "";
    #endif

    #ifdef FreqX4
        si5351.set_freq((variable_frequency_output * SI5351_FREQ_MULT) * 4, SI5351_CLK0);
        LSB_USB = "";
    #endif

    #ifdef FreqX2
        si5351.set_freq((variable_frequency_output * SI5351_FREQ_MULT) * 2, SI5351_CLK0);
        LSB_USB = "";
    #endif

    is_frequency_changed = 0;
  }

  //-------Автоматическое управление диапазонным фильтром

    // Band 80
  if (variable_frequency_output >= 3000001ULL && variable_frequency_output <= 5000000ULL)
    {
      digitalWrite(5, HIGH); 
      digitalWrite(6, LOW); 
      digitalWrite(7, LOW); 
    }
    // Band 40
    if (variable_frequency_output >= 5000001ULL && variable_frequency_output <= 8000000ULL)
    {
      digitalWrite(5, LOW); 
      digitalWrite(6, HIGH); 
      digitalWrite(7, LOW); 
    }
 
    // Band 20
    if (variable_frequency_output >= 12000001ULL && variable_frequency_output <= 15000000ULL)
    {
      digitalWrite(5, LOW); 
      digitalWrite(6, LOW); 
      digitalWrite(7, HIGH); 
    }
  
    

  //------------------Установка шага перестройки частоты по сигналу кнопки----------------------//
  if (get_button())
    {
      switch (step_frequency)
      {
        case 1:
          step_frequency = 10;
          break;
        case 10:
          step_frequency = 100;
          break;
        case 100:
          step_frequency = 1000;
          break;
        case 1000:
          step_frequency = 10000;
          break;
        case 10000:
          step_frequency = 100000;
          break;
        case 100000:
          step_frequency = 1000000;
          break;
        case 1000000:
          step_frequency = 1;
          break;
      }
    display_step();
    }
}




