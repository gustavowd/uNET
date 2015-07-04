#include <stdint.h>

/* MCU and OS includes */
#include "BRTOS.h"
#include "MKL25Z4.h"
#include "system.h"
#include "xwdt.h"

/* Config. files */
#include "AppConfig.h"
#include "NetConfig.h"
#include "BoardConfig.h"

#include "tasks.h"        /* for tasks prototypes */
#include "unet_api.h"   /* for UNET network functions */

BRTOS_TH TH_SYSTEM;
BRTOS_TH TH_NET_APP1;
BRTOS_TH TH_NET_APP2;



int main(void)
{

#if TICKLESS == 1
	// Initialize clock using FLL (40MHz)
	Clock_init();
	Enable_LLS();
#else
	// Init system clock
	xSysCtlClockSet(48000000, xSYSCTL_XTAL_8MHZ | xSYSCTL_OSC_MAIN);
	// Initialize clock using FLL (40MHz)
	//Clock_init();
#endif

	  // Initialize BRTOS
	  BRTOS_Init();

	  ////////////////////////////////////////////////////////

	  if(InstallTask(&System_Time,"System Time",384,30, NULL, &TH_SYSTEM) != OK)
	  {
	    // Oh Oh
	    // Não deveria entrar aqui !!!
	    while(1){};
	  };

#if(NETWORK_ENABLE == 1)
	  UNET_Init();      /* Install 3 task: PHY, MAC and NWK */
#endif

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

	  // Start Task Scheduler
	  if(BRTOSStart() != OK)
	  {
	    // Oh Oh
	    // Não deveria entrar aqui !!!
	    for(;;){};
	  };

	  return 0;
}
