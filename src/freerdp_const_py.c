#include <Python.h>
#include <freerdp/scancode.h>

void FreeRDP_AddConstants(PyObject* module) {
    PyModule_AddIntConstant(module, "KEY_1", RDP_SCANCODE_KEY_1);
    PyModule_AddIntConstant(module, "KEY_R", RDP_SCANCODE_KEY_R);
    PyModule_AddIntConstant(module, "KEY_LMENU", RDP_SCANCODE_LMENU);
}

