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

ObjFunction *  compile(const char* source);

#endif /* compiler_h */
