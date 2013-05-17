#include "pebble_os.h"
#include "pebble_app.h"
#include "pebble_fonts.h"


#define MY_UUID { 0x31, 0x9E, 0x04, 0x5B, 0x83, 0x77, 0x4C, 0x3C, 0xA3, 0x5C, 0x1F, 0x99, 0x90, 0x68, 0xFA, 0x6D }
PBL_APP_INFO(MY_UUID,
             "Tachymeter", "Glenn Loos-Austin",
             1, 0, /* App version */
             RESOURCE_ID_MENU_ICON,
             APP_INFO_STANDARD_APP);

Window _window;

bool _debug_mode = false;
int _debug_update_count =0; //for tracking number of updates, checking updates/second.

bool _timingInProgress = false;
uint32_t _currentMilliseconds = 0;
long _previousMilliseconds = 0;
int _previousInterval = -1;
int _differenceAccumulator = 0;
int _differenceCount = 0;

short _mode = 0; //stopwatch to start.
bool _wantkph = false;

TextLayer _hundredths_txt;
TextLayer _seconds_txt;
TextLayer _tachy_txt;

TextLayer _sublabel_txt;
//TextLayer _superlabel;

TextLayer _top_txt;
TextLayer _mid_txt;
TextLayer _bot_txt;

TextLayer _label[3];
TextLayer _value[3];

TextLayer _time_txt;

AppTimerHandle _timer_handle;
AppContextRef _storedctx;

Animation _millisecond_anim; //hundredths of a second, good to 655 seconds or so.
AnimationImplementation _millisecond_anim_imp;

int _lapcounter = 0;
int _lapaccumulator = 0;
long _lap[30];


//////////////////////////////////
// Pebble API Utility Functions //
//                              //

void setupTextLayerCore ( TextLayer *layer, int x, int y, int width, int height, GFont font, GColor colorChoice, GColor bgChoice, GTextAlignment whatAlign)
{
    text_layer_init(layer, GRect(x, y, width, height)); //_window.layer.frame);
    text_layer_set_text_color(layer, colorChoice);
    text_layer_set_background_color(layer, bgChoice);
    text_layer_set_text_alignment(layer, whatAlign);
    layer_set_frame(&layer->layer, GRect(x, y, width, height));
    text_layer_set_font(layer, font );
}
void setupTextLayer( TextLayer *layer, Window *parent, int x, int y, int width, int height, GFont font, GColor colorChoice, GColor bgChoice, GTextAlignment whatAlign )
{
    setupTextLayerCore (layer,x,y,width,height,font,colorChoice,bgChoice,whatAlign);
    layer_add_child(&parent->layer, &layer->layer);
}

/*
 void setupTextLayer_alt( TextLayer *layer, Layer *parent, int x, int y, int width, int height, GFont font, GColor colorChoice, GColor bgChoice, GTextAlignment whatAlign )
 {
 setupTextLayerCore (layer,x,y,width,height,font,colorChoice,bgChoice,whatAlign);
 layer_add_child(parent, &layer->layer);
 }
 */

//                              //
// End Pebble API Utility Funcs //
//////////////////////////////////




//////////////////////////////////
// C Utility Functions          //
//                              //
char *itoa(int num, char *buff)
{
    //char buff[20] = {};
    int i = 0, temp_num = num, length = 0;
    int offset = 0;
    //char *string = buff;
    
    if (temp_num <0) {
        buff[0] = '-';
        num *= -1;
        offset = 1;
    }
    
    if (temp_num ==0) {
        buff[i] = '0';
        offset = 1;
    }
    
    while(temp_num) {
        temp_num /= 10;
        length++;
    }
    
    for(i = 0; i < length; i++) {
        buff[(length-1+offset)-i] = '0' + (num % 10);
        num /= 10;
    }
    
    buff[i+offset] = '\0';
    
    return buff;
}

int insertString(char *target, char *source, int pos, int max) {
    //NOTE: even though function named insert, it puts a null at end of each insertion.
    //should probably be called "append_and_terminate_from_position", but that seems wordy.
    int sourcePos = 0;
    while (pos < max && source[sourcePos]!='\0') {      //if the source string hasn't reached its end, and we're not at the max length of the target string
        target[pos] = source[sourcePos];                //copy character from source to target
        sourcePos++;                                    //move to the next position in each string.
        pos++;
    }
    target[pos] = '\0';                                 //we're done, add a null string terminator.
    return(pos);                                        //return the position of the terminator, so multiple calls can be easily chained together to build a string.

}

//                              //
// End C Utility Functions      //
//////////////////////////////////



//////////////////////////////////
// Stopwatch Functions          //
//                              //

void update_lap_display() {
    static char label_buffer[3][12];
    static char value_buffer[3][12];
    char itoa_buffer[4];
    int pos;
    int seconds;
    int hundredths;
    int minutes;
    long tachy;
    int tachy_remainder;
    int tachy_tenths;

    int whichDisplay = 0;
    for(int i=_lapcounter-3;i<_lapcounter;i++) {
        if(i>-1) {
            pos=0;
            switch (_mode) {
                case 0: pos = insertString(label_buffer[whichDisplay], "lap " , pos, 12);
                    break;
                case 1: pos = insertString(label_buffer[whichDisplay], "mi " , pos, 12);
                    break;
                case 2: pos = insertString(label_buffer[whichDisplay], "km " , pos, 12);
                    break;
            }
            pos = insertString(label_buffer[whichDisplay], itoa((i+1),itoa_buffer), pos, 12);
            pos = insertString(label_buffer[whichDisplay], ":", pos, 12);
            text_layer_set_text(&_label[whichDisplay],label_buffer[whichDisplay]);
            switch (_mode) {
                case 0:
                    seconds = _lap[i]/1000;
                    hundredths = (_lap[i]%1000)/10;
                    minutes = seconds/60;
                    seconds = seconds%60;
                    
                    pos = 0;
                    pos = insertString(value_buffer[whichDisplay], itoa(minutes,itoa_buffer),pos, 12);
                    pos = insertString(value_buffer[whichDisplay], (seconds<10) ? ":0" : ":",pos, 12);
                    pos = insertString(value_buffer[whichDisplay], itoa(seconds,itoa_buffer),pos, 12);
                    pos = insertString(value_buffer[whichDisplay], (hundredths<10) ? ":0" : ":", pos, 12);
                    pos = insertString(value_buffer[whichDisplay], itoa(hundredths,itoa_buffer), pos, 12);
                    
                    break;
                case 1: 
                case 2:
                    tachy = 3600000 / _lap[i];
                    tachy_remainder = 3600000 % _lap[i];
                    tachy_tenths = tachy_remainder*10 / _lap[i];
                    
                    pos = 0;
                    if (tachy>500) {
                        pos = insertString(value_buffer[whichDisplay], ">500",pos, 12);
                    } else {
                        pos = insertString(value_buffer[whichDisplay], itoa(tachy,itoa_buffer),pos, 12);
                        pos = insertString(value_buffer[whichDisplay], ".",pos, 12);
                        pos = insertString(value_buffer[whichDisplay], itoa(tachy_tenths,itoa_buffer),pos, 12);
                    }
                    break;
            }
            
            text_layer_set_text(&_value[whichDisplay],value_buffer[whichDisplay]);
            layer_set_hidden(&_label[whichDisplay].layer,false);
            layer_set_hidden(&_value[whichDisplay].layer,false);
            whichDisplay++;
        }
    }
    for(;whichDisplay<3;whichDisplay++) {
        layer_set_hidden(&_label[whichDisplay].layer,true);
        layer_set_hidden(&_value[whichDisplay].layer,true);
    }
}

void update_tachy(long workingMilliseconds) {
    static char tachy_buffer[12];
    static int lastUpdate = -1;

    if (workingMilliseconds > 7000) {
        char itoa_buffer[4];

        long tachy = 3600000 / workingMilliseconds;
        int tachy_remainder = 3600000 % workingMilliseconds;
        
        int tachy_tenths = tachy_remainder*10 / workingMilliseconds;
        
        int pos = 0;
        pos = insertString(tachy_buffer, itoa(tachy,itoa_buffer),pos, 12);
        pos = insertString(tachy_buffer, ".",pos, 12);
        pos = insertString(tachy_buffer, itoa(tachy_tenths,itoa_buffer),pos, 12);
        
        if (lastUpdate != 1) {
            text_layer_set_text(&_sublabel_txt,"wait...");
            lastUpdate = 1;
        }
        
    } else
    {
        static int animphase = 0;
        animphase++;
        if (animphase>11) animphase = 0;
        int pos = 0;
        switch (animphase) {
            case 0:
            case 1:
                pos = insertString(tachy_buffer, "-   ",pos, 12);
                break;
            case 2:
            case 3:
            case 10:
            case 11:
                pos = insertString(tachy_buffer, " -  ",pos, 12);
                break;
            case 4:
            case 5:
            case 8:
            case 9:
                pos = insertString(tachy_buffer, "  - ",pos, 12);
                break;
            case 6:
            case 7:
                pos = insertString(tachy_buffer, "   -",pos, 12);
                break;
        }
        if (lastUpdate != 2) {
            if (_mode==1) { text_layer_set_text(&_sublabel_txt,"mph"); }
            else if (_mode==2) {text_layer_set_text(&_sublabel_txt,"kph");}
            lastUpdate = 2;
        }
    }
    text_layer_set_text(&_tachy_txt,tachy_buffer);
    
}

void handle_timer(AppContextRef ctx, AppTimerHandle handle, uint32_t countdown) {
    (void)ctx;
    (void)handle;

    static char hundredths_buffer[4];
    static char seconds_buffer[7];
    char itoa_buffer[4];

    long workingMilliseconds = _previousMilliseconds + _currentMilliseconds;
    int millisecondsOnly = workingMilliseconds%1000;
    int hundredths = millisecondsOnly/10;
    int seconds = workingMilliseconds/1000;
    
    countdown --;
    if (countdown == 0) {
        if (_timingInProgress) {
            update_tachy(_previousMilliseconds + _currentMilliseconds - _lapaccumulator);
        }
        else
        {
            update_tachy(_lapaccumulator/_lapcounter);
        }
    }
    
    if (countdown <= 0) {
        countdown = 1;
    }
    
    int minutes = seconds/60;
    seconds = seconds%60;

    int pos = 0;
    pos = insertString(hundredths_buffer, (hundredths<10) ? ":0" : ":", pos, 4);
    pos = insertString(hundredths_buffer, itoa(hundredths,itoa_buffer), pos, 4);
    text_layer_set_text(&_hundredths_txt,hundredths_buffer);
    
    pos = 0;
    pos = insertString(seconds_buffer, itoa(minutes,itoa_buffer),pos, 7);
    pos = insertString(seconds_buffer, (seconds<10) ? ":0" : ":",pos, 7);
    pos = insertString(seconds_buffer, itoa(seconds,itoa_buffer),pos, 7);
    
    text_layer_set_text(&_seconds_txt,seconds_buffer);
    
    if (_timingInProgress) {
        _timer_handle = app_timer_send_event(ctx, 53, countdown); //rate of screen updates.
            // this is unrelated to rate of actual timer updates. 53 is to try and ensure hundredths look interesting.
    }
}

void millisecond_callback(struct Animation *animation,const uint32_t time){
    (void)animation;;
    
    _currentMilliseconds = time;
        //we won't display this here, to keep this routine as tight as possible. After all, we can't control how often
        // the system will call it, so it might induce unneeded battery drain, screen update happens in handle_timer. 
    
    if (_currentMilliseconds>50000) {  //should be 50000, set to super low value like 50 to check for error creep, didn't find much.
        //50000 means we're running near the max integer value supported by the animation class. Let's kill this and start a new one.
        //doing this presumably introduces some loss of time, at a minimum, the number of milliseconds between when this
        //routine was called, and when the animation_schedule (below) gets called.
        _previousMilliseconds += _currentMilliseconds;
        
        animation_unschedule(&_millisecond_anim);
        animation_init(&_millisecond_anim);
        animation_set_duration(&_millisecond_anim,ANIMATION_NORMALIZED_MAX);
        _millisecond_anim_imp.update = &millisecond_callback;
        animation_set_implementation(&_millisecond_anim,&_millisecond_anim_imp);
        animation_schedule(&_millisecond_anim);
    }
    
    //_debug_update_count++;

}

void handle_second_tick(AppContextRef ctx, PebbleTickEvent *t) {

    int intervalMilliseconds = _previousMilliseconds + _currentMilliseconds;;
    int difference = -1;
        
    if (_previousInterval > 0) {
        difference = intervalMilliseconds - _previousInterval;
        if (difference >0) {
            _differenceAccumulator += difference;
            _differenceCount++;
        }
    }
    
    _previousInterval = intervalMilliseconds;

    
    if (_mode == 0) {
        if(_debug_mode) {
            //show the average ms interval between seconds instead of time - Just to make sure stopwatch is honest.
            
            if (difference > 0) {
                static char interval_buffer[8];
                char itoa_buffer[4];
                int pos=0;
                
                //ms interval per second.
                int diffAvg = _differenceAccumulator*10/_differenceCount;
                pos = insertString(interval_buffer, itoa(diffAvg/10,itoa_buffer),pos, 7);
                pos = insertString(interval_buffer, ".", pos, 7);
                pos = insertString(interval_buffer, itoa(diffAvg%10,itoa_buffer),pos, 7);
                
                /*
                //timer update rate instead, requires uncommenting a line in millisecond callback as well. turns out to be about 25/sec.
                int framerate = _debug_update_count * 1000 / intervalMilliseconds;
                pos = insertString(interval_buffer, itoa(framerate,itoa_buffer),pos, 7); 
                */
                
                text_layer_set_text(&_time_txt,interval_buffer);
            }
            
        } else
        {
            //show the time.
            PblTm tickTime;
            get_time(&tickTime);
            
            static char timeText[] = "00:00";
            const char *timeFormat = clock_is_24h_style() ? "%R" : "%l:%M";
            string_format_time(timeText, sizeof(timeText), timeFormat, &tickTime);
            
            text_layer_set_text(&_time_txt, timeText);
        }
        
    } else {
        text_layer_set_text(&_time_txt,text_layer_get_text(&_seconds_txt));
    }
}

//                              //
// End Stopwatch Functions      //
//////////////////////////////////




//////////////////////////////////
// Click Handler Functions      //
//                              //


void up_long_click_handler(ClickRecognizerRef recognizer, Window *window) {
    if (_mode > 0) {
        _wantkph = !_wantkph;
        if (_wantkph) {
            _mode = 2;
        } else {
            _mode = 1;
        }
        switch (_mode) {
            case 0: //stopwatch
                break;
            case 1: //mph
                text_layer_set_text(&_top_txt,"tachymeter\n(mph)");
                text_layer_set_text(&_sublabel_txt,"mph");
                break;
            case 2: //kph
                text_layer_set_text(&_top_txt,"tachymeter\n(kph)");
                text_layer_set_text(&_sublabel_txt,"kph");
                break;
        }
        update_lap_display();
    }
    else
    {
        _debug_mode = !_debug_mode;
        if (_debug_mode) text_layer_set_text(&_time_txt,"ms/sec");
    }
}

void up_click_handler(ClickRecognizerRef recognizer, Window *window) {
    if (_mode > 0) {
        _mode = 0;
    } else {
        if(_wantkph) {
           _mode= 2; 
        } else {
           _mode= 1;
        }
    }
    switch (_mode) {
        case 0: //stopwatch
            text_layer_set_text(&_top_txt,"stopwatch");
            layer_set_hidden(&_tachy_txt.layer,true);
            layer_set_hidden(&_seconds_txt.layer,false);
            layer_set_hidden(&_hundredths_txt.layer,false);
            layer_set_hidden(&_sublabel_txt.layer,true);
            break;
        case 1: //mph
            text_layer_set_text(&_top_txt,"tachymeter\n(mph)");
            layer_set_hidden(&_tachy_txt.layer,false);
            layer_set_hidden(&_seconds_txt.layer,true);
            layer_set_hidden(&_hundredths_txt.layer,true);
            if (_timingInProgress) { text_layer_set_text(&_sublabel_txt,"mph"); } else
            {text_layer_set_text(&_sublabel_txt,"average mph");}
            layer_set_hidden(&_sublabel_txt.layer,false);
            break;
        case 2: //kph
            text_layer_set_text(&_top_txt,"tachymeter\n(kph)");
            layer_set_hidden(&_tachy_txt.layer,false);
            layer_set_hidden(&_seconds_txt.layer,true);
            layer_set_hidden(&_hundredths_txt.layer,true);
            text_layer_set_text(&_sublabel_txt,"kph");
            if (_timingInProgress) { text_layer_set_text(&_sublabel_txt,"kph"); } else
            {text_layer_set_text(&_sublabel_txt,"average kph");}
            layer_set_hidden(&_sublabel_txt.layer,false);
           break;
    }
    if(_timingInProgress) {
        switch (_mode) {
            case 0: //stopwatch
                text_layer_set_text(&_bot_txt,"lap");
                break;
            case 1: //mph
            case 2: //kph
                text_layer_set_text(&_bot_txt,"mark");
                break;
        }
    }
    update_lap_display();

}

void down_click_handler(ClickRecognizerRef recognizer, Window *window) {
    if(_timingInProgress && _lapcounter<30) {
        // lap or milepost button.
        if (_lapcounter>0) {
            _lap[_lapcounter] = _previousMilliseconds + _currentMilliseconds - _lapaccumulator;
        } else {
            _lap[_lapcounter] = _previousMilliseconds + _currentMilliseconds;
        }
        _lapcounter++;
        _lapaccumulator = _previousMilliseconds + _currentMilliseconds;
        update_tachy(_lapaccumulator/_lapcounter); //_lapaccumulator/_lapcounter
        update_lap_display();
    } else {
        text_layer_set_text(&_mid_txt,"stop");
        switch (_mode) {
            case 0: //stopwatch
                text_layer_set_text(&_bot_txt,"lap");
                break;
            case 1: //mph
            case 2: //kph
                text_layer_set_text(&_bot_txt,"mark");
                break;
            default:
                text_layer_set_text(&_bot_txt,"never!");
                break;
        }

        animation_unschedule(&_millisecond_anim);
        animation_init(&_millisecond_anim);
        animation_set_duration(&_millisecond_anim,ANIMATION_NORMALIZED_MAX);
                // ANIMATION_NORMALIZED_MAX > 60 seconds. Should give ms accuracy of within our target 1/100 second tolerance.
        _millisecond_anim_imp.update = &millisecond_callback;
        //_timerAnim10imp.teardown = &anim_teardown;
        animation_set_implementation(&_millisecond_anim,&_millisecond_anim_imp);
        animation_schedule(&_millisecond_anim);
        
        _timer_handle = app_timer_send_event(_storedctx, 25,1); //25 milliseconds, let's show them results as quickly as possible.
        _timingInProgress = true;
        
        _previousInterval = -1;
    }
}

void select_click_handler(ClickRecognizerRef recognizer, Window *window) {
    if(_timingInProgress) {
        animation_unschedule(&_millisecond_anim);
        _previousMilliseconds += _currentMilliseconds;
        _currentMilliseconds = 0;
        _timingInProgress = false;
        text_layer_set_text(&_mid_txt,"reset");
        text_layer_set_text(&_bot_txt,"cont");
        update_tachy(_lapaccumulator/_lapcounter);
        switch (_mode) {
            case 0:
                break;
            case 1:
                text_layer_set_text(&_sublabel_txt,"average mph");
                break;
            case 2:
                text_layer_set_text(&_sublabel_txt,"average kph");
                break;
        }
    } else {
        _previousMilliseconds = 0;
        _currentMilliseconds = 0; //probably unneccessary
        _lapcounter = 0;
        _lapaccumulator = 0;
        _previousInterval = -1;
        _differenceAccumulator = 0;
        _differenceCount = 0;
        update_lap_display();

        text_layer_set_text(&_time_txt,"");
        text_layer_set_text(&_hundredths_txt,".00");
        text_layer_set_text(&_seconds_txt,"0:00");
        text_layer_set_text(&_tachy_txt,"--");
        text_layer_set_text(&_bot_txt,"start");

    }
}

void dummy_handler(ClickRecognizerRef recognizer, Window *window) {
    //does nothing. Putting this in prevents mysterious ignoring of the first click after a long click.
}

void main_config_provider(ClickConfig **config, Window *window) {
    config[BUTTON_ID_UP]->click.handler = (ClickHandler) up_click_handler;
    config[BUTTON_ID_UP]->long_click.handler = (ClickHandler) up_long_click_handler;
    config[BUTTON_ID_UP]->long_click.release_handler = (ClickHandler) dummy_handler;
    config[BUTTON_ID_UP]->long_click.delay_ms = 750;

    config[BUTTON_ID_DOWN]->click.handler = (ClickHandler) down_click_handler;
    config[BUTTON_ID_SELECT]->click.handler = (ClickHandler) select_click_handler;
    
    (void)window;
}

//                              //
// End Click Handler Functions  //
//////////////////////////////////




//////////////////////////////////
// Core Bootstrapping Functions //
//                              //

void handle_deinit(AppContextRef ctx) {
    (void)ctx;
    
    animation_unschedule_all();
}

void handle_init(AppContextRef ctx) {
    (void)ctx;
    
    _storedctx = ctx;
    
    window_init(&_window, "Tachymeter");
    window_stack_push(&_window, true /* Animated */);
    window_set_background_color(&_window, GColorBlack);
    window_set_fullscreen(&_window, true);
    
    resource_init_current_app(&TACHYMETER_RESOURCES);

    window_set_click_config_provider(&_window, (ClickConfigProvider) main_config_provider);
    
    GFont buttonFont = fonts_get_system_font(FONT_KEY_GOTHIC_18);
    
    setupTextLayer( &_top_txt, &_window, 60, 2, 82, 48, buttonFont, GColorWhite, GColorBlack, GTextAlignmentRight);
    setupTextLayer( &_mid_txt, &_window, 90, 74, 52, 24, buttonFont, GColorWhite, GColorBlack, GTextAlignmentRight);
    setupTextLayer( &_bot_txt, &_window, 90, 145, 52, 24, buttonFont, GColorWhite, GColorBlack, GTextAlignmentRight);

    
    GFont labelfont = fonts_get_system_font(FONT_KEY_GOTHIC_24);
    GFont valuefont = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
    
    for (int i=0;i<3;i++) {
        setupTextLayer( &_label[i], &_window, 0, 96+(i*22), 46, 28, labelfont, GColorWhite, GColorClear, GTextAlignmentRight);
        setupTextLayer( &_value[i], &_window, 48, 96+(i*22), 60, 28, valuefont, GColorWhite, GColorClear, GTextAlignmentLeft);
        text_layer_set_text(&_label[i],"Lap 1:");
        text_layer_set_text(&_value[i],"0:00");
        layer_set_hidden(&_label[i].layer,true);
        layer_set_hidden(&_value[i].layer,true);
    }
    
    GFont secondsFont = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_OSP_DIN_48));
    GFont hundredthsFont = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_OSP_DIN_26));

    setupTextLayer( &_hundredths_txt, &_window, 76, 52, 30, 30, hundredthsFont, GColorWhite, GColorBlack, GTextAlignmentLeft);
    setupTextLayer( &_seconds_txt, &_window, 0, 30, 75, 50, secondsFont, GColorWhite, GColorBlack, GTextAlignmentRight);
    
    setupTextLayer( &_tachy_txt, &_window, 0, 30, 105, 50, secondsFont, GColorWhite, GColorBlack, GTextAlignmentCenter);
    
    text_layer_set_text(&_hundredths_txt,".00");
    text_layer_set_text(&_seconds_txt,"0:00");
    text_layer_set_text(&_tachy_txt,"--");

    GFont timeFont = fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD);
    setupTextLayer( &_time_txt, &_window, 2, 2, 70, 30, timeFont, GColorWhite, GColorBlack, GTextAlignmentLeft);
    text_layer_set_text(&_time_txt,"");
    
    setupTextLayer( &_sublabel_txt, &_window, 0, 77, 105, 22, buttonFont, GColorWhite, GColorBlack, GTextAlignmentCenter);
    text_layer_set_text(&_sublabel_txt," ");
    
    layer_set_hidden(&_tachy_txt.layer,true);
    layer_set_hidden(&_sublabel_txt.layer,true);
    text_layer_set_text(&_top_txt,"stopwatch");
    text_layer_set_text(&_mid_txt,"reset");
    text_layer_set_text(&_bot_txt,"start");
    

}


void pbl_main(void *params) {
    
    _debug_mode = false;
    _debug_update_count =0;
    _timingInProgress = false;
    _currentMilliseconds = 0;
    _previousMilliseconds = 0;
    _previousInterval = -1;
    _differenceAccumulator = 0;
    _differenceCount = 0;
    
    _mode = 0; 
    _wantkph = false;

    _lapcounter = 0;
    _lapaccumulator = 0;

  PebbleAppHandlers handlers = {
    .init_handler = &handle_init,
    .timer_handler = &handle_timer,
    .deinit_handler = &handle_deinit,
      .tick_info = {
          .tick_handler = &handle_second_tick,
          .tick_units = SECOND_UNIT
      }
  };
  app_event_loop(params, &handlers);
}

//                              //
// End Core Bootstrapping       //
//////////////////////////////////

