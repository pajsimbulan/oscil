#pragma once
// UART1 looped back on board 1 (GPIO43 -> 220R -> GPIO44). Call from app_main with the
// acquisition not started. Prints counters every 10 s; never returns.
void test_link_loop(void);