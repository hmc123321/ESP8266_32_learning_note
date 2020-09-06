#include "esp_camera.h"
#include "Arduino.h"
#include "FS.h"                // SD Card ESP32
#include "SD_MMC.h"            // SD Card ESP32
#include "soc/soc.h"           // Disable brownour problems
#include "soc/rtc_cntl_reg.h"  // Disable brownour problems
#include "driver/rtc_io.h"
#include "stdio.h"
#include "freertos/ringbuf.h"

// define the number of bytes you want to access
#define EEPROM_SIZE 1
// Pin definition for CAMERA_MODEL_AI_THINKER
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

#define LED_GPIO 33
#define START_GPIO 16
#define MODE_GPIO 12
/* 捕获照片句柄 */
TaskHandle_t captureHandle = NULL;
/* 保存照片句柄 */
TaskHandle_t savingHandle = NULL;
/* 环形缓冲区存储句柄 */
/* RingbufHandle_t buf_handle; */
/* 照片结构信号量 */
SemaphoreHandle_t PicMutex;
/* 帧同步队列 */
QueueHandle_t FrameQueue;

/* 帧率变量 */
uint8_t frameRate=0;
/* 默认缓冲区大小为QVGA：320*240 */
uint8_t BufferSize=0;
/* 创建图片结构体 */
camera_fb_t * fb = NULL;
uint8_t isRecording=0;



void setup (){
    /* 开启串口调试 */
    Serial.begin(115200);
    /* 初始化时决定帧率,默认30帧，按下（0）为15帧 */
    pinMode (MODE_GPIO,INPUT_PULLUP);
    if (digitalRead(MODE_GPIO)==1)
    {
        frameRate=30;
        BufferSize=320*240;
    }
    else 
    {
        frameRate=15;
        BufferSize=640*480;
    }
    /* 创建1byte队列 */
    FrameQueue=xQueueCreate(1,1);
    /* 创建1互斥量 */
    PicMutex= xSemaphoreCreateMutex();
    /* 创建环形缓冲区，jpeg大小不固定 */
/*     buf_handle = xRingbufferCreate(4096*8, RINGBUF_TYPE_NOSPLIT); */
    /* 分双核处理 */
    xTaskCreatePinnedToCore(take_photo,
                            "take photo",
                            4096*8,
                            NULL,
                            2,
                            &captureHandle,
                            1);
    xTaskCreatePinnedToCore(save_photo,
                            "save photo",
                            4096*8,
                            NULL,
                            1,
                            &savingHandle,
                            0); 
}

void loop()
{

}

void CAM_Init()
{
    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = Y2_GPIO_NUM;
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d7 = Y9_GPIO_NUM;
    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;
    config.pin_sscb_sda = SIOD_GPIO_NUM;
    config.pin_sscb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG; 

    //30帧时使用QVGA(320*240)
    if (frameRate==30){
        config.frame_size = FRAMESIZE_QVGA;
        config.jpeg_quality = 12;
        config.fb_count = 3;
    }
    //15帧时使用VGA(640*480)
    else {
        config.frame_size = FRAMESIZE_VGA;
        config.jpeg_quality = 12;
        config.fb_count = 3;
    }
     // Init Camera
    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("Camera init failed with error 0x%x", err);
        return;
    }
}

void SD_MMC_Init()
{
    if(!SD_MMC.begin())
    {
        Serial.println("Card Mount Failed");
        return;
    }
    uint8_t cardType = SD_MMC.cardType();

    if(cardType == CARD_NONE)
    {
        Serial.println("No SD_MMC card attached");
        return;
    }

    Serial.print("SD_MMC Card Type: ");
    if(cardType == CARD_MMC)
    {
        Serial.println("MMC");
    } else if(cardType == CARD_SD)
    {
        Serial.println("SDSC");
    } else if(cardType == CARD_SDHC)
    {
        Serial.println("SDHC");
    } else 
    {
        Serial.println("UNKNOWN");
    }

    uint64_t cardSize = SD_MMC.cardSize() / (1024 * 1024);
    Serial.printf("SD_MMC Card Size: %lluMB\n", cardSize);
    Serial.printf("Total space: %lluMB\n", SD_MMC.totalBytes() / (1024 * 1024));
    Serial.printf("Used space: %lluMB\n", SD_MMC.usedBytes() / (1024 * 1024));
}


void take_photo (void *pvParameters){
    portTickType xLastWakeTime;
    UBaseType_t res;
    uint8_t getStatus=0,getFrame=0;
    uint32_t last_time;
    pinMode(START_GPIO,INPUT_PULLUP);
    pinMode(LED_GPIO,OUTPUT);//红色LED作为录像指示,低电平有效
    digitalWrite(LED_GPIO,1);//默认关闭

    CAM_Init();/* 开启相机 */

    while(1)
    {
        if (digitalRead(START_GPIO)==0)//按下按钮后，消抖20ms
        {
            vTaskDelay(20/portTICK_RATE_MS);
            if (digitalRead(START_GPIO)==0) 
            {
                while (digitalRead(START_GPIO)==0);//松手起效
                /* 亮灯 */
                digitalWrite(LED_GPIO,getStatus);
                /* 反转标志 */
                getStatus=(~getStatus)&0x01;
                isRecording=getStatus;
                /* 按下按钮才开始计时 */
                xLastWakeTime = xTaskGetTickCount();
            }
        }

        if (getStatus==1){//若录像标置为1，开始持续录制
            /* 帧队列置1，若队列满，说明上一帧没有存完，阻塞等待 */
            xQueueSendToFront(FrameQueue, &getFrame,portMAX_DELAY);
            /* 获取信号量，防止读图片 */
            xSemaphoreTake(PicMutex,portMAX_DELAY);
            /* 拍照 */
            last_time=micros();
            fb = esp_camera_fb_get();
            /* 写入完成释放 */
            xSemaphoreGive(PicMutex);
            Serial.printf("take %u us to record",micros()-last_time);
            /* 判断是否录制成功 */
            if(!fb) 
            {
                Serial.printf("Fail to take photo");
                return;
            }
            /* 若成功，写入环形缓冲区 */
            /* else 
            {
                
                res = xRingbufferSend(buf_handle,fb->buf,fb->len, pdMS_TO_TICKS(100));
                if (res != pdTRUE) {
                    Serial.printf("Failed to send item\n");
                }
            } */
            /* 释放缓存 */
            esp_camera_fb_return(fb);
            /* 阻塞 */ 
            vTaskDelayUntil( &xLastWakeTime, ( (1000/frameRate) / portTICK_RATE_MS ) );
        }
    }
    vTaskDelete(NULL);
}

uint8_t * processPicName(uint32_t picNumber){
    uint8_t path[20]="/pic";
    const uint8_t picEnd[]=".jpg";
    /* 字符串长度从pic后面开始 */
    uint8_t m=4;
    /* 录制24小时最多7位数 */
    path[m++]='0'+(picNumber/10000000);
    path[m++]='0'+(picNumber%10000000/1000000);
    path[m++]='0'+(picNumber%1000000/100000);
    path[m++]='0'+(picNumber%100000/10000);
    path[m++]='0'+(picNumber%10000/100);
    path[m++]='0'+(picNumber%1000/10);
    path[m++]='0'+(picNumber%100/10);
    path[m++]='0'+(picNumber%10);
    /* 拷贝结束字符串 */
    memcpy(&path[m],picEnd,4);
    m+=4;
    /* 结尾补0 */
    path[m]=0;
    return path;
}


void save_photo(void *pvParameters)
{
    File file;
    /* 照片序列号 */
    uint32_t pictureNumber = 0;
    /*照片名*/
    char path[20];
    uint8_t getStatus=0,getFrame=0;
    uint32_t last_time;

    /* 环形缓冲区的读指针 */
    /* uint8_t *ringBufferData; */
    /* 缓冲区长度变量 */
    /* size_t item_size; */
    /* 初始化SD卡 */
    SD_MMC_Init();
    while(1)
    {
        if (isRecording==1)
        {
            memcpy(path,(char *)processPicName(pictureNumber),20);
            /*指针指向环形缓冲区 */
            /* ringBufferData=(uint8_t*)xRingbufferReceive(buf_handle, &item_size, pdMS_TO_TICKS(100)); */
            /* 新建文件 */
            file = SD_MMC.open(path, FILE_WRITE);
            if(!file)
            {
                Serial.println("Failed to open file in writing mode");
            }
            else 
            {   
                /* 队列为空时阻塞等待 */
                xQueueReceive(FrameQueue,&getFrame,portMAX_DELAY);
                /* 队列有数据了，说明已写入一帧 */
                /* 利用互斥量等待拍照完成 */
                xSemaphoreTake(PicMutex,portMAX_DELAY);
                last_time=micros();
                file.write(fb->buf, fb->len);
                xSemaphoreGive(PicMutex);
                Serial.printf("take %u us to save",micros()-last_time);
                /* if (ringBufferData != NULL) {
                for (int i = 0; i < item_size; i++) {
                    file.write(ringBufferData[i]);
                } */
                file.close();
                /* vRingbufferReturnItem(buf_handle, (void *)ringBufferData); */
                pictureNumber+=1;
                /* } */
            }
        }
    }
        vTaskDelete(NULL);
}
