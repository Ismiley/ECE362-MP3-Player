#include "stm32f0xx.h"
#include <stdint.h>
#include <stdio.h>
#include "lcd.h"
#include <string.h>
#include "commands.h"
#include "ff.h"
#include "ffconf.h"


void init_spi1_slow(void){
    RCC->AHBENR |= RCC_AHBENR_GPIOAEN;
    RCC->AHBENR |= RCC_AHBENR_GPIOBEN;
    RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;

    //Set PA5, PA7 alternate functions
    GPIOA->MODER &= ~(3<<(2*5)); //SCK
    GPIOA->MODER &= ~(3<<(2*6)); //MISO
    GPIOA->MODER &= ~(3<<(2*7)); //MOSI

    GPIOA->MODER |= (2<<(2*5));
    GPIOA->MODER |= (2<<(2*6));
    GPIOA->MODER |= (2<<(2*7));

    GPIOA->AFR[0] &= ~(0xfff00000); //PA5, PA6, PA7 to AF0
    GPIOA->AFR[0] |= 0x00000000;

    //disable SPI1
    SPI1->CR1 &= ~SPI_CR1_SPE;

    /*//Set baud rate divisor to Max
    SPI1->CR1 |= SPI_CR1_BR_0;
    SPI1->CR1 |= SPI_CR1_BR_1;
    SPI1->CR1 |= SPI_CR1_BR_2;

    SPI1->CR1 |= SPI_CR1_BR_0;
    SPI1->CR1 &= ~SPI_CR1_BR_1;
    SPI1->CR1 &= ~SPI_CR1_BR_2;*/


    //Set 8-bit Word Size
    SPI1->CR2 |= SPI_CR2_DS_0;
    SPI1->CR2 |= SPI_CR2_DS_1;
    SPI1->CR2 |= SPI_CR2_DS_2;
    SPI1->CR2 &= ~SPI_CR2_DS_3;

    //Config Software Slave Management & Internal Slave Select !! DO WE NEED?
    SPI1->CR1 |= SPI_CR1_SSM;
    SPI1->CR1 |= SPI_CR1_SSI;
    SPI1->CR2 &= ~SPI_CR2_SSOE;


    //Set FIFO reception threshold 8-bit
    SPI1->CR2 |= SPI_CR2_FRXTH;

    //Set Master Mode
    SPI1->CR1 |= SPI_CR1_MSTR;


    //Enable SPI
    SPI1->CR1 |= SPI_CR1_SPE;
}

void sdcard_io_high_speed(){
    SPI1 -> CR1 &= ~SPI_CR1_SPE;
    SPI1->CR1 |= SPI_CR1_BR_0;
    SPI1->CR1 &= ~SPI_CR1_BR_1;
    SPI1->CR1 &= ~SPI_CR1_BR_2;
    SPI1 -> CR1 |= SPI_CR1_SPE;
}

void init_lcd_spi(void){
    //Config PA3, PA2, PA15 to output
    init_spi1_slow();

    GPIOA->MODER &= ~(3<<(2*3));
    GPIOA->MODER &= ~(3<<(2*2));
    GPIOA->MODER &= ~(3<<(2*15));
    GPIOA->MODER |= (1<<(2*3));
    GPIOA->MODER |= (1<<(2*2));
    GPIOA->MODER |= (1<<(2*15));

    sdcard_io_high_speed();
}

static inline void nano_wait(unsigned int n) {
    asm(    "        mov r0,%0\n"
            "repeat: sub r0,#83\n"
            "        bgt repeat\n" : : "r"(n) : "r0", "cc");
}

#define LCDSCROLL
#if defined(LCDSCROLL)
// lineNum:
/*
 * header = 0
 * song1 = 1, artist1 = 2
 * song2 = 3, artist2 = 4
 * song3 = 5, aritst3 = 6
 * song4 = 7, artist4 = 8
 */
void scrollHeader(const char * bigString, int lineNum){
    char temp[24];
    temp[24] = '\0';

    if(lineNum == 0){
        for(int i = 0; i<65-23; i++){
            memcpy(temp, &bigString[i], 23);
            LCD_DrawString(20,35,BLACK,BLUE,temp,16, 1);
            nano_wait(100000000);
            LCD_DrawFillRectangle(15,20,225,60,WHITE);
            LCD_DrawRectangle(15,20,225,60,BLACK);
        }
    }
    else if(lineNum == 1){
        for(int i = 0; i<65-23; i++){
            memcpy(temp, &bigString, 23);
            LCD_DrawString(20,85,BLACK,BLUE,temp,16, 1); //song1 name
            nano_wait(100000000);
            LCD_DrawFillRectangle(15,80,225,105,WHITE); //song1
            LCD_DrawRectangle(15,80,225,105,BLACK);
        }
    }
    else if(lineNum == 2){
            for(int i = 0; i<65-23; i++){
                memcpy(temp, &bigString[i], 23);
                LCD_DrawString(20, 110, BLACK, BLUE, temp, 16, 1); //artist1 name
                nano_wait(100000000);
                LCD_DrawFillRectangle(15,105,225,130,WHITE); //artist1
                LCD_DrawRectangle(15,105,225,130,BLACK);
            }
        }
    else if(lineNum == 3){
                for(int i = 0; i<65-23; i++){
                    memcpy(temp, &bigString[i], 23);
                    LCD_DrawString(20, 145, BLACK, BLUE, temp, 16, 1); //song2 name
                    nano_wait(100000000);
                    LCD_DrawFillRectangle(15,140,225,165,WHITE); //song2
                    LCD_DrawRectangle(15,140,225,165,BLACK);
                }
            }
    else if(lineNum == 4){
                    for(int i = 0; i<65-23; i++){
                        memcpy(temp, &bigString[i], 23);
                        LCD_DrawString(20, 170, BLACK, BLUE, temp, 16, 1); //artist2 name
                        nano_wait(100000000);
                        LCD_DrawFillRectangle(15,165,225,190,WHITE); //artist2
                        LCD_DrawRectangle(15,165,225,190,BLACK);
                    }
                }
    else if(lineNum == 5){
                        for(int i = 0; i<65-23; i++){
                            memcpy(temp, &bigString[i], 23);
                            LCD_DrawString(20, 205, BLACK, BLUE, temp, 16, 1); //song3 name
                            nano_wait(100000000);
                            LCD_DrawFillRectangle(15,200,225,225,WHITE); //song 3
                            LCD_DrawRectangle(15,200,225,225,BLACK);
                        }
                    }
    else if(lineNum == 6){
                            for(int i = 0; i<65-23; i++){
                                memcpy(temp, &bigString[i], 23);
                                LCD_DrawString(20, 230, BLACK, BLUE, temp, 16, 1); //artist3 name
                                nano_wait(100000000);
                                LCD_DrawFillRectangle(15,225,225,250,WHITE); //artist 3
                                LCD_DrawRectangle(15,225,225,250,BLACK);
                            }
                        }
    else if(lineNum == 7){
                                for(int i = 0; i<65-23; i++){
                                    memcpy(temp, &bigString[i], 23);
                                    LCD_DrawString(20, 265, BLACK, BLUE, temp, 16, 1); //song4 name
                                    nano_wait(100000000);
                                    LCD_DrawFillRectangle(15,260,225,285,WHITE); //song 4
                                    LCD_DrawRectangle(15,260,225,285,BLACK);
                                }
                            }

    else if(lineNum == 8){
                                    for(int i = 0; i<65-23; i++){
                                        memcpy(temp, &bigString[i], 23);
                                        LCD_DrawString(20, 290, BLACK, BLUE, temp, 16, 1); //artist 4 name
                                        nano_wait(100000000);
                                        LCD_DrawFillRectangle(15,285,225,310,WHITE); //artist 4
                                        LCD_DrawRectangle(15,285,225,310,BLACK);
                                    }
                                }

}



/*void in_subdir_songs_display(void){
    LCD_Clear(YELLOW);

    LCD_DrawFillRectangle(15,20,225,60,WHITE);
    LCD_DrawRectangle(15,20,225,60,BLACK);
    int i = 1;
    FRESULT res;
    DIR dj;
    FILINFO fno;

    res = f_findfirst(&dj, &fno, "",  "*.wav");
    for(int j = 1; j <= i; j++){
        getfile(i); //maximum of 23 characters
    }
    //const char *q = subdirStrP;

    //scrollHeader(q, 0);


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

    //test scrolling
/*
    scrollHeader(subdirStrP, 0);
    scrollHeader(subdirStrP, 1);
    scrollHeader(subdirStrP, 2);
    scrollHeader(subdirStrP, 3);
    scrollHeader(subdirStrP, 4);
    scrollHeader(subdirStrP, 5);
    scrollHeader(subdirStrP, 6);
    scrollHeader(subdirStrP, 7);
    scrollHeader(subdirStrP, 8);

}*/


/*int main(void)
{
    LCD_Setup();

    in_subdir_songs_display();
}*/
#endif

#define USART
#if defined(USART)

#include <stdio.h>
#include "../../groupproject/inc/fifo.h"
#include "../../groupproject/inc/tty.h"

#define FIFOSIZE 16
char serfifo[FIFOSIZE];
int seroffset = 0;

int main() {
    LCD_Setup();
    mount();
    enableButtons();
    in_subdir_songs_display();
    updateChoice();
}

void enable_sdcard(){
    GPIOB -> BRR |= (1<<(1*2));
}
void disable_sdcard(){
    GPIOB -> BSRR |= (1<<(1*2));
}
void init_sdcard_io(){
    init_spi1_slow();
    GPIOB -> MODER &= ~(3<<(2*2));
    GPIOB -> MODER |= (1<<(2*2));
    disable_sdcard();
}

#endif

