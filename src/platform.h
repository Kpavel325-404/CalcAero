#ifndef CALCLAB_PLATFORM_H
#define CALCLAB_PLATFORM_H
void cl_platform_overlay(void);
int cl_keyboard_start(const char *text);
int cl_keyboard_poll(char out[512]);
int cl_keyboard_active(void);
#endif
