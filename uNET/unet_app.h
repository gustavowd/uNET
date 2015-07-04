/**********************************************************************************
@file   unet_app.h
@brief  public APPs of UNET network
@authors: Gustavo Weber Denardin
          Carlos Henrique Barriquello

Copyright (c) <2009-2013> <Universidade Federal de Santa Maria>

  * Software License Agreement
  *
  * The Software is owned by the authors, and is protected under 
  * applicable copyright laws. All rights are reserved.
  *
  * The above copyright notice shall be included in
  * all copies or substantial portions of the Software.
  *  
  * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  * THE SOFTWARE. 
*********************************************************************************/

#ifndef UNET_APP_H
#define UNET_APP_H

/* This file has the public APPs of the UNET network */


enum APP_TASKS
{
  APP_01,
  APP_02,
  APP_03,
  APP_04,
  APP_05,
  APP_255,      
};

/*
enum destinos
{
  RELE,
  COORDENADOR,   
};
*/



#ifndef ON
#define ON  1
#endif

#ifndef OFF
#define OFF 0
#endif

#ifndef FAIL
#define FAIL (-1)
#endif



// UNET Profiles
#define  GENERAL_PROFILE         (INT8U)0x00
#define  LIGHTING_PROFILE        (INT8U)0x03
#define  MEASUREMENT_PROFILE     (INT8U)0x04
#define  SMART_ENERGY_PROFILE    (INT8U)0x07
#define  BULK_DATA_PROFILE       (INT8U)0x08


// General Profile Commands
#define GENERAL_ONOFF            (INT8U)0x00
#define SIMPLE_METERING          (INT8U)0x02
#define MULTIPLE_METERING        (INT8U)0x03
#define SNIFER_REQ               (INT8U)0x04
#define SNIFER_REP               (INT8U)0x05
#define DEBUG_PKT                (INT8U)0x06
#define RADIO_TXPL               (INT8U)0x07
#define RADIO_NEWPOS             (INT8U)0x08
#define UNET_INFO              	 (INT8U)0x09
#define RADIO_CHAN               (INT8U)0x0A
#define APP_CONFIG_PARAM         (INT8U)0x0B
#define CREATE_UP_PATH         	 (INT8U)0x0C



// General Attribute Defines
#define TEMPERATURE              (INT8U)0x84
#define FILE_S19                 (INT8U)0x89
#define DEBUG_COUNTER            (INT8U)0x8A
#define UNET_STAT                (INT8U)0x90
#define UNET_CONN                (INT8U)0x91
#define FAILURE_REPORT           (INT8U)0x99


// Lighting Profile Commands
#define SIMPLE_METERING          (INT8U)0x02
#define LIGHTING_DIMMING         (INT8U)0x03 
#define SMART_LAMP               (INT8U)0x30
 

// Lighting Attribute Defines
#define LIGHT_LEVEL              (INT8U)0x83 
#define LAMP_STATE               (INT8U)0x8B
#define SENSOR_STATE             (INT8U)0x8C  

 
// Smart Energy Commands
#define SIMPLE_METERING          (INT8U)0x02
#define MULTIPLE_METERING        (INT8U)0x03

// Smart Energy Attribute Defines
#define CURRENT                  (INT8U)0x80
#define VOLTAGE                  (INT8U)0x81
#define ENERGY                   (INT8U)0x82
#define ACTIVE_POWER             (INT8U)0x85
#define APPARENT_POWER           (INT8U)0x86
#define REACTIVE_POWER           (INT8U)0x87
#define POWER_FACTOR             (INT8U)0x88

#endif
