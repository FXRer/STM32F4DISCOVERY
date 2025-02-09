#ifndef _HEADING_H
#define _HEADING_H

#include "global_inc.h"

#define ID_WINDOW_0 (GUI_ID_USER + 0xf0)
#define ID_IMAGE_0 (GUI_ID_USER + 0xf1)
#define ID_IMAGE_1 (GUI_ID_USER + 0xf2)
#define ID_IMAGE_2 (GUI_ID_USER + 0xf3)
#define ID_TEXT_0 (GUI_ID_USER + 0xf5)
#define ID_BUTTON_0 (GUI_ID_USER + 0xf6)
#define ID_IMAGE_0_IMAGE_0 0x00
#define ID_IMAGE_1_IMAGE_0 0x01
#define ID_IMAGE_2_IMAGE_0 0x02

static GUI_CONST_STORAGE GUI_COLOR ColorsBatteryEmpty_27x14[] = {
     0x0000FF,0xFFFFFF,0xD7A50F,0xF0D26C,0x000000
};

static GUI_CONST_STORAGE GUI_LOGPALETTE PalBatteryEmpty_27x14 = {
  5,	// number of entries
  1, 	// Has transparency
  &ColorsBatteryEmpty_27x14[0]
};

static GUI_CONST_STORAGE unsigned char acBatteryEmpty_27x14[] = {
  0x00, 0x04, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x00,
  0x00, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x40,
  0x00, 0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x40,
  0x00, 0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x40,
  0x44, 0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x40,
  0x44, 0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x40,
  0x44, 0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x40,
  0x44, 0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x40,
  0x44, 0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x40,
  0x44, 0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x40,
  0x00, 0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x40,
  0x00, 0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x40,
  0x00, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x40,
  0x00, 0x04, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x00
};

static GUI_CONST_STORAGE GUI_BITMAP _bmBatteryEmpty_27x14 = {
  27, // XSize
  14, // YSize
  14, // BytesPerLine
  4, // BitsPerPixel
  acBatteryEmpty_27x14,  // Pointer to picture data (indices)
  &PalBatteryEmpty_27x14   // Pointer to palette
};


WM_HWIN Createbar(void);
void Heading_Task( void * pvParameters);

#endif
