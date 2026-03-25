idf_component_register(
    SRCS "main.c"
         "APP/src/app_servo.c"
         "BSP/Src/bsp_servo.c"
    INCLUDE_DIRS "."
                 "APP/Inc"
                 "BSP/Inc"
)
