#include <Arduino.h>
#include <SPI.h>
#include <lvgl.h>
#include <TFT_eSPI.h>
#include <Wire.h>
#include <RTClib.h>
#include <max6675.h>
#include <tone32.h>
#include <PID_v1.h>

#define BUZZER_PIN 40
#define BUZZER_CHANNEL 0


//PID
double Setpoint, Input, Output;

double Kp=2, Ki=5, Kd =1;
PID myPID(&Input, &Output, &Setpoint, Kp, Ki, Kd, DIRECT);

int WindowSize = 5000;
unsigned long windowStartTime;

//MAX6675 Sensor: DataOut, ChipSelect, Clock, VCC and Instance of MAX6675
const int thermoDO = 26;
const int thermoCS = 27;
const int thermoCLK = 14;
const int vccPin = 12;
MAX6675 thermocouple(thermoCLK, thermoCS, thermoDO);
//OutputPin for relay
const int RELAY_PIN = 33;
//RTC
RTC_DS3231 rtc;
//REFLOWPARAMETERS
enum phase
{
    idle = 0,
    preheat = 1,
    soak = 2,
    reflow = 3,
    cooldown = 4

};
//Instance of phaseenum
phase currentPhase = idle;
struct profileStruct {

    int preheatTime = 120;
    int soakTime = 120;
    int reflowTime = 120;
    int cooldownTime = 120;

    int preheatCounter = 0;
    int soakCounter = 0;
    int reflowCounter = 0;
    int cooldownCounter = 0;

    float idleTemp = 0;
    float preheatTemp = 30;
    float soakTemp = 40;
    float reflowTemp = 60;

};
//Instance of profile-struct
profileStruct currentProfile;
//Messageindicator for serialprint
bool idleMessage = false;
bool preheatMessage = false;
bool soakMessage = false;
bool reflowMessage = false;
bool cooldownMessage = false;

const char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
//TFT Instance
TFT_eSPI tft = TFT_eSPI(); 
//LVGL stuff
static lv_disp_buf_t disp_buf;
static lv_color_t buf[LV_HOR_RES_MAX * 10];
//Displaysettings
const int screenWidth = 240;
const int screenHeight = 320;
//GUI objects
static lv_obj_t *temperature_meter;
static lv_obj_t *temperature_label;
static lv_obj_t *temperature_celsius;
static lv_obj_t *clockLabel;
static lv_obj_t *startbtnlabel;
static lv_obj_t *statuslabel;
//Timerstuff
unsigned long currentTime = 0;
unsigned long previous1Time = 0;
unsigned long previous2Time = 0;
unsigned long oneSecondInterval = 1000;
unsigned long threeSecondInterval = 3000;
//For assigning the current targettemperature
float currentTargetTemp = preheat;
//Hysteresisvalues
float upperHys = currentTargetTemp + 0.5;
float lowerHys = currentTargetTemp - 0.5;
//Bool for checking if RELAY_PIN is on or off
boolean heaterStatus = false;

#if USE_LV_LOG != 0
/* Serial debugging */
void my_print(lv_log_level_t level, const char *file, uint32_t line, const char *dsc)
{

    Serial.printf("%s@%d->%s\r\n", file, line, dsc);
    Serial.flush();
}
#endif

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
                                    );

    lv_obj_align(ddlist, NULL, LV_ALIGN_IN_TOP_MID, 0, 20);
    lv_obj_set_event_cb(ddlist, profile_handler);
}

void buzzer() {

  Serial.println("buzzZZ");
  tone(BUZZER_PIN, NOTE_C4, 500, BUZZER_CHANNEL);
  noTone(BUZZER_PIN, BUZZER_CHANNEL);
}

static void start_event_handler(lv_obj_t * obj, lv_event_t event)
{

    if(event == LV_EVENT_CLICKED) {
        Serial.println("Clicked\n");
    }
    else if(event == LV_EVENT_VALUE_CHANGED) {
        Serial.println("Toggled\n");

         if(currentPhase == idle){
             currentPhase = preheat;
             Serial.println("Process started.");
         }
         else if(currentPhase !=idle){
             currentPhase = cooldown;
             Serial.println("aborted.. cooling down");
         }

    }
}

void setup()
{
    lv_init();

    Serial.begin(9600); /* prepare for possible serial debug */

    pinMode(vccPin, OUTPUT);
    digitalWrite(vccPin, HIGH);
    pinMode(RELAY_PIN, OUTPUT);

#if USE_LV_LOG != 0
    lv_log_register_print_cb(my_print); /* register print function for debugging */
#endif

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
    //lv_obj_t *miscTab = lv_tabview_add_tab(tv, LV_SYMBOL_WIFI);

    // table, dropdown and charts

    createTable(profileTab, "59", "80", "102", "222");

    createDropdown(profileTab);

    int chart1[9] = {0, 20, 20, 40, 50, 60, 80, 80, 0};
    int chart2[9] = {0, 15, 18, 35, 55, 66, 77, 88, 0};
    createChart(chartTab, chart1, chart2);

    temperature_meter = lv_linemeter_create(homeTab, NULL);
    lv_obj_set_size(temperature_meter, 120, 120);
    lv_obj_align(temperature_meter, NULL, LV_ALIGN_CENTER, 0, 0);
    lv_linemeter_set_value(temperature_meter, 0);
    //Statuslabel on HomeTab
    statuslabel = lv_label_create(homeTab, NULL);
    lv_label_set_text(statuslabel, "Status: Idle");
    lv_obj_align(statuslabel, NULL, LV_ALIGN_IN_TOP_MID, 0, 0);
    //Startbutton on HomeTab
    lv_obj_t * startbtn = lv_btn_create(homeTab, NULL);
    lv_obj_set_event_cb(startbtn, start_event_handler);
    lv_obj_align(startbtn, NULL, LV_ALIGN_CENTER, 0, 90);
    lv_btn_set_checkable(startbtn, true);
    lv_btn_toggle(startbtn);

    startbtnlabel = lv_label_create(startbtn, NULL);
    lv_label_set_text(startbtnlabel, "START");

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

    //testing purposes
    currentPhase = idle;
}

void loop()
{
    //Setting Hysteresis-limits
    float upperHys = currentTargetTemp + 0.5;
    float lowerHys = currentTargetTemp - 0.5;

    currentTime = millis();

    if (currentTime - previous1Time >= oneSecondInterval){
        

        Serial.print(thermocouple.readCelsius());
        Serial.print("    ");
        Serial.print(upperHys);
        Serial.print("    ");
        Serial.println(lowerHys);

        /*
        DateTime time = rtc.now();
        char buf2[10] = "hh:mm:ss";
        Serial.print(" ");
        Serial.println(time.toString(buf2));
        */

        lv_linemeter_set_value(temperature_meter, thermocouple.readCelsius());
        updateTemperatureLabel(thermocouple.readCelsius());

     switch (currentPhase){

        case idle:
            currentTargetTemp = currentProfile.idleTemp;
            lv_label_set_text(statuslabel, "Status: Idle");
            if (idleMessage == false)
            {
                Serial.println("STATUS: IDLE");
                idleMessage = true;
            }
            
            break;
        case preheat:
        lv_label_set_text(statuslabel, "Status: preheat");
            if (currentProfile.preheatCounter < currentProfile.preheatTime)
            {
                currentTargetTemp = currentProfile.preheatTemp;
                if (preheatMessage == false) 
                {
                    Serial.println("STATUS: PREHEAT");
                    preheatMessage = true;
                }
                currentProfile.preheatCounter++;
            }
            else{
                currentProfile.preheatCounter = 0;
                preheatMessage = false;
                currentPhase = soak;
            }
            break;
        case soak:
            lv_label_set_text(statuslabel, "Status: soak");
            if (currentProfile.soakCounter < currentProfile.soakTime)
            {
            currentTargetTemp = currentProfile.soakTemp;

                if(soakMessage == false)
                {
                Serial.println("STATUS: SOAK");
                soakMessage = true;
                }

            currentProfile.soakCounter++;
            }
            else 
            {
                currentProfile.soakCounter = 0;
                soakMessage = false;
                currentPhase = reflow;
            }
            break;
        case reflow:
            lv_label_set_text(statuslabel, "Status: reflow");
            if (currentProfile.reflowCounter < currentProfile.reflowTime)
            {

            currentTargetTemp = currentProfile.reflowTemp;

                if(reflowMessage == false)
                {
                    Serial.println("STATUS: REFLOW");
                    reflowMessage = true;
                }
            currentProfile.reflowCounter++;

            }
            else 
            {
                currentProfile.reflowCounter = 0;
                reflowMessage = false;
                currentPhase = cooldown;
            }
            break;
        case cooldown:
            lv_label_set_text(statuslabel, "Status: cooldown");
            digitalWrite(RELAY_PIN, LOW);
            if (cooldownMessage == false){
            heaterStatus = false;
            currentTargetTemp = currentProfile.idleTemp;
            Serial.println("STATUS: COOLDOWN");
            cooldownMessage = true;
            buzzer(); //TODO - Buzzerfunction
            }
            currentProfile.cooldownCounter++;
            if (currentProfile.cooldownCounter >= currentProfile.cooldownTime){
                currentProfile.cooldownCounter = 0;
                idleMessage = false;
                preheatMessage = false;
                soakMessage = false;
                reflowMessage = false;
                cooldownMessage = false;
                currentPhase = idle;
            }
            break;

     }
     previous1Time = currentTime;

    if (currentPhase != idle && currentPhase != cooldown)
        {

        if (thermocouple.readCelsius() > upperHys)
            {
                // turn off heat
                if (heaterStatus == true)
                {
                    Serial.println("off");
                    digitalWrite(RELAY_PIN, LOW);
                    heaterStatus = false;
                }
            }
        if (thermocouple.readCelsius() < lowerHys)
            {
                //turn RELAY_PIN on
                if (heaterStatus == false)
                {
                    Serial.println("on");
                    digitalWrite(RELAY_PIN, HIGH);
                    heaterStatus = true;
                }
            }
        }
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