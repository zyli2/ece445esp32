file(GLOB_RECURSE srcs "main.c" "src/*.c" "srcs/*.cpp")

idf_component_register(SRCS "${srcs}"
                       INCLUDE_DIRS "./include")
