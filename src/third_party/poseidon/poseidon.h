#include <stdint.h>
#include "f251.h"

void mix_layer_3(felt_t state[]);
void mix_layer_4(felt_t state[]);
void mix_layer_5(felt_t state[]);
void mix_layer_9(felt_t state[]);

void round_3(felt_t state[], int index, uint8_t round_mode);
void round_4(felt_t state[], int index, uint8_t round_mode);
void round_5(felt_t state[], int index, uint8_t round_mode);
void round_9(felt_t state[], int index, uint8_t round_mode);

void permutation_3(felt_t state[]);
void permutation_4(felt_t state[]);
void permutation_5(felt_t state[]);
void permutation_9(felt_t state[]);
