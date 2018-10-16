
#include "py_defines.h"

#define STRINGIFY(x) XSTRINGIFY(x)
#define XSTRINGIFY(x) #x

PyObject* dump  (PyObject* self, PyObject *args, PyObject *kwargs);
PyObject* dumps (PyObject* self, PyObject *args, PyObject *kwargs);
PyObject* dumpb (PyObject* self, PyObject *args, PyObject *kwargs);

PyObject* load  (PyObject* self, PyObject *args, PyObject *kwargs);
PyObject* loads (PyObject* self, PyObject *args, PyObject *kwargs);
PyObject* loadb (PyObject* self, PyObject *args, PyObject *kwargs);


static PyMethodDef mrjsonMethods[] = {
  {"dump",   (PyCFunction) dump,     METH_VARARGS | METH_KEYWORDS, "Encode an object as a json file" },
  {"dumps",  (PyCFunction) dumps,    METH_VARARGS | METH_KEYWORDS, "Encode an object as a json string" },
  {"dumpb",  (PyCFunction) dumpb,    METH_VARARGS | METH_KEYWORDS, "Encode an object as json bytes"  },

  {"load",   (PyCFunction) load,   METH_VARARGS | METH_KEYWORDS, "Decode a json file" },
  {"loads",  (PyCFunction) loads,  METH_VARARGS | METH_KEYWORDS, "Decode a json string" },
  {"loadb",  (PyCFunction) loadb,  METH_VARARGS | METH_KEYWORDS, "Decode a json byte string" },
  {NULL, NULL, 0, NULL}       /* Sentinel */
};

#if PY_MAJOR_VERSION >= 3

static struct PyModuleDef moduledef = {
  PyModuleDef_HEAD_INIT,
  "mrjson",
  0,              /* m_doc */
  -1,             /* m_size */
  mrjsonMethods,  /* m_methods */
  NULL,           /* m_reload */
  NULL,           /* m_traverse */
  NULL,           /* m_clear */
  NULL            /* m_free */
};

#define PYMODINITFUNC       PyObject *PyInit_mrjson(void)
#define PYMODULE_CREATE()   PyModule_Create(&moduledef)
#define MODINITERROR        return NULL

#else

#define PYMODINITFUNC       PyMODINIT_FUNC initmrjson(void)
#define PYMODULE_CREATE()   Py_InitModule("mrjson", mrjsonMethods)
#define MODINITERROR        return

#endif

PYMODINITFUNC
{
  PyObject *m;

  m = PYMODULE_CREATE();

  if (m == NULL) { MODINITERROR; }

  PyModule_AddStringConstant(m, "__version__", STRINGIFY(MRJSON_VERSION));
  PyModule_AddStringConstant(m, "__author__", "Mark Reed <MarkReedZ@mail.com>");
#if PY_MAJOR_VERSION >= 3
  return m;
#endif
}
