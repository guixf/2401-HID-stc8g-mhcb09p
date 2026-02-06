/**************************************************************************************/

#include <STC8G.h>
#include <stdbool.h>
#include <stdint.h>

// Additional SFR definitions for STC8G
#define _P1ASF 0x9D
SFR(P1ASF, 0x9D);
#define _LVDCR 0xFD
SFR(LVDCR, 0xFD);

/************************* 可配置参数区 *************************/
// 工作模式配置
#define DEBUG_MODE         1       // 1=开启串口输出；0=关闭串口
#define WDT_ENABLE         1       // 1=启用看门狗；0=禁用看门狗

#define POWER_CTRL_MODE    1       // 0=低电平开启电源；1=高电平开启电源

// 时间参数（单位：ms）
#define DELAY_0_5S         500     // 通用0.5秒延时
#define DELAY_0_05S        50      // Key1/Key2/Key3低脉冲时长
#define DELAY_10MS         10      // 10ms延时
#define VOLTAGE_THRESHOLD  3000    // 电压阈值（3V）
#define REF_VOLTAGE        1190    // 内部参考电压（1.19V）

// 看门狗超时时间配置
#define WDT_TIMEOUT_8S     0x27    // 约8秒超时
#define WDT_CONFIG         WDT_TIMEOUT_8S  // 选择看门狗超时时间

// LED和继电器预设值
#define LED1_ON_LEVEL      1       // LED1亮时的电平（1=高电平亮）
#define LED2_ON_LEVEL      1       // LED2亮时的电平（1=高电平亮）
#define LED3_ON_LEVEL      1       // LED3亮时的电平（1=高电平亮）
#define RELAY1_OPEN_LEVEL  1       // Relay1打开时的电平
#define RELAY2_OPEN_LEVEL  1       // Relay2打开时的电平
#define RELAY3_OPEN_LEVEL  1       // Relay3打开时的电平

// 调试+串口参数
#define BAUDRATE           115200  // 串口波特率
#define FOSC               24000000// 晶振频率（24MHz）

/************************* IO口定义 *************************/
// 输入口
#define HUMAN_2410S_IN    P32     // 2410s人体检测
#define PIR_IN            P33     // HID传感器
#define LED1_STATUS       P34     // LED1状态输入
#define LED2_STATUS       P35     // LED2状态输入
#define LED3_STATUS       P13     // LED3状态输入
#define RELAY1_FEEDBACK   P36     // Relay1反馈
#define RELAY2_FEEDBACK   P37     // Relay2反馈
#define RELAY3_FEEDBACK   P14     // Relay3反馈

// 输出口
#define POWER_CTRL        P55     // 对外供电开关，高电平打开电源
#define KEY1_OUT          P54     // HMBC09P Key1输出
#define KEY2_OUT          P17     // HMBC09P Key2输出
#define KEY3_OUT          P15     // HMBC09P Key3输出

/************************* 全局变量 *************************/
bool voltage_low_flag = 0;        // 低电压标记
bool voltage_high_flag = 0;       // 高电压标记
uint8_t last_human_state = 0;     // 上一次人体检测状态
uint8_t last_pir_state = 0;       // 上一次手机检测状态
uint8_t last_led1_state = 0;      // 上一次LED1状态
uint8_t last_led2_state = 0;      // 上一次LED2状态
uint8_t last_relay3_state = 0;    // 上一次Relay3状态

/************************* 函数声明 *************************/
void System_Init(void);          // 系统初始化
void LVD_ADC_Init(void);         // LVD+ADC初始化
void Delay_ms(uint16_t ms);      // 毫秒延时
uint16_t Get_VCC_Voltage(void);  // 获取VCC电压
void Detect_Voltage_Status(void);// 检测电压状态
void Output_Key1_Pulse(void);    // Key1输出0.05s脉冲
void Output_Key2_Pulse(void);    // Key2输出0.05s脉冲
void Output_Key3_Pulse(void);    // Key3输出0.05s脉冲
bool Check_LED1_Status(void);    // 检测LED1状态
bool Check_LED2_Status(void);    // 检测LED2状态
bool Check_LED3_Status(void);    // 检测LED3状态
bool Check_Relay1_Status(void);  // 检测Relay1状态
bool Check_Relay2_Status(void);  // 检测Relay2状态
bool Check_Relay3_Status(void);  // 检测Relay3状态
void Process_Key1_Logic(void);   // 处理Key1逻辑
void Process_Key2_Logic(void);   // 处理Key2逻辑
void Process_Key3_Logic(void);   // 处理Key3逻辑
void Check_State_Changes(void);  // 检查所有状态变化

/************************* 看门狗函数声明 *************************/
#if WDT_ENABLE == 1
void WDT_Init(void);            // 看门狗初始化
void WDT_Feed(void);            // 喂狗
#endif

/************************* 调试模式相关函数声明 *************************/
#if DEBUG_MODE == 1
void UART1_Init(void);           // 串口初始化
void UART1_Send_Voltage(uint16_t volt);// 串口发送电压
void UART1_Send_String(char *str); // 串口发送字符串
void Debug_Output_Status(void);  // 调试输出状态信息
#endif

/************************* 主函数 *************************/
void main(void)
{
    // 系统初始化
    System_Init();
    LVD_ADC_Init();
    
    // 看门狗初始化
    #if WDT_ENABLE == 1
    WDT_Init();
    #endif
    
    // 如果开启调试模式，初始化串口
    #if DEBUG_MODE == 1
    UART1_Init();
    UART1_Send_String("System Initialized\r\n");
    #endif
    
    // 打开2410s/HMBC09P供电开关
    POWER_CTRL = POWER_CTRL_MODE ? 1 : 0;
    
    // 延时0.5s
    Delay_ms(DELAY_0_5S);
    
    // 检测电压并标记电压状态
    Detect_Voltage_Status();
    
    // 调试模式下串口输出电压值
    #if DEBUG_MODE == 1
    UART1_Send_Voltage(Get_VCC_Voltage());
    #endif
    
    // 初始化上一次状态
    last_human_state = HUMAN_2410S_IN;
    last_pir_state = PIR_IN;
    last_led1_state = Check_LED1_Status();
    last_led2_state = Check_LED2_Status();
    last_relay3_state = Check_Relay3_Status();
    
    // 首次执行逻辑
    Process_Key1_Logic();
    Process_Key2_Logic();
    Process_Key3_Logic();
    
    // 主循环
    while(1)
    {
        // 检查所有状态变化
        Check_State_Changes();
        
        // 喂狗操作
        #if WDT_ENABLE == 1
        WDT_Feed();
        #endif
        
        // 小延时减少CPU占用
        Delay_ms(DELAY_10MS);
    }
}

/************************* 状态变化检测函数 *************************/
void Check_State_Changes(void)
{
    static uint16_t voltage_check_counter = 0;
    
    // 获取当前所有状态
    uint8_t current_human = HUMAN_2410S_IN;
    uint8_t current_pir = PIR_IN;
    uint8_t current_led1 = Check_LED1_Status();
    uint8_t current_led2 = Check_LED2_Status();
    uint8_t current_relay3 = Check_Relay3_Status();
    
    // 检查Key1相关状态变化（人体检测或LED1状态变化）
    if(current_human != last_human_state || current_led1 != last_led1_state)
    {
        #if DEBUG_MODE == 1
        UART1_Send_String("State change detected: ");
        if(current_human != last_human_state)
            UART1_Send_String("Human ");
        if(current_led1 != last_led1_state)
            UART1_Send_String("LED1 ");
        UART1_Send_String("\r\n");
        Debug_Output_Status();
        #endif
        
        Process_Key1_Logic();
        last_human_state = current_human;
        last_led1_state = current_led1;
    }
    
    // 检查Key2相关状态变化（手机检测或LED2状态变化）
    if(current_pir != last_pir_state || current_led2 != last_led2_state)
    {
        #if DEBUG_MODE == 1
        UART1_Send_String("State change detected: ");
        if(current_pir != last_pir_state)
            UART1_Send_String("Phone ");
        if(current_led2 != last_led2_state)
            UART1_Send_String("LED2 ");
        UART1_Send_String("\r\n");
        Debug_Output_Status();
        #endif
        
        Process_Key2_Logic();
        last_pir_state = current_pir;
        last_led2_state = current_led2;
    }
    
    // 检查Key3相关状态变化（Relay3状态变化）
    if(current_relay3 != last_relay3_state)
    {
        #if DEBUG_MODE == 1
        UART1_Send_String("State change detected: Relay3\r\n");
        Debug_Output_Status();
        #endif
        
        Process_Key3_Logic();
        last_relay3_state = current_relay3;
    }
    
    // 定期电压检测
    voltage_check_counter++;
    if(voltage_check_counter >= 100) // 约每秒检测一次电压（100 * 10ms = 1秒）
    {
        voltage_check_counter = 0;
        Detect_Voltage_Status();
        
        // 电压检测后总是检查Key3逻辑
        Process_Key3_Logic();
    }
}

/************************* Key逻辑处理函数 *************************/
void Process_Key1_Logic(void)
{
    // 逻辑：P3.2高电平+LED1不亮，或者P3.2低电平+LED1亮，则联动Key1
    bool human_detected = (HUMAN_2410S_IN == 1);  // 有人：P3.2高电平
    bool led1_on = Check_LED1_Status();           // LED1亮：P3.4高电平
    
    bool condition1 = human_detected && !led1_on;   // 有人但LED1灭
    bool condition2 = !human_detected && led1_on;   // 无人但LED1亮
    
    if(condition1 || condition2)
    {
        #if DEBUG_MODE == 1
        if(condition1)
            UART1_Send_String("Key1: Human detected but LED1 off -> Sending pulse\r\n");
        else if(condition2)
            UART1_Send_String("Key1: No human but LED1 on -> Sending pulse\r\n");
        #endif
        
        Output_Key1_Pulse();
    }
    #if DEBUG_MODE == 1
    else
    {
        UART1_Send_String("Key1: Conditions not met (");
        UART1_Send_String(human_detected ? "Human, " : "No human, ");
        UART1_Send_String(led1_on ? "LED1 ON" : "LED1 OFF");
        UART1_Send_String(")\r\n");
    }
    #endif
}

void Process_Key2_Logic(void)
{
    // 逻辑：P3.3高电平+LED2不亮，或者P3.3低电平+LED2亮，则联动Key2
    bool phone_home = (PIR_IN == 1);              // 手机在家：P3.3高电平
    bool led2_on = Check_LED2_Status();           // LED2亮：P3.5高电平
    
    bool condition1 = phone_home && !led2_on;   // 手机在家但LED2灭
    bool condition2 = !phone_home && led2_on;   // 手机不在家但LED2亮
    
    if(condition1 || condition2)
    {
        #if DEBUG_MODE == 1
        if(condition1)
            UART1_Send_String("Key2: Phone home but LED2 off -> Sending pulse\r\n");
        else if(condition2)
            UART1_Send_String("Key2: Phone not home but LED2 on -> Sending pulse\r\n");
        #endif
        
        Output_Key2_Pulse();
    }
    #if DEBUG_MODE == 1
    else
    {
        UART1_Send_String("Key2: Conditions not met (");
        UART1_Send_String(phone_home ? "Phone home, " : "Phone not home, ");
        UART1_Send_String(led2_on ? "LED2 ON" : "LED2 OFF");
        UART1_Send_String(")\r\n");
    }
    #endif
}

void Process_Key3_Logic(void)
{
    // 逻辑：电压联动Relay3
    bool relay3_on = Check_Relay3_Status();
    
    bool condition1 = voltage_low_flag && !relay3_on;   // 电压低但Relay3关
    bool condition2 = voltage_high_flag && relay3_on;   // 电压高但Relay3开
    
    if(condition1 || condition2)
    {
        #if DEBUG_MODE == 1
        if(condition1)
            UART1_Send_String("Key3: Low voltage but Relay3 off -> Sending pulse\r\n");
        else if(condition2)
            UART1_Send_String("Key3: High voltage but Relay3 on -> Sending pulse\r\n");
        #endif
        
        Output_Key3_Pulse();
    }
    #if DEBUG_MODE == 1
    else
    {
        UART1_Send_String("Key3: Conditions not met (");
        UART1_Send_String(voltage_low_flag ? "Voltage low, " : 
                         voltage_high_flag ? "Voltage high, " : "Voltage normal, ");
        UART1_Send_String(relay3_on ? "Relay3 ON" : "Relay3 OFF");
        UART1_Send_String(")\r\n");
    }
    #endif
}

/************************* 看门狗函数实现 *************************/
#if WDT_ENABLE == 1
void WDT_Init(void)
{
    // STC8G系列看门狗初始化
    WDT_CONTR = WDT_CONFIG;  // 设置预分频并启用看门狗
    
    // 延时等待稳定
    Delay_ms(10);
    
    #if DEBUG_MODE == 1
    UART1_Send_String("Watchdog Enabled (8s timeout)\r\n");
    #endif
}

void WDT_Feed(void)
{
    // 喂狗操作 - 清除看门狗计时器
    WDT_CONTR = WDT_CONFIG | 0x10; // 设置CLR_WDT位为1清除看门狗计时器
}
#endif

/************************* 调试模式相关函数实现 *************************/
#if DEBUG_MODE == 1
void Debug_Output_Status(void)
{
    UART1_Send_String("Current Status: ");
    
    // 输出人体检测状态
    UART1_Send_String("Human:");
    if(HUMAN_2410S_IN == 1)
        UART1_Send_String("YES");
    else
        UART1_Send_String("NO");
    
    // 输出手机在家状态
    UART1_Send_String(", Phone:");
    if(PIR_IN == 1)
        UART1_Send_String("HOME");
    else
        UART1_Send_String("NOT_HOME");
    
    // 输出LED状态
    UART1_Send_String(", LED1:");
    if(Check_LED1_Status())
        UART1_Send_String("ON");
    else
        UART1_Send_String("OFF");
    
    UART1_Send_String(", LED2:");
    if(Check_LED2_Status())
        UART1_Send_String("ON");
    else
        UART1_Send_String("OFF");
    
    UART1_Send_String(", LED3:");
    if(Check_LED3_Status())
        UART1_Send_String("ON");
    else
        UART1_Send_String("OFF");
    
    // 输出继电器状态
    UART1_Send_String(", Relay3:");
    if(Check_Relay3_Status())
        UART1_Send_String("ON");
    else
        UART1_Send_String("OFF");
    
    // 输出电压状态
    UART1_Send_String(", Voltage:");
    if(voltage_low_flag)
        UART1_Send_String("LOW");
    else if(voltage_high_flag)
        UART1_Send_String("HIGH");
    else
        UART1_Send_String("NORMAL");
    
    UART1_Send_String("\r\n");
}

void UART1_Send_String(char *str)
{
    while(*str != '\0')
    {
        SBUF = *str;
        while(!TI);
        TI = 0;
        str++;
    }
}

// 串口1初始化（115200波特率）
void UART1_Init(void)
{
    SCON = 0x50;                  
    AUXR |= 0x40; 
    AUXR &= 0xFE; 
    TMOD &= 0x0F;
    TL1 = 0xCC;
    TH1 = 0xFF;
    ET1 = 0;
    TR1 = 1;
    EA = 1;       
}

// 串口发送电压值
void UART1_Send_Voltage(uint16_t volt)
{
    uint8_t buf[16];
    buf[0] = 'V'; buf[1] = 'C'; buf[2] = 'C'; buf[3] = ':';
    buf[4] = volt / 1000 + '0';
    buf[5] = (volt % 1000) / 100 + '0';
    buf[6] = (volt % 100) / 10 + '0';
    buf[7] = volt % 10 + '0';
    buf[8] = 'm'; buf[9] = 'V'; buf[10] = '\r'; buf[11] = '\n'; buf[12] = '\0';
    
    UART1_Send_String((char *)buf);
}
#endif

/************************* 系统通用函数实现 *************************/
// 系统初始化
void System_Init(void)
{
    // IO模式配置
    // P3口：P3.2-P3.7高阻输入（人体检测、LED状态、Relay反馈）
    P3M0 &= ~0xFC; 
    P3M1 |= 0xFC;
    
    // P1口：P1.3高阻输入（LED3），P1.4高阻输入（Relay3反馈），P1.5推挽输出（Key3），P1.7推挽输出（Key2）
    P1M0 = (P1M0 & ~0xB0) | 0xA0; // P1.5和P1.7推挽输出
    P1M1 = (P1M1 & ~0xA8) | 0x18; // P1.3和P1.4高阻输入
    
    // P5口：P5.4推挽输出（Key1），P5.5推挽输出（电源控制）
    P5M0 |= 0x30; 
    P5M1 &= ~0x30; 
    
    // 输出口默认电平
    POWER_CTRL = POWER_CTRL_MODE ? 1 : 0;  // 电源常开
    
    KEY1_OUT = 1;
    KEY2_OUT = 1;
    KEY3_OUT = 1;
}

// LVD+ADC初始化
void LVD_ADC_Init(void)
{
    P1ASF = 0x00;               
    ADC_CONTR = 0x80;           
    ADC_RES = 0;                
    ADC_RESL = 0;
    Delay_ms(2);                
    ADC_CONTR |= 0x0F;          
    IE2 |= 0x80; 
    LVDCR = 0x00;               
    LVDCR |= (0b100 << 1);      
    LVDCR |= 0x01;              
    PCON &= ~LVDF;              
}

// 毫秒延时函数
void Delay_ms(uint16_t ms)
{
    uint16_t i, j;
    for(i = ms; i > 0; i--)
        for(j = 2475; j > 0; j--);
}

// 获取VCC电压
uint16_t Get_VCC_Voltage(void)
{
    uint16_t adc_val, voltage;
    ADC_CONTR |= 0x40;          
    while(!(ADC_CONTR & 0x20)); 
    ADC_CONTR &= ~0x20;         
    
    adc_val = (uint16_t)ADC_RES << 4;
    adc_val |= ADC_RESL & 0x0F;
    
    if(adc_val != 0)
    {
        voltage = (uint32_t)REF_VOLTAGE * 4096 / adc_val;
    }
    else
    {
        voltage = 0;
    }
    return voltage;
}

// 检测电压状态
void Detect_Voltage_Status(void)
{
    uint16_t volt = Get_VCC_Voltage();
    if(volt < VOLTAGE_THRESHOLD)
    {
        voltage_low_flag = 1;
        voltage_high_flag = 0;
        
        #if DEBUG_MODE == 1
        UART1_Send_String("Voltage Low detected: ");
        UART1_Send_Voltage(volt);
        #endif
    }
    else
    {
        voltage_low_flag = 0;
        voltage_high_flag = 1;
        
        #if DEBUG_MODE == 1
        UART1_Send_String("Voltage High detected: ");
        UART1_Send_Voltage(volt);
        #endif
    }
}

// Key1输出0.05秒低脉冲
void Output_Key1_Pulse(void)
{
    KEY1_OUT = 0;
    Delay_ms(DELAY_0_05S);
    KEY1_OUT = 1;
}

// Key2输出0.05秒低脉冲
void Output_Key2_Pulse(void)
{
    KEY2_OUT = 0;
    Delay_ms(DELAY_0_05S);
    KEY2_OUT = 1;
}

// Key3输出0.05秒低脉冲
void Output_Key3_Pulse(void)
{
    KEY3_OUT = 0;
    Delay_ms(DELAY_0_05S);
    KEY3_OUT = 1;
}

/************************* 状态检测函数 *************************/
bool Check_LED1_Status(void)
{
    return (LED1_STATUS == LED1_ON_LEVEL) ? 1 : 0;
}

bool Check_LED2_Status(void)
{
    return (LED2_STATUS == LED2_ON_LEVEL) ? 1 : 0;
}

bool Check_LED3_Status(void)
{
    return (LED3_STATUS == LED3_ON_LEVEL) ? 1 : 0;
}

bool Check_Relay1_Status(void)
{
    return (RELAY1_FEEDBACK == RELAY1_OPEN_LEVEL) ? 1 : 0;
}

bool Check_Relay2_Status(void)
{
    return (RELAY2_FEEDBACK == RELAY2_OPEN_LEVEL) ? 1 : 0;
}

bool Check_Relay3_Status(void)
{
    return (RELAY3_FEEDBACK == RELAY3_OPEN_LEVEL) ? 1 : 0;
}

// LVD中断
void LVD_ISR(void) __interrupt(26)
{
    if(PCON & LVDF)
    {
        voltage_low_flag = 1;
        PCON &= ~LVDF;
        
        #if DEBUG_MODE == 1
        UART1_Send_String("LVD Interrupt: Voltage Low\r\n");
        #endif
    }
}
// ==================== 程序结束 ====================