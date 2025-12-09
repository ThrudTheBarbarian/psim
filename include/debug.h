//
//  debug.h
//  modelmem
//
//  Created by ThrudTheBarbarian on 09/12/2025.
//

#ifndef debug_h
#define debug_h

#include "chunk.h"

/*****************************************************************************\
|* Top level dissassembly call, process a chunk
\*****************************************************************************/
void disassembleChunk(Chunk* chunk, const char* name);

/*****************************************************************************\
|* Dissassemble an instruction within a chunk
\*****************************************************************************/
int disassembleInstruction(Chunk* chunk, int offset);

#endif /* debug_h */
