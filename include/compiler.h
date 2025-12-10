//
//  compiler.h
//  modelmem
//
//  Created by ThrudTheBarbarian on 09/12/2025.
//

#ifndef compiler_h
#define compiler_h

#include "vm.h"
#include "object.h"

/*****************************************************************************\
|* Compile the source
\*****************************************************************************/
ObjFunction *  compile(const char* source);

/*****************************************************************************\
|* GC: Do a mark of all the compiler roots we want to keep
\*****************************************************************************/
void markCompilerRoots(void);

#endif /* compiler_h */
