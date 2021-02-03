// Copyright (c) 2015-2019 The HomeKit ADK Contributors
//
// Licensed under the Apache License, Version 2.0 (the “License”);
// you may not use this file except in compliance with the License.
// See [CONTRIBUTORS.md] for the list of HomeKit ADK project authors.

// An example that implements the light bulb HomeKit profile. It can serve as a basic implementation for
// any platform. The accessory logic implementation is reduced to internal state updates and log output.
//
// This implementation is platform-independent.
//
// The code consists of multiple parts:
//
//   1. The definition of the accessory configuration and its internal state.
//
//   2. Helper functions to load and save the state of the accessory.
//
//   3. The definitions for the HomeKit attribute database.
//
//   4. The callbacks that implement the actual behavior of the accessory, in this
//      case here they merely access the global accessory state variable and write
//      to the log to make the behavior easily observable.
//
//   5. The initialization of the accessory state.
//
//   6. Callbacks that notify the server in case their associated value has changed.

#include "HAP.h"

#include "App.h"
#include "DB.h"
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Domain used in the key value store for application data.
 *
 * Purged: On factory reset.
 */
#define kAppKeyValueStoreDomain_Configuration ((HAPPlatformKeyValueStoreDomain) 0x00)

/**
 * Key used in the key value store to store the configuration state.
 *
 * Purged: On factory reset.
 */
#define kAppKeyValueStoreKey_Configuration_State ((HAPPlatformKeyValueStoreDomain) 0x00)

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Global accessory configuration.
 */
typedef struct {
    struct {
        bool lightBulbOn;
        float lightBulbHue;
        float lightBulbSaturation;
        int32_t lightBulbBrightness;
        bool whiteOn;
        int32_t whiteBrightness;
        uint32_t whiteColorTemperature;
    } state;
    HAPAccessoryServerRef* server;
    HAPPlatformKeyValueStoreRef keyValueStore;
} AccessoryConfiguration;

static AccessoryConfiguration accessoryConfiguration;

//----------------------------------------------------------------------------------------------------------------------

/**
 * Load the accessory state from persistent memory.
 */
static void LoadAccessoryState(void) {
    HAPPrecondition(accessoryConfiguration.keyValueStore);

    HAPError err;

    // Load persistent state if available
    bool found;
    size_t numBytes;

    err = HAPPlatformKeyValueStoreGet(
            accessoryConfiguration.keyValueStore,
            kAppKeyValueStoreDomain_Configuration,
            kAppKeyValueStoreKey_Configuration_State,
            &accessoryConfiguration.state,
            sizeof accessoryConfiguration.state,
            &numBytes,
            &found);

    if (err) {
        HAPAssert(err == kHAPError_Unknown);
        HAPFatalError();
    }
    if (!found || numBytes != sizeof accessoryConfiguration.state) {
        if (found) {
            HAPLogError(&kHAPLog_Default, "Unexpected app state found in key-value store. Resetting to default.");
        }
        HAPRawBufferZero(&accessoryConfiguration.state, sizeof accessoryConfiguration.state);
    }
}

/**
 * Save the accessory state to persistent memory.
 */
static void SaveAccessoryState(void) {
    HAPPrecondition(accessoryConfiguration.keyValueStore);

    HAPError err;
    err = HAPPlatformKeyValueStoreSet(
            accessoryConfiguration.keyValueStore,
            kAppKeyValueStoreDomain_Configuration,
            kAppKeyValueStoreKey_Configuration_State,
            &accessoryConfiguration.state,
            sizeof accessoryConfiguration.state);
    if (err) {
        HAPAssert(err == kHAPError_Unknown);
        HAPFatalError();
    }
}

//----------------------------------------------------------------------------------------------------------------------

/**
 * HomeKit accessory that provides the Bridge service.
 *
 * Note: Not constant to enable BCT Manual Name Change.
 */
static HAPAccessory accessory = { .aid = 1,
                                  .category = kHAPAccessoryCategory_Bridges,
                                  .name = "Snow Mesh Bridge",
                                  .manufacturer = "Snow",
                                  .model = "Mesh Bridge 1,1",
                                  .serialNumber = "000000000001",
                                  .firmwareVersion = "1",
                                  .hardwareVersion = "1",
                                  .services = (const HAPService* const[]) { &accessoryInformationService,
                                                                            &hapProtocolInformationService,
                                                                            &pairingService,
                                                                            NULL },
                                  .callbacks = { .identify = IdentifyAccessory } };

/**
 * HomeKit accessory that provides the Light Bulb service.
 *
 * Note: Not constant to enable BCT Manual Name Change.
 */
static HAPAccessory lightBulbAccessory = { .aid = 2,
                                  .category = kHAPAccessoryCategory_BridgedAccessory,
                                  .name = "Snow Light Bulb",
                                  .manufacturer = "Snow",
                                  .model = "LightBulb 1,1",
                                  .serialNumber = "000000000002",
                                  .firmwareVersion = "1",
                                  .hardwareVersion = "1",
                                  .services = (const HAPService* const[]) { &accessoryInformationService,
                                                                            &lightBulbService,
                                                                            &whiteService,
                                                                            NULL },
                                  .callbacks = { .identify = IdentifyAccessory } };

const HAPAccessory* const* bridgedAccessories = (const HAPAccessory *const[]){ &lightBulbAccessory,
                                                                               NULL};
//----------------------------------------------------------------------------------------------------------------------

void mble_mesh_model_set(uint16_t dst, uint8_t *data, uint32_t len);

static void MeshSetOnOff(bool onff)
{
    uint8_t data[4];

    /* TID */
    data[0] = 0x00; 

    /* ONOFF type = 0x0100 */
    data[1] = 0x00; 
    data[2] = 0x01;

    /* Value: uint8_t */
    data[3] = onff;

    mble_mesh_model_set(0x0004, data, sizeof(data));
}

static void MeshSetHSB(float hue, float saturation, uint8_t brightness)
{
    uint8_t data[7];

    /* TID */
    data[0] = 0x00; 

    /* HSV type = 0x0123 */
    data[1] = 0x23; 
    data[2] = 0x01;

    /* Hue: uint16_t */
    data[3] = (uint16_t)hue & 0xFF;
    data[4] = (uint16_t)hue >> 8;

    /* Saturation: uint8_t */
    data[5] = (uint8_t)saturation;

    /* Brightness: uint8_t */
    data[6] = brightness;

    mble_mesh_model_set(0x0004, data, sizeof(data));
}

static void MeshSetBrightness(uint8_t brightness)
{
    uint8_t data[5];

    /* TID */
    data[0] = 0x00;

    /* Brightness type = 0x0121 */
    data[1] = 0x21;
    data[2] = 0x01;

    /* Brightness: uint16_t */
    data[3] = brightness & 0xFF;
    data[4] = brightness >> 8;

    mble_mesh_model_set(0x0004, data, sizeof(data));
}

static void MeshSetColorTemperature(uint32_t colorTemperature)
{
    uint8_t data[4];

    /* TID */
    data[0] = 0x00;

    /* ColorTemperature Percent type = 0x01F1 */
    data[1] = 0xF1;
    data[2] = 0x01;

    /* ColorTemperature Percent: uint8_t */
    
    data[3] = (300 - colorTemperature) * 100 / 350;

    mble_mesh_model_set(0x0004, data, sizeof(data));
}

HAP_RESULT_USE_CHECK
HAPError IdentifyAccessory(
        HAPAccessoryServerRef* server HAP_UNUSED,
        const HAPAccessoryIdentifyRequest* request HAP_UNUSED,
        void* _Nullable context HAP_UNUSED) {
    HAPLogInfo(&kHAPLog_Default, "%s", __func__);
    return kHAPError_None;
}

HAP_RESULT_USE_CHECK
HAPError HandleLightBulbOnRead(
        HAPAccessoryServerRef* server HAP_UNUSED,
        const HAPBoolCharacteristicReadRequest* request HAP_UNUSED,
        bool* value,
        void* _Nullable context HAP_UNUSED) {
    *value = accessoryConfiguration.state.lightBulbOn;
    HAPLogInfo(&kHAPLog_Default, "%s: %s", __func__, *value ? "true" : "false");

    return kHAPError_None;
}

HAP_RESULT_USE_CHECK
HAPError HandleLightBulbOnWrite(
        HAPAccessoryServerRef* server,
        const HAPBoolCharacteristicWriteRequest* request,
        bool value,
        void* _Nullable context HAP_UNUSED) {
    HAPLogInfo(&kHAPLog_Default, "%s: %s", __func__, value ? "true" : "false");
    if (accessoryConfiguration.state.lightBulbOn != value) {
        accessoryConfiguration.state.lightBulbOn = value;

        MeshSetOnOff(accessoryConfiguration.state.lightBulbOn);

        SaveAccessoryState();

        HAPAccessoryServerRaiseEvent(server, request->characteristic, request->service, request->accessory);
    }

    return kHAPError_None;
}

HAP_RESULT_USE_CHECK
HAPError HandleLightBulbHueRead(
        HAPAccessoryServerRef* server HAP_UNUSED,
        const HAPFloatCharacteristicReadRequest* request HAP_UNUSED,
        float* value,
        void* _Nullable context HAP_UNUSED) {
    *value = accessoryConfiguration.state.lightBulbHue;
    HAPLogInfo(&kHAPLog_Default, "%s: %g", __func__, *value);

    return kHAPError_None;
}

HAP_RESULT_USE_CHECK
HAPError HandleLightBulbHueWrite(
        HAPAccessoryServerRef* server,
        const HAPFloatCharacteristicWriteRequest* request,
        float value,
        void* _Nullable context HAP_UNUSED) {
    HAPLogInfo(&kHAPLog_Default, "%s: %g", __func__, value);
    if (accessoryConfiguration.state.lightBulbHue != value) {
        accessoryConfiguration.state.lightBulbHue = value;

        MeshSetHSB(accessoryConfiguration.state.lightBulbHue, accessoryConfiguration.state.lightBulbSaturation, accessoryConfiguration.state.lightBulbBrightness);

        SaveAccessoryState();

        HAPAccessoryServerRaiseEvent(server, request->characteristic, request->service, request->accessory);
    }

    return kHAPError_None;
}

HAP_RESULT_USE_CHECK
HAPError HandleLightBulbSaturationRead(
        HAPAccessoryServerRef* server HAP_UNUSED,
        const HAPFloatCharacteristicReadRequest* request HAP_UNUSED,
        float* value,
        void* _Nullable context HAP_UNUSED) {
    *value = accessoryConfiguration.state.lightBulbSaturation;
    HAPLogInfo(&kHAPLog_Default, "%s: %g", __func__, *value);

    return kHAPError_None;
}

HAP_RESULT_USE_CHECK
HAPError HandleLightBulbSaturationWrite(
        HAPAccessoryServerRef* server,
        const HAPFloatCharacteristicWriteRequest* request,
        float value,
        void* _Nullable context HAP_UNUSED) {
    HAPLogInfo(&kHAPLog_Default, "%s: %g", __func__, value);
    if (accessoryConfiguration.state.lightBulbSaturation != value) {
        accessoryConfiguration.state.lightBulbSaturation = value;

        MeshSetHSB(accessoryConfiguration.state.lightBulbHue, accessoryConfiguration.state.lightBulbSaturation, accessoryConfiguration.state.lightBulbBrightness);

        SaveAccessoryState();

        HAPAccessoryServerRaiseEvent(server, request->characteristic, request->service, request->accessory);
    }

    return kHAPError_None;
}

HAP_RESULT_USE_CHECK
HAPError HandleLightBulbBrightnessRead(
        HAPAccessoryServerRef* server HAP_UNUSED,
        const HAPIntCharacteristicReadRequest* request HAP_UNUSED,
        int32_t* value,
        void* _Nullable context HAP_UNUSED) {
    *value = accessoryConfiguration.state.lightBulbBrightness;
    HAPLogInfo(&kHAPLog_Default, "%s: %d", __func__, *value);

    return kHAPError_None;
}

HAP_RESULT_USE_CHECK
HAPError HandleLightBulbBrightnessWrite(
        HAPAccessoryServerRef* server,
        const HAPIntCharacteristicWriteRequest* request,
        int32_t value,
        void* _Nullable context HAP_UNUSED) {
    HAPLogInfo(&kHAPLog_Default, "%s: %d", __func__, value);
    if (accessoryConfiguration.state.lightBulbBrightness != value) {
        accessoryConfiguration.state.lightBulbBrightness = value;

        MeshSetHSB(accessoryConfiguration.state.lightBulbHue, accessoryConfiguration.state.lightBulbSaturation, accessoryConfiguration.state.lightBulbBrightness);

        SaveAccessoryState();

        HAPAccessoryServerRaiseEvent(server, request->characteristic, request->service, request->accessory);
    }

    return kHAPError_None;
}


HAP_RESULT_USE_CHECK
HAPError HandleWhiteOnRead(
        HAPAccessoryServerRef* server HAP_UNUSED,
        const HAPBoolCharacteristicReadRequest* request HAP_UNUSED,
        bool* value,
        void* _Nullable context HAP_UNUSED) {
    *value = accessoryConfiguration.state.whiteOn;
    HAPLogInfo(&kHAPLog_Default, "%s: %s", __func__, *value ? "true" : "false");

    return kHAPError_None;
}

HAP_RESULT_USE_CHECK
HAPError HandleWhiteOnWrite(
        HAPAccessoryServerRef* server,
        const HAPBoolCharacteristicWriteRequest* request,
        bool value,
        void* _Nullable context HAP_UNUSED) {
    HAPLogInfo(&kHAPLog_Default, "%s: %s", __func__, value ? "true" : "false");
    if (accessoryConfiguration.state.whiteOn != value) {
        accessoryConfiguration.state.whiteOn = value;

        MeshSetOnOff(accessoryConfiguration.state.whiteOn);

        SaveAccessoryState();

        HAPAccessoryServerRaiseEvent(server, request->characteristic, request->service, request->accessory);
    }

    return kHAPError_None;
}

HAP_RESULT_USE_CHECK
HAPError HandleWhiteBrightnessRead(
        HAPAccessoryServerRef* server HAP_UNUSED,
        const HAPIntCharacteristicReadRequest* request HAP_UNUSED,
        int32_t* value,
        void* _Nullable context HAP_UNUSED) {
    *value = accessoryConfiguration.state.whiteBrightness;
    HAPLogInfo(&kHAPLog_Default, "%s: %d", __func__, *value);

    return kHAPError_None;
}

HAP_RESULT_USE_CHECK
HAPError HandleWhiteBrightnessWrite(
        HAPAccessoryServerRef* server,
        const HAPIntCharacteristicWriteRequest* request,
        int32_t value,
        void* _Nullable context HAP_UNUSED) {
    HAPLogInfo(&kHAPLog_Default, "%s: %d", __func__, value);
    if (accessoryConfiguration.state.whiteBrightness != value) {
        accessoryConfiguration.state.whiteBrightness = value;

        MeshSetBrightness(value);

        SaveAccessoryState();

        HAPAccessoryServerRaiseEvent(server, request->characteristic, request->service, request->accessory);
    }

    return kHAPError_None;
}

HAP_RESULT_USE_CHECK
HAPError HandleWhiteColorTemperatureRead(
        HAPAccessoryServerRef* server HAP_UNUSED,
        const HAPUInt32CharacteristicReadRequest* request HAP_UNUSED,
        uint32_t* value,
        void* _Nullable context HAP_UNUSED) {
    if (accessoryConfiguration.state.whiteColorTemperature < 50)
    {
        accessoryConfiguration.state.whiteColorTemperature = 50;
        SaveAccessoryState();
    }
    *value = accessoryConfiguration.state.whiteColorTemperature;
    HAPLogInfo(&kHAPLog_Default, "%s: %d", __func__, *value);

    return kHAPError_None;
}

HAP_RESULT_USE_CHECK
HAPError HandleWhiteColorTemperatureWrite(
        HAPAccessoryServerRef* server,
        const HAPUInt32CharacteristicWriteRequest* request,
        uint32_t value,
        void* _Nullable context HAP_UNUSED) {
    HAPLogInfo(&kHAPLog_Default, "%s: %d", __func__, value);
    if (accessoryConfiguration.state.whiteColorTemperature != value) {
        accessoryConfiguration.state.whiteColorTemperature = value;

        MeshSetColorTemperature(value);

        SaveAccessoryState();

        HAPAccessoryServerRaiseEvent(server, request->characteristic, request->service, request->accessory);
    }

    return kHAPError_None;
}

//----------------------------------------------------------------------------------------------------------------------

void AccessoryNotification(
        const HAPAccessory* accessory,
        const HAPService* service,
        const HAPCharacteristic* characteristic,
        void* ctx HAP_UNUSED) {
    HAPLogInfo(&kHAPLog_Default, "Accessory Notification");

    HAPAccessoryServerRaiseEvent(accessoryConfiguration.server, characteristic, service, accessory);
}

void AppCreate(HAPAccessoryServerRef* server, HAPPlatformKeyValueStoreRef keyValueStore) {
    HAPPrecondition(server);
    HAPPrecondition(keyValueStore);

    HAPLogInfo(&kHAPLog_Default, "%s", __func__);

    HAPRawBufferZero(&accessoryConfiguration, sizeof accessoryConfiguration);
    accessoryConfiguration.server = server;
    accessoryConfiguration.keyValueStore = keyValueStore;
    LoadAccessoryState();
}

void AppRelease(void) {
}

void AppAccessoryServerStart(void) {
    HAPAccessoryServerStartBridge(accessoryConfiguration.server, 
    &accessory,
    bridgedAccessories,
    true);
}

//----------------------------------------------------------------------------------------------------------------------

void AccessoryServerHandleUpdatedState(HAPAccessoryServerRef* server, void* _Nullable context) {
    HAPPrecondition(server);
    HAPPrecondition(!context);

    switch (HAPAccessoryServerGetState(server)) {
        case kHAPAccessoryServerState_Idle: {
            HAPLogInfo(&kHAPLog_Default, "Accessory Server State did update: Idle.");
            return;
        }
        case kHAPAccessoryServerState_Running: {
            HAPLogInfo(&kHAPLog_Default, "Accessory Server State did update: Running.");
            return;
        }
        case kHAPAccessoryServerState_Stopping: {
            HAPLogInfo(&kHAPLog_Default, "Accessory Server State did update: Stopping.");
            return;
        }
    }
    HAPFatalError();
}

const HAPAccessory* AppGetAccessoryInfo() {
    return &accessory;
}

void AppInitialize(
        HAPAccessoryServerOptions* hapAccessoryServerOptions HAP_UNUSED,
        HAPPlatform* hapPlatform HAP_UNUSED,
        HAPAccessoryServerCallbacks* hapAccessoryServerCallbacks HAP_UNUSED) {
    /*no-op*/
}

void AppDeinitialize() {
    /*no-op*/
}
