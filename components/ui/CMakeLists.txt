idf_component_register(SRCS "screens/ui_Screen1.c"
                            "ui.c"
                            "components/ui_comp_hook.c"
                            "ui_helpers.c"
                            
                      INCLUDE_DIRS "."
                                   "screens"
                                   "components"
                      REQUIRES lvgl) # Assuming lvgl is the library required for your UI
