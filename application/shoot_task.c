/****************************************************************************
 *  Copyright (C) 2019 RoboMaster.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of 
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <http://www.gnu.org/licenses/>.
 ***************************************************************************/

#include "sys.h"
#include "shoot.h"
#include "dbus.h"
#include "shoot_task.h"

int32_t shoot_firction_toggle(shoot_t pshoot, uint8_t toggled);
int32_t shoot_lid_toggle(shoot_t pshoot, uint8_t toggled);

/**Edited by Y.H. Liu
 * @Jun 13, 2019: change the FSM for shooting
 * 
 * Implement the control logic described in Control.md
 */
enum mouse_cmd{non, click, press};
typedef enum mouse_cmd mouse_cmd_e;
mouse_cmd_e mouse_shoot_control(rc_device_t rc_dev);

void shoot_task(void const *argument)
{
  uint32_t period = osKernelSysTick();
  rc_device_t prc_dev = NULL;

  shoot_t pshoot = NULL;
  pshoot = shoot_find("shoot");
  prc_dev = rc_device_find("can_rc");

  if (prc_dev == NULL)
  {
  }

  uint32_t shoot_time;

  static uint8_t fric_on = 0; //0x00 for off, 0xFF for on
  static uint8_t lid_open = 0; //0x00 for closed, 0xFF for opened
  while (1)
  {
    if (rc_device_get_state(prc_dev, RC_S1_MID2UP) == RM_OK)
    {
      shoot_firction_toggle(pshoot, fric_on);
      fric_on = ~fric_on;
    }

    if (rc_device_get_state(prc_dev, RC_S1_MID2DOWN) == RM_OK)
    {
      // shoot_set_cmd(pshoot, SHOOT_ONCE_CMD, 1);
      // shoot_time = get_time_ms();
      shoot_lid_toggle(pshoot, lid_open);
      lid_open = ~lid_open;
    }

    /*------ implement the kebboard controlling over shooting ------*/
    if(prc_dev->rc_info.kb.bit.Z ) 
    {
      if(prc_dev->rc_info.kb.bit.F) //turn off the fric regardless the situation
      {
        shoot_firction_toggle(pshoot,1); //assume that currently the fric is on
        fric_on &= ~fric_on; //set fric_on to be 0x00 i.e. off
      }
      if(prc_dev->rc_info.kb.bit.R) //close the lid regardless the situation
      {
        shoot_lid_toggle(pshoot, 1); //same logic as above
        lid_open &= ~lid_open;
      }
    }
    else
    {
      if(prc_dev->rc_info.kb.bit.F) //turn on the fric regardless the situation
      {
        shoot_firction_toggle(pshoot,0); //assume that currently the fric is off
        fric_on |= ~fric_on; //set fric_on to be 0xFF i.e. on
      }
      if(prc_dev->rc_info.kb.bit.R) //open the lid regardless the situation
      {
        shoot_lid_toggle(pshoot, 0); //same logic as above
        lid_open |= ~lid_open;
      }
    }
    
    /*------ implement the function of a trigger ------*/
    if (rc_device_get_state(prc_dev, RC_S2_DOWN) != RM_OK && !fric_on) //not in disabled mode
    {
      if (rc_device_get_state(prc_dev, RC_WHEEL_DOWN) == RM_OK
        || mouse_shoot_control(prc_dev)==press)
      {
        // if (get_time_ms() - shoot_time > 2500)
        // {
        //   shoot_set_cmd(pshoot, SHOOT_CONTINUOUS_CMD, 0);
        // }
        shoot_set_cmd(pshoot, SHOOT_CONTINUOUS_CMD, CONTIN_BULLET_NUM);
      }
      else if (rc_device_get_state(prc_dev, RC_WHEEL_UP) == RM_OK
            || mouse_shoot_control(prc_dev)==click)
      {
        shoot_set_cmd(pshoot, SHOOT_ONCE_CMD, 1);
      }
      else
      {
        shoot_set_cmd(pshoot, SHOOT_STOP_CMD, 0);
      }
      
    }

    shoot_execute(pshoot);
    osDelayUntil(&period, 5);
  }
}

/**Modified by Y.H. Liu
 * @Jun 13, 2019: to support the control by keyboards
 * 
 * Switch on/off the friction wheel
 * @param toggled 0----not turned on yet, so turn the devices on
 *                1----turned on already, so turn off them now
 */
int32_t shoot_firction_toggle(shoot_t pshoot, uint8_t toggled)
{
  if (toggled)
  {
    shoot_set_fric_speed(pshoot, 1000, 1000);
    turn_off_laser();
  }
  else
  {
    shoot_set_fric_speed(pshoot, 1250, 1250);
    turn_on_laser();
  }
  return 0;
}

/**Added by Y.H. Liu
 * @Jun 13, 2019: Declare the funtion, while leaving the defination empty
 * 
 * Send signals to the servo via interfaces to open/close the magazine lid
 * @param toggled 0----not turned on yet, so turn the devices on
 *                1----turned on already, so turn off them now
 */
int32_t shoot_lid_toggle(shoot_t pshoot, uint8_t toggled)
{
  if(toggled)
  {
    close_lid();
  }
  else
  {
    open_lid();
  }
  
}

/**Added by Y.H. Liu
 * @Jun 13, 2019: Define the function
 * 
 * Detect the left key of mouse to instruct the shoot
 */
mouse_cmd_e mouse_shoot_control(rc_device_t rc_dev)
{
  static mouse_cmd_e ret_val = non;
  static int32_t pressed_count = 0;
  uint8_t lKey = rc_dev->rc_info.mouse.l;

  if(lKey)
  {
    switch(ret_val)
    {
      case non:
        ret_val = click;
        pressed_count += 1;
        break;
      case click:
        pressed_count += 1;
        ret_val = pressed_count>CONTINUE_SHOOT_TH ? press : click;
        break;
      default:
        break;
    }
  }
  else
  {
    pressed_count = 0;
    ret_val = non;
  }
  return ret_val;
} 
