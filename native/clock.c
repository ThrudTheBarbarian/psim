//
//  clock.c
//  psim
//
//  Created by ThrudTheBarbarian on 10/12/2025.
//

#include <time.h>

#include "clock.h"

Value clockNative(int argCount, Value* args)
    {
    return NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
    }
