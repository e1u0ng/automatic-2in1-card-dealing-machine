#ifndef MENU_H
#define MENU_H

#include <stdio.h>

typedef struct {
    int num_players;
    int cards_per_hand;
    int extra_cards;
} game_config_t;

typedef enum {
    MENU_NAVIGATE,
    MENU_EDIT_VALUE
} menu_mode_t;

extern menu_mode_t menu_mode;

typedef enum {
    NONE,
    PLAYER,
    CARDS,
    EXTRA
} config_selection_t;

extern config_selection_t config_selection;

typedef enum {
    SHUFFLE=0,
    DEAL=1,
    SHUFFLE_DEAL=2
} shuffle_deal_mode_t;

extern shuffle_deal_mode_t shuffle_deal_mode;

extern volatile bool flag_trigger_menu;
extern bool blink_state;
extern int64_t last_blink_time;

void init_menu(void);
void start_button_timer(void);
void lcd_init_sequence(void);
int get_num_player(void);
int get_cards_per_hand(void);
int get_extra_cards(void);
void set_num_player(int num);
void set_cards_per_hand(int num);
void set_extra_cards(int num);
void blinking_value_timer(int ms);
void update_config_values(void);
void update_shuffle_deal_mode(void);
void update_menu_and_config_states(void);
void menu_ui_task(void *pvParameters);

#endif