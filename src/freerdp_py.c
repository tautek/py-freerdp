#include <Python.h>
#include <structmember.h>
#include <signal.h>
#include "freerdp.h"

/**
 * Module level mapping tracking C pointers
 * for callbacks.
 */
static PyObject* __module_instanceMap = NULL;

/**
 * Defines the FreeRDP class data.
 */
typedef struct {
    PyObject_HEAD
    PyObject* instance;
    PyObject* onConnect;
} FreeRDP;

/**
 * Cyclic garbace collection.
 */
static int FreeRDP_trav(FreeRDP* self, visitproc visit, void* arg) {
    Py_VISIT(self->instance);
    Py_VISIT(self->onConnect);
    Py_XDECREF(self);
    return 0;
}

/**
 * Clear FreeRDP class instance.
 */
static int FreeRDP_clear(FreeRDP* self) {
    Py_CLEAR(self->instance);
    Py_CLEAR(self->onConnect);
    return 0;
}

/**
 * Cleanup FreeRDP class instance.
 */
static void FreeRDP_dealloc(FreeRDP* self) {
    stop(PyCapsule_GetPointer(self->instance, NULL));
    FreeRDP_clear(self);
    Py_TYPE(self)->tp_free(self);
}

/**
 * Static callback point for 'onConnect' event.
 * Maps the callback back to the instance that
 * initiated it.
 */
static void onConnect_callback(void* instance) {
    PyObject* self = PyDict_GetItem(__module_instanceMap, PyUnicode_FromFormat("%d", instance));
    if (self==NULL) { fprintf(stderr,"SELF NULL!!!!");}
    PyObject* args = PyTuple_New(1);
    if (PyTuple_SetItem(args, 0, self) != 0) {
        fprintf(stderr, "TUPLE ERROR!!");
    }
    FreeRDP* realSelf = (FreeRDP*)self;
    PyGILState_STATE state;
    state = PyGILState_Ensure();
    PyEval_CallObject(realSelf->onConnect, args);
    Py_XDECREF(args);
    PyGILState_Release(state);
}

/**
 * Create FreeRDP class type.
 */
static PyObject* FreeRDP_new(PyTypeObject* type, PyObject* args, PyObject* kwargs) {
    FreeRDP* self = (FreeRDP*)type->tp_alloc(type, 0);
    return (PyObject*)self;
}

/**
 * Init FreeRDP class.
 */
static int FreeRDP_init(FreeRDP* self, PyObject* args, PyObject* kwargs) {
    char* args_string;
    PyObject* onConnect;
    if (!PyArg_ParseTuple(args, "sO:set_callback", &args_string, &onConnect))
        return -1;

    if (!PyCallable_Check(onConnect)) {
        PyErr_SetString(PyExc_TypeError, "onConnect must be callable");
        return -1;
    }
    Py_XINCREF(onConnect);
    Py_XDECREF(self->onConnect);
    self->onConnect = onConnect;

    char *argv[100];
    int argc = 1;
    argv[0] = "DUMMY";     //if called in main would be the program name
    char *arg = strtok(args_string, " ");
    while (arg != NULL && argc < 99) {
        argv[argc++] = arg;
        arg = strtok(NULL, " ");
    }

    void* instance = start(argc, argv, onConnect_callback);
       if (PyDict_SetItem(__module_instanceMap, PyUnicode_FromFormat("%d", instance), (PyObject*)self) != 0) {
        return -1;
    }
    PyObject* temp = self->instance;
    self->instance = PyCapsule_New(instance, NULL, NULL);
    Py_XDECREF(temp);
    return 0;
}    

/**
 * Run a command in remote session.
 */
static void FreeRDP_run_command(FreeRDP* self, PyObject* command) {
    char* command_string;
    if (!PyArg_ParseTuple(command, "s", &command_string))
        PyErr_SetString(PyExc_RuntimeError, "expect command");
    char* status = run_command(PyCapsule_GetPointer(self->instance, NULL), command_string);
    if (strlen(status) > 0) {
        PyErr_SetString(PyExc_RuntimeError, status);
    }
}

/**
 * Class representation string.
 */
static PyObject* FreeRDP_repr(FreeRDP* self) {
    return PyUnicode_FromFormat("FreeRDP instance");
}

/**
 * Class members.
 */
static PyMemberDef FreeRDP_members[] = {
    {"instance", T_OBJECT_EX, offsetof(FreeRDP, instance), 0, "API instance"},
    {"onConnect", T_OBJECT_EX, offsetof(FreeRDP, onConnect), 0, "OnConnect callback"},
    {NULL}
};

/**
 * Class methods.
 */
static PyMethodDef FreeRDP_methods[] = {
    {"run_command", (PyCFunction)FreeRDP_run_command, METH_VARARGS, "Run command"},
    {NULL}
};

/**
 * Define FreeRDP class type.
 */ 
static PyTypeObject FreeRDPType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "freerdp.FreeRDP",            /* tp_name */
    sizeof(FreeRDP),              /* tp_basicsize */
    0,                            /* tp_itemsize */
    (destructor)FreeRDP_dealloc,  /* tp_dealloc */
    0,                            /* tp_print */
    0,                            /* tp_getattr */
    0,                            /* tp_setattr */
    0,                            /* tp_reserved */
    (reprfunc)FreeRDP_repr,       /* tp_repr */
    0,                            /* tp_as_number */
    0,                            /* tp_as_sequence */
    0,                            /* tp_as_mapping */
    0,                            /* tp_hash  */
    0,                            /* tp_call */
    0,                            /* tp_str */
    0,                            /* tp_getattro */
    0,                            /* tp_setattro */
    0,                            /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT |
        Py_TPFLAGS_BASETYPE |
        Py_TPFLAGS_HAVE_GC,       /* tp_flags */
    "FreeRDP objects",            /* tp_doc */
    (traverseproc)FreeRDP_trav,   /* tp_traverse */
    (inquiry)FreeRDP_clear,       /* tp_clear */
    0,                            /* tp_richcompare */
    0,                            /* tp_weaklistoffset */
    0,                            /* tp_iter */
    0,                            /* tp_iternext */
    FreeRDP_methods,              /* tp_methods */
    FreeRDP_members,              /* tp_members */
    0,                            /* tp_getset */
    0,                            /* tp_base */
    0,                            /* tp_dict */
    0,                            /* tp_descr_get */
    0,                            /* tp_descr_set */
    0,                            /* tp_dictoffset */
    (initproc)FreeRDP_init,       /* tp_init */
    0,                            /* tp_alloc */
    FreeRDP_new,                  /* tp_new */
};

/**
 * Cleanup module.
 */
static void module_freerdp_free(void* module) {
    destroy(10000);
}

static int module_freerdp_trav(PyObject* module, visitproc visit, void* arg) {
    destroy(10000);
    Py_XDECREF(module);
    return 0;
}

static int module_freerdp_clear(void* module) {
    return 0;
}

/**
 * Define freerdp module.
 */
static PyModuleDef freerdpmodule = {
    PyModuleDef_HEAD_INIT,    
    "freerdp",                         /* m_name     */
    "FreeRDP client",                  /* m_doc      */
    -1,                                /* m_size     */
    NULL,                              /* m_methods  */ 
    NULL,                              /* m_reload   */
    (traverseproc)module_freerdp_trav, /* m_traverse */
    (inquiry)module_freerdp_clear,     /* m_clear    */
    (freefunc)module_freerdp_free      /* m_free     */
};

/**
 * Module init.
 */
PyMODINIT_FUNC
PyInit_freerdp(void) {
    PyEval_InitThreads();

    Py_XDECREF(__module_instanceMap); 
    __module_instanceMap = PyDict_New();
    assert(PyDict_Check(__module_instanceMap));
    if (__module_instanceMap == NULL) {
        return NULL;
    } 
    Py_XINCREF(__module_instanceMap);

    PyObject* m;
    if (PyType_Ready(&FreeRDPType) < 0)
        return NULL;
    m = PyModule_Create(&freerdpmodule);
    if (m == NULL)
        return NULL;
    Py_XINCREF(&FreeRDPType);
    PyModule_AddObject(m, "FreeRDP", (PyObject*)&FreeRDPType);
    return m;
}

