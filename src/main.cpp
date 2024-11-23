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
Rotary r = Rotary(ENCODER_A, ENCODER_B); 

// Переменные кнопок PRE/ATT
int regim = 1;
int flag = 0; 

// Для кварцевых резонаторов полосового фильтра со значением 8867 МГц.
//Эти USB/LSB частоты добавляется или вычитается из частоты VFO в "void loop()"
volatile uint32_t LSB = 50000000ULL; //частота ОГ(гетеродина) для "нижней" боковой. Настр. на ниж. скат КФ.
volatile uint32_t USB = 50300000ULL; //частота ОГ(гетеродина) для "верхней" боковой. Настр. на вверхн. скат КФ.
volatile uint32_t bfo = 50000000ULL; // частота второго гетеродина, при старте вкл.верхняя боковая
volatile uint32_t vfo = 710000000ULL / SI5351_FREQ_MULT;  //стартовая частота при запуске синтезатора.
volatile uint32_t step = 100000;  // Шаг перестройки по умолчанию при старте = 100 кГц
boolean changed_f = 0;
String lsb_usb = "";

//------------------ Установка дополнительных функций здесь  ---------------------------
//Удалить коммент (//) для применения нужного варианта. Задействовать только одно.
#define IF_Offset// Показание на ЖКИ плюс(минус) на значение ПЧ
//#define Direct_conversion // чатота на выходе как на ЖКИ. Прямой выход. Генератор.
//#define FreqX4  // частота на выходе, умноженная на четыре ...
//#define FreqX2  // частота на выходе, умноженная на два ...
//---------------------------------------------------------------------------------------

// Функция установки частоты
void set_frequency(short dir)
{
  if (dir == 1)
    vfo += step;
  if (dir == -1)
    vfo -= step;

  if (vfo > F_MAX)
    vfo = F_MAX;
  if (vfo < F_MIN)
    vfo = F_MIN;

  changed_f = 1;
}

// Установка частоты по сигналу энкодера через прерывание
ISR(PCINT2_vect) 
{
  unsigned char result = r.process();
  if (result == DIR_CW)
    set_frequency(1);
  else if (result == DIR_CCW)
    set_frequency(-1);
}

//Читаем кнопку энкодера
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
  frequency = vfo / 1000000;
    lcd.print(' ');
  lcd.print(frequency);
  lcd.print('.');
  frequency = (vfo % 1000000) / 1000;
  if (frequency < 100)
    lcd.print('0');
  if (frequency < 10)
    lcd.print('0');
  lcd.print(frequency);
  lcd.print('.');
  frequency = vfo % 1000;
  if (frequency < 100)
    lcd.print('0');
  if (frequency < 10)
    lcd.print('0');
  lcd.print(frequency);
  lcd.print("Hz ");
  lcd.setCursor(0, 1);
  lcd.print(lsb_usb);
  //Serial.println(vfo + bfo);
  //Serial.println(tbfo);
}

//Функция отображения шага частоты
void display_step()
{
  lcd.setCursor(9, 1);
  switch (step)
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


void setup() {

  lcd.init();
  lcd.backlight();
  PCICR |= (1 << PCIE2);
  PCMSK2 |= (1 << PCINT18) | (1 << PCINT19);
  sei();
  Serial.begin(19200);
  lcd .begin(16, 2);   
  lcd .clear();
  Wire.begin();
  int32_t correction = 10000; // Значение коррекции частоты синтезатора
  si5351.set_correction(correction, SI5351_PLL_INPUT_XO);
  si5351.init(SI5351_CRYSTAL_LOAD_8PF, 25000000, 0);  // 8pF для кристалла, 25 МГц частота, 0 коррекция
  si5351.set_pll(SI5351_PLL_FIXED, SI5351_PLLA);

  // Установка выходной частоты в соответствии с доп.настройами (см.выше)
#ifdef IF_Offset
  si5351.set_freq((vfo * SI5351_FREQ_MULT) + bfo, SI5351_CLK0);
  //volatile uint32_t vfoT = (vfo * SI5351_FREQ_MULT) + bfo;
  lsb_usb = "USB";
  // Set CLK2 to output bfo frequency
  si5351.set_freq( bfo,SI5351_CLK2);
  //si5351.drive_strength(SI5351_CLK0,SI5351_DRIVE_2MA); //you can set this to 2MA, 4MA, 6MA or 8MA
  //si5351.drive_strength(SI5351_CLK1,SI5351_DRIVE_2MA); //be careful though - measure into 50ohms
  //si5351.drive_strength(SI5351_CLK2,SI5351_DRIVE_2MA); //
#endif

#ifdef Direct_conversion
  si5351.set_freq((vfo * SI5351_FREQ_MULT),SI5351_CLK0);
#endif

#ifdef FreqX4
  si5351.set_freq((vfo * SI5351_FREQ_MULT) * 4,SI5351_CLK0);
#endif

#ifdef FreqX2
  si5351.set_freq((vfo * SI5351_FREQ_MULT) * 2, SI5351_CLK0);
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

int keypressed;

  // Обновление дисплея если частота изменена
if (changed_f)
  {
    display_frequency();

#ifdef IF_Offset
    si5351.set_freq((vfo * SI5351_FREQ_MULT) + bfo, SI5351_CLK0);

    if (vfo >= 10000000ULL & lsb_usb != "USB")
    {
      bfo = USB;
      lsb_usb = "USB";
      si5351.set_freq( bfo, SI5351_CLK2);
      Serial.println("We've switched from LSB to USB");
    }
    else if (vfo < 10000000ULL & lsb_usb != "LSB")
    {
      bfo = LSB;
      lsb_usb = "LSB";
      si5351.set_freq( bfo,SI5351_CLK2);
      Serial.println("We've switched from USB to LSB");
    }

#endif

#ifdef Direct_conversion
    si5351.set_freq((vfo * SI5351_FREQ_MULT), SI5351_CLK0);
    lsb_usb = "";
#endif

#ifdef FreqX4
    si5351.set_freq((vfo * SI5351_FREQ_MULT) * 4, SI5351_CLK0);
    lsb_usb = "";
#endif

#ifdef FreqX2
    si5351.set_freq((vfo * SI5351_FREQ_MULT) * 2, SI5351_CLK0);
    lsb_usb = "";
#endif

    changed_f = 0;
  }

  // Кнопки УВЧ и АТТ ---------------------------------
  {
    if (digitalRead(4) == HIGH && flag == 0) 
      {
        regim++;
        flag = 1;
      //это нужно для того что бы с каждым нажатием кнопки
      //происходило только одно действие
      // плюс защита от "дребезга"  100%

      if (regim > 4) //ограничим количество режимов
        {
          regim = 1; //так как мы используем только одну кнопку,
          // то переключать режимы будем циклично
      }

    }
    if (digitalRead(4) == LOW && flag == 1) //если кнопка НЕ нажата
      //и переменная flag равна - 1 ,то ...
    {
      flag = 0; //обнуляем переменную "knopka"
    }

    if (regim == 1) //первый режим - очистка экрана
    {
      digitalWrite(12, LOW); // на пине нулевой уровень
      digitalWrite(13, LOW);
      lcd.setCursor(5, 1);
      lcd.print("   "); //  "пустое место"

      //здесь может быть любое ваше действие
    }
    if (regim == 2) //второй режим  - вкл. УВЧ
    {
      digitalWrite(12, LOW); //включает PRE
      digitalWrite(13, HIGH);
      lcd.setCursor(5, 1); // место на экране для PRE
      lcd.print("PRE");

      //здесь может быть любое ваше действие
    }

    if (regim == 3) //третий режим - очистка экрана
    {
      digitalWrite(12, LOW); //
      digitalWrite(13, LOW);
      lcd.setCursor(5, 1); // место текста на экране
      lcd.print("   ");    //  "пустое место"

      //здесь может быть любое ваше действие
    }
    if (regim == 4) //третий режим - вкл. АТТ
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
    if (vfo >= 1000000ULL && vfo <= 3000000ULL)
    {
      digitalWrite(14, LOW); // на пине нулевой уровень
      digitalWrite(15, LOW); // на пине нулевой уровень
      digitalWrite(16, LOW); // на пине нулевой уровень
      digitalWrite(17, LOW); // на пине нулевой уровень
    }

    // Band 80
    if (vfo >= 3000001ULL && vfo <= 5000000ULL)
    {
      digitalWrite(14, HIGH); // на пине высокий уровень
      digitalWrite(15, LOW); // на пине нулевой уровень
      digitalWrite(16, LOW); // на пине нулевой уровень
      digitalWrite(17, LOW); // на пине нулевой уровень
    }
    // Band 40
    if (vfo >= 5000001ULL && vfo <= 8000000ULL)
    {
      digitalWrite(14, LOW); // на пине нулевой уровень
      digitalWrite(15, HIGH); // на пине высокий уровень
      digitalWrite(16, LOW); // на пине нулевой уровень
      digitalWrite(17, LOW); // на пине нулевой уровень
    }
    // Band 30
    if (vfo >= 8000001ULL && vfo <= 120000000ULL)
    {
      digitalWrite(14, HIGH); // на пине высокий уровень
      digitalWrite(15, HIGH); // на пине высокий уровень
      digitalWrite(16, LOW); // на пине нулевой уровень
      digitalWrite(17, LOW); // на пине нулевой уровень
    }
    // Band 20
    if (vfo >= 12000001ULL && vfo <= 15000000ULL)
    {
      digitalWrite(14, LOW); // на пине нулевой уровень
      digitalWrite(15, LOW); // на пине нулевой уровень
      digitalWrite(16, HIGH); // на пине высокий уровень
      digitalWrite(17, LOW); // на пине нулевой уровень
    }
    // Band 17
    if (vfo >= 15000001ULL && vfo <= 19000000ULL)
    {
      digitalWrite(14, HIGH); // на пине высокий уровень
      digitalWrite(15, LOW); // на пине нулевой уровень
      digitalWrite(16, HIGH); // на пине высокий уровень
      digitalWrite(17, LOW); // на пине нулевой уровень
    }
    // Band 15
    if (vfo >= 19000001ULL && vfo <= 23000000ULL)
    {
      digitalWrite(14, LOW); // на пине нулевой уровень
      digitalWrite(15, HIGH); // на пине высокий уровень
      digitalWrite(16, HIGH); // на пине высокий уровень
      digitalWrite(17, LOW); // на пине нулевой уровень
    }
    // Band 12
    if (vfo >= 23000001ULL && vfo <= 26000000ULL)
    {
      digitalWrite(14, HIGH); // на пине высокий уровень
      digitalWrite(15, HIGH); // на пине высокий уровень
      digitalWrite(16, HIGH); // на пине высокий уровень
      digitalWrite(17, LOW); // на пине нулевой уровень
    }
    // Band 10
    if (vfo >= 26000001ULL && vfo <= 30000000ULL)
    {
      digitalWrite(14, LOW); // на пине нулевой уровень
      digitalWrite(15, LOW); // на пине нулевой уровень
      digitalWrite(16, LOW); // на пине нулевой уровень
      digitalWrite(17, HIGH); // на пине высокий уровень
    }

    // HAM BAND ----- Границы диапазонов ---------
    // 160-метровый (1,81 - 2 МГц)
    if (vfo >= 1810000ULL && vfo <= 2000000ULL)
    {
      lcd.setCursor(0, 1);
      lcd.print("160m");
    }
    else
      // 80-метровый (3,5 - 3,8 МГц)
      if (vfo >= 3500000ULL && vfo <= 3800000ULL)
      {
        lcd.setCursor(0, 1);
        lcd.print("80m ");
      }
      else
        // 40-метровый (7 - 7,2 МГц)
        if (vfo >= 7000000ULL && vfo <= 7200000ULL)
        {
          lcd.setCursor(0, 1);
          lcd.print("40m ");
        }
        else
          // 30-метровый (только телеграф 10,1 - 10,15 МГц)
          if (vfo >= 10100000ULL && vfo <= 10150000ULL)
          {
            lcd.setCursor(0, 1);
            lcd.print("30m ");
          }
          else
            // 20-метровый (14 - 14,35 МГц)
            if (vfo >= 14000000ULL && vfo <= 14350000ULL)
            {
              lcd.setCursor(0, 1);
              lcd.print("20m ");
            }
            else
              // 17-метровый (18,068 - 18,168 МГц)
              if (vfo >= 18068000ULL && vfo <= 18168000ULL)
              {
                lcd.setCursor(0, 1);
                lcd.print("17m ");
              }
              else
                // 15-метровый (21 - 21,45 МГц)
                if (vfo >= 21000000ULL && vfo <= 21450000ULL)
                {
                  lcd.setCursor(0, 1);
                  lcd.print("15m ");
                }
                else
                  // 12-метровый (24,89 - 25,14 МГц)
                  if (vfo >= 24890000ULL && vfo <= 25140000ULL)
                  {
                    lcd.setCursor(0, 1);
                    lcd.print("12m ");
                  }
                  else
                    // 10-метровый (28 - 29,7 МГц)
                    if (vfo >= 28000000ULL && vfo <= 29700000ULL)
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
      switch (step)
      {
        case 1:
          step = 10;
          break;
        case 10:
          step = 100;
          break;
        case 100:
          step = 1000;
          break;
        case 1000:
          step = 10000;
          break;
        case 10000:
          step = 100000;
          break;
        case 100000:
          step = 1000000;
          break;
        case 1000000:
          step = 1;
          break;
      }
      display_step();
    }


  }


}