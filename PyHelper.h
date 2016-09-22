#include "Python.h"

#if PY_VERSION_HEX >= 0x03000000
#define PyBuffer_FromMemory(s, len) PyMemoryView_FromMemory(s, len, PyBUF_READ|PyBUF_WRITE)
#endif
