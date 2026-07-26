 set(APP_SRC "menu_test.c")
# set(APP_SRC "lcd_test.c")
# set(APP_SRC "rotary_test.c")
# set(APP_SRC "rotary_test.c")
# set(APP_SRC "servo_test.c")
# set(APP_SRC "motor_test.c")

idf_component_register(
    SRCS "${APP_SRC}"
    INCLUDE_DIRS "."
)
