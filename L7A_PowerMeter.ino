// 20200406 update 
// v.1.1
/************************************
*            2020/04/05             *
*  1. fix receive data string       *
*  1.1 fix indata[7] = gain         *
*  1.2 fix indata[8] = total        *
*  2. fix getADC function           *
************************************/

/******************************************************************************************
void freshHMI(void)      // 每秒更新HMI (1 Second Average)畫面一次
void sendResult(void)    // 每分更新HMI (1 Minute Average)畫面一次 , 並使用 SoftwareSerial 的 Xbee上傳資料到APC
void resetTimer(void)    // 系統每經過 rstTime 的時間後 , 進行Reset Arduino及HMI
void serialFlush()       // 清空UART BUFFER的資料
void PAN_write(void)     // 透過SoftwareSerial 進行 Xbee資料寫入
void EndCmd(void)        // 傳送HMI命令的前後 , 須加上EndCmd , 即連續傳送 0xFF 0xFF 0xFF
void getADC(void)        // 讀取ADS1115各Channel的數值
int receiveSetting(void) // 讀取RX Buffer , 並判斷收到的資料與命令為何
*****************************************************************************************/

#include <Wire.h>
#include <math.h>
#include <SoftwareSerial.h>
#include <Adafruit_ADS1015.h>

SoftwareSerial xbeeSerial(2,3);  //RX,TX

Adafruit_ADS1115 ads1(0x48);    //設定ads位址0x48、0x49、0x4A、0x4B中取二
Adafruit_ADS1115 ads2(0x49);

//const unsigned long rstTime = 86400000;
const unsigned long rstTime = 86400000;  
unsigned long tmpInt;
int strCnt ;
char buf[12];
char buf2[12];
char buf3[12];

double aa ,bb ;
int newInt;
    
unsigned long timerSec ;
unsigned long timerMin ;
unsigned long timerRst ;
int cntMin ;
int cntSec ;

int16_t adc1[4] ;
int16_t adc2[4] ;
int16_t adc[8];

unsigned long adcSumSec[8];
float currentSec[8];
float currentMin[8];
int tmp;

// garbage , PAN ID , Voltage , CT Type , CT Number , MAC Address , Phase , Gain , Total
//    0    ,    1   ,    2    ,    3    ,      4    ,       5     ,   6   ,   7   ,   8
String indata[9];   //字串陣列

//------- APC Xbee Define ---------
const char mac_addr[] = "0013A2000013A200";
const char STX = 0x02;
const char ETX = 0x03;
const char CR = 0x13;
const char LF = 0x10;
String OutputString ;
//------- APC Xbee Define ---------

int panID ;
int voltage;
int ctType;
int ctNum;
int phase ;
float phaseValue;
float gain;   // gain of current

void(* resetFunc) (void) = 0; // set reset command

void setup()
{     
   Wire.begin();
   xbeeSerial.begin(9600);   // xbee阜口
   Serial.begin(9600);
   delay(50);
   Serial.println(F("Booting"));
   serialFlush();      //? 接收HMI參數，下面已有判斷式，因 HMI delay 1000而躲過              
   ads1.begin();
   ads2.begin();

   while(1) {   // 直到接收到HMI回傳的參數
       if(receiveSetting()==1)  { 
           break;
       }
       delay(30);  
    } 
    
    Serial.println(F("Received Setting !")) ;    
    EndCmd();  
    Serial.write("page0.t10.txt=\"Arduino Online!!\"");
    EndCmd();  


//轉換參數並print
    panID = indata[1].toInt();
    voltage = indata[2].toInt();
    ctType = indata[3].toInt();
    ctNum = indata[4].toInt();   
    indata[5].toCharArray(mac_addr,17);
    phase = indata[6].toInt();
    gain = indata[7].toInt() / 100.0;

    if (phase == 1){
        phaseValue = 1.723;
    }else {
        phaseValue = 1.0;  
    }
    
    Serial.println();
    Serial.print(F("PanID : "));     Serial.println(panID);
    Serial.print(F("Voltage : "));   Serial.println(voltage);
    Serial.print(F("CT Type: "));    Serial.println(ctType);
    Serial.print(F("CT Number: "));  Serial.println(ctNum);
    Serial.print(F("MAC Address: "));Serial.println(mac_addr);
    Serial.print(F("Phase : "));     Serial.println(phase);
    Serial.print(F("Gain : "));     Serial.println(gain);
    PAN_write();  // write panID to Xbee by SoftwareSerial

    ads1.begin();
    ads2.begin();

    for (int i = 0 ; i < 4 ; i++){     //adc1[4],adc2[4], currentSec[8]=0.0, currentMin[8]=0.0, adcSumSec[8] 用於處理電壓電流值
        adc1[i]=0 ;
        adc2[i]=0 ;
        currentSec[i]=0.0;
        currentMin[i]=0.0;
        currentSec[i+4]=0.0;
        currentMin[i+4]=0.0;
        adcSumSec[i];
        adcSumSec[i+4];      
    }

    cntMin = 0;
    cntSec = 0  ;
    timerSec = millis();
    timerMin = millis();
    timerRst = millis();
}

// ----------------------------------------------------------------------------

void loop()
{   
    resetTimer();       // reset arduino everyday
    receiveSetting();   // receive USART command
    getADC();           // Loading ADC value   //讀取數位轉板的數位值

    //--------------------------  1 second avg ------------------------
    if ((millis() - timerSec) > 1000 ){
        freshHMI();               //是不是應該放for迴圈下面        
        for (int i = 0 ; i < 8 ; i ++) {          
            // 讀取ADC1115輸入加總 , 計算每秒平均電流
            currentSec[i] = (adcSumSec[i] / cntSec * 0.1875) /1000.0 *ctType /5.0 * gain ;          
            // 每分鐘平均電流儲存於 currentMin
            currentMin[i] = currentMin[i]+currentSec[i];
            adcSumSec[i] = 0;     //迴圈外統一清空字串
        } 
        cntMin++;   //秒數跟分數計算方式??   
        cntSec = 0 ;
        timerSec = millis();   //更新秒數計算的時間
    }
    //----------------------  1 minute avg --------------------------------
    else if ((millis() - timerMin) > 60000 ){        
        for (int i = 0 ; i < 8 ; i ++) {  
            currentMin[i] = currentMin[i] / cntMin;   //分鐘數平均?
        }
        sendResult();
        cntMin = 0 ;
        for (int i = 0 ; i < 8 ; i ++) {
            currentMin[i]=0;
        }
        timerMin = millis();             
    }    
}  //void loop()

//--------------------------------------------------------------------------

int receiveSetting(void){
    int flag;
    flag =0;
    strCnt = 0 ; 
    
    for (int i = 0 ; i < 9 ; i++) {         //清空陣列!? 可用 memset(indata, '\0', sizeof(indata))
      indata[i]="";
    }        

    while (Serial.available()) {
        char c = Serial.read();             // 0A,PAN,Voltage....,GAIN,0A          
        // 如果沒有讀到\n (0x0A)  而且也沒讀到逗號 , (0x2C) , 那就持續讀字串
        if(c!='\n' && c!=0x2C){  // 0x2C ==> ,
            indata[8] += c;         
        }
        // 如果有讀到逗點 , (0x2C)  那就存成下一筆
        else if(c==0x2C) {
            indata[strCnt] = indata[8] ;
            indata[8] = "" ;  
            strCnt++;                //跑完參數更新後indata[]長這樣 [XX , PAN , Voltage ,CT Type , CT number , MAC Address , Phase , GAIN , XX]
                        
        }
        delay(3);    // 沒有延遲的話 UART 串口速度會跟不上Arduino的速度，會導致資料不完整
    }      

    // 判斷是否收到 HMI 的重啟請求
    // 如果 indata[1] 收到 rest命令 , 便傳送 rest命令給 HMI 要求重啟
    // 並將 Arduino 本體進行 reset
    if (indata[1] == "rest"){  // 確定有讀到rest再進行 rest
        Serial.println(F("Reset"));        //? 
        for (int i = 0 ; i < 3 ; i++){
            delay(50);
            EndCmd();               //?
            Serial.write("rest");
            EndCmd();
            delay(50);                
        }
        resetFunc();        
    }

    // 判斷是否收到增益值
    // 如 indata[2] 收到 101 , 代表為 1.01 , 需除以100
    if (indata[1]=="GAIN"){
              gain = indata[2].toInt()/100.0;    
              //Serial.println("Receive Loop ") ;
              Serial.println(gain);
    }
    
    if (indata[1] != ""){    //確定有讀到命令再進行 print
        for (int i = 0 ; i < 9 ; i++) {
            if(indata[i] != "") {
                Serial.print(F("indata["));
                Serial.print(i);
                Serial.print(F("] = "));
                Serial.println(indata[i]);            
            }        
            strCnt =0;     //?不須初始化
            //indata[7]="";
        }      
    }
        
    for(int i = 0 ; i < 8 ; i++){        //應直接寫 if(indata[7]==""
        if (indata[i]==""){
            flag=0;                   
        }else
            flag=1;
    }
    indata[8]="";    //不須初始化
    return flag;
}

//--------------------------------------------------------------------------

void getADC(void)    //讀取電壓電流值
{ 
  cntSec++; 
  for (int i = 0 ; i < 4 ; i++){
      adc[i]   = ads1.readADC_SingleEnded(i);      
      adc[i+4] = ads2.readADC_SingleEnded(i);
      if(adc[i]  < 0) { adc[i]  =0;}
      if(adc[i+4]< 0) { adc[i+4]=0;}
  }
  for (int i = 0 ; i < 8 ; i++){  
      adcSumSec[i] = adcSumSec[i] + adc[i];
  }
}

//--------------------------------------------------------------------------

void EndCmd(void)   {
    Serial.write(0xFF);
    Serial.write(0xFF);
    Serial.write(0xFF);    
}

//--------------------------------------------------------------------------
 
void PAN_write(void) {
    delay(1000);
    xbeeSerial.print("+++")  ;
    delay(1100);
    xbeeSerial.println();
    xbeeSerial.print("ATID ");
    xbeeSerial.println(panID);
    delay(1000);
    xbeeSerial.print("ATJV ");
    xbeeSerial.println(1);
    delay(1000);
    xbeeSerial.println("ATWR");
    delay(1000);
    xbeeSerial.println("ATAC");
    delay(1000);
    xbeeSerial.println("ATCN");
    delay(2000);      
    Serial.println(F("PAN Setting Done"));
}  

//--------------------------------------------------------------------------

void serialFlush(){
    while(Serial.available() > 0) {
      char t = Serial.read();
    }
}   

//--------------------------------------------------------------------------

void resetTimer(void){
    // reset Arduino & HMI everyday
    // 1 Day = 86,400 Seconds = 86,400,000 mini Seconds
    if ((millis() - timerRst) > rstTime) {
        serialFlush();                         //? 為何還要接收資料
        for(int i = 0 ; i < 5 ; i++){          //? 為何要重啟五次
            EndCmd();
            Serial.write("rest");
            EndCmd();
            delay(100);  
            }
     resetFunc();
     }
}

//---------------------------------------------------------------------------
// update avg data every second
void freshHMI(void)
{
    //----- Average Current in 1 Second ----

    aa = modf(currentSec[0]*100,&bb);      //拆解浮點數currentSec[0],小數點部分存到bb，整數為aa
    newInt = int(bb);                      //浮點數currentSec[0]的小數點存到newint
    itoa(newInt , buf , 10);               //將newint由轉換為字串放入buf(10進位)
    indata[6] = "x11.val=";
    indata[6].toCharArray(buf2 , 30);      //將字串轉為字元陣列,大小為30
    strcpy(buf3 , buf2);                   //將buf2加入buf3     x11.val=82  
    strcpy(buf3 + strlen(buf2) , buf);     //合併字串 
    EndCmd();
    Serial.write(buf3);
    EndCmd();    
    
    aa = modf(currentSec[1]*100,&bb);
    newInt = int(bb);
    itoa(newInt , buf , 10);
    indata[6] = "x21.val=";
    indata[6].toCharArray(buf2 , 30); 
    strcpy(buf3 , buf2);
    strcpy(buf3 + strlen(buf2) , buf);              // 合併字串 
    EndCmd();
    Serial.write(buf3);
    EndCmd();   

    aa = modf(currentSec[2]*100,&bb);
    newInt = int(bb);
    itoa(newInt , buf , 10);
    indata[6] = "x31.val=";
    indata[6].toCharArray(buf2 , 30); 
    strcpy(buf3 , buf2);
    strcpy(buf3 + strlen(buf2) , buf);              // 合併字串 
    EndCmd(); 
    Serial.write(buf3);
    EndCmd();   

    aa = modf(currentSec[3]*100,&bb);
    newInt = int(bb);
    itoa(newInt , buf , 10);
    indata[6] = "x41.val=";
    indata[6].toCharArray(buf2 , 30); 
    strcpy(buf3 , buf2);
    strcpy(buf3 + strlen(buf2) , buf);              // 合併字串 
    EndCmd();
    Serial.write(buf3);
    EndCmd();   

    aa = modf(currentSec[4]*100,&bb);
    newInt = int(bb);
    itoa(newInt , buf , 10);
    indata[6] = "x51.val=";
    indata[6].toCharArray(buf2 , 30); 
    strcpy(buf3 , buf2);
    strcpy(buf3 + strlen(buf2) , buf);              // 合併字串 
    EndCmd();
    Serial.write(buf3);
    EndCmd();   

    aa = modf(currentSec[5]*100,&bb);
    newInt = int(bb);
    itoa(newInt , buf , 10);
    indata[6] = "x61.val=";
    indata[6].toCharArray(buf2 , 30); 
    strcpy(buf3 , buf2);
    strcpy(buf3 + strlen(buf2) , buf);              // 合併字串 
    EndCmd();
    Serial.write(buf3);
    EndCmd();   

    aa = modf(currentSec[6]*100,&bb);
    newInt = int(bb);
    itoa(newInt , buf , 10);
    indata[6] = "x71.val=";
    indata[6].toCharArray(buf2 , 30); 
    strcpy(buf3 , buf2);
    strcpy(buf3 + strlen(buf2) , buf);              // 合併字串 
    EndCmd();
    Serial.write(buf3);
    EndCmd();   

    aa = modf(currentSec[7]*100,&bb);
    newInt = int(bb);
    itoa(newInt , buf , 10);
    indata[6] = "x81.val=";
    indata[6].toCharArray(buf2 , 30); 
    strcpy(buf3 , buf2);
    strcpy(buf3 + strlen(buf2) , buf);              // 合併字串 
    EndCmd();
    Serial.write(buf3);
    EndCmd();   
    
    //-------------------------------- Average Power in 1 Second ------------------------------ 
    delay(20) ; 
    aa = modf(currentSec[0]*100*voltage*phaseValue/1000 , &bb);   //計算電壓
    newInt = int(bb);
    itoa(newInt , buf , 10);
    indata[6] = "x12.val=";
    indata[6].toCharArray(buf2 , 30); 
    strcpy(buf3 , buf2);
    strcpy(buf3 + strlen(buf2) , buf);              // 合併字串 
    EndCmd();
    Serial.write(buf3);
    EndCmd();    

    aa = modf(currentSec[1]*100*voltage*phaseValue/1000 , &bb);
    newInt = int(bb);
    itoa(newInt , buf , 10);
    indata[6] = "x22.val=";
    indata[6].toCharArray(buf2 , 30); 
    strcpy(buf3 , buf2);
    strcpy(buf3 + strlen(buf2) , buf);              // 合併字串 
    EndCmd();
    Serial.write(buf3);
    EndCmd();   

    aa = modf(currentSec[2]*100*voltage*phaseValue/1000 , &bb);
    newInt = int(bb);
    itoa(newInt , buf , 10);
    indata[6] = "x32.val=";
    indata[6].toCharArray(buf2 , 30); 
    strcpy(buf3 , buf2);
    strcpy(buf3 + strlen(buf2) , buf);              // 合併字串 
    EndCmd();
    Serial.write(buf3);
    EndCmd();   

    aa = modf(currentSec[3]*100*voltage*phaseValue/1000 , &bb);
    newInt = int(bb);
    itoa(newInt , buf , 10);
    indata[6] = "x42.val=";
    indata[6].toCharArray(buf2 , 30); 
    strcpy(buf3 , buf2);
    strcpy(buf3 + strlen(buf2) , buf);              // 合併字串 
    EndCmd();
    Serial.write(buf3);
    EndCmd();   

    aa = modf(currentSec[4]*100*voltage*phaseValue/1000 , &bb);
    newInt = int(bb);
    itoa(newInt , buf , 10);
    indata[6] = "x52.val=";
    indata[6].toCharArray(buf2 , 30); 
    strcpy(buf3 , buf2);
    strcpy(buf3 + strlen(buf2) , buf);              // 合併字串 
    EndCmd();
    Serial.write(buf3);
    EndCmd();   

    aa = modf(currentSec[5]*100*voltage*phaseValue/1000 , &bb);
    newInt = int(bb);
    itoa(newInt , buf , 10);
    indata[6] = "x62.val=";
    indata[6].toCharArray(buf2 , 30); 
    strcpy(buf3 , buf2);
    strcpy(buf3 + strlen(buf2) , buf);              // 合併字串 
    EndCmd();
    Serial.write(buf3);
    EndCmd();   

    aa = modf(currentSec[6]*100*voltage*phaseValue/1000 , &bb);
    newInt = int(bb);
    itoa(newInt , buf , 10);
    indata[6] = "x72.val=";
    indata[6].toCharArray(buf2 , 30); 
    strcpy(buf3 , buf2);
    strcpy(buf3 + strlen(buf2) , buf);              // 合併字串 
    EndCmd();
    Serial.write(buf3);
    EndCmd();   

    aa = modf(currentSec[7]*100*voltage*phaseValue/1000 , &bb);
    newInt = int(bb);
    itoa(newInt , buf , 10);
    indata[6] = "x82.val=";
    indata[6].toCharArray(buf2 , 30); 
    strcpy(buf3 , buf2);
    strcpy(buf3 + strlen(buf2) , buf);              // 合併字串 
    EndCmd();
    Serial.write(buf3);
    EndCmd();   

    Serial.println();
}

//--------------------------------------------------------------------------------------------

void sendResult(void)  // send to Xbee & refresh hmi
{
    Serial.println();
    //-------------------------- Average Current in 1 Minute -----------------------
    aa = modf(currentMin[0]*100,&bb);    
    //aa = modf(tmp/1023,&bb);
    newInt = int(bb);
    itoa(newInt , buf , 10);
    indata[6] = "x13.val=";
    indata[6].toCharArray(buf2 , 30); 
    strcpy(buf3 , buf2);
    strcpy(buf3 + strlen(buf2) , buf);              // 合併字串 
    EndCmd();
    Serial.write(buf3);
    EndCmd();    

    aa = modf(currentMin[1]*100,&bb);
    //aa = modf(tmp/1023*100,&bb);
    newInt = int(bb);
    itoa(newInt , buf , 10);
    indata[6] = "x23.val=";
    indata[6].toCharArray(buf2 , 30); 
    strcpy(buf3 , buf2);
    strcpy(buf3 + strlen(buf2) , buf);              // 合併字串 
    EndCmd();
    Serial.write(buf3);
    EndCmd();   

    aa = modf(currentMin[2]*100,&bb);
    newInt = int(bb);
    itoa(newInt , buf , 10);
    indata[6] = "x33.val=";
    indata[6].toCharArray(buf2 , 30); 
    strcpy(buf3 , buf2);
    strcpy(buf3 + strlen(buf2) , buf);              // 合併字串 
    EndCmd();
    Serial.write(buf3);
    EndCmd();   

    aa = modf(currentMin[3]*100,&bb);
    newInt = int(bb);
    itoa(newInt , buf , 10);
    indata[6] = "x43.val=";
    indata[6].toCharArray(buf2 , 30); 
    strcpy(buf3 , buf2);
    strcpy(buf3 + strlen(buf2) , buf);              // 合併字串 
    EndCmd();
    Serial.write(buf3);
    EndCmd();   

    aa = modf(currentMin[4]*100,&bb);
    newInt = int(bb);
    itoa(newInt , buf , 10);
    indata[6] = "x53.val=";
    indata[6].toCharArray(buf2 , 30); 
    strcpy(buf3 , buf2);
    strcpy(buf3 + strlen(buf2) , buf);              // 合併字串 
    EndCmd();
    Serial.write(buf3);
    EndCmd();   

    aa = modf(currentMin[5]*100,&bb);
    newInt = int(bb);
    itoa(newInt , buf , 10);
    indata[6] = "x63.val=";
    indata[6].toCharArray(buf2 , 30); 
    strcpy(buf3 , buf2);
    strcpy(buf3 + strlen(buf2) , buf);              // 合併字串 
    EndCmd();
    Serial.write(buf3);
    EndCmd();   

    aa = modf(currentMin[6]*100,&bb);
    newInt = int(bb);
    itoa(newInt , buf , 10);
    indata[6] = "x73.val=";
    indata[6].toCharArray(buf2 , 30); 
    strcpy(buf3 , buf2);
    strcpy(buf3 + strlen(buf2) , buf);              // 合併字串 
    EndCmd();
    Serial.write(buf3);
    EndCmd();   

    aa = modf(currentMin[7]*100,&bb);
    newInt = int(bb);
    itoa(newInt , buf , 10);
    indata[6] = "x83.val=";
    indata[6].toCharArray(buf2 , 30); 
    strcpy(buf3 , buf2);
    strcpy(buf3 + strlen(buf2) , buf);              // 合併字串 
    EndCmd();
    Serial.write(buf3);
    EndCmd();   
    
    //----------------------------- Average Power in 1 Minute ------------------------------ 
    delay(20) ; 
    aa = modf(currentMin[0]*100*voltage*phaseValue/1000 , &bb);    //計算電壓
    newInt = int(bb);
    itoa(newInt , buf , 10);
    indata[6] = "x14.val=";
    indata[6].toCharArray(buf2 , 30); 
    strcpy(buf3 , buf2);
    strcpy(buf3 + strlen(buf2) , buf);              // 合併字串 
    EndCmd();
    Serial.write(buf3);
    EndCmd();    

    aa = modf(currentMin[1]*100*voltage*phaseValue/1000 , &bb);
    newInt = int(bb);
    itoa(newInt , buf , 10);
    indata[6] = "x24.val=";
    indata[6].toCharArray(buf2 , 30); 
    strcpy(buf3 , buf2);
    strcpy(buf3 + strlen(buf2) , buf);              // 合併字串 
    EndCmd();
    Serial.write(buf3);
    EndCmd();   

    aa = modf(currentMin[2]*100*voltage*phaseValue/1000 , &bb);
    newInt = int(bb);
    itoa(newInt , buf , 10);
    indata[6] = "x34.val=";
    indata[6].toCharArray(buf2 , 30); 
    strcpy(buf3 , buf2);
    strcpy(buf3 + strlen(buf2) , buf);              // 合併字串 
    EndCmd();
    Serial.write(buf3);
    EndCmd();   

    aa = modf(currentMin[3]*100*voltage*phaseValue/1000 , &bb);
    newInt = int(bb);
    itoa(newInt , buf , 10);
    indata[6] = "x44.val=";
    indata[6].toCharArray(buf2 , 30); 
    strcpy(buf3 , buf2);
    strcpy(buf3 + strlen(buf2) , buf);              // 合併字串 
    EndCmd();
    Serial.write(buf3);
    EndCmd();   

    aa = modf(currentMin[4]*100*voltage*phaseValue/1000 , &bb);
    newInt = int(bb);
    itoa(newInt , buf , 10);
    indata[6] = "x54.val=";
    indata[6].toCharArray(buf2 , 30); 
    strcpy(buf3 , buf2);
    strcpy(buf3 + strlen(buf2) , buf);              // 合併字串 
    EndCmd();
    Serial.write(buf3);
    EndCmd();   

    aa = modf(currentMin[5]*100*voltage*phaseValue/1000 , &bb);
    newInt = int(bb);
    itoa(newInt , buf , 10);
    indata[6] = "x64.val=";
    indata[6].toCharArray(buf2 , 30); 
    strcpy(buf3 , buf2);
    strcpy(buf3 + strlen(buf2) , buf);              // 合併字串 
    EndCmd();
    Serial.write(buf3);
    EndCmd();   

    aa = modf(currentMin[6]*100*voltage*phaseValue/1000 , &bb);
    newInt = int(bb);
    itoa(newInt , buf , 10);
    indata[6] = "x74.val=";
    indata[6].toCharArray(buf2 , 30); 
    strcpy(buf3 , buf2);
    strcpy(buf3 + strlen(buf2) , buf);              // 合併字串 
    EndCmd();
    Serial.write(buf3);
    EndCmd();   

    aa = modf(currentMin[7]*100*voltage*phaseValue/1000 , &bb);
    newInt = int(bb);
    itoa(newInt , buf , 10);
    indata[6] = "x84.val=";
    indata[6].toCharArray(buf2 , 30); 
    strcpy(buf3 , buf2);
    strcpy(buf3 + strlen(buf2) , buf);              // 合併字串 
    EndCmd();
    Serial.write(buf3);
    EndCmd();   

    Serial.println();
\\將各頻道的電流數值串起來         
    indata[0]="";
    for (int i = 0 ; i < ctNum ; i++){
        indata[0] = String(indata[0]) + "," + String(currentMin[i])  ;        
    }    
    
    indata[6]= String(mac_addr) + "," + String(ctNum+2) + "," + String(voltage) + "," + 
               String(phase) + String(indata[0]) ;
    
    Serial.print(STX); 
    Serial.print(indata[6]);
    Serial.print(ETX);
    Serial.print(CR);
    Serial.print(LF);
    Serial.println();

    xbeeSerial.print(STX); 
    xbeeSerial.print(indata[6]);
    xbeeSerial.print(ETX);
    xbeeSerial.print(CR);
    xbeeSerial.print(LF);
    xbeeSerial.println();    
}
