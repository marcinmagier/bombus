
add_app_includes(".")


add_app_cflags("-std=gnu99")


add_app_sources(args.c)
add_app_sources(utils.c)

if(NOT CMAKE_BUILD_VARIANT STREQUAL "test")
    add_app_sources(main.c)
endif()

