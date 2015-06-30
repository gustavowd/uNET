/***********************************************************************************
@file   app.c
@brief  functions to send and receive data in/from the network
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
**********************************************************************************/


#include "hardware.h"
#include "BRTOS.h"
#include "xhw_types.h"
#include "xgpio.h"

/* Config. files */
#include "BRTOSConfig.h"
#include "BoardConfig.h"
#include "AppConfig.h"

#include "unet_api.h"
#include "app.h" 


#if (BOOTLOADER_ENABLE == 1)
  #include "MCF51_Bootloader.h"    
#endif

#if PROCESSOR == COLDFIRE_V1
#pragma warn_implicitconv off
#pragma warn_unusedarg off
#endif

/********/

INT8U    Config_LUX_THRESHOLD_MIN = 0;
INT8U    Config_LUX_THRESHOLD_MAX = 0;
INT8U    Config_PARENT_THRESHOLD = 0;
INT8U    Config_PARENT_THRESHOLD_MIN = 0;
INT8U    Config_REPORT_PERIOD_1000MS = 30;
INT8U    Config_REPORT_JITTER_100MS = 1;

INT8U NetGeneralONOFF(INT8U state, INT8U direction, INT16U destiny)
{
  INT8U j = 0;
  INT8U status = 0;

  acquireRadio();

  NWKPayload[j++] = APP_01;
  NWKPayload[j++] = GENERAL_PROFILE;
  NWKPayload[j++] = GENERAL_ONOFF;
  NWKPayload[j++] = state;

  if (direction == DOWN_ROUTE){
	  status = DownRoute(START_ROUTE,(INT8U)(j));
  }else{
	  #if (USE_REACTIVE_UP_ROUTE == 1)
	  status = ReactiveUpRoute(START_ROUTE,(INT8U)(j),destiny);
	  #else
	  status = NO_ROUTE_AVAILABLE;
	  #endif
  }

  releaseRadio();

  return status;
}




/**
* @fn     Decode_General_Profile
* @brief  function to decode messages for General profile
**/
void Decode_General_Profile(void)
{
    switch(app_packet.APP_Command)
    {
      /* message to turn on/off */
      case GENERAL_ONOFF:

        if (app_packet.APP_Command_Attribute == ON)
        {              
           //SensorLight_Disable();
        	GPIOPinReset(GPIOB_BASE, GPIO_PIN_18);
        }
        if(app_packet.APP_Command_Attribute == OFF)
        {          
            //SensorLight_Enable();
        	GPIOPinSet(GPIOB_BASE, GPIO_PIN_18);
        }  
        break;
        
      case SNIFER_REQ:
	#if 0
        for(counter=0;counter<4;counter++)
        {
          lat.bytes[counter] = nwk_packet.NWK_Src_Lat[counter];
        }
        
        for(counter=0;counter<4;counter++)
        {
          lon.bytes[counter] = nwk_packet.NWK_Src_Long[counter];
        }        
        
        SniferRep(UP_ROUTE,macAddr, &lat, &lon);
	#endif
        
        break;        
      
      
      case DEBUG_PKT:
        //Process_Debug_Packet();
        break; 
      
      case RADIO_TXPL:
        /* set TX output power */           
        SetTxPower(app_packet.APP_Command_Attribute & 0xF8);         

        
#if (defined TEST_PIN && TEST_PIN == 1)        
        PIN_RELAY = ON;
        DelayTask(500);
        PIN_RELAY = OFF;
#endif        
        
      break;
      
      case APP_CONFIG_PARAM:
        switch (app_packet.APP_Command_Attribute){
          case LUX_THRESHOLD_MIN:
          Config_LUX_THRESHOLD_MIN = app_packet.APP_Payload[0];
          break;
          case LUX_THRESHOLD_MAX:
          Config_LUX_THRESHOLD_MAX = app_packet.APP_Payload[0];
          break;
          case PARENT_THRESHOLD:
          Config_PARENT_THRESHOLD = app_packet.APP_Payload[0];
          break;
          case PARENT_THRESHOLD_MIN:
          Config_PARENT_THRESHOLD_MIN = app_packet.APP_Payload[0];
          break;
          case REPORT_PERIOD_1000MS:
          Config_REPORT_PERIOD_1000MS = app_packet.APP_Payload[0];
          break;
          case REPORT_JITTER_100MS:
          Config_REPORT_JITTER_100MS = app_packet.APP_Payload[0];
          break;          
          default:
          break;
        }
      
      break;
                    
      default:
        break;
    }
}

/**
* @fn     Decode_Lighting_Profile
* @brief  function to decode messages for Lighting profile
**/
void Decode_Lighting_Profile(void)
{
#ifdef SET_LIGHT_LEVEL
    INT8U level = 0;
#endif
       
    switch(app_packet.APP_Command)
    {
      case LIGHTING_DIMMING:
		#ifdef SET_LIGHT_LEVEL
        level = app_packet.APP_Command_Attribute;
        SET_LIGHT_LEVEL();
        #endif        
                                
        break;
      
      default:
        break;
    }
}


/**
* @fn     Decode_SmartEnergy_Profile
* @brief  function to decode messages for Smart Energy profile
**/

void Decode_SmartEnergy_Profile(void)
{   
    INT8U Attribute = 0;
    switch(app_packet.APP_Command)
    {
      case SIMPLE_METERING:
        Attribute = app_packet.APP_Command_Attribute;
        
        switch(Attribute)
        {
          case CURRENT:
            break;
            
          case VOLTAGE:
            break;
            
          case ENERGY:
            break;
            
          default:
            break;
        }
        
        break;
      
      default:
        break;
    }
}

void SniferRep(INT8U direction, INT16U atributte, INT16U destiny)
{
  INT8U i = 0;
  INT8U j = 0;
  INT8U status = 0;
  INT8U cnt = 0;
                                                                    
  NWKPayload[j++] = APP_01;
  NWKPayload[j++] = GENERAL_PROFILE;
  NWKPayload[j++] = SNIFER_REP; 
  
  //conta quantos vizinhos tem
  cnt = 0; 
  for(i=0;i<NEIGHBOURHOOD_SIZE;i++) 
  {
    if (unet_neighbourhood[i].Addr_16b != 0xFFFE)
    {
      cnt++;  
    }
  } 
  
  NWKPayload[j++] = cnt; 
    
  for(i=0;i<(NEIGHBOURHOOD_SIZE-1);i++) 
  {
    if (unet_neighbourhood[i].Addr_16b != 0xFFFE)
    {                      
      NWKPayload[j++] = (INT8U)(unet_neighbourhood[i].Addr_16b >> 8);
      NWKPayload[j++] = (INT8U)(unet_neighbourhood[i].Addr_16b & 0xFF);
      NWKPayload[j++] = unet_neighbourhood[i].NeighborRSSI;
      NWKPayload[j++] = unet_neighbourhood[i].NeighborStatus.bits.Symmetric;
    }
  }
  
  NWKPayload[j++] = (INT8U)(atributte >> 8);
  NWKPayload[j++] = (INT8U)(atributte & 0xFF);
  NWKPayload[j++] = (INT8U) CHANNEL_INIT_VALUE;
  NWKPayload[j++] = (INT8U) GetTxPower();
  
 
  if (direction == DOWN_ROUTE)
    status = DownRoute(START_ROUTE,(INT8U)(j));
  else
    status = ReactiveUpRoute(START_ROUTE,(INT8U)(j), destiny);

  (void)status;
}


/**
* @fn     NetMultiMeasureSE
* @brief  function to send message for Smart Energy profile
**/
#if 0
INT8U NetMultiMeasureSE(SE_STRUCT *se)
{

  INT8U i = 0;
  INT8U j = 0;  
  INT8U count = 0; 
  INT8U status = 0;
  
  acquireRadio();
  
  NWKPayload[j++] = APP_01;
  NWKPayload[j++] = SMART_ENERGY_PROFILE;
  NWKPayload[j++] = MULTIPLE_METERING;   
  
  //"se" é um ponteiro que contem o endereço da estrutura SE_ESTRUCTURE          
  // Descobre o número de medidas a serem enviadas
  for(i=0;i<8;i++)
  {
    if ((se->params.Byte >> i) && 1)
    {
      count++;
    }
  }
  
  NWKPayload[j++] = count;
  
  //verifica se o bit voltage da variavel params é 1. Se for envia.  
  if(se->params.Bits.Voltage)      
  {
      //NWKPayload[j++] = VOLTAGE;
      NWKPayload[j++] = (INT8U)(se->v_rms >> 8);
      NWKPayload[j++] = (INT8U)(se->v_rms & 0xFF);             
  }
  //verifica se o bit current da variavel params é 1. Se for envia     
  if(se->params.Bits.Current)   
  {
      //NWKPayload[j++] = CURRENT;
      NWKPayload[j++] = (INT8U)(se->i_rms >> 8);
      NWKPayload[j++] = (INT8U)(se->i_rms & 0xFF);             
  }
  
  
  if(se->params.Bits.Power_Factor)      
  {
      //NWKPayload[j++] = POWER_FACTOR;
      NWKPayload[j++] = (INT8U)(se->power_factor >> 8);
      NWKPayload[j++] = (INT8U)(se->power_factor & 0xFF);            
  }
  
  
  if(se->params.Bits.Apparent_Power)
  {
      //NWKPayload[j++] = APPARENT_POWER;
      NWKPayload[j++] = (INT8U)(se->power_S >> 24);
      NWKPayload[j++] = (INT8U)((se->power_S & 0xFFFFFF) >> 16);
      NWKPayload[j++] = (INT8U)((se->power_S & 0xFFFF) >> 8);
      NWKPayload[j++] = (INT8U)(se->power_S & 0xFF);            
  }
  
  
  if(se->params.Bits.Reactive_Power)      
  {
      //NWKPayload[j++] = REACTIVE_POWER;
      NWKPayload[j++] = (INT8U)(se->power_Q >> 24);
      NWKPayload[j++] = (INT8U)((se->power_Q & 0xFFFFFF) >> 16);
      NWKPayload[j++] = (INT8U)((se->power_Q & 0xFFFF) >> 8);
      NWKPayload[j++] = (INT8U)(se->power_Q & 0xFF);        
  }
      
  
  if(se->params.Bits.Active_Power)      
  {
      //NWKPayload[j++] = ACTIVE_POWER;
      NWKPayload[j++] = (INT8U)(se->power_P >> 24);
      NWKPayload[j++] = (INT8U)((se->power_P & 0xFFFFFF) >> 16);
      NWKPayload[j++] = (INT8U)((se->power_P & 0xFFFF) >> 8);
      NWKPayload[j++] = (INT8U)(se->power_P & 0xFF);           
  }
  
  
  if(se->params.Bits.Energy)
  {
      //NWKPayload[j++] = ENERGY;
      NWKPayload[j++] = (INT8U)(se->energy_meter >> 24);
      NWKPayload[j++] = (INT8U)((se->energy_meter & 0xFFFFFF) >> 16);
      NWKPayload[j++] = (INT8U)((se->energy_meter & 0xFFFF) >> 8);
      NWKPayload[j++] = (INT8U)(se->energy_meter & 0xFF);           
  }  
  
  
  status = DownRoute(START_ROUTE,(INT8U)(j));  
  
  releaseRadio(); 
  
  return status;
}
#endif


/**
* @fn     NetSimpleMeasureSE
* @brief  function to send a message for Smart Energy profile
**/

INT8U NetSimpleMeasureSE(INT8U MeasureType, INT16U Value16, INT32U Value32)
{
  INT8U j = 0;
  INT8U status = 0;  
  
  acquireRadio();
  
          
  NWKPayload[0] = APP_01;
  NWKPayload[1] = SMART_ENERGY_PROFILE;
  NWKPayload[2] = MULTIPLE_METERING;
  NWKPayload[3] = MeasureType;
  
  
  switch(MeasureType)
  {
    case CURRENT:
      NWKPayload[4] = (INT8U)(Value16 >> 8);
      NWKPayload[5] = (INT8U)(Value16 & 0xFF);
      j = 2;
      break;
    case VOLTAGE:
      NWKPayload[4] = (INT8U)(Value16 >> 8);
      NWKPayload[5] = (INT8U)(Value16 & 0xFF);            
      j = 2;
      break;
    case ENERGY:
      NWKPayload[4] = (INT8U)(Value32 >> 24);
      NWKPayload[5] = (INT8U)((Value32 & 0xFFFFFF) >> 16);
      NWKPayload[6] = (INT8U)((Value32 & 0xFFFF) >> 8);
      NWKPayload[7] = (INT8U)(Value32 & 0xFF);
      j = 4;
      break;
    case ACTIVE_POWER:
      NWKPayload[4] = (INT8U)(Value32 >> 24);
      NWKPayload[5] = (INT8U)((Value32 & 0xFFFFFF) >> 16);
      NWKPayload[6] = (INT8U)((Value32 & 0xFFFF) >> 8);
      NWKPayload[7] = (INT8U)(Value32 & 0xFF);
      j = 4;
      break;
    case REACTIVE_POWER:
      NWKPayload[4] = (INT8U)(Value32 >> 24);
      NWKPayload[5] = (INT8U)((Value32 & 0xFFFFFF) >> 16);
      NWKPayload[6] = (INT8U)((Value32 & 0xFFFF) >> 8);
      NWKPayload[7] = (INT8U)(Value32 & 0xFF);
      j = 4;
      break;
    case APPARENT_POWER:
      NWKPayload[4] = (INT8U)(Value32 >> 24);
      NWKPayload[5] = (INT8U)((Value32 & 0xFFFFFF) >> 16);
      NWKPayload[6] = (INT8U)((Value32 & 0xFFFF) >> 8);
      NWKPayload[7] = (INT8U)(Value32 & 0xFF);
      j = 4;
      break;
    case POWER_FACTOR:
      NWKPayload[4] = (INT8U)(Value16 >> 8);
      NWKPayload[5] = (INT8U)(Value16 & 0xFF);            
      j = 2;
      break;
    
  }
  
  
  status = DownRoute(START_ROUTE,(INT8U)(4+j));   
  releaseRadio();
  
  return status;
}




/**
* @fn     NetGeneralInfo
* @brief  function to send message for General profile
**/

INT8U NetGeneralInfo(INT8U MeasureType, INT8U Value8,INT16U Value16)
{
  INT8U j = 0;
  INT8U status = 0;
  
  acquireRadio();

            
  NWKPayload[0] = APP_01;
  NWKPayload[1] = GENERAL_PROFILE;
  NWKPayload[2] = SIMPLE_METERING;
  NWKPayload[3] = MeasureType;
  
  
  switch(MeasureType)
  {
    case TEMPERATURE:
    case FAILURE_REPORT:
      NWKPayload[4] = (INT8U)Value8;
      j = 1;
      break;
    default:
      NWKPayload[4] = (INT8U)((Value16 & 0xFFFF) >> 8);
      NWKPayload[5] = (INT8U)(Value16 & 0xFF);
      j = 2;
      break;      
  }
  

  status = DownRoute(START_ROUTE,(INT8U)(4+j));
  
  releaseRadio();
  
  return status;
}


/**
* @fn     NetLightingProfile
* @brief  function to send message for Lighting profile
**/

INT8U NetLightingProfile(INT8U Command, INT8U Parameter, INT8U Value8, INT16U Value16)
{
  INT8U j = 0;
  INT8U status = 0;
  
  acquireRadio();
  
  NWKPayload[0] = APP_01;
  NWKPayload[1] = LIGHTING_PROFILE;
  NWKPayload[2] = Command;
  NWKPayload[3] = Parameter;
  
  
  switch(Parameter)
  {
    case LIGHT_LEVEL:
      NWKPayload[4] = (INT8U)(Value16 >> 8);
      NWKPayload[5] = (INT8U)(Value16 & 0xFF);
      j = 2;
      break;  
    case LAMP_STATE:
      NWKPayload[4] = (INT8U)Value8;
      j = 1;
      break;    
    case SENSOR_STATE:
      NWKPayload[4] = (INT8U)Value8;
      j = 1;
      break;                
  }
  

  status = DownRoute(START_ROUTE,(INT8U)(4+j));  
  
  releaseRadio();
  
  return status;
}


/**
* @fn     NetDebugPacket
* @brief  function to send message for Debug profile
**/
INT8U NetDebugPacket(INT8U Command, INT8U Parameter, INT8U Value8, INT16U Value16, INT16U destiny)
{
  INT8U j = 0;
  INT8U status = 0;
  
  acquireRadio();
  
  NWKPayload[0] = APP_01;
  NWKPayload[1] = GENERAL_PROFILE;
  NWKPayload[2] = Command;
  NWKPayload[3] = Parameter;
 

  NWKPayload[4] = Value16 >> 8;
  NWKPayload[5] = Value16 & 0xFF; 
  j=2;
  
  status = OneHopRoute((INT8U)(4+j),destiny);
  
  releaseRadio();
  
  return status;
}

/**
* @fn     UNET_TX_ToBaseSation
* @brief  function to send data to nearest base station (coordinator)
**/

INT8U UNET_TX_ToBaseSation(INT8U Command, INT8U Attribute, UINT_DWORD * ptr_data, INT8U nbr_bytes){
    
    INT8U j = 0; 
    
    if(nbr_bytes > MAX_APP_PAYLOAD_SIZE-2 || ptr_data == NULL) {
        return SEND_ERROR;
    }
    
    if(thisNodeDepth >= ROUTE_TO_BASESTATION_LOST){
        return SEND_ERROR;
    }
    
    acquireRadio();
    
      /* set app header */
      NWKPayload[0] = APP_01;
      NWKPayload[1] = GENERAL_PROFILE;
      NWKPayload[2] = Command;
      NWKPayload[3] = Attribute;
      
      for(j=0; j<nbr_bytes;j++){        
        NWKPayload[j+4] = ptr_data->int8u[j];
      }
      
      NWKPayload[j+4+0] = (INT8U)(macAddr >> 8);
      NWKPayload[j+4+1] = (INT8U)(macAddr & 0xFF);
      
      j = DownRoute(START_ROUTE,(INT8U)j+4+2);
    
      releaseRadio();

      return j;
}



#if 0
#define DEBUG_WRITE(x);

void Reason_of_Reset(void);

void Reason_of_Reset(void)
{  
  INT8U reason = 0;
  
  UserEnterCritical();
    reason = SRS;
  UserExitCritical();
  
  switch(reason)
  {
    case 0b00000000:
      DEBUG_WRITE("Reset caused by BDM");
      break;
      
    case 0b00000010:
      DEBUG_WRITE("Reset caused by low voltage");
      break;      
      
    case 0b00001000:
      DEBUG_WRITE("Reset caused by illegal address");
      break;
      
    case 0b00010000:
      DEBUG_WRITE("Reset caused by illegal opcode");
      break;
      
    case 0b00100000:
      DEBUG_WRITE("Reset caused by watchdog");
      break;
      
    case 0b01000000:
      DEBUG_WRITE("Reset caused by reset pin");
      break;
      
    case 0b10000010:
    case 0b10000000:
      DEBUG_WRITE("Power on Reset, Cold Reset");
      break;
      
    default:
      DEBUG_WRITE("Unknown or several reasons");
      break;      
  }
}

static FAIL_T fail = NO_FAIL;

void ReportFailure_Get(FAIL_T *failure_kind){
  *failure_kind = fail;
}

void ReportFailure_Set(FAIL_T failure_kind){
  fail = failure_kind;
}


#endif
