#ifndef NET_CONFIG_H
#define NET_CONFIG_H

#include "BRTOS.h"

/// Define network support           
#define NETWORK_ENABLE                      1

// Network device types
#define   PAN_COORDINATOR                   0
#define   ROUTER                            1

// Network device type: see options above
#define   DEVICE_TYPE                       PAN_COORDINATOR

// Define router type
#define   ROUTER1                   		0
#define   ROUTER2                           1

#define   ROUTER_TYPE                       ROUTER2


#define ROUTER_AUTO_ASSOCIATION				TRUE
#if (ROUTER_AUTO_ASSOCIATION == TRUE)
#define ROUTER_AUTO_ASSOCIATION_MAC_ADDR	1
#endif
#ifndef ROUTER_AUTO_ASSOCIATION_MAC_ADDR
	#error	Defina o endereço MAC do roteador na variável ROUTER_AUTO_ASSOCIATION_MAC_ADDR
#endif

// CPU memory alignment
#define CPU_32_BITS                         1
#define CPU_16_BITS                         0
#define CPU_8_BITS                          0

// Reactive up route - 1 = on, 0 = off
#define USE_REACTIVE_UP_ROUTE               1
#define REACTIVE_UP_ROUTE_AUTO_MAINTENANCE  1

// UNET Tasks Priorities
#define RF_EventHandlerPriority     (INT8U)31
#define UNET_Mutex_Priority         (INT8U)27
#define MAC_HandlerPriority         (INT8U)22
#define NWK_HandlerPriority         (INT8U)23
#define APP1_Priority               (INT8U)24
#define APP2_Priority               (INT8U)25
#define APP3_Priority               (INT8U)26


// APPs signals 
#define SIGNAL_APP1       App1_event
//#define SIGNAL_APP2       App1_event

// UNET Tasks Stacks
#if ((DEVICE_TYPE == PAN_COORDINATOR) || (DEVICE_TYPE == INSTALLER))
#define UNET_RF_Event_StackSize    (384)
#define UNET_MAC_StackSize         (384)
#define UNET_NWK_StackSize         (1280)
#else
#define UNET_RF_Event_StackSize    (256)
#define UNET_MAC_StackSize         (384)
#define UNET_NWK_StackSize         (1088)
#endif

// Ping Times
#define TX_TIMEOUT       50
#define PING_TIME		 10
#define MAX_PING_TIME	  8
#define PING_RETRIES	  3

// UpRoute Times
#define MAX_UPROUTE_MAINTENANCE_TIME	30

/// RF Buffer Size
#define RFBufferSize      (INT16U)768      // max. 6 packets (128B)

/// Memory locations for network address and configurations
#if PROCESSOR == COLDFIRE_V1
#if (DEVICE_TYPE == PAN_COORDINATOR)
  #define LAT_MEM_ADDRESS    0x0001FC00
  #define LON_MEM_ADDRESS    0x0001FC04
  #define MAC16_MEM_ADDRESS
  #define MAC64_MEM_ADDRESS  0x00001FF0
  #define PANID_MEM_ADDRESS
  #define PANID_INIT_VALUE   0x4742
  #define MAC16_INIT_VALUE   0x0000
  #define ROUTC_INIT_VALUE   0x01
#else
  #define LAT_MEM_ADDRESS    0x000021C0
  #define LON_MEM_ADDRESS    0x000021C4
  #define MAC16_MEM_ADDRESS  0x000021C8
  #define MAC64_MEM_ADDRESS  0x00001FF0
  #define PANID_MEM_ADDRESS  0x000021CC
  #define PANID_INIT_VALUE   0xFFFF
  #define MAC16_INIT_VALUE   0xFFFF
  #define ROUTC_INIT_VALUE   0x00
#endif
#endif

#if PROCESSOR == ARM_Cortex_M0
#if (DEVICE_TYPE == PAN_COORDINATOR)
  #define LAT_MEM_ADDRESS    0x0001F000
  #define LON_MEM_ADDRESS    0x0001F004
  #define MAC16_MEM_ADDRESS
  #define MAC64_MEM_ADDRESS  0x0001F800
  #define PANID_MEM_ADDRESS
  #define PANID_INIT_VALUE   0x4742
  #define MAC16_INIT_VALUE   0x0000
  #define ROUTC_INIT_VALUE   0x01  
#else 
  #define LAT_MEM_ADDRESS    0x0001F000
  #define LON_MEM_ADDRESS    0x0001F004
  #define MAC16_MEM_ADDRESS  0x0001F008
  #define PANID_MEM_ADDRESS  0x0001F00C
  #define MAC64_MEM_ADDRESS  0x0001F800
  #define PANID_INIT_VALUE   0xFFFF
  #define MAC16_INIT_VALUE   0xFFFF
  #define ROUTC_INIT_VALUE   0x00  
#endif
#endif

// IEEE EUI - globally unique number
#define EUI_7 0xCA
#define EUI_6 0xBA
#define EUI_5 0x60                       
#define EUI_4 0x89
#define EUI_3 0x50
#define EUI_2 0x16
#define EUI_1 0x77
#define EUI_0 0x84



#endif









