
#include "py_defines.h"
#include <stdio.h>
#include "dconv.h"

#define IS_NAN(x) Py_IS_NAN(x)
#define IS_INF(x) Py_IS_INFINITY(x)
//#define IS_NAN(x) std::isnan(x)
//#define IS_INF(x) std::isinf(x)

#define RESERVE_STRING(_len) (2 + ((_len) * 6))

static const char g_hexChars[] = "0123456789abcdef";

typedef struct _encoder
{
  char *start, *end, *s;
  int depth;  
  int indent;
  int skipKeys;
  int ensure_ascii;
  int sortKeys;
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
    if ( (size_t) ((__enc)->end - (__enc)->s) < (size_t) (__len))  { resizeBuffer((__enc), (__len)); }\


static inline void reverse(char* begin, char* end)
{
  char t;
  while (end > begin) {
    t = *end;
    *end-- = *begin;
    *begin++ = t;
  }
}

static int doStringNoEscapes (Encoder *e, const char *str, const char *end)
{
  char *of = e->s;
  if ( (end-str) == 0 ) printf("passed 0 len string?\n");
  //printf("%d\n", end-str);
  //printf("%.*s\n", end-str, str);

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
          return 1;
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
        //if (e->escapeForwardSlashes)
        //{
          //(*of++) = '\\';
          //(*of++) = '/';
        //}
        //else
        //{
          // Same as default case below.
        (*of++) = (*str);
        //}
        break;
      }
      case 0x26: // '&'
      case 0x3c: // '<'
      case 0x3e: // '>'
      {
        //if (e->encodeHTMLChars) {
          // Fall through to \u00XX case below.
        //} else {
        (*of++) = (*str);
        break;
        //}
      }
      case 0x01:
      case 0x02:
      case 0x03:
      case 0x04:
      case 0x05:
      case 0x06:
      case 0x07:
      case 0x0b:
      case 0x0e:
      case 0x0f:
      case 0x10:
      case 0x11:
      case 0x12:
      case 0x13:
      case 0x14:
      case 0x15:
      case 0x16:
      case 0x17:
      case 0x18:
      case 0x19:
      case 0x1a:
      case 0x1b:
      case 0x1c:
      case 0x1d:
      case 0x1e:
      case 0x1f:
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

int encode( PyObject *o, Encoder *e ) {
  resizeBufferIfNeeded(e,2048);
  if ( o == Py_None ) {
    *(e->s++) = 'n'; *(e->s++) = 'u'; *(e->s++) = 'l'; *(e->s++) = 'l';
  }
  else if ( PyBool_Check(o) ) {
    if ( o == Py_True ) {
    *(e->s++) = 't'; *(e->s++) = 'r'; *(e->s++) = 'u'; *(e->s++) = 'e';
    } else {
    *(e->s++) = 'f'; *(e->s++) = 'a'; *(e->s++) = 'l'; *(e->s++) = 's'; *(e->s++) = 'e';
    }
  }
  else if ( PyFloat_Check(o) ) {
    double d = PyFloat_AsDouble(o);
    if (d == -1.0 && PyErr_Occurred()) return 0;
    if (IS_NAN(d)) {
      *(e->s++) = 'N'; *(e->s++) = 'a'; *(e->s++) = 'N';
    }
    else if ( IS_INF(d) ) {
      if ( d < 0 ) {
        *(e->s++) = '-';
        *(e->s++) = 'I'; *(e->s++) = 'n'; *(e->s++) = 'f'; *(e->s++) = 'i'; *(e->s++) = 'n'; *(e->s++) = 'i'; *(e->s++) = 't'; *(e->s++) = 'y';
      } else {
        *(e->s++) = 'I'; *(e->s++) = 'n'; *(e->s++) = 'f'; *(e->s++) = 'i'; *(e->s++) = 'n'; *(e->s++) = 'i'; *(e->s++) = 't'; *(e->s++) = 'y';
      }
    } else {
      e->s = dtoa(d, e->s, 324);
    }
  }
  else if ( PyLong_Check(o) ) {
    char *s = e->s;
    int overflow;
    long long i = PyLong_AsLongLongAndOverflow(o, &overflow);
    if (i == -1 && PyErr_Occurred()) return 0;
    if (overflow == 0) {
      do *s++ = (char)(48 + (i % 10ULL)); while(i /= 10ULL);
      if (i < 0) *s++ = '-';
    } else {
      unsigned long long ui = PyLong_AsUnsignedLongLong(o);
      if (PyErr_Occurred()) return 0;
      do *s++ = (char)(48 + (ui % 10ULL)); while(ui /= 10ULL);
    }
    reverse( e->s, s-1 );
    e->s += s - e->s;
  }
  else if (PyUnicode_Check(o)) {
    *(e->s++) = '\"';
    Py_ssize_t l;
    const char* s = PyUnicode_AsUTF8AndSize(o, &l);
    if (s == NULL) return 0; //ERR
    //printf("str len %d\n", l);
    //if ( l == 0 ) printf("%.*s\n", 32, (e->s-32));
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
  else if (PyList_Check(o)) {
    *(e->s++) = '[';
    Py_ssize_t size = PyList_GET_SIZE(o);
    if ( size == 0 ) {
      *(e->s++) = ']';
    } else {
      e->depth += 1;
      for (Py_ssize_t i = 0; i < size; i++) {
        if (Py_EnterRecursiveCall(" while JSONifying list object")) return 0;
        PyObject* item = PyList_GET_ITEM(o, i);
        int r = encode(item, e);
        Py_LeaveRecursiveCall();
        *(e->s++) = ',';
        if (!r) return 0;
      }
      e->depth -= 1;
      *((e->s)-1) = ']'; // overwriting last comma 
    }
  }
  else if (PyTuple_Check(o)) {
    *(e->s++) = '[';
    Py_ssize_t size = PyTuple_GET_SIZE(o);
    if ( size == 0 ) {
      *(e->s++) = ']';
    } else {
      e->depth += 1;
      for (Py_ssize_t i = 0; i < size; i++) {
        if (Py_EnterRecursiveCall(" while JSONifying list object")) return 0;
        PyObject* item = PyTuple_GET_ITEM(o, i);
        int r = encode(item, e);
        Py_LeaveRecursiveCall();
        *(e->s++) = ',';
        if (!r) return 0;
      }
      e->depth -= 1;
      *((e->s)-1) = ']'; // overwriting last comma 
    }
  }
  else if (PyDict_Check(o)) {
    *(e->s++) = '{';

    Py_ssize_t pos = 0;
    PyObject* key;
    PyObject* item;

    if (1) { //!e->sortKeys) {
      while (PyDict_Next(o, &pos, &key, &item)) {
        if (PyUnicode_Check(key)) {
          Py_ssize_t l;
          const char* key_str = PyUnicode_AsUTF8AndSize(key, &l);
          if (key_str == NULL) return 0;
          //printf("key len %d\n", l);
          if ( l <= 0 || l > UINT_MAX ) {
            PyErr_SetString(PyExc_TypeError, "Bad key string length");
            return 0;
          }
          resizeBufferIfNeeded(e,l);
          *(e->s++) = '\"';
          doStringNoEscapes(e, key_str, key_str+l);
          *(e->s++) = '\"';
          *(e->s++) = ':';
          if (Py_EnterRecursiveCall(" while JSONifying dict object")) return 0;
          int r = encode(item, e);
          Py_LeaveRecursiveCall();
          if (!r) return 0;
          *(e->s++) = ',';
        }
        else if (!e->skipKeys) {
          PyErr_SetString(PyExc_TypeError, "keys must be strings");
          return 0;
        }
      }
    }
    if ( pos == 0 ) {
      *(e->s++) = '}'; // empty 
    } else {
      *((e->s)-1) = '}'; // overwriting last comma 
    }
  }
  else {
    printf("Unknown type!?\n");
    PyErr_Format(PyExc_TypeError, "%R is not JSON serializable", o);
    return 0;
  }
  return 1;
}

int do_encode(PyObject *o, Encoder *enc ) {
  int len = 65536;
  char *s = (char *) malloc (len);
  if (!s) {
    //SetError(obj, enc, 'Could not reserve memory block");
    return 0;
  }

  enc->start = s;
  enc->end   = s + len;
  enc->s = s;
  return encode (o, enc);

}

PyObject* toJson(PyObject* self, PyObject *args, PyObject *kwargs) {
  static char *kwlist[] = { "obj", "ensure_ascii", "sort_keys", "indent", NULL };

  PyObject *oinput = NULL;
  PyObject *oensureAscii = NULL;
  //PyObject *oencodeHTMLChars = NULL;
  //PyObject *oescapeForwardSlashes = NULL;
  PyObject *osortKeys = NULL;
  PyObject* oindent = NULL;

  Encoder enc = { NULL,NULL,NULL,0,0,0,0,0 };

  if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O|OOO", kwlist, &oinput, &oensureAscii, &osortKeys, &oindent)) return NULL;

  if (oensureAscii != NULL && !PyObject_IsTrue(oensureAscii)) enc.ensure_ascii = 0;
  if (osortKeys != NULL && PyObject_IsTrue(osortKeys)) enc.sortKeys = 1;
  if (oindent == NULL ) {
    enc.indent = -1;
  } else {
    enc.indent = PyLong_AsLong(oindent);
    //if ( PyErr_Occurred() == NULL ) printf("no err yet\n");
  }

  //if (oencodeHTMLChars != NULL && PyObject_IsTrue(oencodeHTMLChars)) encoder.encodeHTMLChars = 1;
  //if (oescapeForwardSlashes != NULL && !PyObject_IsTrue(oescapeForwardSlashes)) encoder.escapeForwardSlashes = 0;

  int r = do_encode( oinput, &enc );
 
  if ( r != 0 ) {
    //printf("%.*s\n", 3, enc.start);
    //printf("YAY");
    *enc.s = '\0';
    //PyErr_PrintEx(5);
    //printf("YAY enc len %d\n", (enc.s)-(enc.start));
    //PyObject *o = PyString_FromString (enc.start); 
    //PyObject *o = PyUnicode_FromKindAndData(PyUnicode_1BYTE_KIND, enc.start, enc.s-enc.start);
    PyObject *o = PyUnicode_FromStringAndSize(enc.start, enc.s-enc.start);
    free(enc.start);
    return o;
  } 
  free(enc.start);
  return NULL;
}

//char *toJson(PyObject *o, char *_buffer, size_t _cbBuffer)

PyObject* toJsonFile(PyObject* self, PyObject *args, PyObject *kwargs)
{
  PyObject *o, *file, *string, *write, *argtuple;

  if (!PyArg_ParseTuple (args, "OO", &o, &file)) return NULL; 

  if (!PyObject_HasAttrString (file, "write"))
  { 
    PyErr_Format (PyExc_TypeError, "expected file");
    return NULL;
  }

  write = PyObject_GetAttrString (file, "write");

  if (!PyCallable_Check (write))
  {
    Py_XDECREF(write);
    PyErr_Format (PyExc_TypeError, "expected file");
    return NULL;
  }

  argtuple = PyTuple_Pack(1, o);

  string = toJson(self, argtuple, kwargs);

  if (string == NULL)
  { 
    Py_XDECREF(write);
    Py_XDECREF(argtuple);
    return NULL;
  }

  Py_XDECREF(argtuple);

  argtuple = PyTuple_Pack (1, string);
  if (argtuple == NULL)
  {
    Py_XDECREF(write);
    return NULL;
  }
  if (PyObject_CallObject (write, argtuple) == NULL)
  {
    Py_XDECREF(write);
    Py_XDECREF(argtuple);
    return NULL;
  }

  Py_XDECREF(write);
  Py_DECREF(argtuple);
  Py_XDECREF(string);

  Py_RETURN_NONE;
}

