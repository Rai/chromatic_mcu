#include "color_correct_lcd.h"

#include "esp_log.h"
#include "osd_shared.h"
#include "lvgl.h"
#include "mutex.h"
#include "settings.h"

LV_IMG_DECLARE(img_toggle_on);
LV_IMG_DECLARE(img_toggle_off);

enum {
    kToggleBtnX_px = 77,
    kToggleBtnY_px = 55,
};

typedef struct ColorCorrectLCD {
    lv_obj_t* pImgToggleOffObj;
    lv_obj_t* pImgToggleOnObj;
    ColorCorrectLCDState_t eCurrentState;
    fnOnUpdateCb_t fnOnUpdateCb;
} ColorCorrectLCD_t;

static const char* TAG = "ColorCorrectLCD";
static ColorCorrectLCD_t _Ctx;

static void SaveToSettings(const ColorCorrectLCDState_t eNewState);

OSD_Result_t ColorCorrectLCD_Draw(void* arg)
{
    if (arg == NULL)
    {
        return kOSD_Result_Err_NullDataPtr;
    }

    lv_obj_t *const pScreen = (lv_obj_t *const) arg;

    if (_Ctx.eCurrentState == kColorCorrectLCDState_Off)
    {
        if (_Ctx.pImgToggleOffObj == NULL)
        {
            _Ctx.pImgToggleOffObj = lv_img_create(pScreen);
            lv_obj_align(_Ctx.pImgToggleOffObj, LV_ALIGN_TOP_LEFT, kToggleBtnX_px, kToggleBtnY_px);
            lv_img_set_src(_Ctx.pImgToggleOffObj, &img_toggle_off);
        }
    }
    else
    {
        if (_Ctx.pImgToggleOnObj == NULL)
        {
            _Ctx.pImgToggleOnObj = lv_img_create(pScreen);
            lv_obj_align(_Ctx.pImgToggleOnObj, LV_ALIGN_TOP_LEFT, kToggleBtnX_px, kToggleBtnY_px);
            lv_img_set_src(_Ctx.pImgToggleOnObj, &img_toggle_on);
        }
    }

    return kOSD_Result_Ok;
}

void ColorCorrectLCD_Update(ColorCorrectLCDState_t eNewState)
{
    if ((unsigned)eNewState >= kNumColorCorrectLCDState)
    {
        return;
    }

    ColorCorrectLCD_OnTransition(NULL);

    if (Mutex_Take(kMutexKey_ColorCorrectLCD) == kMutexResult_Ok)
    {
        const ColorCorrectLCDState_t eCurrentState = _Ctx.eCurrentState;
        _Ctx.eCurrentState = eNewState;

        if (eNewState != eCurrentState)
        {
            SaveToSettings(eNewState);
        }

        (void) Mutex_Give(kMutexKey_ColorCorrectLCD);
    }

    if (_Ctx.fnOnUpdateCb != NULL)
    {
        _Ctx.fnOnUpdateCb();
    }
}

OSD_Result_t ColorCorrectLCD_OnButton(const Button_t Button, const ButtonState_t State, void *arg)
{
    (void)arg;

    switch (Button)
    {
        case kButton_A:
        {
            if (State == kButtonState_Pressed)
            {
                ESP_LOGI(TAG, "Updating Color Correction LCD from %d", _Ctx.eCurrentState);

                ColorCorrectLCD_Update(_Ctx.eCurrentState ^ 1);
            }
            break;
        }
        default:
            break;
    }
    return kOSD_Result_Ok;
}

OSD_Result_t ColorCorrectLCD_OnTransition(void* arg)
{
    (void)arg;

    lv_obj_t** ToDelete[] = {
        &_Ctx.pImgToggleOffObj,
        &_Ctx.pImgToggleOnObj,
    };

    for (size_t i = 0; i < ARRAY_SIZE(ToDelete); i++)
    {
        if (*ToDelete[i] != NULL)
        {
            lv_obj_del(*ToDelete[i]);
            *(ToDelete[i]) = NULL;
        }
    }

    return kOSD_Result_Ok;
}

ColorCorrectLCDState_t ColorCorrectLCD_GetState(void)
{
    return _Ctx.eCurrentState;
}

OSD_Result_t ColorCorrectLCD_ApplySetting(SettingValue_t const *const pValue)
{
    if (pValue == NULL)
    {
        return kOSD_Result_Err_NullDataPtr;
    }

    if (pValue->eType == kSettingDataType_U8)
    {
        const ColorCorrectLCDState_t eState = (pValue->U8 == 0) ? kColorCorrectLCDState_Off : kColorCorrectLCDState_On;
        _Ctx.eCurrentState = eState;
    }
    else
    {
        return kOSD_Result_Err_UnexpectedSettingDataType;
    }

    return kOSD_Result_Ok;
}

void ColorCorrectUSB_RegisterOnUpdateCb(fnOnUpdateCb_t fnOnUpdate)
{
    _Ctx.fnOnUpdateCb = fnOnUpdate;
}

static void SaveToSettings(const ColorCorrectLCDState_t eNewState)
{
    OSD_Result_t eResult;
    if ((eResult = Settings_Update(kSettingKey_ColorCorrectLCD, eNewState)) != kOSD_Result_Ok)
    {
        ESP_LOGE(TAG, "LCD color correction save failed: %d", eResult);
    }
}
