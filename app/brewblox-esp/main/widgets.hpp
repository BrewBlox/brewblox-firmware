#include "lvgl.h"
#include <string>
class NormalWidget {
public:
    NormalWidget(lv_obj_t* grid)
    {
        makeObj(grid, "label", "value1", "value2");
    }
    NormalWidget(lv_obj_t* grid, std::string labelTxt, std::string value1Txt, std::string value2Txt)
    {
        makeObj(grid, labelTxt.c_str(), value1Txt.c_str(), value2Txt.c_str());
    }

    void setLabel(std::string txt)
    {
        lv_label_set_text(label, txt.c_str());
    }

    void setValue1(std::string txt)
    {
        lv_label_set_text(value1, txt.c_str());
    }

    void setValue2(std::string txt)
    {
        lv_label_set_text(value2, txt.c_str());
    }

private:
    void makeObj(lv_obj_t* grid, const char* labelTxt, const char* value1Txt, const char* value2Txt)
    {
        obj = lv_obj_create(grid, NULL);
        lv_obj_set_size(obj, 140, 140);

        label = lv_label_create(obj, NULL);
        lv_label_set_text(label, labelTxt);
        lv_obj_align(label, NULL, LV_ALIGN_CENTER, 0, 50);
        lv_label_set_align(label, LV_LABEL_ALIGN_CENTER);

        value1 = lv_label_create(obj, NULL);
        lv_label_set_text(value1, value1Txt);
        lv_obj_align(value1, NULL, LV_ALIGN_CENTER, 0, 0);
        lv_label_set_align(value1, LV_LABEL_ALIGN_CENTER);

        value2 = lv_label_create(obj, NULL);
        lv_label_set_text(value2, value2Txt);
        lv_obj_align(value2, NULL, LV_ALIGN_CENTER, 0, -40);
        lv_label_set_align(value2, LV_LABEL_ALIGN_CENTER);
    }
    lv_obj_t* obj;
    lv_obj_t* label;
    lv_obj_t* value1;
    lv_obj_t* value2;
};

class PidWidget {
public:
    PidWidget(lv_obj_t* grid)
    {
        makeObj(grid, "PID", "1.0", "2.0", "3.0", "4.0");
    }
    PidWidget(lv_obj_t* grid, std::string labelTxt, std::string value1Txt, std::string value2Txt, std::string value3Txt, std::string value4Txt)
    {
        makeObj(grid, labelTxt.c_str(), value1Txt.c_str(), value2Txt.c_str(), value3Txt.c_str(), value4Txt.c_str());
    }

    void setLabel(std::string txt)
    {
        lv_label_set_text(label, txt.c_str());
    }

    void setValue1(std::string txt)
    {
        lv_label_set_text(value1, txt.c_str());
    }

    void setValue2(std::string txt)
    {
        lv_label_set_text(value2, txt.c_str());
    }
    void setValue3(std::string txt)
    {
        lv_label_set_text(value3, txt.c_str());
    }
    void setValue4(std::string txt)
    {
        lv_label_set_text(value4, txt.c_str());
    }

private:
    void makeObj(lv_obj_t* grid, const char* labelTxt, const char* value1Txt, const char* value2Txt, const char* value3Txt, const char* value4Txt)
    {
        obj = lv_obj_create(grid, NULL);
        lv_obj_set_size(obj, 140, 140);

        label = lv_label_create(obj, NULL);
        lv_label_set_text(label, labelTxt);
        lv_obj_align(label, NULL, LV_ALIGN_CENTER, 0, 50);
        lv_label_set_align(label, LV_LABEL_ALIGN_CENTER);

        value1 = lv_label_create(obj, NULL);
        lv_label_set_text(value1, value1Txt);
        lv_obj_align(value1, NULL, LV_ALIGN_CENTER, -20, 0);
        lv_label_set_align(value1, LV_LABEL_ALIGN_CENTER);

        value2 = lv_label_create(obj, NULL);
        lv_label_set_text(value2, value2Txt);
        lv_obj_align(value2, NULL, LV_ALIGN_CENTER, -20, -40);
        lv_label_set_align(value2, LV_LABEL_ALIGN_CENTER);

        value3 = lv_label_create(obj, NULL);
        lv_label_set_text(value3, value3Txt);
        lv_obj_align(value3, NULL, LV_ALIGN_CENTER, 20, 0);
        lv_label_set_align(value3, LV_LABEL_ALIGN_CENTER);

        value4 = lv_label_create(obj, NULL);
        lv_label_set_text(value4, value4Txt);
        lv_obj_align(value4, NULL, LV_ALIGN_CENTER, 20, -40);
        lv_label_set_align(value4, LV_LABEL_ALIGN_CENTER);
    }
    lv_obj_t* obj;
    lv_obj_t* label;
    lv_obj_t* value1;
    lv_obj_t* value2;
    lv_obj_t* value3;
    lv_obj_t* value4;
};