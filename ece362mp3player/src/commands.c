#include "stm32f0xx.h"
#include "ff.h"
#include "ffconf.h"
#include "lcd.h"
#include "tty.h"
#include "commands.h"
#include <string.h>
#include <stdio.h>

// Data structure for the mounted file system.
FATFS fs_storage;

typedef struct{
    char fileformat[4];
    int file_size;
    char subformat[4];
    char subformat_id[4];
    int chunk_bits;                         // 16or18or40 due to pcm it is 16 here
    short int audio_format;                     // little or big endian
    short int num_channels;                     // 2 here for left and right
    int sample_rate;                        // sample_rate denotes the sampling rate.
    int byte_rate;                              // bytes  per second
    short int bytes_per_frame;
    short int bits_per_sample;
    char data_id[4];                        // "data" written in ascii
    int data_size;
}head;

typedef union {
    struct {
        unsigned int bisecond:5; // seconds divided by 2
        unsigned int minute:6;
        unsigned int hour:5;
        unsigned int day:5;
        unsigned int month:4;
        unsigned int year:7;
    };
} fattime_t;

static FIL wavFile;
#define SAMPLES 8000
BYTE wavBuffer[SAMPLES];
int end;

static fattime_t fattime;

void set_fattime(int year, int month, int day, int hour, int minute, int second)
{
    fattime_t newtime;
    newtime.year = year - 1980;
    newtime.month = month;
    newtime.day = day;
    newtime.hour = hour;
    newtime.minute = minute;
    newtime.bisecond = second/2;
    int len = sizeof newtime;
    memcpy(&fattime, &newtime, len);
}

void advance_fattime(void)
{
    fattime_t newtime = fattime;
    newtime.bisecond += 1;
    if (newtime.bisecond == 30) {
        newtime.bisecond = 0;
        newtime.minute += 1;
    }
    if (newtime.minute == 60) {
        newtime.minute = 0;
        newtime.hour += 1;
    }
    if (newtime.hour == 24) {
        newtime.hour = 0;
        newtime.day += 1;
    }
    if (newtime.month == 2) {
        if (newtime.day >= 29) {
            int year = newtime.year + 1980;
            if ((year % 1000) == 0) { // we have a leap day in 2000
                if (newtime.day > 29) {
                    newtime.day -= 28;
                    newtime.month = 3;
                }
            } else if ((year % 100) == 0) { // no leap day in 2100
                if (newtime.day > 28)
                newtime.day -= 27;
                newtime.month = 3;
            } else if ((year % 4) == 0) { // leap day for other mod 4 years
                if (newtime.day > 29) {
                    newtime.day -= 28;
                    newtime.month = 3;
                }
            }
        }
    } else if (newtime.month == 9 || newtime.month == 4 || newtime.month == 6 || newtime.month == 10) {
        if (newtime.day == 31) {
            newtime.day -= 30;
            newtime.month += 1;
        }
    } else {
        if (newtime.day == 0) { // cannot advance to 32
            newtime.day = 1;
            newtime.month += 1;
        }
    }
    if (newtime.month == 13) {
        newtime.month = 1;
        newtime.year += 1;
    }

    fattime = newtime;
}

uint32_t get_fattime(void)
{
    return *(uint32_t *)&fattime;
}



void print_error(FRESULT fr, const char *msg)
{
    const char *errs[] = {
            [FR_OK] = "Success",
            [FR_DISK_ERR] = "Hard error in low-level disk I/O layer",
            [FR_INT_ERR] = "Assertion failed",
            [FR_NOT_READY] = "Physical drive cannot work",
            [FR_NO_FILE] = "File not found",
            [FR_NO_PATH] = "Path not found",
            [FR_INVALID_NAME] = "Path name format invalid",
            [FR_DENIED] = "Permision denied",
            [FR_EXIST] = "Prohibited access",
            [FR_INVALID_OBJECT] = "File or directory object invalid",
            [FR_WRITE_PROTECTED] = "Physical drive is write-protected",
            [FR_INVALID_DRIVE] = "Logical drive number is invalid",
            [FR_NOT_ENABLED] = "Volume has no work area",
            [FR_NO_FILESYSTEM] = "Not a valid FAT volume",
            [FR_MKFS_ABORTED] = "f_mkfs aborted",
            [FR_TIMEOUT] = "Unable to obtain grant for object",
            [FR_LOCKED] = "File locked",
            [FR_NOT_ENOUGH_CORE] = "File name is too large",
            [FR_TOO_MANY_OPEN_FILES] = "Too many open files",
            [FR_INVALID_PARAMETER] = "Invalid parameter",
    };
    if (fr < 0 || fr >= sizeof errs / sizeof errs[0])
        printf("%s: Invalid error\n", msg);
    else
        printf("%s: %s\n", msg, errs[fr]);
}

void mount(int argc, char *argv[])
{
    FATFS *fs = &fs_storage;
    if (fs->id != 0) {
        return;
    }
    f_mount(fs, "", 1);
}

void DACSetup(int sample_rate, int bitrate){
    end = 0;
    RCC -> AHBENR |= RCC_AHBENR_GPIOAEN;
    GPIOA -> MODER |= (3<<(2*4));

    RCC -> APB1ENR |= RCC_APB1ENR_TIM6EN;
    TIM6 -> PSC = 1 - 1;
    TIM6 -> ARR =  (48000000 / sample_rate) - 1;
    TIM6 -> CR2 |= TIM_CR2_MMS_1;
    //TIM6 -> DIER |= TIM_DIER_UDE;
    TIM6 -> CR1 |= TIM_CR1_ARPE;
    TIM6 -> CR1 |= TIM_CR1_CEN;

    RCC -> AHBENR |= RCC_AHBENR_DMAEN;
    DMA1_Channel3 -> CCR &= ~DMA_CCR_EN;
    if(bitrate == 8){
        DMA1_Channel3 -> CMAR = (uint32_t) &wavBuffer;
        DMA1_Channel3 -> CPAR = (uint32_t) (&(DAC -> DHR8R1));
        DMA1_Channel3 -> CCR &= ~DMA_CCR_MSIZE;
        DMA1_Channel3 -> CCR &= ~DMA_CCR_PSIZE;
        DMA1_Channel3 -> CNDTR = 8000;
    }else if(bitrate == 16){
        DMA1_Channel3 -> CMAR = (uint32_t) &wavBuffer;
        DMA1_Channel3 -> CPAR = (uint32_t) (&(DAC -> DHR12L1));
        DMA1_Channel3 -> CCR |= DMA_CCR_MSIZE_0;
        DMA1_Channel3 -> CCR |= DMA_CCR_PSIZE_0;
        DMA1_Channel3 -> CNDTR = 4000;
    }
    DMA1_Channel3 -> CCR |= DMA_CCR_DIR;
    DMA1_Channel3 -> CCR |= DMA_CCR_MINC;
    DMA1_Channel3 -> CCR |= DMA_CCR_CIRC;
    DMA1_Channel3 -> CCR |= DMA_CCR_HTIE;
    DMA1_Channel3 -> CCR |= DMA_CCR_TCIE;
    DMA1_Channel3 -> CCR |= DMA_CCR_EN;
    NVIC -> ISER[0] |= 1 << DMA1_Channel2_3_IRQn;

    RCC -> APB1ENR |= RCC_APB1ENR_DACEN;
    DAC -> CR &= ~DAC_CR_EN1;
    DAC -> CR &= ~DAC_CR_TSEL1_0 | ~DAC_CR_TSEL1_1 | ~DAC_CR_TSEL1_2;
    DAC -> CR |= DAC_CR_TEN1;
    DAC -> CR |= DAC_CR_DMAEN1;
    DAC -> CR |= DAC_CR_EN1;
}

char* title;
char* artist;

char* parse_title(char* filename){
    char* sub = ".";
    char* file;

    file = strtok(filename, sub);
    title = strtok(file, "-");
    artist = strtok(NULL, "");
    return file;
}

FSIZE_t pointer = 0;
head header;
int currentChoice = 1;
int curPlay;
int files = 0;
int page = 0;
int paused = 0;
int timeremaining;
char* filename;

void numFiles(){
    FRESULT res;
    DIR dir;
    static FILINFO fno;
    const char *path = "";

    res = f_opendir(&dir, path);
    for (;;) {
        res = f_readdir(&dir, &fno);                   /* Read a directory item */
        if (res != FR_OK || fno.fname[0] == 0) break;  /* Break on error or end of dir */
        files++;
    }
}

void pause(){
    if(paused == 0){
        DisableDMA();
        paused = 1;
    }else if(paused == 1){
        EnableDMA();
        paused = 0;
    }
}

void initplay(int choice){
    DIR dj;
    FILINFO fno;
    FRESULT fr;

    fr = f_findfirst(&dj, &fno, "",  "*.wav");
    if(fr != FR_OK){return;}

    int i = 1;

    while(i < choice){
        fr = f_findnext(&dj, &fno);
        if(fr != FR_OK || fno.fname[0] == 0){break;}
        i++;
    }
    in_subdir_songs_display();
    curPlay = choice;
    play(fno.fname);
}
void play(char* filename){
    FRESULT fr;
    UINT br;
    paused = 0;
    DisableDMA();
    if(f_open(&wavFile, filename, FA_READ) != FR_OK){
        return;
    }
    fr = f_read(&wavFile, &header, sizeof(head), &br);
    if(fr != FR_OK){
        return;
    }
    if(header.bits_per_sample == 8){
        fr = f_read(&wavFile, &wavBuffer, sizeof(wavBuffer), &br);
        if(fr != FR_OK){
            return;
        }
    }else if(header.bits_per_sample == 16){
        fr = f_read(&wavFile, &wavBuffer, sizeof(wavBuffer), &br);
        for(int i = 0; i < 8000; i++){
            wavBuffer[i] = wavBuffer[i] ^ 0x80;
        }
        if(fr != FR_OK){
            return;
        }
    }
    timeremaining = header.file_size - 40;
    DACSetup(header.sample_rate, header.bits_per_sample);
}

void DisableDMA(){
    DMA1_Channel3 -> CCR &= ~DMA_CCR_EN;
}

void EnableDMA(){
    DMA1_Channel3 -> CCR |= DMA_CCR_EN;
}

void read_first_half(){
    UINT bw;
    f_read(&wavFile, &wavBuffer, 4000, &bw);
    if(bw != 4000){
        end = 1;
    }
    if(header.bits_per_sample == 16){
        for(int i = 0; i < 4000; i++){
            wavBuffer[i] = wavBuffer[i] ^ 0x80;
        }
    }
}

void read_second_half(){
    UINT bw;
    f_read(&wavFile, &wavBuffer[4000], 4000, &bw);
    if(bw != 4000){
        end = 1;
    }
    if(header.bits_per_sample == 16){
        for(int i = 4000; i < 8000; i++){
            wavBuffer[i] = wavBuffer[i] ^ 0x80;
        }
    }
}

void progressbar(){
    FRESULT fr;
    DIR dj;
    FILINFO fno;

    int remain = (header.file_size - timeremaining) / (header.sample_rate * (header.bits_per_sample / 8));
    int totaltime = header.file_size / (header.sample_rate * (header.bits_per_sample / 8));
    int timeleftsec = remain % 60;
    int timeleftmin = remain / 60;
    int totaltimesec = totaltime % 60;
    int totaltimemin = totaltime / 60;

    char buffer1[1];
    char buffer2[2];
    char buffer3[1];
    char buffer4[2];
    itoa(timeleftmin, buffer1, 10);
    itoa(timeleftsec, buffer2, 10);
    itoa(totaltimemin, buffer3, 10);
    itoa(totaltimesec, buffer4, 10);

    if(15 + (((float)header.file_size - (float)timeremaining) / (float)header.file_size)*210 < 55){
        LCD_DrawFillRectangle(39,23,55,39,YELLOW);
    }

    LCD_DrawFillRectangle(15,20,15 + (((float)header.file_size - (float)timeremaining) / (float)header.file_size)*210,60,BLUE);
    if((timeleftsec/10) < 1){
        buffer2[1] = buffer2[0];
        buffer2[0] = '0';
    }
    if((totaltimesec/10) < 1){
        buffer4[1] = buffer4[0];
        buffer4[0] = '0';
    }

    LCD_DrawString(23,25,RED,RED,buffer1,16, 1);
    LCD_DrawString(31,25,RED,RED,":",16, 1);
    LCD_DrawString(39,25,RED,RED,buffer2,16, 1);
    LCD_DrawString(185,25,RED,RED,buffer3,16, 1);
    LCD_DrawString(193,25,RED,RED,":",16, 1);
    LCD_DrawString(201,25,RED,RED,buffer4,16, 1);

    fr = f_findfirst(&dj, &fno, "",  "*.wav");
    if(fr != FR_OK){return;}

    int i = 1;

    while(i < curPlay){
        fr = f_findnext(&dj, &fno);
        if(fr != FR_OK || fno.fname[0] == 0){break;}
        i++;
    }
    filename = parse_title(fno.fname);

    LCD_DrawString(18,0,RED,WHITE,title,16,1);
    LCD_DrawString(225-((strlen(artist))*8),0,RED,WHITE,artist,16,1);
}

void DMA1_CH2_3_DMA2_CH1_2_IRQHandler(void){
    if(DMA1 -> ISR & DMA_ISR_TCIF3){
        DMA1 -> IFCR |= DMA_IFCR_CTCIF3;
        read_second_half();
        timeremaining = timeremaining - 8000;
        progressbar();

    }else if(DMA1 -> ISR & DMA_ISR_HTIF3){
        DMA1 -> IFCR |= DMA_IFCR_CHTIF3;
        read_first_half();
    }
    if(end == 1){
        DisableDMA();
    }
}

void enableButtons(){
    numFiles();
    RCC -> AHBENR |= RCC_AHBENR_GPIOAEN;
    GPIOA -> MODER &= ~(3<<(2*8)) | ~(3<<(2*9)) | ~(3<<(2*10)) | ~(3<<(2*10));

    RCC -> APB2ENR |= RCC_APB2ENR_SYSCFGCOMPEN;
    SYSCFG -> EXTICR[3] &= ~0x0000ffff;
    EXTI -> RTSR |= EXTI_RTSR_TR8 | EXTI_RTSR_TR9 | EXTI_RTSR_TR10 | EXTI_RTSR_TR11;
    EXTI -> IMR |= EXTI_IMR_MR8 | EXTI_IMR_MR9 | EXTI_IMR_MR10 | EXTI_IMR_MR11;
    NVIC -> ISER[0] |= 1 << EXTI4_15_IRQn;
}

void EXTI4_15_IRQHandler(){\
    if((EXTI -> PR & EXTI_PR_PR8) == EXTI_PR_PR8){
        DisableDMA();
        EXTI -> PR |= EXTI_PR_PR8;
        if(currentChoice == 1){
            currentChoice = files - 1;
        }else{
            currentChoice--;
        }
        page = (currentChoice - 1) / 4;
        updateChoice();
        EnableDMA();
    }else if((EXTI -> PR & EXTI_PR_PR9) == EXTI_PR_PR9){
        DisableDMA();
        EXTI -> PR |= EXTI_PR_PR9;
        if(currentChoice == files - 1){
            currentChoice = 1;
        }else{
            currentChoice++;
        }
        page = (currentChoice - 1) / 4;
        updateChoice();
        EnableDMA();
    }else if((EXTI -> PR & EXTI_PR_PR10) == EXTI_PR_PR10){
        DisableDMA();
        EXTI -> PR |= EXTI_PR_PR10;
        initplay(currentChoice);
    }else if((EXTI -> PR & EXTI_PR_PR11) == EXTI_PR_PR11){
        EXTI -> PR |= EXTI_PR_PR11;
        pause();
    }
}

void getfile(int choice){
    DIR dj;
    FILINFO fno;
    FRESULT fr;

    fr = f_findfirst(&dj, &fno, "",  "*.wav");
    if(fr != FR_OK){return;}

    int i = 0;

    while(i < choice){
        fr = f_findnext(&dj, &fno);
        if(fr != FR_OK || fno.fname[0] == 0){break;}
        i++;
    }
    filename = parse_title(fno.fname);
    LCD_DrawString(20,85 + ((choice - (page*4)) * 60),BLACK,BLUE,title,16, 1);
    LCD_DrawString(20,110 + ((choice - (page*4)) * 60),BLACK,BLUE,artist,16, 1);
}

void in_subdir_songs_display(void){
    LCD_Clear(YELLOW);

    LCD_DrawFillRectangle(15,80,225,105,WHITE);
    LCD_DrawFillRectangle(15,105,225,130,WHITE);
    LCD_DrawRectangle(15,80,225,105,BLACK);
    LCD_DrawRectangle(15,105,225,130,BLACK);

    LCD_DrawFillRectangle(15,140,225,165,WHITE);
    LCD_DrawFillRectangle(15,165,225,190,WHITE);
    LCD_DrawRectangle(15,140,225,165,BLACK);
    LCD_DrawRectangle(15,165,225,190,BLACK);

    LCD_DrawFillRectangle(15,200,225,225,WHITE);
    LCD_DrawFillRectangle(15,225,225,250,WHITE);
    LCD_DrawRectangle(15,200,225,225,BLACK);
    LCD_DrawRectangle(15,225,225,250,BLACK);

    LCD_DrawFillRectangle(15,260,225,285,WHITE);
    LCD_DrawFillRectangle(15,285,225,310,WHITE);
    LCD_DrawRectangle(15,260,225,285,BLACK);
    LCD_DrawRectangle(15,285,225,310,BLACK);
    int num;
    num = (page*4) + 3;

    for(int j = 0 + (page * 4); j <= num; j++){
        getfile(j); //maximum of 23 characters
    }
}

void updateChoice(){
    in_subdir_songs_display();
    LCD_DrawFillRectangle(15,80 + (60 * (currentChoice - (page*4) - 1)),225,105 + (60 * (currentChoice - (page*4) - 1)),GRAY);
    LCD_DrawFillRectangle(15,105 + (60 * (currentChoice - (page*4) - 1)),225,130 + (60 * (currentChoice - (page*4) - 1)),GRAY);
    LCD_DrawRectangle(15,80 + (60 * (currentChoice - (page*4) - 1)),225,105 + (60 * (currentChoice - (page*4) - 1)),BLACK);
    LCD_DrawRectangle(15,105 + (60 * (currentChoice - (page*4) - 1)),225,130 + (60 * (currentChoice - (page*4) - 1)),BLACK);

    getfile(currentChoice - 1);
}
