/*
  Вся эта программа изначально написана NT7S и SQ9NJE.
  Оригинальный код, немного подправленный мною, под мои нужды.
  Ознакомится с оригиналом можно здесь
  http://nt7s.com/
  http://sq9nje.pl/
  http://ak2b.blogspot.com/
*/

#include <Rotary.h>
#include <si5351.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

//Константы
#define F_MIN        3500000L               // Нижний предел частоты
#define F_MAX        14350000L              // Верхний предел частоты
#define OLED_RESET   4
#define ENCODER_A    3                      // Encoder pin A
#define ENCODER_B    2                      // Encoder pin B
#define ENCODER_BTN  11


// Переменные нужные для кнопок PRE/ATT
int regim = 1; // для кнопки
int flag = 0; // для кнопки

LiquidCrystal_I2C lcd(0x27,16,2); // set the LiquidCrystal_I2C  address to 0x27 for a 16 chars and 2 line display  

Si5351 si5351;
Rotary r = Rotary(ENCODER_A, ENCODER_B);

// Для кварцевых резонаторов со значением 8867 МГц.
volatile uint32_t LSB = 50000000ULL; //частота ОГ(гетеродина) для "нижней" боковой. Настр. на ниж. скат КФ.
volatile uint32_t USB = 50300000ULL; //частота ОГ(гетеродина) для "верхней" боковой. Настр. на вверхн. скат КФ.
volatile uint32_t bfo = 50000000ULL; //стартовать с "верхней" ...
//Эти USB/LSB частоты добавляется или вычитается из частоты VFO в "void loop()"
//В этом примере если начальная частота будет 14.20000 плюс 9.001500 то на выходе clk0 = 23.2015Mhz
volatile uint32_t vfo = 710000000ULL / SI5351_FREQ_MULT; //стартовая частота при запуске синтезатора.
volatile uint32_t radix = 100000;  // Шаг перестройки по умолчанию при старте = 100 кГц
boolean changed_f = 0;
String tbfo = "";


//------------------ Установка дополнительных функций здесь  ---------------------------
//Удалить коммент (//) для применения нужного варианта. Задействовать только одно.
#define IF_Offset // Показание на ЖКИ плюс(минус) на значение ПЧ
//#define Direct_conversion // чатота на выходе как на ЖКИ. Прямой выход. Генератор.
//#define FreqX4  // частота на выходе, умноженная на четыре ...
//#define FreqX2  // частота на выходе, умноженная на два ...
//---------------------------------------------------------------------------------------

/**************************************/
/* Interrupt service routine for      */
/* encoder frequency change           */
/**************************************/
ISR(PCINT2_vect) {
  unsigned char result = r.process();
  if (result == DIR_CW)
    set_frequency(1);
  else if (result == DIR_CCW)
    set_frequency(-1);
}

/**************************************/
/* Change the frequency               */
/* dir = 1    Increment               */
/* dir = -1   Decrement               */
/**************************************/
void set_frequency(short dir)
{
  if (dir == 1)
    vfo += radix;
  if (dir == -1)
    vfo -= radix;

  if (vfo > F_MAX)
    vfo = F_MAX;
  if (vfo < F_MIN)
    vfo = F_MIN;

  changed_f = 1;
}

/**************************************/
/* Read the button with debouncing    */
/**************************************/
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

/**************************************/
/* Displays the frequency             */
/**************************************/
void display_frequency()
{
  uint16_t f, g;

  lcd.setCursor(3, 0);
  f = vfo / 1000000; 	//variable is now vfo instead of 'frequency'
  if (f < 10)
    lcd.print(' ');
  lcd.print(f);
  lcd.print('.');
  f = (vfo % 1000000) / 1000;
  if (f < 100)
    lcd.print('0');
  if (f < 10)
    lcd.print('0');
  lcd.print(f);
  lcd.print('.');
  f = vfo % 1000;
  if (f < 100)
    lcd.print('0');
  if (f < 10)
    lcd.print('0');
  lcd.print(f);
  lcd.print("Hz ");
  lcd.setCursor(0, 1);
  lcd.print(tbfo);
  //Serial.println(vfo + bfo);
  //Serial.println(tbfo);
}

/**************************************/
/* Отображаем шаг изменения частоты */
/**************************************/
void display_radix()
{
  lcd.setCursor(9, 1);
  switch (radix)
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


  // начальное сообщение
  //   LiquidCrystal_I2C .print("SINTIZER Si5351"); // текст на экране - "  HELLO !!! "
  //   delay(90000); // Время на прочтение начального сообщения 3 сек

  Serial.begin(19200);
  lcd .begin(16, 2);          // Initialize and clear the LiquidCrystal_I2C 
  lcd .clear();
  Wire.begin();

  si5351.set_correction(71000000); //**mine. There is a calibration sketch in File/Examples/si5351Arduino
  //where you can determine the correction by using the serial monitor.
  //ввести калибровочное знечение в строчке si5351.set_correction(хххххххх).
  //Нужно вычислить запустив "si5351calibration" в папке с примерами библиотеки si5351.

  //initialize the Si5351
  si5351.init(SI5351_CRYSTAL_LOAD_8PF, 0); //If you're using a 27Mhz crystal, put in 25000000 instead of 0
  // 0 is the default crystal frequency of 25Mhz.

  si5351.set_pll(SI5351_PLL_FIXED, SI5351_PLLA);
  // Set CLK0 to output the starting "vfo" frequency as set above by vfo = ?

#ifdef IF_Offset
  si5351.set_freq((vfo * SI5351_FREQ_MULT) + bfo, SI5351_PLL_FIXED, SI5351_CLK0);
  volatile uint32_t vfoT = (vfo * SI5351_FREQ_MULT) + bfo;
  tbfo = "USB";
  // Set CLK2 to output bfo frequency
  si5351.set_freq( bfo, 0, SI5351_CLK2);
  //si5351.drive_strength(SI5351_CLK0,SI5351_DRIVE_2MA); //you can set this to 2MA, 4MA, 6MA or 8MA
  //si5351.drive_strength(SI5351_CLK1,SI5351_DRIVE_2MA); //be careful though - measure into 50ohms
  //si5351.drive_strength(SI5351_CLK2,SI5351_DRIVE_2MA); //
#endif

#ifdef Direct_conversion
  si5351.set_freq((vfo * SI5351_FREQ_MULT), SI5351_PLL_FIXED, SI5351_CLK0);
#endif

#ifdef FreqX4
  si5351.set_freq((vfo * SI5351_FREQ_MULT) * 4, SI5351_PLL_FIXED, SI5351_CLK0);
#endif

#ifdef FreqX2
  si5351.set_freq((vfo * SI5351_FREQ_MULT) * 2, SI5351_PLL_FIXED, SI5351_CLK0);
#endif

  pinMode(ENCODER_BTN, INPUT_PULLUP);
  PCICR |= (1 << PCIE2);           // Enable pin change interrupt for the encoder
  PCMSK2 |= (1 << PCINT18) | (1 << PCINT19);
  sei();
  display_frequency();  // Update the display
  display_radix();

  // Порты, пины для управл. напр. УВЧ и АТТ
  pinMode(12, OUTPUT); // для кнопки УВЧ = A2
  pinMode(13, OUTPUT); //  для кнопки АТТ = A1

  // Порты, пины управл. напр. для дешифр. CD4028
  pinMode(14, OUTPUT); // b0 для A
  pinMode(15, OUTPUT); // b1 для B
  pinMode(16, OUTPUT); // b2 для D
  pinMode(17, OUTPUT); // b3 для C

}


void loop(){
int keypressed;

  // Update the display if the frequency has been changed
if (changed_f)
  {
    display_frequency();

#ifdef IF_Offset
    si5351.set_freq((vfo * SI5351_FREQ_MULT) + bfo, SI5351_PLL_FIXED, SI5351_CLK0);
    // Вы также можете вычесть BFO, чтобы удовлетворить ваши потребности
    //si5351.set_freq((vfo * SI5351_FREQ_MULT) - bfo  , SI5351_PLL_FIXED, SI5351_CLK0);

    if (vfo >= 10000000ULL & tbfo != "USB")
    {
      bfo = USB;
      tbfo = "USB";
      si5351.set_freq( bfo, 0, SI5351_CLK2);
      Serial.println("We've switched from LSB to USB");
    }
    else if (vfo < 10000000ULL & tbfo != "LSB")
    {
      bfo = LSB;
      tbfo = "LSB";
      si5351.set_freq( bfo, 0, SI5351_CLK2);
      Serial.println("We've switched from USB to LSB");
    }

#endif

#ifdef Direct_conversion
    si5351.set_freq((vfo * SI5351_FREQ_MULT), SI5351_PLL_FIXED, SI5351_CLK0);
    tbfo = "";
#endif

#ifdef FreqX4
    si5351.set_freq((vfo * SI5351_FREQ_MULT) * 4, SI5351_PLL_FIXED, SI5351_CLK0);
    tbfo = "";
#endif

#ifdef FreqX2
    si5351.set_freq((vfo * SI5351_FREQ_MULT) * 2, SI5351_PLL_FIXED, SI5351_CLK0);
    tbfo = "";
#endif

    changed_f = 0;
  }

  // Кнопки УВЧ и АТТ ---------------------------------
  {
    if (digitalRead(4) == HIGH && flag == 0) //если кнопка нажата
      // и перемення flag равна 0 , то ...
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
      LiquidCrystal_I2C .setCursor(5, 1);
      LiquidCrystal_I2C .print("   "); //  "пустое место"

      //здесь может быть любое ваше действие
    }
    if (regim == 2) //второй режим  - вкл. УВЧ
    {
      digitalWrite(12, LOW); //включает PRE
      digitalWrite(13, HIGH);
      LiquidCrystal_I2C .setCursor(5, 1); // место на экране для PRE
      LiquidCrystal_I2C .print("PRE");

      //здесь может быть любое ваше действие
    }

    if (regim == 3) //третий режим - очистка экрана

    {
      digitalWrite(12, LOW); //
      digitalWrite(13, LOW);
      LiquidCrystal_I2C .setCursor(5, 1); // место текста на экране
      LiquidCrystal_I2C .print("   ");    //  "пустое место"

      //здесь может быть любое ваше действие
    }


    if (regim == 4) //третий режим - вкл. АТТ
    {
      digitalWrite(12, HIGH); //включает АТТ
      digitalWrite(13, LOW);
      LiquidCrystal_I2C .setCursor(5, 1); // место на экране для АТТ
      LiquidCrystal_I2C .print("ATT");
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
      LiquidCrystal_I2C .setCursor(0, 1);
      LiquidCrystal_I2C .print("160m");
    }

    else
      // 80-метровый (3,5 - 3,8 МГц)
      if (vfo >= 3500000ULL && vfo <= 3800000ULL)
      {
        LiquidCrystal_I2C .setCursor(0, 1);
        LiquidCrystal_I2C .print("80m ");
      }
      else
        // 40-метровый (7 - 7,2 МГц)
        if (vfo >= 7000000ULL && vfo <= 7200000ULL)
        {
          LiquidCrystal_I2C .setCursor(0, 1);
          LiquidCrystal_I2C .print("40m ");
        }
        else
          // 30-метровый (только телеграф 10,1 - 10,15 МГц)
          if (vfo >= 10100000ULL && vfo <= 10150000ULL)
          {
            LiquidCrystal_I2C .setCursor(0, 1);
            LiquidCrystal_I2C .print("30m ");
          }
          else
            // 20-метровый (14 - 14,35 МГц)
            if (vfo >= 14000000ULL && vfo <= 14350000ULL)
            {
              LiquidCrystal_I2C .setCursor(0, 1);
              LiquidCrystal_I2C .print("20m ");
            }
            else
              // 17-метровый (18,068 - 18,168 МГц)
              if (vfo >= 18068000ULL && vfo <= 18168000ULL)
              {
                LiquidCrystal_I2C .setCursor(0, 1);
                LiquidCrystal_I2C .print("17m ");
              }
              else
                // 15-метровый (21 - 21,45 МГц)
                if (vfo >= 21000000ULL && vfo <= 21450000ULL)
                {
                  LiquidCrystal_I2C .setCursor(0, 1);
                  LiquidCrystal_I2C .print("15m ");
                }
                else
                  // 12-метровый (24,89 - 25,14 МГц)
                  if (vfo >= 24890000ULL && vfo <= 25140000ULL)
                  {
                    LiquidCrystal_I2C .setCursor(0, 1);
                    LiquidCrystal_I2C .print("12m ");
                  }
                  else
                    // 10-метровый (28 - 29,7 МГц)
                    if (vfo >= 28000000ULL && vfo <= 29700000ULL)
                    {
                      LiquidCrystal_I2C .setCursor(0, 1);
                      LiquidCrystal_I2C .print("10m ");
                    }
                    else
                      // Если за границей любительских - очистка экрана
                    {
                      LiquidCrystal_I2C .setCursor(0, 1);
                      LiquidCrystal_I2C .print("    ");
                    }

    //-----------------------------------
    // Button press changes the frequency change step for 1 Hz steps
    if (get_button())
    {
      switch (radix)
      {
        case 1:
          radix = 10;
          break;
        case 10:
          radix = 100;
          break;
        case 100:
          radix = 1000;
          break;
        case 1000:
          radix = 10000;
          break;
        case 10000:
          radix = 100000;
          break;
        case 100000:
          radix = 1000000;
          break;
        case 1000000:
          radix = 1;
          break;
      }
      display_radix();
    }


  }


}