idf_component_register(SRCS "oscil_afe.c" "oscil_status.c" "oscil_pinwalk.c" "oscil_trigger.c" "oscil_decimate.c"
                            "oscil_link.c" "oscil_uart.c" "oscil_link_uart.c"
                            "oscil_dds.c"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES esp_hw_support esp_rom)