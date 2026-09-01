#define LED_PIN 25

unsigned long last_led_change;
bool enable = true;
float frequency = 1;
bool toggle = HIGH;

void setup()
{
  pinMode(LED_BUILTIN, OUTPUT);
  
  last_led_change = millis();
}

void loop()
{
  if (enable == false)
  {
    return;
  }

  unsigned long now = millis();
  
  if (last_led_change + 1000 / frequency  < now)
  {
    toggle =! toggle;
    digitalWrite(LED_BUILTIN, toggle);
    last_led_change = now;
  }
}