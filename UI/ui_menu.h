/**
 * @file    ui_menu.h
 * @brief   OLED menu state machine.
 */

#ifndef __UI_MENU_H
#define __UI_MENU_H

#include "ui_types.h"
#include "bsp_key.h"

void UI_Menu_Init(void);
UiAction_e UI_Menu_HandleKey(KeyEvent_e event, UiStatus_t *status);
void UI_Menu_Render(const UiStatus_t *status);
UiPage_e UI_Menu_GetPage(void);

#endif /* __UI_MENU_H */
