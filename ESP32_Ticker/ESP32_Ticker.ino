#include <Ticker.h>

Ticker flipper;

int count = 0;
int LED_BUILTIN=2;
void flip() {
  int state = digitalRead(LED_BUILTIN);  // get the current state of GPIO1 pin
  digitalWrite(LED_BUILTIN, !state);     // set pin to the opposite state

  ++count;
  // 当翻转次数达到20次的时候，切换led的闪烁频率，每隔0.1s翻转一次
  if (count == 20) {
    flipper.attach(0.1, flip);
  }
  // 当次数达到120次的时候关闭ticker
  else if (count == 120) {
    flipper.detach();
  }
}

void setup() {
  //LED_BUILTIN 对应板载LED的IO口
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  //每隔0.3s 翻转一下led状态
  flipper.attach(0.3, flip);
}

void loop() {
}
