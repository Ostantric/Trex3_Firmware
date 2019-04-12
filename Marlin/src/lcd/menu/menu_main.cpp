/**
 * Marlin 3D Printer Firmware
 * Copyright (C) 2016 MarlinFirmware [https://github.com/MarlinFirmware/Marlin]
 *
 * Based on Sprinter and grbl.
 * Copyright (C) 2011 Camiel Gubbels / Erik van der Zalm
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

//
// Main Menu
//

#include "../../inc/MarlinConfigPre.h"

#if HAS_LCD_MENU

#include "menu.h"
#include "../../module/temperature.h"
#include "../../gcode/queue.h"
#include "../../module/printcounter.h"
#include "../../module/stepper.h"

#if ENABLED(POWER_LOSS_RECOVERY)
  #include "../../feature/power_loss_recovery.h"
#endif

void lcd_pause() {
  #if ENABLED(POWER_LOSS_RECOVERY)
    if (recovery.enabled) recovery.save(true, false);
  #endif

  #if ENABLED(PARK_HEAD_ON_PAUSE)
    lcd_advanced_pause_show_message(ADVANCED_PAUSE_MESSAGE_INIT, ADVANCED_PAUSE_MODE_PAUSE_PRINT, active_extruder);
    enqueue_and_echo_commands_P(PSTR("M25 P; \n M24"));
  #elif ENABLED(SDSUPPORT)
    enqueue_and_echo_commands_P(PSTR("M25"));
  #elif defined(ACTION_ON_PAUSE)
    SERIAL_ECHOLNPGM("//action:" ACTION_ON_PAUSE);
  #endif
  planner.synchronize();
}

void lcd_resume() {
  #if ENABLED(SDSUPPORT)
    if (card.isPaused()) enqueue_and_echo_commands_P(PSTR("M24"));
  #elif defined(ACTION_ON_RESUME)
    SERIAL_ECHOLNPGM("//action:" ACTION_ON_RESUME);
  #endif
}

#if ENABLED(SDSUPPORT)

  #include "../../sd/cardreader.h"

  void lcd_sdcard_stop() {
    wait_for_heatup = wait_for_user = false;
    card.flag.abort_sd_printing = true;
    ui.set_status_P(PSTR(MSG_PRINT_ABORTED), -1);
    ui.return_to_status();
  }

  void menu_sdcard_abort_confirm() {
    START_MENU();
    MENU_ITEM(function, MSG_STOP_PRINT, lcd_sdcard_stop);
    MENU_BACK(MSG_MAIN);
    END_MENU();
  }

#endif // SDSUPPORT

#if ENABLED(DUAL_X_CARRIAGE)

  #include "../../module/motion.h"

  void _recalc_IDEX_settings() {
    if (active_extruder) {                      // For the 2nd extruder re-home so the next tool-change gets the new offsets.
      enqueue_and_echo_commands_P(PSTR("G28")); // In future, we can babystep the 2nd extruder (if active), making homing unnecessary.
      active_extruder = 0;
    }
  }

  void menu_IDEX() {
    START_MENU();
    MENU_BACK(MSG_MAIN);

    MENU_ITEM(gcode, MSG_IDEX_MODE_AUTOPARK,  PSTR("M605 S1\nG28 X\nG1 X100"));
    const bool need_g28 = !(TEST(axis_known_position, Y_AXIS) && TEST(axis_known_position, Z_AXIS));
    MENU_ITEM(gcode, MSG_IDEX_MODE_DUPLICATE, need_g28
      ? PSTR("M605 S1\nT0\nG28\nM605 S2 X200\nG28 X\nG1 X100")                // If Y or Z is not homed, do a full G28 first
      : PSTR("M605 S1\nT0\nM605 S2 X200\nG28 X\nG1 X100")
    );
    MENU_ITEM(gcode, MSG_IDEX_MODE_SCALED_COPY, need_g28
      ? PSTR("M605 S1\nT0\nG28\nM605 S2 X200\nG28 X\nG1 X100\nM605 S3 X200")  // If Y or Z is not homed, do a full G28 first
      : PSTR("M605 S1\nT0\nM605 S2 X200\nG28 X\nG1 X100\nM605 S3 X200")
    );
    MENU_ITEM(gcode, MSG_IDEX_MODE_FULL_CTRL, PSTR("M605 S0\nG28 X"));
    MENU_MULTIPLIER_ITEM_EDIT_CALLBACK(float52, MSG_IDEX_X_OFFSET , &hotend_offset[X_AXIS][1], MIN(X2_HOME_POS, X2_MAX_POS) - 25.0, MAX(X2_HOME_POS, X2_MAX_POS) + 25.0, _recalc_IDEX_settings);
    MENU_MULTIPLIER_ITEM_EDIT_CALLBACK(float52, MSG_IDEX_Y_OFFSET , &hotend_offset[Y_AXIS][1], -10.0, 10.0, _recalc_IDEX_settings);
    MENU_MULTIPLIER_ITEM_EDIT_CALLBACK(float52, MSG_IDEX_Z_OFFSET , &hotend_offset[Z_AXIS][1], -10.0, 10.0, _recalc_IDEX_settings);
    MENU_ITEM(gcode, MSG_IDEX_SAVE_OFFSETS, PSTR("M500"));
    END_MENU();
  }

#endif

#if ENABLED(SPINDLE_LASER_ENABLE) && DISABLED(SPINDLE_LASER_PWM)
void menu_LASER() {
    START_MENU();
    MENU_BACK(MSG_MAIN);
    MENU_ITEM(gcode, MSG_LASER_ON,  PSTR("M3"));
    MENU_ITEM(gcode, MSG_LASER_OFF, PSTR("M5"));
    END_MENU();
  }
#endif

void menu_tune();
void menu_motion();
void menu_temperature();
void menu_configuration();
void menu_user();
void menu_temp_e0_filament_change();
void menu_change_filament();
void menu_info();
void menu_led();

void menu_main() {
  START_MENU();
  MENU_BACK(MSG_WATCH);

  const bool busy = printer_busy();

  if (busy) {
    MENU_ITEM(function, MSG_PAUSE_PRINT, lcd_pause);
    #if ENABLED(SDSUPPORT)
      if (card.isFileOpen())
        MENU_ITEM(submenu, MSG_STOP_PRINT, menu_sdcard_abort_confirm);
    #endif
    MENU_ITEM(submenu, MSG_TUNE, menu_tune);
  }
  else {
    MENU_ITEM(function, MSG_RESUME_PRINT, lcd_resume);
    MENU_ITEM(submenu, MSG_MOTION, menu_motion);
    MENU_ITEM(submenu, MSG_TEMPERATURE, menu_temperature);
    #if ENABLED(DUAL_X_CARRIAGE)
      MENU_ITEM(submenu, MSG_IDEX_MENU, menu_IDEX);
    #endif
    #if ENABLED(SPINDLE_LASER_ENABLE) && DISABLED(SPINDLE_LASER_PWM)
      MENU_ITEM(submenu, MSG_LASER_MENU, menu_LASER);
    #endif
  }

  MENU_ITEM(submenu, MSG_CONFIGURATION, menu_configuration);

  #if ENABLED(CUSTOM_USER_MENUS)
    MENU_ITEM(submenu, MSG_USER_MENU, menu_user);
  #endif

  #if ENABLED(ADVANCED_PAUSE_FEATURE)
    #if E_STEPPERS == 1 && DISABLED(FILAMENT_LOAD_UNLOAD_GCODES)
      if (thermalManager.targetHotEnoughToExtrude(active_extruder))
        MENU_ITEM(gcode, MSG_FILAMENTCHANGE, PSTR("M600 B0"));
      else
        MENU_ITEM(submenu, MSG_FILAMENTCHANGE, menu_temp_e0_filament_change);
    #else
      MENU_ITEM(submenu, MSG_FILAMENTCHANGE, menu_change_filament);
    #endif
  #endif

  #if ENABLED(LCD_INFO_MENU)
    MENU_ITEM(submenu, MSG_INFO_MENU, menu_info);
  #endif

  #if ENABLED(LED_CONTROL_MENU)
    MENU_ITEM(submenu, MSG_LED_CONTROL, menu_led);
  #endif

  //
  // Switch power on/off
  //
  #if HAS_POWER_SWITCH
    if (powersupply_on)
      MENU_ITEM(gcode, MSG_SWITCH_PS_OFF, PSTR("M81"));
    else
      MENU_ITEM(gcode, MSG_SWITCH_PS_ON, PSTR("M80"));
  #endif

  //
  // Autostart
  //
  #if ENABLED(SDSUPPORT) && ENABLED(MENU_ADDAUTOSTART)
    if (!busy)
      MENU_ITEM(function, MSG_AUTOSTART, card.beginautostart);
  #endif

#if ENABLED(SDSUPPORT)
    if (card.isDetected()) {
      if(!card.isFileOpen()) {
      MENU_ITEM(submenu, MSG_CARD_MENU, menu_sdcard);
      #if !PIN_EXISTS(SD_DETECT)
        MENU_ITEM(gcode, MSG_CHANGE_SDCARD, PSTR("M21"));  // SD-card changed by user
      #endif
      }
    }
    else {
      MENU_ITEM(function, MSG_NO_CARD, NULL);
      #if !PIN_EXISTS(SD_DETECT)
        MENU_ITEM(gcode, MSG_INIT_SDCARD, PSTR("M21")); // Manually initialize the SD-card via user interface
      #endif
    }
  #endif
  END_MENU();
}

#endif // HAS_LCD_MENU
