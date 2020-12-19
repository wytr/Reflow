#include <Arduino.h>
#include <SPI.h>
#include <lvgl.h>
#include <TFT_eSPI.h>
#include <stdio.h>



TFT_eSPI tft = TFT_eSPI(); /* TFT instance */
static lv_disp_buf_t disp_buf;
static lv_color_t buf[LV_HOR_RES_MAX * 10];

int screenWidth = 240;
int screenHeight = 320;
int temp = 1;
int upw = 1;
int dws = 0;
static lv_obj_t * temperature_meter;
static lv_obj_t * temperature_label;
static lv_obj_t * temperature_celsius;



#if USE_LV_LOG != 0
/* Serial debugging */
void my_print(lv_log_level_t level, const char * file, uint32_t line, const char * dsc)
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
bool my_touchpad_read(lv_indev_drv_t * indev_driver, lv_indev_data_t * data)
{
    uint16_t touchX, touchY;

    bool touched = tft.getTouch(&touchX, &touchY, 600);

    if(!touched)
    {
      data->state = LV_INDEV_STATE_REL;
      return false;
    }
    else
    {
      data->state = LV_INDEV_STATE_PR;
    }

    if(touchX>screenWidth || touchY > screenHeight)
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

void printEvent(String Event, lv_event_t event)
{
  
  Serial.print(Event);
  printf(" ");

  switch(event) {
      case LV_EVENT_PRESSED:
          printf("Pressed\n");
          break;

      case LV_EVENT_SHORT_CLICKED:
          printf("Short clicked\n");
          break;

      case LV_EVENT_CLICKED:
          printf("Clicked\n");
          break;

      case LV_EVENT_LONG_PRESSED:
          printf("Long press\n");
          break;

      case LV_EVENT_LONG_PRESSED_REPEAT:
          printf("Long press repeat\n");
          break;

      case LV_EVENT_RELEASED:
          printf("Released\n");
          break;
  }
}

static void tab1_event_cb(lv_obj_t * tab1, lv_event_t event)
{
    switch(event) {
        case LV_EVENT_PRESSED:
            printf("Pressed\n");
            break;

        case LV_EVENT_SHORT_CLICKED:
            printf("Short clicked\n");
            break;

        case LV_EVENT_CLICKED:
            printf("Clicked\n");
            break;

        case LV_EVENT_LONG_PRESSED:
            printf("Long press\n");
            break;

        case LV_EVENT_LONG_PRESSED_REPEAT:
            printf("Long press repeat\n");
            break;

        case LV_EVENT_RELEASED:
            printf("Released\n");
            break;
    }

       /*Etc.*/
}

void UpdateTemperatureLabel(int value){
    char buffer[12]={0,};
    snprintf (buffer, sizeof(buffer),"%i%",value);
    lv_label_set_text(temperature_label, buffer);
}

void lv_ex_table_1(lv_obj_t tab){
    lv_obj_t * table = lv_table_create(&tab, NULL);


}

void createLabel(lv_obj_t obj, const char * text) {
  lv_obj_t * label = lv_label_create(&obj, NULL);
   lv_label_set_text(label, text);
}
/*
-----------------------------------------------------------------------------------------------
 ____    ____    ______  __  __  ____    
/\  _`\ /\  _`\ /\__  _\/\ \/\ \/\  _`\  
\ \,\L\_\ \ \L\_\/_/\ \/\ \ \ \ \ \ \L\ \
 \/_\__ \\ \  _\L  \ \ \ \ \ \ \ \ \ ,__/
   /\ \L\ \ \ \L\ \ \ \ \ \ \ \_\ \ \ \/ 
   \ `\____\ \____/  \ \_\ \ \_____\ \_\ 
    \/_____/\/___/    \/_/  \/_____/\/_/ 

*/

void setup()
{
    Serial.begin(9600); /* prepare for possible serial debug */

    lv_init();

#if USE_LV_LOG != 0
    lv_log_register_print_cb(my_print); /* register print function for debugging */
#endif

    pinMode(12, OUTPUT);

    tft.begin(); /* TFT init */
    tft.setRotation(0); /* Landscape orientation  = 1*/

    uint16_t calData[5] = { 275, 3620, 264, 3532, 2 };
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

    //lv_obj_t *label = lv_label_create(lv_scr_act(), NULL);
    //lv_label_set_text(label, "Hello Arduino! (V7.0.X)");
    //lv_obj_align(label, NULL, LV_ALIGN_CENTER, 0, 0);

    lv_theme_t * th = lv_theme_material_init(LV_THEME_DEFAULT_COLOR_PRIMARY, LV_THEME_DEFAULT_COLOR_SECONDARY, LV_THEME_DEFAULT_FLAG, LV_THEME_DEFAULT_FONT_SMALL , LV_THEME_DEFAULT_FONT_NORMAL, LV_THEME_DEFAULT_FONT_SUBTITLE, LV_THEME_DEFAULT_FONT_TITLE);     
    lv_theme_set_act(th);

    //SCREEN
    lv_obj_t * scr = lv_cont_create(NULL, NULL);
    lv_scr_load(scr);
    //TABVIEW
    lv_obj_t * tv = lv_tabview_create(scr, NULL);
    
    lv_obj_set_size(tv, LV_HOR_RES_MAX, LV_VER_RES_MAX);

    //TABS
    lv_obj_t * tab0 = lv_tabview_add_tab(tv, LV_SYMBOL_HOME);

    lv_obj_t * tab1 = lv_tabview_add_tab(tv, LV_SYMBOL_SETTINGS);

    lv_obj_t * tab3 = lv_tabview_add_tab(tv, LV_SYMBOL_IMAGE);
    
    //lv_obj_set_event_cb(tab1, tab1_event_cb);


    createLabel(*tab0, "Erster Tab");
    createLabel(*tab1, "Zweiter Tab");

    lv_ex_table_1(*tab1);

    temperature_meter = lv_linemeter_create(tab0, NULL);
    lv_obj_set_size(temperature_meter,120,120);
    lv_obj_align(temperature_meter,NULL,LV_ALIGN_CENTER,0,0);
    lv_linemeter_set_value(temperature_meter, 0);

    temperature_label = lv_label_create(tab0, NULL);
    lv_label_set_text(temperature_label, "N/A");
    lv_obj_align(temperature_label, NULL, LV_ALIGN_CENTER,0,0);

    temperature_celsius = lv_label_create(tab0, NULL);
    lv_label_set_text(temperature_celsius, "°C");
    lv_obj_align(temperature_celsius, NULL, LV_ALIGN_CENTER,0,20);



}

/*
-----------------------------------------------------------------------------------------------
 __       _____   _____   ____    
/\ \     /\  __`\/\  __`\/\  _`\  
\ \ \    \ \ \/\ \ \ \/\ \ \ \L\ \
 \ \ \  __\ \ \ \ \ \ \ \ \ \ ,__/
  \ \ \L\ \\ \ \_\ \ \ \_\ \ \ \/ 
   \ \____/ \ \_____\ \_____\ \_\ 
    \/___/   \/_____/\/_____/\/_/ 

*/

void loop()
{
    
    if (upw == 1){
        temp++;
    }
     if (dws == 1){
        temp--;
    }

    if (temp == 120){
        temp = 119;
        dws = 1;
        upw = 0;
    }

      if (temp == 20){
        temp = 21;
        dws = 0;
        upw = 1;
    }
    
    if (temp < 50){
        digitalWrite(12, HIGH);
    }

    if (temp > 50){
        digitalWrite(12, LOW);
    }

    lv_linemeter_set_value(temperature_meter,temp);
    UpdateTemperatureLabel(temp*2.5);
    lv_task_handler(); /* let the GUI do its work */
    
    delay(10);
    
}