# Collect every .c/.cpp in main/ and its sub-folders
file(GLOB_RECURSE APP_SRCS
     "*.c"
     "*.cpp")

idf_component_register(
    SRCS        "${APP_SRCS}"
    INCLUDE_DIRS "include"
)

# Optional: extra warnings for *this* component only
target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wno-missing-braces
    -Wmissing-field-initializers
)
