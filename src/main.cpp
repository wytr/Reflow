#include <Arduino.h>
#include <SPI.h>
#include <lvgl.h>
#include <TFT_eSPI.h>
#include "Wire.h"
#include "RTClib.h"
#include <max6675.h>

// MAX6675 Sensor
int thermoDO = 26;
int thermoCS = 27;
int thermoCLK = 14;
int vccPin = 12;
MAX6675 thermocouple(thermoCLK, thermoCS, thermoDO);

RTC_DS3231 rtc;

char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

TFT_eSPI tft = TFT_eSPI(); /* TFT instance */
static lv_disp_buf_t disp_buf;
static lv_color_t buf[LV_HOR_RES_MAX * 10];

int screenWidth = 240;
int screenHeight = 320;
static lv_obj_t *temperature_meter;
static lv_obj_t *temperature_label;
static lv_obj_t *temperature_celsius;
static lv_obj_t *clockLabel;

unsigned long currentTime;
unsigned long previous1Time;
unsigned long previous2Time;
unsigned long secondInterval = 500;
unsigned long threeSecondInterval = 3000;

int heater = 33;
float upperHeat = 55;
float lowerHeat = 50;
boolean heaterStatus = false;

#if USE_LV_LOG != 0
/* Serial debugging */
void my_print(lv_log_level_t level, const char *file, uint32_t line, const char *dsc)
{

    Serial.printf("%s@%d->%s\r\n", file, line, dsc);
    Serial.flush();
}
#endif

/* Display flushing */
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p)
{
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushColors(&color_p->full, w * h, true);
    tft.endWrite();

    lv_disp_flush_ready(disp);
}

/*Read the touchpad*/
bool my_touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data)
{
    uint16_t touchX, touchY;

    bool touched = tft.getTouch(&touchX, &touchY, 600);

    if (!touched)
    {
        data->state = LV_INDEV_STATE_REL;
        return false;
    }
    else
    {
        data->state = LV_INDEV_STATE_PR;
    }

    if (touchX > screenWidth || touchY > screenHeight)
    {
        Serial.println("Y or y outside of expected parameters..");
        Serial.print("y:");
        Serial.print(touchX);
        Serial.print(" x:");
        Serial.print(touchY);
    }
    else
    {
        /*Set the coordinates*/
        data->point.x = touchX;
        data->point.y = touchY;

        Serial.print("Data x");
        Serial.println(touchX);

        Serial.print("Data y");
        Serial.println(touchY);
    }

    return false; /*Return `false` because we are not buffering and no more data to read*/
}

static void profile_handler(lv_obj_t *obj, lv_event_t event)
{
    if (event == LV_EVENT_VALUE_CHANGED)
    {
        // TODO load profile into table
        char buf[32];
        lv_dropdown_get_selected_str(obj, buf, sizeof(buf));
        printf("Option: %s\n", buf);
    }
}

void updateTemperatureLabel(float value)
{
    char buffer[12] = {0};
    snprintf(buffer, sizeof(buffer), "%.2f", value);
    lv_label_set_text(temperature_label, buffer);
}

static void createTable(lv_obj_t *parent, char *temp1, char *temp2, char *time1, char *time2)
{
    //TODO Set Profile Settings

    lv_obj_t *table = lv_table_create(parent, NULL);
    lv_obj_set_size(table, 200, 150);
    lv_obj_align(table, NULL, LV_ALIGN_CENTER, -5, 0);

    lv_table_set_col_width(table, 0, 65);
    lv_table_set_col_width(table, 1, 60);
    lv_table_set_col_width(table, 2, 60);

    lv_table_set_col_cnt(table, 3);
    lv_table_set_row_cnt(table, 2);
    /*Fill the first column*/
    lv_table_set_cell_value(table, 0, 0, "Soak");
    lv_table_set_cell_value(table, 1, 0, "Rflw");

    /*Fill the second column*/
    lv_table_set_cell_value(table, 0, 1, temp1);
    lv_table_set_cell_value(table, 1, 1, temp2);

    /*Fill the third column*/
    lv_table_set_cell_value(table, 0, 2, time1);
    lv_table_set_cell_value(table, 1, 2, time2);
}

static void createChart(lv_obj_t *parent, int chart1[9], int chart2[9])
{
    // TODO create chart from profile Data and from Sensor Values

    /*Create a chart*/
    lv_obj_t *chart;
    chart = lv_chart_create(parent, NULL);
    lv_obj_set_size(chart, 200, 150);
    lv_obj_align(chart, NULL, LV_ALIGN_CENTER, 0, 0);
    lv_chart_set_type(chart, LV_CHART_TYPE_LINE); /*Show lines and points too*/

    /*Add two data series*/
    lv_chart_series_t *ser1 = lv_chart_add_series(chart, LV_COLOR_BLACK);
    lv_chart_series_t *ser2 = lv_chart_add_series(chart, LV_COLOR_RED);

    /*Set the next points on 'ser1'*/
    const int SIZE = 9;
    for (int i = 0; i < SIZE; i++)
    {
        lv_chart_set_next(chart, ser1, chart1[i]);
    }

    /*Directly set points on 'ser2'*/
    for (int j = 0; j < SIZE; j++)
    {
        ser2->points[j] = chart2[j];
    }

    lv_chart_refresh(chart); /*Required after direct set*/
}

static void createDropdown(lv_obj_t *parent)
{
    // TODO load profiles from memory
    lv_obj_t *ddlist = lv_dropdown_create(parent, NULL);
    lv_dropdown_set_options(ddlist, "Profil1\n"
                                    "Profil2\n"
                                    "Graka\n"
                                    "Mainboard\n"
                                    "Raspberry");

    lv_obj_align(ddlist, NULL, LV_ALIGN_IN_TOP_MID, 0, 20);
    lv_obj_set_event_cb(ddlist, profile_handler);
}

void setup()
{
    Serial.begin(9600); /* prepare for possible serial debug */
    pinMode(vccPin, OUTPUT);
    digitalWrite(vccPin, HIGH);
    lv_init();

    pinMode(heater, OUTPUT);

#if USE_LV_LOG != 0
    lv_log_register_print_cb(my_print); /* register print function for debugging */
#endif

    pinMode(12, OUTPUT);

    tft.begin();        /* TFT init */
    tft.setRotation(0); /* Landscape orientation  = 1*/

    uint16_t calData[5] = {275, 3620, 264, 3532, 2};
    tft.setTouch(calData);

    lv_disp_buf_init(&disp_buf, buf, NULL, LV_HOR_RES_MAX * 10);

    /*Initialize the display*/
    lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = 240;
    disp_drv.ver_res = 320;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.buffer = &disp_buf;
    lv_disp_drv_register(&disp_drv);

    /*Initialize the (dummy) input device driver*/
    lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = my_touchpad_read;
    lv_indev_drv_register(&indev_drv);

    lv_theme_t *th = lv_theme_material_init(LV_THEME_DEFAULT_COLOR_PRIMARY, LV_THEME_DEFAULT_COLOR_SECONDARY, LV_THEME_DEFAULT_FLAG,
                                            LV_THEME_DEFAULT_FONT_SMALL, LV_THEME_DEFAULT_FONT_NORMAL, LV_THEME_DEFAULT_FONT_SUBTITLE, LV_THEME_DEFAULT_FONT_TITLE);
    lv_theme_set_act(th);

    //SCREEN
    lv_obj_t *scr = lv_cont_create(NULL, NULL);
    lv_scr_load(scr);
    //TABVIEW
    lv_obj_t *tv = lv_tabview_create(scr, NULL);
    lv_tabview_set_btns_pos(tv, LV_TABVIEW_TAB_POS_BOTTOM);
    lv_tabview_set_anim_time(tv, 50);
    lv_obj_set_size(tv, LV_HOR_RES_MAX, LV_VER_RES_MAX);
    //TABS
    lv_obj_t *homeTab = lv_tabview_add_tab(tv, LV_SYMBOL_HOME);
    lv_obj_t *profileTab = lv_tabview_add_tab(tv, LV_SYMBOL_SETTINGS);
    lv_obj_t *chartTab = lv_tabview_add_tab(tv, LV_SYMBOL_IMAGE);
    lv_obj_t *miscTab = lv_tabview_add_tab(tv, LV_SYMBOL_WIFI);

    // table, dropdown and charts
    createTable(profileTab, "59", "80", "102", "222");
    createDropdown(profileTab);

    int chart1[9] = {22, 33, 44, 55, 66, 77, 88, 99, 101};
    int chart2[9] = {11, 15, 33, 44, 55, 66, 77, 88, 99};
    createChart(chartTab, chart1, chart2);

    temperature_meter = lv_linemeter_create(homeTab, NULL);
    lv_obj_set_size(temperature_meter, 120, 120);
    lv_obj_align(temperature_meter, NULL, LV_ALIGN_CENTER, 0, 0);
    lv_linemeter_set_value(temperature_meter, 0);

    temperature_label = lv_label_create(homeTab, NULL);
    lv_label_set_text(temperature_label, "N/A");
    lv_obj_align(temperature_label, NULL, LV_ALIGN_CENTER, 0, 0);

    temperature_celsius = lv_label_create(homeTab, NULL);
    lv_label_set_text(temperature_celsius, "°C");
    lv_obj_align(temperature_celsius, NULL, LV_ALIGN_CENTER, 0, 20);

    clockLabel = lv_label_create(homeTab, NULL);
    lv_obj_align(clockLabel, NULL, LV_ALIGN_IN_TOP_LEFT, 3, 0);

    if (!rtc.begin())
    {
        Serial.println("Couldn't find RTC");
        while (1)
            ;
    }

    if (rtc.lostPower())
    {
        Serial.println("RTC lost power, lets set the time!");
        // following line sets the RTC to the date &amp; time this sketch was compiled
        rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
        // This line sets the RTC with an explicit date &amp; time, for example to set
        // January 21, 2014 at 3am you would call:
        // rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));
    }
}

void loop()
{
    currentTime = millis();

    if (currentTime - previous1Time >= secondInterval)
    {
        Serial.print(thermocouple.readCelsius());
        DateTime time = rtc.now();
        char buf2[10] = "hh:mm:ss";
        Serial.print(" ");
        Serial.println(time.toString(buf2));


        if (isnan(sqrt(thermocouple.readCelsius())))
        {
            Serial.println("Sensor value failure");
            // turn off heat
            digitalWrite(heater, LOW);
        }
        else
        {
            if (thermocouple.readCelsius() > upperHeat)
            {
                // turn off heat
                if (heaterStatus == true)
                {
                    Serial.println("Heater off");
                    digitalWrite(heater, LOW);
                    heaterStatus = false;
                }
            }
            if (thermocouple.readCelsius() < lowerHeat)
            {
                //keep heater on
                if (heaterStatus == false)
                {
                    Serial.println("Heater on");
                    digitalWrite(heater, HIGH);
                    heaterStatus = true;
                }
            }

            lv_linemeter_set_value(temperature_meter, thermocouple.readCelsius());
            updateTemperatureLabel(thermocouple.readCelsius());
        }

        previous1Time = currentTime;
    }

    if (currentTime - previous2Time >= threeSecondInterval)
    {
        char buf1[9] = "hh:mm";
        DateTime now = rtc.now();
        lv_label_set_text(clockLabel, now.toString(buf1));
        previous2Time = currentTime;
    }
    lv_task_handler();
}