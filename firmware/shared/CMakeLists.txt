idf_component_register(SRCS "oscil_afe.c" "oscil_status.c" "oscil_pinwalk.c"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES esp_hw_support esp_rom)