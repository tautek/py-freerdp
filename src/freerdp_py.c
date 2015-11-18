#include <Python.h>
#include <structmember.h>
#include <signal.h>
#include "freerdp.h"
#include "freerdp_const_py.h"

#define FR_LOG(MSG) fprintf(stderr, "%s\n", MSG); 
#define FR_DEBUG(MSG) fprintf(stderr, "%s\n", MSG);
#define GETSTATE(m) ((struct module_state*)PyModule_GetState(m))

/**
 * Global pointer to module.
 */
PyObject* __global_module = NULL;

/**
 * Module level mapping tracking C pointers
 * for callbacks.
 */
struct module_state {
    PyObject* _module_instanceMap;
};

/**
 * Defines the FreeRDP class data.
 */
typedef struct {
    PyObject_HEAD
    PyObject* _instance;
    PyObject* _onConnect;
} FreeRDP;

/**
 * Cyclic garbace collection.
 */
static int FreeRDP_trav(FreeRDP* self, visitproc visit, void* arg) {
    FR_DEBUG("FreeRDP_trav+")
    Py_VISIT(self->_instance);
    Py_VISIT(self->_onConnect);
    //Py_XDECREF(self);
    FR_DEBUG("-FreeRDP_trav")
    return 0;
}

/**
 * Clear FreeRDP class instance.
 */
static int FreeRDP_clear(FreeRDP* self) {
    FR_DEBUG("FreeRDP_clear+")
    Py_CLEAR(self->_instance);
    Py_CLEAR(self->_onConnect);
    FR_DEBUG("-FreeRDP_clear")
    return 0;
}

/**
 * Cleanup FreeRDP class instance.
 */
static void FreeRDP_dealloc(FreeRDP* self) {
    FR_DEBUG("FreeRDP_dealloc+")
    stop(PyCapsule_GetPointer(self->_instance, NULL));
    FreeRDP_clear(self);
    Py_TYPE(self)->tp_free(self);
    FR_DEBUG("-FreeRDP_dealloc")
}

/**
 * Static callback point for 'onConnect' event.
 * Maps the callback back to the instance that
 * initiated it.
 */
static void onConnect_callback(void* instance) {
    FR_DEBUG("onConnect_callback+")
    PyObject* self = PyDict_GetItem(GETSTATE(__global_module)->_module_instanceMap, PyUnicode_FromFormat("%d", instance));
    if (self==NULL) { fprintf(stderr,"SELF NULL!!!!");}
    PyObject* args = PyTuple_New(1);
    if (PyTuple_SetItem(args, 0, self) != 0) {
        fprintf(stderr, "TUPLE ERROR!!");
    }
    FreeRDP* realSelf = (FreeRDP*)self;
    PyGILState_STATE state;
    state = PyGILState_Ensure();
    PyEval_CallObject(realSelf->_onConnect, args);
    Py_XDECREF(args);
    PyGILState_Release(state);
    FR_DEBUG("-onConnect_callback")
}

/**
 * Create FreeRDP class type.
 */
static PyObject* FreeRDP_new(PyTypeObject* type, PyObject* args, PyObject* kwargs) {
    FR_DEBUG("FreeRDP_new+")
    FreeRDP* self = (FreeRDP*)type->tp_alloc(type, 0);
    FR_DEBUG("-FreeRDP_new")
    return (PyObject*)self;
}

/**
 * Init FreeRDP class.
 */
static int FreeRDP_init(FreeRDP* self, PyObject* args, PyObject* kwargs) {
    FR_DEBUG("FreeRDP_init+")
    char* args_string;
    PyObject* onConnect;
    if (!PyArg_ParseTuple(args, "sO:set_callback", &args_string, &onConnect))
        return -1;
    if (!PyCallable_Check(onConnect)) {
        PyErr_SetString(PyExc_TypeError, "onConnect must be callable");
        return -1;
    }
    Py_XINCREF(onConnect);
    Py_XDECREF(self->_onConnect);
    self->_onConnect = onConnect;

    char *argv[100];
    int argc = 1;
    argv[0] = "DUMMY";     //if called in main would be the program name
    char *arg = strtok(args_string, " ");
    while (arg != NULL && argc < 99) {
        argv[argc++] = arg;
        arg = strtok(NULL, " ");
    }

    void* instance = start(argc, argv, onConnect_callback);
    if (PyDict_SetItem(GETSTATE(__global_module)->_module_instanceMap, PyUnicode_FromFormat("%d", instance), (PyObject*)self) != 0) {
        return -1;
    }
    PyObject* temp = self->_instance;
    self->_instance = PyCapsule_New(instance, NULL, NULL);
    Py_XDECREF(temp);
    FR_DEBUG("FreeRDP_init-")
    return 0;
}    

/**
 * Run a command in remote session.
 */
static PyObject* FreeRDP_run_command(FreeRDP* self, PyObject* command) {
    FR_DEBUG("FreeRDP_run_command+")
    char* command_string;
    if (!PyArg_ParseTuple(command, "s", &command_string))
        PyErr_SetString(PyExc_RuntimeError, "expect command");
    run_command(PyCapsule_GetPointer(self->_instance, NULL), command_string);
    FR_DEBUG("-FreeRDP_run_command")
    Py_RETURN_NONE;
}

static PyObject* FreeRDP_press_keys(FreeRDP* self, PyObject* args) {
    FR_DEBUG("FreeRDP_press_keys+")
    PyObject* list;
    if (!PyArg_ParseTuple(args, "O", &list))
        PyErr_SetString(PyExc_RuntimeError, "expect keys");
    int count = PySequence_Fast_GET_SIZE(list);
    DWORD keys[count];
    int index;
    for (index=0; index < count; ++index) {
        keys[index] = PyLong_AsUnsignedLong(PySequence_Fast_GET_ITEM(list, index));
    }
    press_keys(PyCapsule_GetPointer(self->_instance, NULL), count, keys);
    Py_XDECREF(list);
    FR_DEBUG("-FreeRDP_press_keys")
    Py_RETURN_NONE;
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
    {NULL}
};

/**
 * Class methods.
 */
static PyMethodDef FreeRDP_methods[] = {
    {"run_command", (PyCFunction)FreeRDP_run_command, METH_VARARGS, "Run command"},
    {"press_keys", (PyCFunction)FreeRDP_press_keys, METH_VARARGS, "Press keys"},
    {NULL, NULL}
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
    FR_DEBUG("module_freerdp_free+")
    //destroy(10000);
    FR_DEBUG("-module_freerdp_free")
}

static int module_freerdp_trav(PyObject* module, visitproc visit, void* arg) {
    FR_DEBUG("module_freerdp_trav+")
    //destroy(10000);
    //Py_XDECREF(module);
    FR_DEBUG("-module_freerdp_trav")
    return 0;
}

static int module_freerdp_clear(void* module) {
    FR_DEBUG("module_freerdp_clear+")
    FR_DEBUG("-module_freerdp_clear")
    return 0;
}

/**
 * Define freerdp module.
 */
static PyModuleDef freerdpmodule = {
    PyModuleDef_HEAD_INIT,    
    "freerdp",                         /* m_name     */
    "FreeRDP client",                  /* m_doc      */
    sizeof(struct module_state),       /* m_size     */
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
    PyEval_InitThreads();
    PyEval_InitThreads();
    PyObject* module;
    if (PyType_Ready(&FreeRDPType) < 0)
        return NULL;
    module = PyModule_Create(&freerdpmodule);
    if (module == NULL)
        return NULL;
    struct module_state *state = GETSTATE(module);
    state->_module_instanceMap = PyDict_New();
    if (state->_module_instanceMap == NULL) {
        Py_XDECREF(module);
        return NULL;
    }
    Py_XINCREF(&FreeRDPType);
    PyModule_AddObject(module, "FreeRDP", (PyObject*)&FreeRDPType);
    FreeRDP_AddConstants(module);
    __global_module = module;
    return module;
}

