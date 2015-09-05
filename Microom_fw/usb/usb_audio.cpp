/*
 * usb_cdc.cpp
 *
 *  Created on: 03 ����. 2015 �.
 *      Author: Kreyl
 */

#include "ch.h"
#include "hal.h"
#include "usb_audio.h"
#include "usb.h"
#include "usb_lld.h"
#include "descriptors_audio.h"
#include "main.h"

UsbAudio_t UsbAu;

#define USBDrv      USBD1   // USB driver to use


#if 1 // ========================== Endpoints ==================================
// ==== EP1 ====
void OnDataTransmitted(USBDriver *usbp, usbep_t ep) { UsbAu.IOnDataTransmitted(usbp, ep); }

static USBInEndpointState ep1instate;

// EP1 initialization structure (both IN and OUT).
static const USBEndpointConfig ep1config = {
    USB_EP_MODE_TYPE_ISOC,
    NULL,                   // setup_cb
    OnDataTransmitted,      // in_cb
    NULL,                   // out_cb
    64,                     // in_maxsize
    64,                     // out_maxsize
    &ep1instate,            // in_state
    NULL,                   // out_state
    2,                      // in_multiplier: Determines the space allocated for the TXFIFO as multiples of the packet size
    NULL                    // setup_buf: Pointer to a buffer for setup packets. Set this field to NULL for non-control endpoints
};
#endif

#if 1 // ============================ Events ===================================
static void usb_event(USBDriver *usbp, usbevent_t event) {
    switch (event) {
        case USB_EVENT_RESET:
            return;
        case USB_EVENT_ADDRESS:
            return;
        case USB_EVENT_CONFIGURED:
            chSysLockFromISR();
            /* Enable the endpoints specified in the configuration.
            Note, this callback is invoked from an ISR so I-Class functions must be used.*/
            usbInitEndpointI(usbp, EP_DATA_IN_ID, &ep1config);
//            sduConfigureHookI(&UsbCDC.SDU2);   // Resetting the state of the CDC subsystem
            App.SignalEvtI(EVTMSK_USB_READY);
            chSysUnlockFromISR();
            return;
        case USB_EVENT_SUSPEND:
            chSysLockFromISR();
            App.SignalEvtI(EVTMSK_USB_READY);
            chSysUnlockFromISR();
            return;
        case USB_EVENT_WAKEUP:
            return;
        case USB_EVENT_STALLED:
            return;
    } // switch
}
#endif

struct SetupPkt_t {
    uint8_t bmRequestType;
    uint8_t bRequest;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;
};

/* ==== Setup Packet handler ====
 * true         Message handled internally.
 * false        Message not handled. */
bool OnSetupPkt(USBDriver *usbp) {
    SetupPkt_t *Setup = (SetupPkt_t*)usbp->setup;
    if(Setup->bmRequestType == 0x01) { // Host2Device, standard, recipient=interface
        if(Setup->bRequest == USB_REQ_SET_INTERFACE) {
            if(Setup->wIndex == 1) {
                usbSetupTransfer(usbp, NULL, 0, NULL);
                // wValue contains alternate setting
                chSysLockFromISR();
                if(Setup->wValue == 1) {    // Transmission on
                    App.SignalEvtI(EVTMSK_START_LISTEN);
                }
                else App.SignalEvtI(EVTMSK_STOP_LISTEN);
                chSysUnlockFromISR();
                return true;
            }
        }
    }
    return false;
}


#if 1  // ==== USB driver configuration ====
const USBConfig UsbCfg = {
    usb_event,          // This callback is invoked when an USB driver event is registered
    GetDescriptor,      // Device GET_DESCRIPTOR request callback
    OnSetupPkt,         // This hook allows to be notified of standard requests or to handle non standard requests
    NULL                // Start Of Frame callback
};
#endif

const int16_t sinconst[16] = {
        0, 1000, 3000, 5000, 7000, 5000, 3000, 1000, 0, -1000, -3000, -5000, -7000, -5000, -3000, -1000
};

/* 8000 Hz sampling freq, 2 bytes per sample. One frame per millisecond
 * means 8 samples per frame, but every second IN packet is lost. Therefore,
 * 16 samples per frame required.
 */

void UsbAudio_t::IOnDataTransmitted(USBDriver *usbp, usbep_t ep) {
//    if(IBuf.Get(&ISample.Word) == OK) {
        usbPrepareTransmit(&USBD1, EP_DATA_IN_ID, (uint8_t*)&sinconst[0], 32);
        chSysLockFromISR();
        usbStartTransmitI(&USBD1, EP_DATA_IN_ID);
        chSysUnlockFromISR();
//    }
    /* Transmit zero sized packet in case the last one has maximum allowed
       size. Otherwise the recipient may expect more data coming soon and
       not return buffered data to app. See section 5.8.3 Bulk Transfer
       Packet Size Constraints of the USB Specification document.*/
//    else if((usbp->epc[ep]->in_state->txsize > 0U) &&
//               ((usbp->epc[ep]->in_state->txsize &
//                ((size_t)usbp->epc[ep]->in_maxsize - 1U)) == 0U)) {
//        usbPrepareTransmit(&USBD1, EP_DATA_IN_ID, nullptr, 0);
//        chSysLockFromISR();
//        usbStartTransmitI(&USBD1, EP_DATA_IN_ID);
//        chSysUnlockFromISR();
//    }
}


void UsbAudio_t::Init() {
    // GPIO
    PinSetupAlterFunc(GPIOA, 11, omOpenDrain, pudNone, AF10);
    PinSetupAlterFunc(GPIOA, 12, omOpenDrain, pudNone, AF10);
    // Objects
    w[0] = -16000;
    w[1] = 16000;
//    sduObjectInit(&SDU2);
//    sduStart(&SDU2, &SerUsbCfg);
}

void UsbAudio_t::Connect() {
    usbDisconnectBus(&USBDrv);
    chThdSleepMilliseconds(1500);
    usbStart(&USBDrv, &UsbCfg);
    usbConnectBus(&USBDrv);
}

void UsbAudio_t::Put(uint16_t AValue) {
    // If the USB driver is not in the appropriate state then transactions must not be started
    if(usbGetDriverStateI(&USBD1) != USB_ACTIVE) return;
    IBuf.Put(AValue);
    // If there is no ongoing transaction then a new transaction is started
    if(!usbGetTransmitStatusI(&USBD1, EP_DATA_IN_ID)) {
        IBuf.Get(&ISample.Word);     // Something will be there definitely, as AValue was put there
        usbPrepareTransmit(&USBD1, EP_DATA_IN_ID, ISample.b, 2);
        chSysLock();
        usbStartTransmitI(&USBD1, EP_DATA_IN_ID);
        chSysUnlock();
    } // if not transmitting
}

