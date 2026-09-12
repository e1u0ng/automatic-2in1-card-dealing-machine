#ifndef MENU_H
#define MENU_H

#include <stdio.h>

typedef struct {
    int num_players; // the number of players
    int cards_per_hand; // the number of cards per player hand
    int extra_cards; // the number of cards to be dispensed into a separate pile at the end of dealing
} game_config_t;

/**
 * @brief MENU_NAVIGATE: not editing any config values, MENU_EDIT: editing a config value on screen
 */
typedef enum {
    MENU_NAVIGATE,
    MENU_EDIT_VALUE
} menu_mode_t;

extern menu_mode_t menu_mode;

/**
 * @brief NONE: not editing, PLAYER: editing player value, CARDS: editing cards per hand value, EXTRA: editing extra cards value
 */
typedef enum {
    NONE,
    PLAYER,
    CARDS,
    EXTRA
} config_selection_t;

extern config_selection_t config_selection;


/**
 * @brief SHUFFLE: only shuffle, DEAL: only deal, SHUFFLE_DEAL: shuffle then deal
 */
typedef enum {
    SHUFFLE=0,
    DEAL=1,
    SHUFFLE_DEAL=2
} shuffle_deal_mode_t;

extern shuffle_deal_mode_t shuffle_deal_mode;

extern volatile bool flag_trigger_menu;
extern bool blink_state;
extern int64_t last_blink_time;

/**
 * @brief initializes menu hardware
 */
void init_menu(void);

/**
 * @brief starts the button polling timer
 */
void start_button_timer(void);

/**
 * @brief initialize starting menu ui
 */
void lcd_init_sequence(void);

/**
 * @brief get player value from the config
 * @return the number of players
 */
int get_num_player(void);

/**
 * @brief get cards per hand value from the config
 * @return the number of cards per player hand
 */
int get_cards_per_hand(void);

/**
 * @brief get extra cards value from the config
 * @return the number of cards to be dispensed into a separate pile at the end of dealing
 */
int get_extra_cards(void);

/**
 * @brief updates player value in the config
 * @param num the number of players
 */
void set_num_player(int num);

/**
 * @brief updates cards per hand value in the config
 * @param num the number of cards per player hand
 */
void set_cards_per_hand(int num);

/**
 * @brief updates extra card value in the config
 * @param num the number of cards to be dispensed into a separate pile at the end of dealing
 */
void set_extra_cards(int num);

/**
 * @brief loops to update blinking number when editing value
 */
void blinking_value_timer(int ms);

/**
 * @brief rotating menu encoder will update config value if editing
 */
void update_config_values(void);

/**
 * @brief rotating shuffle-deal encoder will update state between shuffle, deal, and shuffle-deal modes
 */
void update_shuffle_deal_mode(void);

/**
 * @brief updates state between navigating and editing modes
 */
void update_menu_and_config_states(void);

/**
 * @brief background task to update menu ui when config values and shuffle-deal mode changes
 */
void menu_ui_task(void *pvParameters);

#endif