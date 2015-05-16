#include <pebble.h>
  
#define NUM_MENU_SECTIONS 1
#define NUM_FIRST_MENU_ITEMS 5

static Window *window;
static TextLayer *text_layer;
static char msg[100]; // message to send to phone
static bool standby = false;
static bool dancing = false;
static bool celsius = true;
static AppTimer *timer; // used for polling for current temperature
static int refresh_time = 1000; // frequency of polling updates (milliseconds)

static SimpleMenuLayer *s_simple_menu_layer;
static SimpleMenuSection s_menu_sections[NUM_MENU_SECTIONS];
static SimpleMenuItem s_first_menu_items[NUM_FIRST_MENU_ITEMS];

/* Called when a message is successfully transmitted to phone */
void out_sent_handler(DictionaryIterator *sent, void *context) {
  // outgoing message was delivered -- do nothing
}

/* Called when a message fails to make it to the phone */
void out_failed_handler(DictionaryIterator *failed, AppMessageResult reason, void *context) {
  text_layer_set_text(text_layer, "Unable to connect with phone.");
}

/* Returns x^n, given x and n as inputs */
int our_pow(int base, int power) {
  int result = 1;
  while (power > 0) {
    result = result * base;
    power--;
  }
  return result;
}

/* Determines if the input is a digit or not */
int our_isdigit(char c) {
  return c >= '0' && c <= '9';
}

/** Converts a string of characters to a double.
  * This method was adapted from its original version, which appears online at
  * https://code.google.com/p/unladen-swallow/source/browse/branches/release-2009Q1-maint/Python/atof.c
  */
double our_atof(char *s) {
  double a = 0.0;
  int e = 0;
  int c;
  int negative = 0;
  if (*s == '-') {
    negative = 1;
    s++;
  }
  while ((c = *s++) != '\0' && our_isdigit(c)) {
    a = a*10.0 + (c - '0');
  }
  if (c == '.') {
    while ((c = *s++) != '\0' && our_isdigit(c)) {
      a = a*10.0 + (c - '0');
      e = e-1;
    }
  }
  if (c == 'e' || c == 'E') {
    int sign = 1;
    int i = 0;
    c = *s++;
    if (c == '+')
      c = *s++;
    else if (c == '-') {
      c = *s++;
      sign = -1;
    }
    while (our_isdigit(c)) {
      i = i*10 + (c - '0');
      c = *s++;
    }
    e += i*sign;
  }
  while (e > 0) {
    a *= 10.0;
    e--;
  }
  while (e < 0) {
    a *= 0.1;
    e++;
  }
  if (negative) a = -1 * a;
  return a;
}

/* Converts an input double to an array of chars, stored in the input buffer */
void double_to_char_array(double num, char* buffer) {
  int dig_left = 0;

  // Determine how many digits there are to the left of the decimal point
  double num_cpy = num;
  while (num_cpy >= 1) {
    num_cpy = num_cpy / 10;
    dig_left++;
  }
  
  // Copy all digits to the left of the decimal place one by one
  int num_left_cpy = (int) num;
  int digit = 1;
  while (digit <= dig_left) {
    int ten_pow = our_pow(10, dig_left - digit);
    int this_dig = num_left_cpy / ten_pow;
    buffer[digit - 1] = this_dig + '0';
    num_left_cpy = num_left_cpy % ten_pow;
    digit++;
  }
  
  buffer[digit - 1] = '.';
  digit++;
  
  // Copy all digits to the right of the decimal place
  double num_right_cpy = num - ((int) num);
  while (digit < 7) {
    num_right_cpy = num_right_cpy * 10;
    int this_dig = ((int) num_right_cpy) % 10;
    buffer[digit - 1] = this_dig + '0';
    digit++;
  }
}

/* Converts an integer to an array of chars and stores it in the input buffer */
void int_to_char_array(int num, char* buffer) {
  int num_cpy = num;

  // Determine number of digits
  int num_digits = 0;
  while (num_cpy >= 1) {
    num_cpy = num_cpy / 10;
    num_digits++;
  }
  
  // Copy the digits one by one into the buffer
  num_cpy = num;
  int digit = 1;
  while (digit <= num_digits) {
    int ten_pow = our_pow(10, num_digits - digit);
    int this_dig = num_cpy / ten_pow;
    buffer[digit - 1] = this_dig + '0';
    num_cpy = num_cpy % ten_pow;
    digit++;
  }
  
  // Null-terminate the char array
  buffer[digit - 1] = '\0';
}

/* Converts a char array with a celsius temperature into a char array with a fahrenheit temperature */
void convert_to_f(char* cel_str) {
 char cel_str_copy[7];
 strncpy(cel_str_copy, cel_str, 6);
 cel_str_copy[6] = '\0';
 double cel_temp = our_atof(cel_str_copy);
 double f_temp = cel_temp * (9.0 / 5.0) + 32; // Celsius to Fahrenheit conversion
 char f_str[100];
 
 double_to_char_array(f_temp, f_str);
 
 for (int i = 0; i < 6; i++) {
   cel_str[i] = f_str[i];
 }
}

/* Handles incoming messages from the phone */
void in_received_handler(DictionaryIterator *received, void *context) {
   
  // Looks for key #0 (current temperature) in the incoming message
  Tuple *msg_tuple = dict_find(received, 0); 
  if (msg_tuple) {
    strncpy(msg, msg_tuple->value->cstring, 6);
    msg[6] = '\0';
    if (!celsius) {
      convert_to_f(msg);
      strcat(msg, " F");
    } else {
      strcat(msg, " C");
    }
    text_layer_set_text(text_layer, msg);
    return;
  }
 
  // Looks for key #1 (statistics) in the incoming message
  msg_tuple = dict_find(received, 1);
  if (msg_tuple) {
    char* p = msg_tuple->value->cstring;
    strcpy(msg, "Min: ");

    if (!celsius) {
      convert_to_f(p);
      strncat(msg, p, 6);
      strcat(msg, " F\nMax: ");
    } else {
      strncat(msg, p, 6);
      strcat(msg, " C\nMax: ");
    }

    while (*p != ',') {p++;}
    p++;

    if (!celsius) {
      convert_to_f(p);
      strncat(msg, p, 6);
      strcat(msg, " F\nAvg: ");
    } else {
      strncat(msg, p, 6);
      strcat(msg, " C\nAvg: ");
    }

    while (*p != ',') {p++;}
    p++;

    if (!celsius) {
      convert_to_f(p);
      strncat(msg, p, 6);
      strcat(msg, " F\n\nActive time:\n");
    } else {
      strncat(msg, p, 6);
      strcat(msg, " C\n\nActive time:\n");
    }
       
    while (*p != ',') {p++;}
    p++;         
        
    // Convert average time in seconds to average time in days, hours, minutes, and seconds
    int time = (int) our_atof(p);
    char buf[100];
    if (time >= 86400) {
      int days = time / 86400;
      int_to_char_array(days, buf);
      strcat(msg, buf);
      strcat(msg, " d ");
      time = time % 86400;
    }
    if (time >= 3600) {
      int hours = time / 3600;
      int_to_char_array(hours, buf);
      strcat(msg, buf);
      strcat(msg, " hr ");
      time = time % 3600;
    }
    if (time >= 60) {
      int mins = time / 60;
      int_to_char_array(mins, buf);
      strcat(msg, buf);
      strcat(msg, " min ");
      time = time % 60;
    }
    if (time >= 0) {
      int_to_char_array(time, buf);
      strcat(msg, buf);
      strcat(msg, " sec");
    }
     
    text_layer_set_text(text_layer, msg);
    return;
  } 
  
  // Looks for key #404 (no statistics data available) in the incoming message   
  msg_tuple = dict_find(received, 404);
  if (msg_tuple) {
    strcpy(msg, "No data available (You've been in standby mode for too long)");
    text_layer_set_text(text_layer, msg);
    return;
  }
 
  // Looks for key #13 (unable to connect to serve) in the incoming message
  msg_tuple = dict_find(received, 13);
  if (msg_tuple) {
    strcpy(msg, "Unable to connect to server");
    text_layer_set_text(text_layer, msg);
    return;
  }
 
  // Looks for key #2 (dance mode initiated) in the incoming message
  msg_tuple = dict_find(received, 2);
  if (msg_tuple) {
    strcpy(msg, "Dancing!");
    text_layer_set_text(text_layer, msg);
    return;
  }

  // Looks for key #3 (unable to connect to Arduino) in the incoming message
  msg_tuple = dict_find(received, 3);
  if (msg_tuple) {
    strcpy(msg, "Server can't connect to the Arduino");
    text_layer_set_text(text_layer, msg);
    return;
  }
  
  // Otherwise: couldn't identify message key
  strcpy(msg, "Error - unidentified key!");
  text_layer_set_text(text_layer, msg);
}

/* Called when the incoming message is dropped */
void in_dropped_handler(AppMessageResult reason, void *context) {
  // incoming message dropped
  text_layer_set_text(text_layer, "Error receiving message.");
}

/* Handles the user clicking the select button to refresh the current temperature */
void refresh_temp_click_handler(ClickRecognizerRef recognizer, void* context) {
  if (standby) {
    text_layer_set_text(text_layer, "In standby mode\n\n\n\n...you nincompoop");
    return;
  } else if (dancing) {
    text_layer_set_text(text_layer, "Too busy dancing!");
    return;
  }
  
  DictionaryIterator *iter;
  app_message_outbox_begin(&iter);
  // sends a request for the current temperature, using key #0
  Tuplet value = TupletCString(0, "hello?");
  dict_write_tuplet(iter, &value);
  app_message_outbox_send();
 
  // Reschedule the timer to poll for the current temp in 1 second
  app_timer_reschedule(timer, refresh_time);
}

/* Called when the timer's time elapses - polling for current temperature */
static void timer_callback(void *data) {
  refresh_temp_click_handler(NULL, NULL);
  timer = app_timer_register(refresh_time, timer_callback, NULL);
}

/* Handles the user clicking the select button to refresh the statistics */
void refresh_stats_click_handler(ClickRecognizerRef recognizer, void* context) {
  text_layer_set_text(text_layer, "Retrieving\nInformation...");
  
  DictionaryIterator *iter;
  app_message_outbox_begin(&iter);
  // sends a request for the statistics, using key #1
  Tuplet value = TupletCString(1, "hello?");
  dict_write_tuplet(iter, &value);
  app_message_outbox_send();
}

/* Sends a request to either "stand by" or "wake up" */
void send_standby_message() {
  DictionaryIterator *iter;
  app_message_outbox_begin(&iter);
  if (standby) {
    Tuplet value = TupletCString(2, "Standby");
    dict_write_tuplet(iter, &value);
  } else {
    Tuplet value = TupletCString(3, "Wake Up");
    dict_write_tuplet(iter, &value);
  }
  app_message_outbox_send();
}

/* Sends a request to change the units between celsius and fahrenheit */
void send_change_units_message() {
  DictionaryIterator *iter;
  app_message_outbox_begin(&iter);
  if (celsius) {
    Tuplet value = TupletCString(4, "Use celsius");
    dict_write_tuplet(iter, &value);
  } else {
    Tuplet value = TupletCString(5, "Use Fahrenheit");
    dict_write_tuplet(iter, &value);
  }
  app_message_outbox_send();
}

/* Sends a request to put the Arduino into or out of dance mode */
void send_dance_mode_message() {
  DictionaryIterator *iter;
  app_message_outbox_begin(&iter);
  if (dancing) {
    Tuplet value = TupletCString(6, "Dance!");
    dict_write_tuplet(iter, &value);
  } else {
    Tuplet value = TupletCString(7, "Stop Dancing!");
    dict_write_tuplet(iter, &value);
  }
  app_message_outbox_send();
}

/* Handles the user pressing the back button (returns the user to the main menu) */
void back_handler(ClickRecognizerRef recognizer, void* context) {
  app_timer_cancel(timer); // Stop polling for the current temperature
  window_stack_pop(true);
  window = window_stack_get_top_window();
}

/* Handles the user pressing the back button to stop dance mode and return to main menu */
void stop_dancing_back_handler(ClickRecognizerRef recognizer, void* context) {
  dancing = false;
  send_dance_mode_message();
  window_stack_pop(true);
  window = window_stack_get_top_window();
}

/* this registers the appropriate function to the appropriate button (current temperature) */
void config_provider_current_temp(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, refresh_temp_click_handler);
  window_single_click_subscribe(BUTTON_ID_BACK, back_handler);
}

/* this registers the appropriate function to the appropriate button (statistics)*/
void config_provider_statistics(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, refresh_stats_click_handler);
  window_single_click_subscribe(BUTTON_ID_BACK, back_handler);
}

/* this registers the appropriate function to the appropriate button (dance mode)*/
void config_provider_dance_mode(void *context) {
  window_single_click_subscribe(BUTTON_ID_BACK, stop_dancing_back_handler);
}

/* Called when statistics is clicked */
static void statistics_callback(int index, void *ctx) {
  
  window = window_create();
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(text_layer));

  window_stack_push(window, true);
  
  window_set_click_config_provider(window, config_provider_statistics);
  
  refresh_stats_click_handler(NULL, NULL); // Refresh/initiate stats
}

/* This is called when current temp is selected */
static void current_temp_callback(int index, void *ctx) {

  window = window_create();
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(text_layer));
  
  window_stack_push(window, true);
  
  window_set_click_config_provider(window, config_provider_current_temp);
  
  text_layer_set_text(text_layer, "Retrieving Information...");
  refresh_temp_click_handler(NULL, NULL); // Refresh/initiate current temperature
  timer = app_timer_register(3000, timer_callback, NULL); // Initiate polling
}

/* This is called when change units is selected */
static void units_callback(int index, void *ctx) {

  if (celsius) {
    s_first_menu_items[2].subtitle = "Fahrenheit";
    celsius = false;
  } else {
    s_first_menu_items[2].subtitle = "Celsius";
    celsius = true;
  }
  send_change_units_message();
  layer_mark_dirty(simple_menu_layer_get_layer(s_simple_menu_layer));
}

/* This is called when standby is selected */
static void standby_callback(int index, void *ctx) {

  if (standby) {
    s_first_menu_items[3].title = "Standby";
    standby = false;
  } else {
    s_first_menu_items[3].title = "Wake Up";
    standby = true;
  }
  send_standby_message();
  layer_mark_dirty(simple_menu_layer_get_layer(s_simple_menu_layer));
}

/* This is called when dance mode is selected */
static void dance_mode_callback(int index, void *ctx) {
  
  window = window_create();
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(text_layer));
  
  window_stack_push(window, true);
  
  window_set_click_config_provider(window, config_provider_dance_mode);

  if (standby) {
    text_layer_set_text(text_layer, "You've gotta wake me up to put me in dance mode!");
    return;
  }
  
  dancing = true;
  text_layer_set_text(text_layer, "Initiating dance mode...\n\n\n\n(buckle up)");
  send_dance_mode_message();
}

/* Create the main menu window */
static void window_load(Window *window) {  
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_frame(window_layer);
  
  s_first_menu_items[0] = (SimpleMenuItem) {
    .title = "Current Temperature",
    .callback = current_temp_callback,
  };
  
  s_first_menu_items[1] = (SimpleMenuItem) {
    .title = "Statistics",
    .callback = statistics_callback,
  };
  
  s_first_menu_items[2] = (SimpleMenuItem) {
    .title = "Change Units",
    .subtitle = "Celsius",
    .callback = units_callback,
  };
  
   s_first_menu_items[3] = (SimpleMenuItem) {
    .title = "Standby",
    .callback = standby_callback,
  };
  
  s_first_menu_items[4] = (SimpleMenuItem) {
    .title = "Dance mode",
    .subtitle = "Try it!",
    .callback = dance_mode_callback,
  };
  
  s_menu_sections[0] = (SimpleMenuSection) {
    .num_items = NUM_FIRST_MENU_ITEMS,
    .items = s_first_menu_items,
  };

  s_simple_menu_layer = simple_menu_layer_create(bounds, window, s_menu_sections, NUM_MENU_SECTIONS, NULL);
  
  text_layer = text_layer_create((GRect) { .origin = { 0, 35 }, .size = { bounds.size.w, 120 } });
  text_layer_set_text_alignment(text_layer, GTextAlignmentCenter);
  
  layer_add_child(window_layer, simple_menu_layer_get_layer(s_simple_menu_layer));
}

/* Called when the app is exited */
static void window_unload(Window *window) {
  text_layer_destroy(text_layer);
  simple_menu_layer_destroy(s_simple_menu_layer);
}

/* Initializes the main menu window */
static void init(void) {
  window = window_create();
  window_set_window_handlers(window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  
  // for registering AppMessage handlers
  app_message_register_inbox_received(in_received_handler);
  app_message_register_inbox_dropped(in_dropped_handler);
  app_message_register_outbox_sent(out_sent_handler);
  app_message_register_outbox_failed(out_failed_handler);
  const uint32_t inbound_size = 64;
  const uint32_t outbound_size = 64;
  app_message_open(inbound_size, outbound_size);
  
  const bool animated = true;
  window_stack_push(window, animated);
}

/* De-initializes the main menu window */
static void deinit(void) {
  window_destroy(window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}