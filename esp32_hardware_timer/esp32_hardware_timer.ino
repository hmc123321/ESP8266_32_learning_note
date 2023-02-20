void IRAM_ATTR onTimer();

hw_timer_t * RefreshTimer = NULL;            //声明一个定时器

void setup() {
  //Hardware Timer init 
  Serial.begin(115200);
   
  RefreshTimer = timerBegin(0, 80, true);                //初始化，APB是80M，使用80分频获得1us时基
  timerAttachInterrupt(RefreshTimer, &onTimer, true);    //调用中断函数
  timerAlarmWrite(RefreshTimer, 1000000, true);        //timerBegin的参数二 1s定时值，是否重载
  timerAlarmEnable(RefreshTimer);                //定时器使能

}

void loop() {
  // put your main code here, to run repeatedly:

}

void IRAM_ATTR onTimer() {            //中断函数
  Serial.println("Hardware Timer");
}

