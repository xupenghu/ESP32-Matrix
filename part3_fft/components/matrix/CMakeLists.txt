set(COMPONENT_ADD_INCLUDEDIRS ./include/)

set(COMPONENT_SRCS 
                    "src/anim.c"
                    "src/i2s_parallel.c"
                    "src/val2pwm.c"
                    "src/hub75_display.c"
                    "src/image.c")

register_component()
