set(COMPONENT_ADD_INCLUDEDIRS include)
set(COMPONENT_SRCS "owb.c" "owb_gpio.c" "owb_rmt.c")

# Tell the build system we depend on the 'driver' component
set(COMPONENT_REQUIRES driver)

register_component()