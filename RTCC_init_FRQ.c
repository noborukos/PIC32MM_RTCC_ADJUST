// PIC32MMのRTCCクロック源をLPRCにした場合に
// 周波数の誤差をCPUクロックを基準にして補正
// CPUクロックは外付けの振動子でそれなりに精度があると仮定
// RTCCの分周期を0にして1秒間にどれだけ時間が進むかで
// LPRCの周波数を求める

/////////////////////////////////////////////////
#include <xc.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <string.h>

/////////////////////////////////////////////////
// RTCCアラーム発生フラグ
volatile bool rtcc_alarm_flag;


/////////////////////////////////////////////////
// レジスタ書き換え制御
inline static void SYSTEM_RegUnlock(void)
{
    SYSKEY = 0x0; //write invalid key to force lock
    SYSKEY = 0xAA996655; //write Key1 to SYSKEY
    SYSKEY = 0x556699AA; //write Key2 to SYSKEY
}
inline static void SYSTEM_RegLock(void)
{
   SYSKEY = 0x00000000; 
}

/////////////////////////////////////////////////
// LPRC周波数取得
uint32_t LPRC_frq_culc( void )
{
    uint32_t t_wait;
    uint32_t t_start;
    uint32_t now_time;
    uint32_t now_date;
    uint32_t offs;
    uint32_t total_offs;
    uint16_t cnt;
    uint32_t i;
    
    sprintf((char*)&deb_msg[0],"LPRC FRQ culc\r\n");
    UART1_puts(&deb_msg[0]);
    total_offs = 0;
// 6回繰り返して5回分の平均を取る
    for( cnt = 0; cnt < 6; cnt++ )
    {
        SYSTEM_RegUnlock();
        RTCCON1CLR = (1 << _RTCCON1_WRLOCK_POSITION);
        RTCCON1CLR = (1 << _RTCCON1_ON_POSITION);
     
        // set 2000-01-01 00-00-00
        RTCDATE = 0x00010101; // Year/Month/Date/Wday
        RTCTIME = 0x00000000; //  hours/minutes/seconds
        // DIV 00000; CLKSEL LPRC; FDIV 0; 
        RTCCON2 = 0x00000001;
        // Enable RTCC 
        RTCCON1SET = (1 << _RTCCON1_ON_POSITION);
        RTCCON1SET = (1 << _RTCCON1_WRLOCK_POSITION);
        SYSTEM_RegLock();
// CPUクロックを基準に1秒間でRTCCがどれだけ進むか計算
//        delay_ms(1000);
        t_wait = ( SYS_CLK_FREQ / 2000 ) * 1000;    //1000msec 
        t_start = _mfc0(9,0);
        while( ( _mfc0(9,0) - t_start ) < t_wait ){}
        now_time = RTCTIME;
        now_date = RTCDATE;
        
        offs = 0;
        offs = offs + ( ( ( ( ( now_time >> 28 ) & 0x0f ) * 10) + ( ( ( now_time >> 24 ) & 0x0f ) * 1 ) ) * 3600 );
        offs = offs + ( ( ( ( ( now_time >> 20 ) & 0x0f ) * 10) + ( ( ( now_time >> 16 ) & 0x0f ) * 1 ) ) * 60   );
        offs = offs + ( ( ( ( ( now_time >> 12 ) & 0x0f ) * 10) + ( ( ( now_time >> 8  ) & 0x0f ) * 1 ) ) * 1    );
        sprintf((char*)&deb_msg[0],"%08X %08X\r\n", now_time, offs);
        UART1_puts(&deb_msg[0]);
        if( cnt != 0 ){ total_offs = total_offs + offs; } // 1回目は除去
    }
 
    total_offs = ( total_offs / 5 ) * 2; //RTCCは0.5秒刻み
    total_offs = total_offs + 130;  // !!!微調整!!! とりあえず
    sprintf((char*)&deb_msg[0],"LPRC FRQ = %dHz [%08X] \r\n", total_offs, total_offs );
    UART1_puts(&deb_msg[0]);

// 戻り値は周波数(Hz)
    return total_offs;
}

/////////////////////////////////////////////////
// RTCC初期化
// 引数は周波数(Hz)
void RTCC_Initialize_FRQ(uint32_t f)
{
  
    SYSTEM_RegUnlock();
    RTCCON1CLR = (1 << _RTCCON1_WRLOCK_POSITION);
    RTCCON1CLR = (1 << _RTCCON1_ON_POSITION);
     
    // set 2000-01-01 14-28-37
    RTCDATE = 0x00010101; // Year/Month/Date/Wday
    RTCTIME = 0x00000000; //  hours/minutes/seconds

    // set 2021-04-11 00-00-00        
    ALMDATE = 0x41100; // Month/Date/Wday
    ALMTIME = 0x00000000; // hours/minutes/seconds

#if 1
    // 1時間毎アラーム
    // ON enabled; OUTSEL Alarm Event; WRLOCK disabled; AMASK Every Hour; ALMRPT 0; RTCOE disabled; CHIME enabled; ALRMEN enabled; 
    RTCCON1 = 0xC5008000;
#else
    // 1分毎アラーム
    // ON enabled; OUTSEL Alarm Event; WRLOCK disabled; AMASK Every Minute; ALMRPT 0; RTCOE disabled; CHIME enabled; ALRMEN enabled; 
    RTCCON1 = 0xC3008000;
#endif
    
    // DIV (引数の1/2); CLKSEL LPRC; FDIV 0; 
    RTCCON2 = ( ( f / 2 ) << 16 ) + 0x0001;

    // Enable RTCC 
    RTCCON1SET = (1 << _RTCCON1_ON_POSITION);
    RTCCON1SET = (1 << _RTCCON1_WRLOCK_POSITION);
    SYSTEM_RegLock();

    IEC0bits.RTCCIE = 1;
}

/////////////////////////////////////////////////
// RTCC割り込み
void __attribute__ ((vector(_RTCC_VECTOR), interrupt(IPL1SOFT))) _ISR_RTCC(void) 
{
    rtcc_alarm_flag = true;
    IFS0CLR= 1 << _IFS0_RTCCIF_POSITION;
}

/////////////////////////////////////////////////
// アラームイベントフラグリセット
void reset_rtcc_alarm_event_flag(void)
{
    rtcc_alarm_flag = false;
}

/////////////////////////////////////////////////
// アラームイベントフラグチェック
bool is_rtcc_alarm_event_flag(void)
{
    return rtcc_alarm_flag;
}


