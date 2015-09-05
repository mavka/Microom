/*
 * descriptors_audio.h
 *
 *  Created on: 04 09 2015 �.
 *      Author: Kreyl
 */

#ifndef USB_DESCRIPTORS_AUDIO_H_
#define USB_DESCRIPTORS_AUDIO_H_

// Endpoints to be used
#define EP_DATA_IN_ID       1

// Endpoint Sizes for Full-Speed devices
#define EP0_SZ              64  // Control Endpoint must have a packet size of 64 bytes
#define EP_INTERRUPT_SZ     8   // Max size is 64 bytes
#define EP_BULK_SZ          64  // Max size is 64 bytes

#define USBD_VID            0x0483  // ST Micro
#define USBD_PID            0x5730  // Audio

#ifdef __cplusplus
extern "C" {
#endif
const USBDescriptor *GetDescriptor(USBDriver *usbp, uint8_t dtype, uint8_t dindex, uint16_t lang);
#ifdef __cplusplus
}
#endif

#endif /* USB_DESCRIPTORS_AUDIO_H_ */
