
/*********************************************************************************************************
*                                               BRTOS
*                                Brazilian Real-Time Operating System
*                            Acronymous of Basic Real-Time Operating System
*
*                              
*                                  Open Source RTOS under MIT License
*
*
*
*                                              OS Tasks
*
*
*   Author:   Gustavo Weber Denardin
*   Revision: 1.0
*   Date:     20/03/2009
*
*********************************************************************************************************/


/* MCU and OS includes */
#include "BRTOS.h"
#include "xhw_types.h"
#include "xsysctl.h"
#include "xgpio.h"
#include "xwdt.h"
#include "xhw_memmap.h"
#include "accel.h"
#include "xhw_ints.h"
#include "xhw_uart.h"
#include "xuart.h"
#include "xcore.h"
#include "xhw_gpio.h"
#include "system.h"
#include "FLASH.h"
#include <string.h>

/* Config. files */
#include "BRTOSConfig.h"
#include "AppConfig.h"
#include "NetConfig.h"
#include "BoardConfig.h"

/* Function prototypes */
#include "unet_api.h"
#include "app.h"
#include "tasks.h"


/* Includes ------------------------------------------------------------------*/




/*************************************************/
/* Task: keep watchdog happy and system time     */
/*************************************************/
void System_Time(void *param)
{
   /* task setup */
   INT16U i = 0;

   (void)param;
   OSResetTime();
   
#if (WATCHDOG == 1)
   /* Initialize the WDT */
   WDTimerInit(WDT_MODE_NORMAL | xWDT_INTERVAL_1K_32MS);
   WDTimerEnable();
#else
	WDTimerDisable();
#endif
  
   /* task main loop */
   for (;;)
   {

	   #if (WATCHDOG == 1)
	   /* Feed WDT counter */
	   WDTimerRestart();
	   #endif

      DelayTask(10); /* wait 10 ticks -> 10 ms */

      #if RADIO_DRIVER_WATCHDOG == 1
           //Radio_Count_states();
      #endif
      
      i++;
      if (i >= 100)
      {
        OSUpdateUptime();
        i = 0;
      }
   }
}

#if (DEVICE_TYPE == ROUTER)
#if (ROUTER_TYPE == ROUTER1)
void pisca_led_net(void *param)
{
	(void)param;
	for(;;)
	{
		// Envia mensagem para o coordenador
		NetGeneralONOFF(TRUE, 0);
		DelayTask(1000);
		NetGeneralONOFF(FALSE, 0);
		DelayTask(1000);
	}
}
#endif

#if (ROUTER_TYPE == ROUTER2)
#if 0
void make_path(void *param)
{
	(void)param;
	for(;;)
	{
		// Envia mensagem para o coordenador
		NetGeneralCreateUpPath();
		DelayTask(5000);
	}
}
#endif
#endif
#endif


#if (DEVICE_TYPE == PAN_COORDINATOR)
void pisca_led_net(void *param)
{
	INT16U neighbor;
	int i = 0;
	(void)param;
	for(;;)
	{
		// Envia mensagem para o primeiro vizinho da lista
		for(i=0;i<ROUTING_UP_TABLE_SIZE;i++){
			//if ((unet_neighbourhood[i].Addr_16b != 0xFFFE) && (unet_neighbourhood[i].NeighborStatus.bits.Symmetric == TRUE))
			if (unet_routing_up_table[i].DestinyAddr != 0xFFFE)
			{
				neighbor = unet_neighbourhood[i].Addr_16b;
				NetGeneralONOFF(TRUE, neighbor);
				DelayTask(1000);
				NetGeneralONOFF(FALSE, neighbor);
			}
			DelayTask(100);
		}
	}
}
#endif

#include "UART.h"
#include "utils.h"

void UNET_App_1_Decode(void *param)
{
#if (defined BOOTLOADER_ENABLE) && (BOOTLOADER_ENABLE==1)
   INT8U  ret = 0;
#endif

#if (DEVICE_TYPE == PAN_COORDINATOR)
   char buffer[8];
#endif
   (void)param;

	// Enables the port clock
	SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOB);

	GPIOPinConfigure(GPIO_PB18_PB18);
	GPIOPinConfigure(GPIO_PB19_PB19);

	// Enables Drive Strength
	GPIOPadConfigSet (GPIOB_BASE, (GPIO_PIN_18 | GPIO_PIN_19), PORT_TYPE_DSE_HIGH);

	// Set port for LED to output
	GPIOPinSet(GPIOB_BASE, (GPIO_PIN_18 | GPIO_PIN_19));
	xGPIODirModeSet(GPIOB_BASE, (GPIO_PIN_18 | GPIO_PIN_19), xGPIO_DIR_MODE_OUT);

	#if (DEVICE_TYPE == PAN_COORDINATOR)
    // Init serial driver
	Init_UART0();
	#endif

   /* task main loop */
   for (;;)
   {

      /* Wait event from APP layer, with or without timeout */
#if (defined BOOTLOADER_ENABLE) && (BOOTLOADER_ENABLE==1)
	      ret = OSSemPend(SIGNAL_APP1, SIGNAL_TIMEOUT);
#else
	      (void)OSSemPend(SIGNAL_APP1, 0);
#endif

      #if (defined BOOTLOADER_ENABLE) && (BOOTLOADER_ENABLE==1)
      if(ret != TIMEOUT){
      #endif

       acquireRadio();

       switch(app_packet.APP_Profile)
       {
        case GENERAL_PROFILE:
		  #if (DEVICE_TYPE == PAN_COORDINATOR)
          (void)UARTPutString(UART0_BASE, "Pacote do perfil geral recebido do nó ");
          (void)UARTPutString(0x4006A000, PrintDecimal(nwk_packet.NWK_Source, buffer));\
          (void)UARTPutString(0x4006A000, "\n\r");
		  #endif
          Decode_General_Profile();
          break;

        case LIGHTING_PROFILE:
		  #if (DEVICE_TYPE == PAN_COORDINATOR)
          (void)UARTPutString(UART0_BASE, "Pacote do perfil lighting recebido!\n\r");
		  #endif
          //Decode_Lighting_Profile();
          break;

        #if (defined BOOTLOADER_ENABLE) && (BOOTLOADER_ENABLE==1)
        case BULK_DATA_PROFILE:
          WBootloader_Handler();
          break;
        #endif

        default:
		  #if (DEVICE_TYPE == PAN_COORDINATOR)
          (void)UARTPutString(UART0_BASE, "Pacote de dados genérico recebido!\n\r");
		  #endif
          break;
       }
       releaseRadio();

      #if (defined BOOTLOADER_ENABLE) && (BOOTLOADER_ENABLE==1)
      }else{
        WBootloader_Handler_Timeout();
      }
      #endif

   }
}



void pisca_led(void *param)
{
	CHAR8 *test_pointer = "Teste";
	CHAR8 data_test[6];
	int status = 0;
	int i = 0;

	(void)param;

	// Enables the port clock
	SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOB);
	SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOD);

	// Inicializa o módulo de memória FLASH
	InitFlash();

	// Lê 5 bytes da memória FLASH
	ReadFromFlash(0x1F040, data_test, 5);

	data_test[5]=0;
	status = strcmp(test_pointer, data_test);

	// Verifica se a FLASH já havia sido escrita
	if (status)
	{
		// Escreve 5 bytes na memória FLASH
		WriteToFlash(test_pointer, 0x1F040, 5);
	}

	// Configures pin as I/O
	GPIOPinConfigure(GPIO_PB18_PB18);
	GPIOPinConfigure(GPIO_PB19_PB19);

	// Enables Drive Strength
	GPIOPadConfigSet (GPIOB_BASE, (GPIO_PIN_18 | GPIO_PIN_19), PORT_TYPE_DSE_HIGH);

	// Set port for LED to output
	GPIOPinSet(GPIOB_BASE, (GPIO_PIN_18 | GPIO_PIN_19));
	xGPIODirModeSet(GPIOB_BASE, (GPIO_PIN_18 | GPIO_PIN_19), xGPIO_DIR_MODE_OUT);

	for(;;)
	{
		for(i=0;i<6;i++)
		{
			data_test[i] = 0;
		}

		ReadFromFlash(0x1F040, data_test, 5);

		status = strcmp(test_pointer, data_test);

		// Só pisca o outro LED se a FLASH leu o valor correto
		GPIOPinReset(GPIOB_BASE, GPIO_PIN_18);
		if (!status)
		{
			GPIOPinSet(GPIOB_BASE, GPIO_PIN_19);
		}
		DelayTask(200);

		GPIOPinSet(GPIOB_BASE, GPIO_PIN_18);
		if (!status)
		{
			GPIOPinReset(GPIOB_BASE, GPIO_PIN_19);
		}
		DelayTask(200);
	}
}



#if (USB_DEMO)
void Task_USB(void)
{
	/* task setup */

	//usb_init();

	// Set USB IRQ priority to 3
	//set_irq_priority (73, 3);

	/* Initialize the USB Test Application */
	(void)cdc_Init();

	usb_terminal_init(cdc_putch);
	(void)usb_terminal_add_cmd((command_t*)&usb_ver_cmd);

    while(1)
    {
       /* Call the application task */
    	usb_terminal_process();
    }
}
#endif


