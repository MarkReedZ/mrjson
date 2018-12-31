
#include "py_defines.h"
#include <stdio.h>
#include "dconv.h"

#if defined(_MSC_VER)
#define inline __inline
//#else
//#include <stdint.h>
#endif

#define IS_NAN(x) Py_IS_NAN(x)
#define IS_INF(x) Py_IS_INFINITY(x)

static const char g_hexChars[] = "0123456789abcdef";

typedef struct _encoder
{
  char *start, *end, *s;
  int depth;  
  int indent;
  int skip_keys;
  int ensure_ascii;
  int sort_keys;
} Encoder;

static int SetError(const char *message)
{
  PyErr_Format (PyExc_ValueError, "%s", message);
  return 0;
}
static int resizeBuffer(Encoder *e, size_t len)
{
  size_t curSize = e->end - e->start;
  size_t newSize = curSize * 2;
  size_t offset = e->s - e->start;

  while (newSize < curSize + len) newSize *= 2;

  e->start = (char *) realloc (e->start, newSize);
  if (e->start == NULL)
  {
    SetError ("Could not reserve memory block");
    printf("resizeBuffer failed\n");
    return 0;
  }

  e->s = e->start + offset;
  e->end = e->start + newSize;
  return 1;
}

#define resizeBufferIfNeeded(__enc, __len) \
    if ( (size_t) ((__enc)->end - (__enc)->s) < (size_t) (__len))  { resizeBuffer((__enc), (__len)); }

static inline void reverse(char* begin, char* end)
{
  char t;
  while (end > begin) {
    t = *end;
    *end-- = *begin;
    *begin++ = t;
  }
}

static void doStringNoEscapes (Encoder *e, const char *str, const char *end)
{
  char *of = e->s;
  if ( (end-str) == 0 ) return; //printf("passed 0 len string?\n");

  for (;;)
  {
    switch (*str)
    {
      case 0x00:
      {
        if (str < end) {
          *(of++) = '\\'; *(of++) = 'u'; *(of++) = '0'; *(of++) = '0'; *(of++) = '0'; *(of++) = '0';
          break;
        } else {
          e->s += (of - e->s);
          return;
        }
      }
      case '\"': (*of++) = '\\'; (*of++) = '\"'; break;
      case '\\': (*of++) = '\\'; (*of++) = '\\'; break;
      case '\b': (*of++) = '\\'; (*of++) = 'b'; break;
      case '\f': (*of++) = '\\'; (*of++) = 'f'; break;
      case '\n': (*of++) = '\\'; (*of++) = 'n'; break;
      case '\r': (*of++) = '\\'; (*of++) = 'r'; break;
      case '\t': (*of++) = '\\'; (*of++) = 't'; break;

      case '/':
      {
        (*of++) = (*str);
        break;
      }
      case 0x26: // '&'
      case 0x3c: // '<'
      case 0x3e: // '>'
      {
        (*of++) = (*str);
        break;
      }
      case 0x01: case 0x02: case 0x03: case 0x04: case 0x05: case 0x06: case 0x07: 
      case 0x0b:
      case 0x0e: case 0x0f: case 0x10: case 0x11: case 0x12: case 0x13: case 0x14: case 0x15: case 0x16:
      case 0x17: case 0x18: case 0x19: case 0x1a: case 0x1b: case 0x1c: case 0x1d: case 0x1e: case 0x1f:
      {
        *(of++) = '\\'; *(of++) = 'u'; *(of++) = '0'; *(of++) = '0';
        *(of++) = g_hexChars[ (unsigned char) (((*str) & 0xf0) >> 4)];
        *(of++) = g_hexChars[ (unsigned char) ((*str) & 0x0f)];
        break;
      }
      default: (*of++) = (*str); break;
    }
    str++;
  }
}

#define CHAR4_INT(a, b, c, d)         \
   (unsigned int)((d << 24) | (c << 16) | (b << 8) | a)

#define CHAR8_LONG(a, b, c, d, e, f, g, h)       \
   (((uint64_t)h << 56) | ((uint64_t)g << 48) | ((uint64_t)f << 40)   \
    | ((uint64_t)e << 32) | (d << 24) | (c << 16) | (b << 8) | a)


int encode( PyObject *o, Encoder *e ) {
  resizeBufferIfNeeded(e,2048);

  if ( o == Py_True ) {
    //*((unsigned int*)e->s) = CHAR4_INT('t','r','u','e'); e->s += 4;
    memcpy( e->s, "true", 4 ); e->s += 4;
  } 
  else if ( o == Py_False ) {
    *((uint64_t*)e->s) = CHAR8_LONG('f','a','l','s','e',0,0,0); e->s += 5;
    //memcpy( e->s, "false", 5 ); e->s += 5;
  }
  else if ( PyLong_Check(o) ) {
    char *s = e->s;
    int overflow;
    long long i = PyLong_AsLongLongAndOverflow(o, &overflow);
    if (i == -1 && PyErr_Occurred()) return 0;
    if (overflow == 0) {
      if ( i >= 0 ) {
        do *s++ = (char)(48 + (i % 10ULL)); while(i /= 10ULL); 
      } else {
        i *= -1;
        do *s++ = (char)(48 + (i % 10ll)); while(i /= 10ll); 
        *s++ = '-';
      }
    } else {

      // TODO '[-123123123123123123123123123123]' [100000000000000000000] if overflow try PyLong_AsDouble
      unsigned long long ui = PyLong_AsUnsignedLongLong(o);
      if (PyErr_Occurred()) return 0;
      do *s++ = (char)(48 + (ui % 10ULL)); while(ui /= 10ULL);
    }
    reverse( e->s, s-1 );
    e->s += s - e->s;
  }
#if PY_MAJOR_VERSION < 3
  else if ( PyInt_Check(o) ) {
    char *s = e->s;
    //int overflow;
    long long i = PyInt_AS_LONG(o);
    if ( i >= 0 ) {
      do *s++ = (char)(48 + (i % 10ULL)); while(i /= 10ULL); 
    } else {
      i *= -1;
      do *s++ = (char)(48 + (i % 10ll)); while(i /= 10ll); 
      *s++ = '-';
    }
    reverse( e->s, s-1 );
    e->s += s - e->s;
  }
#endif
  else if (PyUnicode_Check(o)) {
    Py_ssize_t l;
#if PY_MAJOR_VERSION >= 3
    const char* s = PyUnicode_AsUTF8AndSize(o, &l);
#else
    const char* s = PyString_AsString( PyUnicode_AsUTF8String(o) );
    l = PyUnicode_GET_SIZE(o);
#endif
    *(e->s++) = '\"';
    if (s == NULL) return 0; //ERR
    if ( l <= 0 || l > UINT_MAX ) {
      if ( l != 0 ) {
        PyErr_SetString(PyExc_TypeError, "Bad string length");
        return 0;
      }
    } else {
      resizeBufferIfNeeded(e,l); // TODO error if buf not allocated
      doStringNoEscapes(e, s, s+l);
    }
    *(e->s++) = '\"';
  }
#if PY_MAJOR_VERSION < 3
  else if (PyString_Check(o)) {
    Py_ssize_t l;
    const char* s = PyString_AS_STRING(o);
    *(e->s++) = '\"';
    l = PyString_GET_SIZE(o);
    if (s == NULL) return 0; //ERR
    if ( l <= 0 || l > UINT_MAX ) {
      if ( l != 0 ) {
        PyErr_SetString(PyExc_TypeError, "Bad string length");
        return 0;
      }
    } else {
      resizeBufferIfNeeded(e,l); // TODO error if buf not allocated
      doStringNoEscapes(e, s, s+l);
    }
    *(e->s++) = '\"';
  }
#endif
  else if (PyList_Check(o)) {
    Py_ssize_t size = PyList_GET_SIZE(o);
    *(e->s++) = '[';
    if ( size == 0 ) {
      *(e->s++) = ']';
    } else {
      Py_ssize_t i;
      e->depth += 1;
      for (i = 0; i < size; i++) {
        PyObject* item;
        int r;
        if (Py_EnterRecursiveCall(" while JSONifying list object")) return 0;
        item = PyList_GET_ITEM(o, i);
        r = encode(item, e);
        Py_LeaveRecursiveCall();
        *(e->s++) = ',';
        if (!r) return 0;
      }
      e->depth -= 1;
      *((e->s)-1) = ']'; // overwriting last comma 
    }
  }
  else if (PyTuple_Check(o)) {
    Py_ssize_t size = PyTuple_GET_SIZE(o);
    *(e->s++) = '[';
    if ( size == 0 ) {
      *(e->s++) = ']';
    } else {
      Py_ssize_t i;
      e->depth += 1;
      for (i = 0; i < size; i++) {
        PyObject* item;
        int r;
        if (Py_EnterRecursiveCall(" while JSONifying list object")) return 0;
        item = PyTuple_GET_ITEM(o, i);
        r = encode(item, e);
        Py_LeaveRecursiveCall();
        *(e->s++) = ',';
        if (!r) return 0;
      }
      e->depth -= 1;
      *((e->s)-1) = ']'; // overwriting last comma 
    }
  }
  else if (PyDict_Check(o)) {

    PyObject* key;
    PyObject* item;
    Py_ssize_t pos = 0;
#if PY_MAJOR_VERSION < 3
    int empty = 1;
#endif
    *(e->s++) = '{';
    if (1) { //!e->sort_keys) {
      while (PyDict_Next(o, &pos, &key, &item)) {
#if PY_MAJOR_VERSION < 3
        empty = 0;
#endif
        if (PyUnicode_Check(key)) {
          int r;
          Py_ssize_t l;
#if PY_MAJOR_VERSION >= 3
          const char* key_str = PyUnicode_AsUTF8AndSize(key, &l);
#else
          const char* key_str = PyString_AsString( PyUnicode_AsUTF8String(key) );
          //const char* key_str = PyUnicode_AsUTF8String(key);
          l = PyUnicode_GET_SIZE(key);
#endif
          if (key_str == NULL) return 0;
          if ( l < 0 || l > UINT_MAX ) {
            PyErr_SetString(PyExc_TypeError, "Bad key string length");
            return 0;
          }
          resizeBufferIfNeeded(e,l);
          *(e->s++) = '\"';
          doStringNoEscapes(e, key_str, key_str+l);
          *(e->s++) = '\"'; *(e->s++) = ':';
          if (Py_EnterRecursiveCall(" while JSONifying dict object")) return 0;
          r = encode(item, e);
          Py_LeaveRecursiveCall();
          if (!r) return 0;
          *(e->s++) = ',';
        }
#if PY_MAJOR_VERSION < 3
        else if (PyString_Check(key)) {
          int r;
          Py_ssize_t l;
          const char* key_str = PyString_AS_STRING(key);
          l = PyString_GET_SIZE(key);
          if (key_str == NULL) return 0;
          if ( l <= 0 || l > UINT_MAX ) {
            PyErr_SetString(PyExc_TypeError, "Bad key string length");
            return 0;
          }
          resizeBufferIfNeeded(e,l);
          *(e->s++) = '\"';
          doStringNoEscapes(e, key_str, key_str+l);
          *(e->s++) = '\"'; *(e->s++) = ':';
          if (Py_EnterRecursiveCall(" while JSONifying dict object")) return 0;
          r = encode(item, e);
          Py_LeaveRecursiveCall();
          if (!r) return 0;
          *(e->s++) = ',';
        }
#endif
#if PY_MAJOR_VERSION < 3
        else if (PyLong_Check(key) || PyInt_Check(key)) {
#else
        else if (PyLong_Check(key)) {
#endif
          int r;
          *(e->s++) = '"';
          encode(key, e);
          *(e->s++) = '"'; *(e->s++) = ':';
          if (Py_EnterRecursiveCall(" while JSONifying dict object")) return 0;
          r = encode(item, e);
          Py_LeaveRecursiveCall();
          if (!r) return 0;
          *(e->s++) = ',';
        }
        //else if (!e->skip_keys) {
          //PyErr_SetString(PyExc_TypeError, "keys must be strings or numbers unless skipkeys is true");
        else {
          PyErr_SetString(PyExc_TypeError, "keys must be strings or numbers");
          return 0;
        }
      }
    }
#if PY_MAJOR_VERSION < 3
    if ( empty ) {
#else
    if ( pos == 0 ) {
#endif
      *(e->s++) = '}'; // empty dict
    } else {
      *((e->s)-1) = '}'; // overwriting last comma 
    }
  }
  else if ( PyFloat_Check(o) ) {
    double d = PyFloat_AsDouble(o);
    if (d == -1.0 && PyErr_Occurred()) return 0;
    if (IS_NAN(d)) {
      *((unsigned int*)e->s) = CHAR4_INT('N','a','N',0); e->s += 3;
    }
    else if ( IS_INF(d) ) {
      if ( d < 0 ) {
        *(e->s++) = '-';
        *((uint64_t*)e->s) = CHAR8_LONG('I','n','f','i','n','i','t','y'); e->s += 8;
      } else {
        *((uint64_t*)e->s) = CHAR8_LONG('I','n','f','i','n','i','t','y'); e->s += 8;
        //memcpy( e->s, "Infinity", 8 ); e->s += 8; 
      }
    } else {
      e->s = dtoa(d, e->s, 324);
    }
  }
  else if ( o == Py_None ) {
    memcpy( e->s, "null", 4 ); e->s += 4;
  }
  else {

    // TODO Check __json__, to_dict, toDict, _asDict
    if (PyObject_HasAttrString(o, "__json__"))
    {
      PyObject* func = PyObject_GetAttrString(o, "__json__"); 
      PyObject* res = PyObject_CallFunctionObjArgs(func, NULL);
      Py_DECREF(func);
  
      if (PyErr_Occurred()) {
        PyErr_SetString(PyExc_TypeError, "Unserializable object's __json__ threw an error");
        Py_XDECREF(res);
        return 0;
      }
      if (res == NULL) {
        PyErr_SetString(PyExc_TypeError, "Unserializable object's __json__ call failed");
        return 0;
      }
  
      if (!PyString_Check(res) && !PyUnicode_Check(res))
      {
        Py_DECREF(res);
        PyErr_Format (PyExc_TypeError, "expected string from custom object's __json__ method.");
        return 0;
      }

      {
      Py_ssize_t l;
#if PY_MAJOR_VERSION >= 3
      const char* p = PyUnicode_AsUTF8AndSize(res, &l);
#else
      //const char* p = PyUnicode_AsUTF8String(res);
      const char* p;
      if ( PyUnicode_Check(res) ) {
        p = PyString_AsString( PyUnicode_AsUTF8String(res) );
      } else {
        p = PyString_AsString( res );
      }
      l = PyUnicode_GET_SIZE(res);
#endif
      if (p == NULL) return 0; //TODO ERR

      memcpy(e->s, p, l);
      e->s += l;
      }
  
    } else {

      PyObject* objectsRepresentation = PyObject_Repr(o);
      const char* msg;
#if PY_MAJOR_VERSION >= 3
      Py_ssize_t sz;
      msg = PyUnicode_AsUTF8AndSize(objectsRepresentation, &sz);
#else
      msg  = PyString_AsString(objectsRepresentation);
#endif
      PyErr_Format(PyExc_TypeError, "%s is not JSON serializable, add an __json__ function", msg);
      return 0;
    }
  }
  return 1;
}

int do_encode(PyObject *o, Encoder *enc ) {
  int len = 65536;
  char *s = (char *) malloc (len);
  if (!s) {
    SetError("Could not reserve memory block");
    return 0;
  }

  enc->start = s;
  enc->end   = s + len;
  enc->s = s;
  return encode (o, enc);
}

PyObject* dumps(PyObject* self, PyObject *args, PyObject *kwargs) {
  static char *kwlist[] = { "obj", "ensure_ascii", "sort_keys", "indent", "skipkeys", "output_bytes", NULL };
  int r;

  PyObject *o = NULL;
  PyObject *ensure_ascii = NULL;
  PyObject *sort_keys = NULL;
  PyObject *indent = NULL;
  PyObject *skip_keys = NULL;
  PyObject *bytes = NULL;

  Encoder enc = { NULL,NULL,NULL,0,0,0,0,0 };

  if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O|OOOOO", kwlist, &o, &ensure_ascii, &sort_keys, &indent, &skip_keys, &bytes)) return NULL;

  if (ensure_ascii != NULL && !PyObject_IsTrue(ensure_ascii)) enc.ensure_ascii = 0;
  if (sort_keys    != NULL &&  PyObject_IsTrue(sort_keys   )) enc.sort_keys = 1;
  if (skip_keys    != NULL &&  PyObject_IsTrue(skip_keys   )) enc.skip_keys = 1;
  if (indent == NULL ) {
    enc.indent = -1;
  } else {
    enc.indent = PyLong_AsLong(indent);
  }

  r = do_encode( o, &enc );
 
  if ( r != 0 ) {
    PyObject *ret;
    *enc.s = '\0';
    if ( bytes != NULL ) { 
      ret = PyBytes_FromStringAndSize(enc.start, enc.s-enc.start);
    } else {
      ret = PyUnicode_FromStringAndSize(enc.start, enc.s-enc.start);
    }
    free(enc.start);
    return ret;
  } 
  free(enc.start);
  return NULL;
}

PyObject* dumpb(PyObject* self, PyObject *args, PyObject *kwargs) {
  if (kwargs == NULL) kwargs = PyDict_New();
  PyDict_SetItemString( kwargs, "output_bytes", Py_True );
  return dumps( self, args, kwargs );
}

PyObject* dump(PyObject* self, PyObject *args, PyObject *kwargs)
{
  PyObject *o, *file, *string, *write, *argtuple;

  if (!PyArg_ParseTuple (args, "OO", &o, &file)) return NULL; 

  if (!PyObject_HasAttrString (file, "write")) { PyErr_Format (PyExc_TypeError, "expected file"); return NULL; }

  write = PyObject_GetAttrString (file, "write");

  if (!PyCallable_Check (write)) { Py_XDECREF(write); PyErr_Format (PyExc_TypeError, "expected file"); return NULL; }

  argtuple = PyTuple_Pack(1, o);

  string = dumps(self, argtuple, kwargs);

  if (string == NULL) { Py_DECREF(write); Py_XDECREF(argtuple); return NULL; }

  Py_XDECREF(argtuple);

  argtuple = PyTuple_Pack (1, string);
  if (argtuple == NULL) { Py_DECREF(write); return NULL; }

  if (PyObject_CallObject (write, argtuple) == NULL) { Py_DECREF(write); Py_XDECREF(argtuple); return NULL; }

  Py_DECREF(write);
  Py_DECREF(argtuple);
  Py_DECREF(string);

  Py_RETURN_NONE;
}

