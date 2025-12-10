//
//  native.c
//  psim
//
//  Created by ThrudTheBarbarian on 10/12/2025.
//

#include "clock.h"

#include "vm.h"
/*****************************************************************************\
|* Called by the VM to install any native functions desired. This must be
|* populated to make calls to defineNative() for each native function
\*****************************************************************************/
void installNativeFunctions(void)
    {
    defineNative("clock", clockNative);
    }


