#define MANUFACTURER_ID 57073
#define DEVICE_ID 1
#define REVISION 2

#define COMOBJ_motion 0
#define COMOBJ_eco2 1
#define COMOBJ_eco2MaxLimitReached 2
#define COMOBJ_tvoc 3
#define COMOBJ_tvocMaxLimitReached 4
#define COMOBJ_temp 5
#define COMOBJ_tempMinLimitReached 6
#define COMOBJ_tempMaxLimitReached 7
#define COMOBJ_rh 8
#define COMOBJ_rhMinLimitReached 9
#define COMOBJ_rhMaxLimitReached 10
#define COMOBJ_press 11
#define COMOBJ_lux 12
#define PARAM_ledMode 0
#define PARAM_cyclicResendIntervalMs 1
#define PARAM_as312TriggerMode 2
#define PARAM_ccs811Mode 3
#define PARAM_ccs811Eco2DiffReportingThreshold 4
#define PARAM_ccs811Eco2MaxLimit 5
#define PARAM_ccs811Eco2MaxTriggeredValue 6
#define PARAM_ccs811TvocDiffReportingThreshold 7
#define PARAM_ccs811TvocMaxLimit 8
#define PARAM_ccs811TvocMaxTriggeredValue 9
#define PARAM_bme280PollingIntervalMs 10
#define PARAM_bme280TempDiffReportingThreshold 11
#define PARAM_bme280TempMinLimit 12
#define PARAM_bme280TempMinTriggeredValue 13
#define PARAM_bme280TempMaxLimit 14
#define PARAM_bme280TempMaxTriggeredValue 15
#define PARAM_bme280RhDiffReportingThreshold 16
#define PARAM_bme280RhMinLimit 17
#define PARAM_bme280RhMinTriggeredValue 18
#define PARAM_bme280RhMaxLimit 19
#define PARAM_bme280RhMaxTriggeredValue 20
#define PARAM_bme280PressDiffReportingThreshold 21
#define PARAM_max44009LuxDiffReportingThresholdPct 22
        
KnxComObject KnxDevice::_comObjectsList[] = {
    /* Index 0 - motion */ KnxComObject(KNX_DPT_1_001, 0x34),
    /* Index 1 - eco2 */ KnxComObject(KNX_DPT_9_008, 0x34),
    /* Index 2 - eco2MaxLimitReached */ KnxComObject(KNX_DPT_1_001, 0x34),
    /* Index 3 - tvoc */ KnxComObject(KNX_DPT_9_008, 0x34),
    /* Index 4 - tvocMaxLimitReached */ KnxComObject(KNX_DPT_1_001, 0x34),
    /* Index 5 - temp */ KnxComObject(KNX_DPT_9_001, 0x34),
    /* Index 6 - tempMinLimitReached */ KnxComObject(KNX_DPT_1_001, 0x34),
    /* Index 7 - tempMaxLimitReached */ KnxComObject(KNX_DPT_1_001, 0x34),
    /* Index 8 - rh */ KnxComObject(KNX_DPT_9_007, 0x34),
    /* Index 9 - rhMinLimitReached */ KnxComObject(KNX_DPT_1_001, 0x34),
    /* Index 10 - rhMaxLimitReached */ KnxComObject(KNX_DPT_1_001, 0x34),
    /* Index 11 - press */ KnxComObject(KNX_DPT_9_006, 0x34),
    /* Index 12 - lux */ KnxComObject(KNX_DPT_9_004, 0x34)
};
const byte KnxDevice::_numberOfComObjects = sizeof (_comObjectsList) / sizeof (KnxComObject); // do not change this code
       
byte KonnektingDevice::_paramSizeList[] = {
    /* Index 0 - ledMode */ PARAM_UINT8,
    /* Index 1 - cyclicResendIntervalMs */ PARAM_UINT32,
    /* Index 2 - as312TriggerMode */ PARAM_UINT8,
    /* Index 3 - ccs811Mode */ PARAM_UINT8,
    /* Index 4 - ccs811Eco2DiffReportingThreshold */ PARAM_UINT16,
    /* Index 5 - ccs811Eco2MaxLimit */ PARAM_UINT16,
    /* Index 6 - ccs811Eco2MaxTriggeredValue */ PARAM_UINT8,
    /* Index 7 - ccs811TvocDiffReportingThreshold */ PARAM_UINT16,
    /* Index 8 - ccs811TvocMaxLimit */ PARAM_UINT16,
    /* Index 9 - ccs811TvocMaxTriggeredValue */ PARAM_UINT8,
    /* Index 10 - bme280PollingIntervalMs */ PARAM_UINT32,
    /* Index 11 - bme280TempDiffReportingThreshold */ PARAM_UINT8,
    /* Index 12 - bme280TempMinLimit */ PARAM_INT8,
    /* Index 13 - bme280TempMinTriggeredValue */ PARAM_UINT8,
    /* Index 14 - bme280TempMaxLimit */ PARAM_INT8,
    /* Index 15 - bme280TempMaxTriggeredValue */ PARAM_UINT8,
    /* Index 16 - bme280RhDiffReportingThreshold */ PARAM_UINT8,
    /* Index 17 - bme280RhMinLimit */ PARAM_UINT8,
    /* Index 18 - bme280RhMinTriggeredValue */ PARAM_UINT8,
    /* Index 19 - bme280RhMaxLimit */ PARAM_UINT8,
    /* Index 20 - bme280RhMaxTriggeredValue */ PARAM_UINT8,
    /* Index 21 - bme280PressDiffReportingThreshold */ PARAM_UINT8,
    /* Index 22 - max44009LuxDiffReportingThresholdPct */ PARAM_UINT8
};
const byte KonnektingDevice::_numberOfParams = sizeof (_paramSizeList); // do not change this code