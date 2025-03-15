#include "color_correct_lcd.h"

#include "esp_log.h"
#include "osd_shared.h"
#include "lvgl.h"
#include "mutex.h"
#include "settings.h"

LV_IMG_DECLARE(img_option_dis);
LV_IMG_DECLARE(img_option_en);

enum {
    kToggleOptX_px = 77,

    kToggleOptTop_Y_px = 52,
    kToggleOptBtm_Y_px = 76,
};

typedef struct ColorCorrectLCD {
    lv_obj_t* pImgOptionTopObj;
    lv_obj_t* pImgOptionBtmObj;
    lv_obj_t* pOptionTopTextObj;
    lv_obj_t* pOptionBtmTextObj;
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
    
    
    const bool IsDisabled = (_Ctx.eCurrentState == kColorCorrectLCDState_Off);

    if (_Ctx.pImgOptionTopObj == NULL)
    {
        _Ctx.pImgOptionTopObj = lv_img_create(pScreen);

        lv_obj_t* pOff = _Ctx.pImgOptionTopObj;
        lv_obj_align(pOff, LV_ALIGN_TOP_LEFT, kToggleOptX_px, IsDisabled ? kToggleOptBtm_Y_px : kToggleOptTop_Y_px);
        lv_img_set_src(pOff, &img_option_dis);
    }

    if (_Ctx.pImgOptionBtmObj == NULL)
    {
        _Ctx.pImgOptionBtmObj = lv_img_create(pScreen);

        lv_obj_t* pOn = _Ctx.pImgOptionBtmObj;
        lv_obj_align(pOn, LV_ALIGN_TOP_LEFT, kToggleOptX_px, IsDisabled ? kToggleOptTop_Y_px : kToggleOptBtm_Y_px);
        lv_img_set_src(pOn, &img_option_en);
    }

    if (_Ctx.pOptionTopTextObj == NULL)
    {
        _Ctx.pOptionTopTextObj = lv_label_create(IsDisabled ? _Ctx.pImgOptionBtmObj : _Ctx.pImgOptionTopObj);
        lv_obj_align(_Ctx.pOptionTopTextObj, LV_ALIGN_CENTER, 1, 1);
        lv_label_set_long_mode(_Ctx.pOptionTopTextObj, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_align(_Ctx.pOptionTopTextObj, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_width(_Ctx.pOptionTopTextObj, img_option_en.header.w * 0.50);
        lv_obj_add_style(_Ctx.pOptionTopTextObj, OSD_GetStyleTextBlack(), 0);
        lv_label_set_text(_Ctx.pOptionTopTextObj, "RAW COLOR");
    }

    if (_Ctx.pOptionBtmTextObj == NULL)
    {
        _Ctx.pOptionBtmTextObj = lv_label_create(IsDisabled ? _Ctx.pImgOptionTopObj : _Ctx.pImgOptionBtmObj);
        lv_obj_align(_Ctx.pOptionBtmTextObj, LV_ALIGN_CENTER, 1, 1);
        lv_label_set_long_mode(_Ctx.pOptionBtmTextObj, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_align(_Ctx.pOptionBtmTextObj, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_width(_Ctx.pOptionBtmTextObj, img_option_en.header.w * 0.80);
        lv_obj_add_style(_Ctx.pOptionBtmTextObj, OSD_GetStyleTextBlack(), 0);
        lv_label_set_text(_Ctx.pOptionBtmTextObj, "CORRECTED COLOR");
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
                const ColorCorrectLCDState_t eState = _Ctx.eCurrentState;
                ESP_LOGI(TAG, "Updating Color Correction LCD from %d", eState);

                ColorCorrectLCD_Update(eState ^ 1);
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
        &_Ctx.pOptionTopTextObj,
        &_Ctx.pOptionBtmTextObj,
        &_Ctx.pImgOptionTopObj,
        &_Ctx.pImgOptionBtmObj,
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
