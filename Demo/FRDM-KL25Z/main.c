/* ###################################################################
**     Filename    : main.c
**     Project     : UNET_v2
**     Processor   : MKL25Z128VLK4
**     Version     : Driver 01.01
**     Compiler    : GNU C Compiler
**     Date/Time   : 2015-09-27, 21:09, # CodeGen: 0
**     Abstract    :
**         Main module.
**         This module contains user's application code.
**     Settings    :
**     Contents    :
**         No public methods
**
** ###################################################################*/
/*!
** @file main.c
** @version 01.01
** @brief
**         Main module.
**         This module contains user's application code.
*/         
/*!
**  @addtogroup main_module main module documentation
**  @{
*/         
/* MODULE main */
#include <stdint.h>


/* Including needed modules to compile this module/procedure */
#include "Cpu.h"
#include "Events.h"
/* Including shared modules, which are used for whole project */
#include "PE_Types.h"
#include "PE_Error.h"
#include "PE_Const.h"
#include "IO_Map.h"

/* User includes (#include below this line is not maintained by Processor Expert) */

/* MCU and OS includes */
#include "BRTOS.h"
#include "system.h"
#include "xwdt.h"

/* Config. files */
#include "AppConfig.h"
#include "NetConfig.h"
#include "BoardConfig.h"

#include "tasks.h"        /* for tasks prototypes */
#include "unet_api.h"   /* for UNET network functions */
#include "low_power.h"

BRTOS_TH TH_SYSTEM;
BRTOS_TH TH_NET_APP1;
BRTOS_TH TH_NET_APP2;

/*lint -save  -e970 Disable MISRA rule (6.3) checking. */
int main(void)
/*lint -restore Enable MISRA rule (6.3) checking. */
{
  /* Write your local variable definition here */

  /*** Processor Expert internal initialization. DON'T REMOVE THIS CODE!!! ***/
  PE_low_level_init();
  /*** End of Processor Expert internal initialization.                    ***/

  /* Write your code here */
  /* For example: for(;;) { } */
  low_power();
  
  // Initialize BRTOS
  BRTOS_Init();

  ////////////////////////////////////////////////////////

  if(InstallTask(&System_Time,"System Time",384,SystemTaskPriority, NULL, &TH_SYSTEM) != OK)
  {
    // Oh Oh
    // Não deveria entrar aqui !!!
    while(1){};
  };

#if(NETWORK_ENABLE == 1)
  UNET_Init();      /* Install 3 task: PHY, MAC and NWK */

  if(InstallTask(&UNET_App_1_Decode,"Decode app 1 profiles",512,APP1_Priority, NULL, &TH_NET_APP1) != OK)
  {
    // Oh Oh
    // Não deveria entrar aqui !!!
    while(1){};
  };

#if (DEVICE_TYPE == PAN_COORDINATOR)
  if(InstallTask(&pisca_led_net,"Blink LED Example",512,APP2_Priority, NULL, &TH_NET_APP2) != OK)
  {
    // Oh Oh
    // Não deveria entrar aqui !!!
    while(1){};
  };
#endif

#if (DEVICE_TYPE == ROUTER)
#if (ROUTER_TYPE == ROUTER1)
  if(InstallTask(&pisca_led_net,"Blink LED Example",512,APP2_Priority, NULL, &TH_NET_APP2) != OK)
  {
    // Oh Oh
    // Não deveria entrar aqui !!!
    while(1){};
  };
#endif

#if (ROUTER_TYPE == ROUTER2)
  if(InstallTask(&make_path,"Make up path",512,APP2_Priority, NULL, &TH_NET_APP2) != OK)
  {
    // Oh Oh
    // Não deveria entrar aqui !!!
    while(1){};
  };
#endif
#endif
#endif

#if 1
  if(InstallTask(&led_activity,"Blink LED for activity",256,20, NULL, NULL) != OK)
  {
    // Oh Oh
    // Não deveria entrar aqui !!!
    while(1){};
  };
#endif
  
  // Start Task Scheduler
  if(BRTOSStart() != OK)
  {
    // Oh Oh
    // Não deveria entrar aqui !!!
    for(;;){};
  };  

  /*** Don't write any code pass this line, or it will be deleted during code generation. ***/
  /*** RTOS startup code. Macro PEX_RTOS_START is defined by the RTOS component. DON'T MODIFY THIS CODE!!! ***/
  #ifdef PEX_RTOS_START
    PEX_RTOS_START();                  /* Startup of the selected RTOS. Macro is defined by the RTOS component. */
  #endif
  /*** End of RTOS startup code.  ***/
  /*** Processor Expert end of main routine. DON'T MODIFY THIS CODE!!! ***/
  for(;;){}
  /*** Processor Expert end of main routine. DON'T WRITE CODE BELOW!!! ***/
} /*** End of main routine. DO NOT MODIFY THIS TEXT!!! ***/

/* END main */
/*!
** @}
*/
/*
** ###################################################################
**
**     This file was created by Processor Expert 10.3 [05.09]
**     for the Freescale Kinetis series of microcontrollers.
**
** ###################################################################
*/
