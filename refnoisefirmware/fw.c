/*
 * This file is part of the libopencm3 project.
 *
 * Copyright (C) 2010 Gareth McMullin <gareth@blacksphere.co.nz>
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

/* Modified by mlaaks to control GPIO
 * The original example code can be obtained from:
 * 
 * https://github.com/libopencm3/libopencm3-examples
 * 
 * file:
 *  /examples/stm32/f1/stm32-h103/usb_cdcacm/cdcacm.c
 */

#include <stdlib.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/usb/usbd.h>
#include <libopencm3/usb/cdc.h>
#include <libopencm3/stm32/f1/nvic.h>
#include <libopencm3/cm3/systick.h>
#include <libopencm3/cm3/cortex.h>
#include <libopencm3/cm3/scb.h>
#include <libopencm3/stm32/exti.h>

#define DFUAPP_ADDRESS	0x08000400
#define VTOR_ADDRESS    0x08000400
#define JUMP_TO_APP(ADDRESS,VECTOR_TABLE){														\
							static uint32_t app_address = (uint32_t)ADDRESS;					\
							static uint8_t *p_app = (uint8_t *) &app_address;					\
							static uint32_t vect_table  = (uint32_t)VECTOR_TABLE;				\
							SCB_VTOR = vect_table & 0xFFFF;										\
							asm volatile("msr msp, %0"::"g"(*(volatile uint32_t *)vect_table)); \
							(*(void (**)())(app_address+4))();										\
}//should we just use asm("bl app_address") here? or modify that call such that app_address is immediate, not pointer where from the address is fetched.
// it was (*(void (**)())(app_address + 4))();
//function pointer syntax:
//  void (*fun_ptr)(int) = &fun; 
/*

							asm volatile("ldr r3, [%0]"::"r"(*(volatile uint32_t *)app_address));	\
							asm volatile("blx r3");\
							*/

//extern uint32_t _start;
static uint32_t dfuapp_address = (uint32_t) DFUAPP_ADDRESS;


//magic string. When encountered, jump to DFU bootloader:
static char magicstr[] = "I_AM_A_WIZARD";
static uint32_t magicctr = 0;
static size_t magichappens = sizeof(magicstr)-1;

static bool exit_to_dfu = false;
static int  exit_to_dfu_cnt = 16;

static const struct usb_device_descriptor dev = {
	.bLength = USB_DT_DEVICE_SIZE,
	.bDescriptorType = USB_DT_DEVICE,
	.bcdUSB = 0x0200,
	.bDeviceClass = USB_CLASS_CDC,
	.bDeviceSubClass = 0,
	.bDeviceProtocol = 0,
	.bMaxPacketSize0 = 64,
	.idVendor = 0x0483,
	.idProduct = 0x5740,
	.bcdDevice = 0x0200,
	.iManufacturer = 1,
	.iProduct = 2,
	.iSerialNumber = 3,
	.bNumConfigurations = 1,
};

/*
 * This notification endpoint isn't implemented. According to CDC spec its
 * optional, but its absence causes a NULL pointer dereference in Linux
 * cdc_acm driver.
 */
static const struct usb_endpoint_descriptor comm_endp[] = {{
	.bLength = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType = USB_DT_ENDPOINT,
	.bEndpointAddress = 0x83,
	.bmAttributes = USB_ENDPOINT_ATTR_INTERRUPT,
	.wMaxPacketSize = 16,
	.bInterval = 255,
}};

static const struct usb_endpoint_descriptor data_endp[] = {{
	.bLength = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType = USB_DT_ENDPOINT,
	.bEndpointAddress = 0x01,
	.bmAttributes = USB_ENDPOINT_ATTR_BULK,
	.wMaxPacketSize = 64,
	.bInterval = 1,
}, {
	.bLength = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType = USB_DT_ENDPOINT,
	.bEndpointAddress = 0x82,
	.bmAttributes = USB_ENDPOINT_ATTR_BULK,
	.wMaxPacketSize = 64,
	.bInterval = 1,
}};

static const struct {
	struct usb_cdc_header_descriptor header;
	struct usb_cdc_call_management_descriptor call_mgmt;
	struct usb_cdc_acm_descriptor acm;
	struct usb_cdc_union_descriptor cdc_union;
} __attribute__((packed)) cdcacm_functional_descriptors = {
	.header = {
		.bFunctionLength = sizeof(struct usb_cdc_header_descriptor),
		.bDescriptorType = CS_INTERFACE,
		.bDescriptorSubtype = USB_CDC_TYPE_HEADER,
		.bcdCDC = 0x0110,
	},
	.call_mgmt = {
		.bFunctionLength =
			sizeof(struct usb_cdc_call_management_descriptor),
		.bDescriptorType = CS_INTERFACE,
		.bDescriptorSubtype = USB_CDC_TYPE_CALL_MANAGEMENT,
		.bmCapabilities = 0,
		.bDataInterface = 1,
	},
	.acm = {
		.bFunctionLength = sizeof(struct usb_cdc_acm_descriptor),
		.bDescriptorType = CS_INTERFACE,
		.bDescriptorSubtype = USB_CDC_TYPE_ACM,
		.bmCapabilities = 0,
	},
	.cdc_union = {
		.bFunctionLength = sizeof(struct usb_cdc_union_descriptor),
		.bDescriptorType = CS_INTERFACE,
		.bDescriptorSubtype = USB_CDC_TYPE_UNION,
		.bControlInterface = 0,
		.bSubordinateInterface0 = 1,
	 },
};

static const struct usb_interface_descriptor comm_iface[] = {{
	.bLength = USB_DT_INTERFACE_SIZE,
	.bDescriptorType = USB_DT_INTERFACE,
	.bInterfaceNumber = 0,
	.bAlternateSetting = 0,
	.bNumEndpoints = 1,
	.bInterfaceClass = USB_CLASS_CDC,
	.bInterfaceSubClass = USB_CDC_SUBCLASS_ACM,
	.bInterfaceProtocol = USB_CDC_PROTOCOL_AT,
	.iInterface = 0,

	.endpoint = comm_endp,

	.extra = &cdcacm_functional_descriptors,
	.extralen = sizeof(cdcacm_functional_descriptors),
}};

static const struct usb_interface_descriptor data_iface[] = {{
	.bLength = USB_DT_INTERFACE_SIZE,
	.bDescriptorType = USB_DT_INTERFACE,
	.bInterfaceNumber = 1,
	.bAlternateSetting = 0,
	.bNumEndpoints = 2,
	.bInterfaceClass = USB_CLASS_DATA,
	.bInterfaceSubClass = 0,
	.bInterfaceProtocol = 0,
	.iInterface = 0,

	.endpoint = data_endp,
}};

static const struct usb_interface ifaces[] = {{
	.num_altsetting = 1,
	.altsetting = comm_iface,
}, {
	.num_altsetting = 1,
	.altsetting = data_iface,
}};

static const struct usb_config_descriptor config = {
	.bLength = USB_DT_CONFIGURATION_SIZE,
	.bDescriptorType = USB_DT_CONFIGURATION,
	.wTotalLength = 0,
	.bNumInterfaces = 2,
	.bConfigurationValue = 1,
	.iConfiguration = 0,
	.bmAttributes = 0x80,
	.bMaxPower = 0x32,

	.interface = ifaces,
};

static const char *usb_strings[] = {
	"COHERENT RTLSDR",
	"Reference noise controller:",
	"https://github.com/mlaaks/coherent-rtlsdr",
};

/* Buffer to be used for control requests. */
uint8_t usbd_control_buffer[128];

static enum usbd_request_return_codes cdcacm_control_request(usbd_device *usbd_dev, struct usb_setup_data *req, uint8_t **buf,
		uint16_t *len, void (**complete)(usbd_device *usbd_dev, struct usb_setup_data *req))
{
	(void)complete;
	(void)buf;
	(void)usbd_dev;
	if (!exit_to_dfu){
		switch (req->bRequest) {
		case USB_CDC_REQ_SET_CONTROL_LINE_STATE: {
			/*
			 * This Linux cdc_acm driver requires this to be implemented
			 * even though it's optional in the CDC spec, and we don't
			 * advertise it in the ACM functional descriptor.
			 */
			char local_buf[10];
			struct usb_cdc_notification *notif = (void *)local_buf;

			/* We echo signals back to host as notification. */
			notif->bmRequestType = 0xA1;
			notif->bNotification = USB_CDC_NOTIFY_SERIAL_STATE;
			notif->wValue = 0;
			notif->wIndex = 0;
			notif->wLength = 2;
			local_buf[8] = req->wValue & 3;
			local_buf[9] = 0;
			// usbd_ep_write_packet(0x83, buf, 10);
			return USBD_REQ_HANDLED;
			}
		case USB_CDC_REQ_SET_LINE_CODING:
			if (*len < sizeof(struct usb_cdc_line_coding))
				return USBD_REQ_NOTSUPP;
			return USBD_REQ_HANDLED;
		}
	}
	return USBD_REQ_NOTSUPP;
}

static void noisestate(bool);
static void fanstate(bool);
static bool areyouawizard(uint8_t c);
static void cdcacm_data_rx_cb(usbd_device *usbd_dev, uint8_t ep)
{
	(void)ep;
	(void)usbd_dev;

	char buf[64];
	int len = usbd_ep_read_packet(usbd_dev, 0x01, buf, 64);
	if (!exit_to_dfu){
		if (len) {
	        switch (buf[0])
	        {
	            case 'X': //enable referencenoise amps
	               		noisestate(true);
	                break;
	            case 'x': //disable amps
	                	noisestate(false);
	                break;
	            case 'F': //enable fan
	                	fanstate(true);
	                break;
	            case 'f': //disable fan
	                	fanstate(false);
	                break;
	            default:
	                break;
	                
	        }
	        if (areyouawizard(buf[0]))
	        {
	        	uint8_t msg[]="JUMPING TO DFU-MODE";
	        	usbd_ep_write_packet(usbd_dev, 0x82, msg, sizeof(msg));
	        	exit_to_dfu = true;
	        }
	        else{
				usbd_ep_write_packet(usbd_dev, 0x82, buf, len);
				buf[len] = 0;
			}
		}
	}
}

static void cdcacm_set_config(usbd_device *usbd_dev, uint16_t wValue)
{
	(void)wValue;
	(void)usbd_dev;

	usbd_ep_setup(usbd_dev, 0x01, USB_ENDPOINT_ATTR_BULK, 64, cdcacm_data_rx_cb);
	usbd_ep_setup(usbd_dev, 0x82, USB_ENDPOINT_ATTR_BULK, 64, NULL);
	usbd_ep_setup(usbd_dev, 0x83, USB_ENDPOINT_ATTR_INTERRUPT, 16, NULL);

	usbd_register_control_callback(
				usbd_dev,
				USB_REQ_TYPE_CLASS | USB_REQ_TYPE_INTERFACE,
				USB_REQ_TYPE_TYPE | USB_REQ_TYPE_RECIPIENT,
				cdcacm_control_request);
}

static void noisestate(bool state){
	if (state)
	{
		gpio_set(GPIOB, GPIO14);
		gpio_clear(GPIOC,GPIO13);
	}
	else
	{
		gpio_clear(GPIOB,GPIO14);
		gpio_set(GPIOC,GPIO13);
	}
}

static void fanstate(bool state){
	if (state)
	{
		gpio_set(GPIOB, GPIO12);
	}
	else
	{
		gpio_clear(GPIOB,GPIO12);
	}
}

static void gpioinit()
{
	rcc_periph_clock_enable(RCC_GPIOA);
    rcc_periph_clock_enable(RCC_GPIOB);
    rcc_periph_clock_enable(RCC_GPIOC);


    gpio_set_mode(GPIOC, GPIO_MODE_OUTPUT_2_MHZ,
                  GPIO_CNF_OUTPUT_PUSHPULL, GPIO13);

    gpio_set_mode(GPIOB, GPIO_MODE_OUTPUT_2_MHZ,
                  GPIO_CNF_OUTPUT_PUSHPULL, GPIO12);
	
	gpio_set_mode(GPIOB, GPIO_MODE_OUTPUT_2_MHZ,
                  GPIO_CNF_OUTPUT_PUSHPULL, GPIO14);

	gpio_set_mode(GPIOC, GPIO_MODE_OUTPUT_50_MHZ,
		      GPIO_CNF_OUTPUT_PUSHPULL, GPIO11);
	gpio_set(GPIOC, GPIO11);


}

static void gpiouninit()
{
	gpio_set_mode(GPIOC,GPIO_MODE_INPUT,GPIO_CNF_INPUT_FLOAT,GPIO13);
	gpio_set_mode(GPIOB,GPIO_MODE_INPUT,GPIO_CNF_INPUT_FLOAT,GPIO12);
	gpio_set_mode(GPIOB,GPIO_MODE_INPUT,GPIO_CNF_INPUT_FLOAT,GPIO14);
	gpio_set_mode(GPIOC,GPIO_MODE_INPUT,GPIO_CNF_INPUT_FLOAT,GPIO11);
	rcc_periph_clock_disable(RCC_GPIOC);
    rcc_periph_clock_disable(RCC_GPIOB);
    rcc_periph_clock_disable(RCC_GPIOA);
}

static bool areyouawizard(uint8_t c){

	magicctr = (c == magicstr[magicctr]) ? magicctr + 1 : 0;

	if (magicctr >= magichappens)
	{
		return true;
	}
	else
	{
		return false;
	}
}

static void delay(){
	int i;
	for (i = 0; i < 0x800000; i++)
		__asm__("nop");
}

static void disableallints(){
	nvic_disable_irq(NVIC_OTG_FS_IRQ);
	nvic_disable_irq(NVIC_USB_WAKEUP_IRQ);
	systick_counter_disable();
	systick_interrupt_disable();
	cm_disable_interrupts();
}

static void usbd_soft_disconnect(){
	delay();
	delay();
	delay();
	gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_2_MHZ,
		GPIO_CNF_OUTPUT_PUSHPULL, GPIO12);
	gpio_clear(GPIOA, GPIO12);
	delay();
	delay();
	delay();
}

static void zeromem(uint8_t *p, size_t len){
	int cnt;
	for (cnt=0;cnt<len;cnt++)
		p[cnt]=0;

}

int main(void)
{

	usbd_device *usbd_dev;

	//JUMP_TO_APP(DFUAPP_ADDRESS,VTOR_ADDRESS);
	//JUMP TO BOOTLOADER CODE:
	//JUMP_TO_APP(DFUAPP_ADDRESS);
	//__asm__("msr msp, %0" : : "r" (dfuapp_address));
	//(*(void (**)())(dfuapp_address + 4))();



	//rcc_clock_setup_in_hsi_out_48mhz();
	rcc_clock_setup_in_hse_8mhz_out_72mhz();
	gpioinit();

	noisestate(false);
	fanstate(true);

	usbd_dev = usbd_init(&st_usbfs_v1_usb_driver, &dev, &config, usb_strings, 3, usbd_control_buffer, sizeof(usbd_control_buffer));
	usbd_register_set_config_callback(usbd_dev, cdcacm_set_config);

	delay();
    //gpio_clear(GPIOC, GPIO11);


	while ((!exit_to_dfu) &(exit_to_dfu_cnt>0))
	{
		usbd_poll(usbd_dev);
		exit_to_dfu_cnt -= (exit_to_dfu) ? 1 : 0; 
	}
	
	usbd_disconnect(usbd_dev,true);
	zeromem((uint8_t *) usbd_dev,sizeof(usbd_dev));
	gpiouninit();
	disableallints();


	delay();

	//revert to HSI clock:
	//rcc_clock_setup_in_hsi_out_48mhz();

	//the great jump
	JUMP_TO_APP(DFUAPP_ADDRESS,VTOR_ADDRESS);
}
