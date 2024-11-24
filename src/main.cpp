#include <Rotary.h>
#include <si5351.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#define FREQUENCY_MIN 3500000L  // 3.5000_Мгц
#define FREQUENCY_MAX 14350000L // 14.3500_Мгц            
#define ENCODER_A    3                      
#define ENCODER_B    2   
#define ENCODER_BTN  11          

LiquidCrystal_I2C lcd(0x27,16,2); 
Si5351 si5351; 
Rotary encoder = Rotary(ENCODER_A, ENCODER_B); 

volatile uint32_t variable_frequency_output = 710000000ULL / SI5351_FREQ_MULT;  // Частота ГПД
volatile uint32_t reference_frequency_output = 50000000ULL; // частота опорного генератора, при старте вкл.верхняя боковая
volatile uint32_t LSB = 50000000ULL; // частота ОГ для "нижней" боковой. Настр. на ниж. скат КФ.
volatile uint32_t USB = 50300000ULL; // частота ОГ для "верхней" боковой. Настр. на вверхн. скат КФ.
volatile uint32_t step_frequency = 100000;  // шаг перестройки, по умолчанию, при старте = 100 кГц
boolean is_frequency_changed = 0; // Флаг изменения частоты
String LSB_USB = "";   // Переменная для отображения верхней или нижней боковой

//------------------ Установка дополнительных функций здесь  ---------------------------
// Удалить коммент (//) для применения нужного варианта. Задействовать только одно.
#define IF_Offset// Показание на ЖКИ плюс(минус) на значение ПЧ
// #define Direct_conversion // чатота на выходе как на ЖКИ. Прямой выход. Генератор.
// #define FreqX4  // частота на выходе, умноженная на четыре ...
// #define FreqX2  // частота на выходе, умноженная на два ...
//---------------------------------------------------------------------------------------


void set_frequency(short direction_frequency)
// Функция установки частоты
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
// Функция установки частоты по сигналу энкодера через прерывание
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
  lcd.setCursor(0, 1);
  lcd.print(LSB_USB);
  //Serial.println(variable_frequency_output + reference_frequency_output);
  //Serial.println(tbfo);
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

void setup()
// ------------------Статические установки перед запуском основного цикла------------------------------
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

}

void loop()
  // ------------------------------ГЛАВНЫЙ ЦИКЛ------------------------------
{

  // --------Установка выходных частот синтезатора и отображение режима LSB/USB----------

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

    //-------Автоматическое управление диапазонным фильтром с помощью дешифратора CD4028-----------
    // Band 160
  if (variable_frequency_output >= 1000000ULL && variable_frequency_output <= 3000000ULL)
    {
      digitalWrite(14, LOW); 
      digitalWrite(15, LOW); 
      digitalWrite(16, LOW); 
      digitalWrite(17, LOW);  
    }
    // Band 80
  if (variable_frequency_output >= 3000001ULL && variable_frequency_output <= 5000000ULL)
    {
      digitalWrite(14, HIGH); 
      digitalWrite(15, LOW); 
      digitalWrite(16, LOW); 
      digitalWrite(17, LOW); 
    }
    // Band 40
    if (variable_frequency_output >= 5000001ULL && variable_frequency_output <= 8000000ULL)
    {
      digitalWrite(14, LOW); 
      digitalWrite(15, HIGH); 
      digitalWrite(16, LOW); 
      digitalWrite(17, LOW); 
    }
    // Band 30
    if (variable_frequency_output >= 8000001ULL && variable_frequency_output <= 120000000ULL)
    {
      digitalWrite(14, HIGH); 
      digitalWrite(15, HIGH); 
      digitalWrite(16, LOW); 
      digitalWrite(17, LOW); 
    }
    // Band 20
    if (variable_frequency_output >= 12000001ULL && variable_frequency_output <= 15000000ULL)
    {
      digitalWrite(14, LOW); 
      digitalWrite(15, LOW); 
      digitalWrite(16, HIGH); 
      digitalWrite(17, LOW); 
    }
    // Band 17
    if (variable_frequency_output >= 15000001ULL && variable_frequency_output <= 19000000ULL)
    {
      digitalWrite(14, HIGH); 
      digitalWrite(15, LOW); 
      digitalWrite(16, HIGH); 
      digitalWrite(17, LOW); 
    }
    // Band 15
    if (variable_frequency_output >= 19000001ULL && variable_frequency_output <= 23000000ULL)
    {
      digitalWrite(14, LOW); 
      digitalWrite(15, HIGH); 
      digitalWrite(16, HIGH); 
      digitalWrite(17, LOW); 
    }
    // Band 12
    if (variable_frequency_output >= 23000001ULL && variable_frequency_output <= 26000000ULL)
    {
      digitalWrite(14, HIGH); 
      digitalWrite(15, HIGH); 
      digitalWrite(16, HIGH); 
      digitalWrite(17, LOW); 
    }
    // Band 10
    if (variable_frequency_output >= 26000001ULL && variable_frequency_output <= 30000000ULL)
    {
      digitalWrite(14, LOW); 
      digitalWrite(15, LOW); 
      digitalWrite(16, LOW); 
      digitalWrite(17, HIGH); 
    }

  // -------------------Автоматическое отображение диапазона на дисплее-----------------------------
  lcd.setCursor(0, 1);
  if (variable_frequency_output >= 1810000ULL && variable_frequency_output <= 2000000ULL) {
    lcd.print("160m");
  } else if (variable_frequency_output >= 3500000ULL && variable_frequency_output <= 3800000ULL) {
    lcd.print("80m ");
  } else if (variable_frequency_output >= 7000000ULL && variable_frequency_output <= 7200000ULL) {
    lcd.print("40m ");
  } else if (variable_frequency_output >= 10100000ULL && variable_frequency_output <= 10150000ULL) {
    lcd.print("30m ");
  } else if (variable_frequency_output >= 14000000ULL && variable_frequency_output <= 14350000ULL) {
    lcd.print("20m ");
  } else if (variable_frequency_output >= 18068000ULL && variable_frequency_output <= 18168000ULL) {
    lcd.print("17m ");
  } else if (variable_frequency_output >= 21000000ULL && variable_frequency_output <= 21450000ULL) {
    lcd.print("15m ");
  } else if (variable_frequency_output >= 24890000ULL && variable_frequency_output <= 25140000ULL) {
    lcd.print("12m ");
  } else if (variable_frequency_output >= 28000000ULL && variable_frequency_output <= 29700000ULL) {
    lcd.print("10m ");
  } else {
    lcd.print("    ");
  }

    // ------------------Установка шага перестройки частоты по сигналу кнопки-----------------
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


