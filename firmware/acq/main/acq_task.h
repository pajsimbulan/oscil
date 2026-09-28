#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "oscil_link_uart.h"

#define ACQ_R 1600 //record length, samples per channel per frame
#define ACQ_COLS 800 // screen columns: one(min, max) pair per col per channel

typedef enum { ACQ_RUN, ACQ_STOP, ACQ_SINGLE} acq_run_t; // numbering is on the wire (oscil_proto.h)

// Board 1's LINK1 to board 2 and its capture-error count. Global because main.c and the
// status line use them too; they keep the s_ prefix they had as file statics.
extern link_t s_link1;
extern volatile uint32_t s_cap_errors;      // failed bursts since boot


//one decimated frame, what board 1 will send to board 2 over link1
typedef struct {
    uint32_t seq; //frame counter
    uint32_t rate_hz; //achieved sample rate
    float frac; //trigger position between samples 0...1
    bool triggered; //false: auto fired without a trigger
    uint16_t mn[2][ACQ_COLS]; //[channel][column]
    uint16_t mx[2][ACQ_COLS];
} acq_frame_t;

//sets up the ADCs, the DMA sampler and the panel, then starts the acq task
//on core 1.  Call once from app_main
esp_err_t acq_start(void);

//RUN,STOP, or SINGLE.  safe from any task
void acq_set_run(acq_run_t r);

