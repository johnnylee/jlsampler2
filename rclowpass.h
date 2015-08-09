#ifndef RCLOWPASS_H_
#define RCLOWPASS_H_

#include <stdint.h>

void rcLowPass(int16_t *out, int len, double freq, int order);

#endif // RCLOWPASS_H_
