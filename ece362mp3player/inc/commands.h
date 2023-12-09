#ifndef __COMMANDS_H__
#define __COMMANDS_H__

struct commands_t {
    const char *cmd;
    void      (*fn)(int argc, char *argv[]);
};

//void command_shell(void);
void updateChoice();
void in_subdir_songs_display(void);
void getfile(int choice);
void EXTI4_15_IRQHandler();
void enableButtons();
void DMA1_CH2_3_DMA2_CH1_2_IRQHandler(void);
void progressbar();
void read_second_half();
void read_first_half();
void EnableDMA();
void DisableDMA();
void play(char* filename);
void initplay(int choice);
void pause();
void numFiles();
void DACSetup(int sample_rate, int bitrate);
void mount();
void time_write();

#endif /* __COMMANDS_H_ */
