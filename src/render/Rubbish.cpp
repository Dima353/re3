#include "common.h"
#include "patcher.h"
#include "Rubbish.h"

WRAPPER void CRubbish::Render(void) { EAXJMP(0x512190); }