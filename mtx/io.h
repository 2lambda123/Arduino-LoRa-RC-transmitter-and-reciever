#ifndef _IO_H_
#define _IO_H_

void readSwitchesAndButtons();
void determineButtonEvent();
void readSticks();
void computeChannelOutputs();

int calcRateExpo(int _input, int _rate, int _expo);
int linearInterpolate(int xValues[], int yValues[], uint8_t numValues, int pointX);

#endif
