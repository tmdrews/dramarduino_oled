/*  _
** |_|___ ___
** | |_ -|_ -|
** |_|___|___|
**  iss(c)2020
**
**  public site: https://forum.defence-force.org/viewtopic.php?f=9&t=1699
**
**  Updated: 2020.10.05 - fixed bit operation - @john
**           2020.10.29 - fixed more bit operation - @thekorex
*            2021.11.09 - Added a blink ratio to slow down the blinking during testing 
*                       - Also added more feedback on the serial port
*                       - Could also add a screen to better follow the process like some version on Ebay - @Franks3DShop
*            2022.03.10 - Added LCD 16x2 with PCF8574 module to display progress, success and errors. Connect the LCD
*                         SDA to A4 and SCL to A5, +5 to Vcc and GND to GND
*                         it will work if it is not connected and also report on the USB Serial port.
*            2022.04.29 - Changed LCD 16x2 to 128x64 OLED Display, connection same as LCD 16x2 
*/

/* ================================================================== */
#include <SoftwareSerial.h>
#include <Adafruit_SSD1306.h> // Adafruit SSD1306 Display Library

int BLINK_RATIO = 0; // This is a divider to slow down the blinking of the led
int BLINK_COUNT = 0;

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET     4 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define DI          15  // PC1
#define DO           8  // PB0
#define CAS          9  // PB1
#define RAS         17  // PC3
#define WE          16  // PC2

#define XA0         18  // PC4
#define XA1          2  // PD2
#define XA2         19  // PC5
#define XA3          6  // PD6
#define XA4          5  // PD5
#define XA5          4  // PD4
#define XA6          7  // PD7
#define XA7          3  // PD3
#define XA8         14  // PC0

#define M_TYPE      10  // PB2
#define R_LED       11  // PB3
#define G_LED       12  // PB4

#define RXD          0  // PD0
#define TXD          1  // PD1

#define BUS_SIZE     9

/* ================================================================== */
volatile int bus_size;

//SoftwareSerial USB(RXD, TXD);

const unsigned int a_bus[BUS_SIZE] = {
  XA0, XA1, XA2, XA3, XA4, XA5, XA6, XA7, XA8
};

void setBus(unsigned int a) {
  int i;
  for (i = 0; i < BUS_SIZE; i++) {
    digitalWrite(a_bus[i], a & 1);
    a /= 2;
  }
}

void writeAddress(unsigned int r, unsigned int c, int v) {
  /* row */
  setBus(r);
  digitalWrite(RAS, LOW);

  /* rw */
  digitalWrite(WE, LOW);

  /* val */
  digitalWrite(DI, (v & 1)? HIGH : LOW);

  /* col */
  setBus(c);
  digitalWrite(CAS, LOW);

  digitalWrite(WE, HIGH);
  digitalWrite(CAS, HIGH);
  digitalWrite(RAS, HIGH);
}

int readAddress(unsigned int r, unsigned int c) {
  int ret = 0;

  /* row */
  setBus(r);
  digitalWrite(RAS, LOW);

  /* col */
  setBus(c);
  digitalWrite(CAS, LOW);

  /* get current value */
  ret = digitalRead(DO);

  digitalWrite(CAS, HIGH);
  digitalWrite(RAS, HIGH);

  return ret;
}

void error(int r, int c)
{
  unsigned long a = ((unsigned long)c << bus_size) + r;
  digitalWrite(R_LED, LOW);
  digitalWrite(G_LED, HIGH);
  interrupts();
  Serial.print(" FAILED at $");
  Serial.println(a, HEX);
  Serial.flush();
  
  // OLED Display output
  display.clearDisplay();
  display.setTextSize(1);                     // Normal 1:1 pixel scale
  display.setTextColor(SSD1306_WHITE);        // Draw white text
  display.setCursor(0,0);                     // Start at top-left corner
  display.print("Error at $");                // Print to get one line
  display.println(String(a, HEX));            // Position of Memory Error
  display.setCursor(0,8);
  display.println("Test failed");
  display.setTextSize(2);                     // Text size increased
  display.setCursor(16,32);
  display.println("Chip bad");
  display.display();
  delay(2000);
  while (1)
    ;
}

void ok(void)
{
  digitalWrite(R_LED, HIGH);
  digitalWrite(G_LED, LOW);
  interrupts();
  Serial.println();
  Serial.println(" Chip OK!");
  Serial.flush();
  
  // OLED Display output
  display.clearDisplay();
  display.setTextSize(2);                     // Text size increased
  display.setTextColor(SSD1306_WHITE);        // Draw white text
  display.setCursor(21,24);                    // Start at line 24 to center text
  display.println("Chip OK!");
  display.display();
  delay(2000);

  
  while (1)
    ;
}

void blink(void)
{
  digitalWrite(G_LED, LOW);
  digitalWrite(R_LED, LOW);
  delay(1000);
  digitalWrite(R_LED, HIGH);
  digitalWrite(G_LED, HIGH);
  }

void green() {
  BLINK_COUNT++;
  if (BLINK_COUNT > BLINK_RATIO) {
    BLINK_COUNT = 0;
    digitalWrite(G_LED, !digitalRead(G_LED));
  }
}

void fill(int v) {
  int r, c, g = 0;
  v &= 1;
  for (c = 0; c < (1<<bus_size); c++) {
    green();
    for (r = 0; r < (1<<bus_size); r++) {
      writeAddress(r, c, v);
      if (v != readAddress(r, c))
        error(r, c);
    }
    g ^= 1;
  }
  blink();
}

void fillx(int v) {
  int r, c, g = 0;
  v &= 1;
  for (c = 0; c < (1<<bus_size); c++) {
    green();
    for (r = 0; r < (1<<bus_size); r++) {
      writeAddress(r, c, v);
      if (v != readAddress(r, c))
        error(r, c);
        v ^= 1;
    }

    g ^= 1;
  }
  blink();
}

void setup() {
  int i;
  Serial.begin(115200);

  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {  // Address 0x3D for 128x64
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }



  //  Serial.begin(115200);
  while (!Serial)
    ; /* wait */

  Serial.println();
  Serial.print("DRAM TESTER ");

  for (i = 0; i < BUS_SIZE; i++)
    pinMode(a_bus[i], OUTPUT);

  pinMode(CAS, OUTPUT);
  pinMode(RAS, OUTPUT);
  pinMode(WE, OUTPUT);
  pinMode(DI, OUTPUT);

  pinMode(R_LED, OUTPUT);
  pinMode(G_LED, OUTPUT);

  pinMode(M_TYPE, INPUT);
  pinMode(DO, INPUT);

  digitalWrite(WE, HIGH);
  digitalWrite(RAS, HIGH);
  digitalWrite(CAS, HIGH);

  digitalWrite(R_LED, HIGH);
  digitalWrite(G_LED, HIGH);

  if (digitalRead(M_TYPE)) {
    /* jumper not set - 41256 */
    bus_size = BUS_SIZE;
      display.clearDisplay();
      display.setTextColor(SSD1306_WHITE);            // Draw white text
      display.setTextSize(2);                     
      display.setCursor(34,24);                   
      display.print("41256");
      display.display();
      delay(3000);
    Serial.print("256Kx1 ");
  } else {
    /* jumper set - 4164 */
    bus_size = BUS_SIZE - 1;
      display.clearDisplay();
      display.setTextColor(SSD1306_WHITE);            // Draw white text
      display.setTextSize(2);                     
      display.setCursor(40,24);                   
      display.print("4164");
      display.display();
      delay(3000);
    Serial.print("64Kx1 ");
  }
  Serial.flush();

  digitalWrite(R_LED, LOW);
  digitalWrite(G_LED, LOW);

  noInterrupts();
  for (i = 0; i < (1 << BUS_SIZE); i++) {
    digitalWrite(RAS, LOW);
    digitalWrite(RAS, HIGH);
  }
  digitalWrite(R_LED, HIGH);
  digitalWrite(G_LED, HIGH);
  interrupts();
}

void loop() {
  interrupts();
  Serial.println();
  Serial.print("Test pattern 01");
  // OLED Display output
  display.clearDisplay();
  display.setTextSize(1);                     // Normal 1:1 pixel scale
  display.setTextColor(SSD1306_WHITE);        // Draw white text
  display.setCursor(0,24);                    // Start at top-left corner
  display.println("   Test pattern 01  ");
  display.display();
  delay(2000);
  noInterrupts(); fillx(0);

  interrupts();
  Serial.println();
  Serial.print("Testing pattern 10");
  // OLED Display output
  display.clearDisplay();
  display.setTextSize(1);                     // Normal 1:1 pixel scale
  display.setTextColor(SSD1306_WHITE);        // Draw white text
  display.setCursor(0,24);                    // Start at top-left corner
  display.println("   Test pattern 10  ");
  display.display();
  delay(2000);
  noInterrupts(); fillx(1);

  interrupts();
  Serial.println();
  Serial.print("Testing with all 0");
  // OLED Display output
  display.clearDisplay();
  display.setTextSize(1);                     // Normal 1:1 pixel scale
  display.setTextColor(SSD1306_WHITE);        // Draw white text
  display.setCursor(0,24);                    // Start at top-left corner
  display.println("   Test with all 0  ");
  display.display();
  delay(2000);
  noInterrupts(); fill(0);

  interrupts();
  Serial.println();
  Serial.print("Test with all 1");
  // OLED Display output
  display.clearDisplay();
  display.setTextSize(1);                     // Normal 1:1 pixel scale
  display.setTextColor(SSD1306_WHITE);        // Draw white text
  display.setCursor(0,24);                    // Start at top-left corner
  display.println("   Test with all 1  ");
  display.display();
  delay(2000);
  noInterrupts(); fill(1);
  ok();
}
