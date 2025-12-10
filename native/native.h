//
//  native.h
//  psim
//
//  Created by ThrudTheBarbarian on 10/12/2025.
//

#ifndef native_h
#define native_h


/*****************************************************************************\
|* Called by the VM to install any native functions desired. This must be
|* populated to make calls to defineNative() for each native function
\*****************************************************************************/
void installNativeFunctions(void);

#endif /* native_h */
