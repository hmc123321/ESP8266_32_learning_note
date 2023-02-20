#define KEY_PERIOD pdMS_TO_TICKS(500)

TimerHandle_t Key_Timer;

void setup() {
  Serial.begin(115200);
  Key_Timer = xTimerCreate(
    "Key_Timer_Task",
    KEY_PERIOD,
    pdTRUE,
    0,
    vTimerCallBack
    );
  xTimerStart(Key_Timer,0);
}

void loop() {
  Serial.println("Task_loop");
  delay(1000);
}

void vTimerCallBack(TimerHandle_t xTimer)
{
  Serial.println("Task_Timer");
}