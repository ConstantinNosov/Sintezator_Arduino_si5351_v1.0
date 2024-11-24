#include <Rotary.h>
#include <si5351.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#define F_MIN        3500000L 
#define F_MAX        14350000L              
#define OLED_RESET   4
#define ENCODER_A    3                      
#define ENCODER_B    2   
#define ENCODER_BTN  11          

LiquidCrystal_I2C lcd(0x27,16,2); 
Si5351 si5351; 
Rotary encoder = Rotary(ENCODER_A, ENCODER_B); 

// Переменные кнопок PRE/ATT
int mode = 1;
int flag = 0; 

// Для кварцевых резонаторов полосового фильтра со значением 8867 МГц.
//Эти USB/LSB частоты добавляется или вычитается из частоты VARIABLE_FREQUENCY_OUTPUT в "void loop()"
volatile uint32_t LSB = 50000000ULL; // частота ОГ(гетеродина) для "нижней" боковой. Настр. на ниж. скат КФ.
volatile uint32_t USB = 50300000ULL; // частота ОГ(гетеродина) для "верхней" боковой. Настр. на вверхн. скат КФ.
volatile uint32_t reference_frequency_output = 50000000ULL; // частота опорного гетеродина, при старте вкл.верхняя боковая
volatile uint32_t variable_frequency_output = 710000000ULL / SI5351_FREQ_MULT;  // Частота ГПД
volatile uint32_t step_frequency = 100000;  // шаг перестройки по умолчанию при старте = 100 кГц
boolean changed_f = 0; // Флаг для обновления дисплея при изменении частоты
String LSB_USB = "";   // Переменная для отображения верхней или нижней боковой

//------------------ Установка дополнительных функций здесь  ---------------------------
// Удалить коммент (//) для применения нужного варианта. Задействовать только одно.
#define IF_Offset// Показание на ЖКИ плюс(минус) на значение ПЧ
// #define Direct_conversion // чатота на выходе как на ЖКИ. Прямой выход. Генератор.
// #define FreqX4  // частота на выходе, умноженная на четыре ...
// #define FreqX2  // частота на выходе, умноженная на два ...
//---------------------------------------------------------------------------------------

// Функция установки частоты
void set_frequency(short dir)
{
  if (dir == 1)
    variable_frequency_output += step_frequency;
  if (dir == -1)
    variable_frequency_output -= step_frequency;
  if (variable_frequency_output > F_MAX)
    variable_frequency_output = F_MAX;
  if (variable_frequency_output < F_MIN)
    variable_frequency_output = F_MIN;
  changed_f = 1;
}

// Установка частоты по сигналу энкодера через прерывание
ISR(PCINT2_vect) 
{
  unsigned char result = encoder.process();
  if (result == DIR_CW)
    set_frequency(1);
  else if (result == DIR_CCW)
    set_frequency(-1);
}

//Функция чтения кнопки энкодера,возвращает true если нажата
boolean get_button()
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

 //Функция вывода значения частоты на дисплей
void display_frequency()
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

//Функция отображения шага частоты
void display_step()
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

//Статические становки перед запуском основного цикла
void setup() {
  lcd.init();
  lcd.backlight();
  PCICR |= (1 << PCIE2);
  PCMSK2 |= (1 << PCINT18) | (1 << PCINT19);
  sei();
  Serial.begin(19200);
  lcd.begin(16, 2);   
  lcd.clear();
  Wire.begin();
  int32_t correction = 10000; // Значение коррекции частоты синтезатора
  si5351.set_correction(correction, SI5351_PLL_INPUT_XO);
  si5351.init(SI5351_CRYSTAL_LOAD_8PF, 25000000, 0);  // 8pF для кристалла, 25 МГц частота, 0 коррекция
  si5351.set_pll(SI5351_PLL_FIXED, SI5351_PLLA);

  // Установка выходной частоты в соответствии с доп.настройами (см.выше)
#ifdef IF_Offset
  si5351.set_freq((variable_frequency_output* SI5351_FREQ_MULT) + reference_frequency_output, SI5351_CLK0);
  //volatile uint32_t vfoT = (variable_frequency_output * SI5351_FREQ_MULT) + reference_frequency_output;
  LSB_USB = "USB";
  // Set CLK2 to output reference_frequency_output
  si5351.set_freq( reference_frequency_output
,SI5351_CLK2);
  //si5351.drive_strength(SI5351_CLK0,SI5351_DRIVE_2MA); //you can set this to 2MA, 4MA, 6MA or 8MA
  //si5351.drive_strength(SI5351_CLK1,SI5351_DRIVE_2MA); //be careful though - measure into 50ohms
  //si5351.drive_strength(SI5351_CLK2,SI5351_DRIVE_2MA); //
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
  PCICR |= (1 << PCIE2);           // Enable pin change interrupt for the encoder
  PCMSK2 |= (1 << PCINT18) | (1 << PCINT19);
  sei();
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
//ГЛАВНЫЙ ЦИКЛ
void loop(){

  // Обновление дисплея если частота изменена
if (changed_f)
  {
    display_frequency();

#ifdef IF_Offset
    si5351.set_freq((variable_frequency_output * SI5351_FREQ_MULT) + reference_frequency_output, SI5351_CLK0);

    if (variable_frequency_output >= 10000000ULL && LSB_USB != "USB")
    {
      reference_frequency_output = USB;
      LSB_USB = "USB";
      si5351.set_freq( reference_frequency_output, SI5351_CLK2);
      Serial.println("We've switched from LSB to USB");
    }
    else if (variable_frequency_output < 10000000ULL && LSB_USB != "LSB")
    {
      reference_frequency_output = LSB;
      LSB_USB = "LSB";
      si5351.set_freq( reference_frequency_output
,SI5351_CLK2);
      Serial.println("We've switched from USB to LSB");
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

    changed_f = 0;
  }

  // Кнопки УВЧ и АТТ ---------------------------------
  {
    if (digitalRead(4) == HIGH && flag == 0) 
      {
        mode++;
        flag = 1;
      //это нужно для того что бы с каждым нажатием кнопки
      //происходило только одно действие
      // плюс защита от "дребезга"  100%

      if (mode > 4) //ограничим количество режимов
        {
          mode = 1; //так как мы используем только одну кнопку,
          // то переключать режимы будем циклично
      }

    }
    if (digitalRead(4) == LOW && flag == 1) //если кнопка НЕ нажата
      //и переменная flag равна - 1 ,то ...
    {
      flag = 0; //обнуляем переменную "knopka"
    }

    if (mode == 1) //первый режим - очистка экрана
    {
      digitalWrite(12, LOW); // на пине нулевой уровень
      digitalWrite(13, LOW);
      lcd.setCursor(5, 1);
      lcd.print("   "); //  "пустое место"

      //здесь может быть любое ваше действие
    }
    if (mode == 2) //второй режим  - вкл. УВЧ
    {
      digitalWrite(12, LOW); //включает PRE
      digitalWrite(13, HIGH);
      lcd.setCursor(5, 1); // место на экране для PRE
      lcd.print("PRE");

      //здесь может быть любое ваше действие
    }

    if (mode == 3) //третий режим - очистка экрана
    {
      digitalWrite(12, LOW); //
      digitalWrite(13, LOW);
      lcd.setCursor(5, 1); // место текста на экране
      lcd.print("   ");    //  "пустое место"

      //здесь может быть любое ваше действие
    }
    if (mode == 4) //третий режим - вкл. АТТ
    {
      digitalWrite(12, HIGH); //включает АТТ
      digitalWrite(13, LOW);
      lcd.setCursor(5, 1); // место на экране для АТТ
      lcd.print("ATT");
    }
    //  --------------------------------
    //  для кнопки РТТ -----------------
    //  ---------------------------------
    // Для управления CD4028
    //---A0-A1-A2-A3 ---pin, porn Arduino Pro Mini
    //---b0-b1-b2-b3----band
    //---00-00-00-00----160m
    //---11-00-00-00-----80m
    //---00-11-00-00-----40m
    //---11-11-00-00-----30m
    //---00-00-11-00-----20m
    //---11-00-11-00-----17m
    //---00-11-11-00-----15m
    //---11-11-11-00-----12m
    //---00-00-00-11-----10m

    // Band 160
    if (variable_frequency_output >= 1000000ULL && variable_frequency_output <= 3000000ULL)
    {
      digitalWrite(14, LOW); // на пине нулевой уровень
      digitalWrite(15, LOW); // на пине нулевой уровень
      digitalWrite(16, LOW); // на пине нулевой уровень
      digitalWrite(17, LOW); // на пине нулевой уровень
    }

    // Band 80
    if (variable_frequency_output >= 3000001ULL && variable_frequency_output <= 5000000ULL)
    {
      digitalWrite(14, HIGH); // на пине высокий уровень
      digitalWrite(15, LOW); // на пине нулевой уровень
      digitalWrite(16, LOW); // на пине нулевой уровень
      digitalWrite(17, LOW); // на пине нулевой уровень
    }
    // Band 40
    if (variable_frequency_output >= 5000001ULL && variable_frequency_output <= 8000000ULL)
    {
      digitalWrite(14, LOW); // на пине нулевой уровень
      digitalWrite(15, HIGH); // на пине высокий уровень
      digitalWrite(16, LOW); // на пине нулевой уровень
      digitalWrite(17, LOW); // на пине нулевой уровень
    }
    // Band 30
    if (variable_frequency_output >= 8000001ULL && variable_frequency_output <= 120000000ULL)
    {
      digitalWrite(14, HIGH); // на пине высокий уровень
      digitalWrite(15, HIGH); // на пине высокий уровень
      digitalWrite(16, LOW); // на пине нулевой уровень
      digitalWrite(17, LOW); // на пине нулевой уровень
    }
    // Band 20
    if (variable_frequency_output >= 12000001ULL && variable_frequency_output <= 15000000ULL)
    {
      digitalWrite(14, LOW); // на пине нулевой уровень
      digitalWrite(15, LOW); // на пине нулевой уровень
      digitalWrite(16, HIGH); // на пине высокий уровень
      digitalWrite(17, LOW); // на пине нулевой уровень
    }
    // Band 17
    if (variable_frequency_output >= 15000001ULL && variable_frequency_output <= 19000000ULL)
    {
      digitalWrite(14, HIGH); // на пине высокий уровень
      digitalWrite(15, LOW); // на пине нулевой уровень
      digitalWrite(16, HIGH); // на пине высокий уровень
      digitalWrite(17, LOW); // на пине нулевой уровень
    }
    // Band 15
    if (variable_frequency_output >= 19000001ULL && variable_frequency_output <= 23000000ULL)
    {
      digitalWrite(14, LOW); // на пине нулевой уровень
      digitalWrite(15, HIGH); // на пине высокий уровень
      digitalWrite(16, HIGH); // на пине высокий уровень
      digitalWrite(17, LOW); // на пине нулевой уровень
    }
    // Band 12
    if (variable_frequency_output >= 23000001ULL && variable_frequency_output <= 26000000ULL)
    {
      digitalWrite(14, HIGH); // на пине высокий уровень
      digitalWrite(15, HIGH); // на пине высокий уровень
      digitalWrite(16, HIGH); // на пине высокий уровень
      digitalWrite(17, LOW); // на пине нулевой уровень
    }
    // Band 10
    if (variable_frequency_output >= 26000001ULL && variable_frequency_output <= 30000000ULL)
    {
      digitalWrite(14, LOW); // на пине нулевой уровень
      digitalWrite(15, LOW); // на пине нулевой уровень
      digitalWrite(16, LOW); // на пине нулевой уровень
      digitalWrite(17, HIGH); // на пине высокий уровень
    }

    // HAM BAND ----- Границы диапазонов ---------
    // 160-метровый (1,81 - 2 МГц)
    if (variable_frequency_output >= 1810000ULL && variable_frequency_output <= 2000000ULL)
    {
      lcd.setCursor(0, 1);
      lcd.print("160m");
    }
    else
      // 80-метровый (3,5 - 3,8 МГц)
      if (variable_frequency_output >= 3500000ULL && variable_frequency_output <= 3800000ULL)
      {
        lcd.setCursor(0, 1);
        lcd.print("80m ");
      }
      else
        // 40-метровый (7 - 7,2 МГц)
        if (variable_frequency_output >= 7000000ULL && variable_frequency_output <= 7200000ULL)
        {
          lcd.setCursor(0, 1);
          lcd.print("40m ");
        }
        else
          // 30-метровый (только телеграф 10,1 - 10,15 МГц)
          if (variable_frequency_output >= 10100000ULL && variable_frequency_output <= 10150000ULL)
          {
            lcd.setCursor(0, 1);
            lcd.print("30m ");
          }
          else
            // 20-метровый (14 - 14,35 МГц)
            if (variable_frequency_output >= 14000000ULL && variable_frequency_output <= 14350000ULL)
            {
              lcd.setCursor(0, 1);
              lcd.print("20m ");
            }
            else
              // 17-метровый (18,068 - 18,168 МГц)
              if (variable_frequency_output >= 18068000ULL && variable_frequency_output <= 18168000ULL)
              {
                lcd.setCursor(0, 1);
                lcd.print("17m ");
              }
              else
                // 15-метровый (21 - 21,45 МГц)
                if (variable_frequency_output >= 21000000ULL && variable_frequency_output <= 21450000ULL)
                {
                  lcd.setCursor(0, 1);
                  lcd.print("15m ");
                }
                else
                  // 12-метровый (24,89 - 25,14 МГц)
                  if (variable_frequency_output >= 24890000ULL && variable_frequency_output <= 25140000ULL)
                  {
                    lcd.setCursor(0, 1);
                    lcd.print("12m ");
                  }
                  else
                    // 10-метровый (28 - 29,7 МГц)
                    if (variable_frequency_output >= 28000000ULL && variable_frequency_output <= 29700000ULL)
                    {
                      lcd.setCursor(0, 1);
                      lcd.print("10m ");
                    }
                    else
                      // Если за границей любительских - очистка экрана
                    {
                      lcd.setCursor(0, 1);
                      lcd.print("    ");
                    }

    // Нажатие кнопки изменяет шаг изменения частоты
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


}