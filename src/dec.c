#include "dec.h"
#include "py_defines.h"
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#ifdef __AVX2__
#include <immintrin.h>
#endif

#define JSON_MAX_DEPTH 32

static char errmsg[256];

void printErr(void) {
  PyObject *type, *value, *traceback;
  PyErr_Fetch(&type, &value, &traceback);
  printf("Unhandled exception :\n");
  PyObject_Print( type, stdout, 0 ); printf("\n");
  PyObject_Print( value, stdout, 0 ); printf("\n");
}

static inline bool _isspace(char c)  { return (c == ' ') || (c >= '\t' && c <= '\r'); }
static inline bool _isdelim(char c)  { return  c == ','  || c == ':' || c == ']' || c == '}' || _isspace(c) || !c; }
static inline bool _isdigit(char c)  { return  c >= '0'  && c <= '9'; }
static inline bool _isxdigit(char c) { return (c >= '0'  && c <= '9') || ((c & ~' ') >= 'A' && (c & ~' ') <= 'F'); }

static inline int char2int(char c) {
    if (c <= '9') return c - '0';
    return (c & ~' ') - 'A' + 10;
}

// Reads a number from s and sets outptr to the character following the number
static PyObject* parseNumber(char *s, char **outptr) {
  bool flt = false;
  char *st = s;
  char ch = *s;
  int64_t l = 0;
  double d;
  int cnt = 16;
  long frac = 0;
  long prec = 1;
  int neg = 0;
  unsigned int exponent = 0;
  PyObject *ret;

  if (ch == '-') ++s;

  while (_isdigit(*s) && (--cnt != 0)) {
    l = (l * 10) + (*s++ - '0');
  }

  // We're done unless its a float, exponent, or is too big: 1.2, 10e5, or more than 16 digits
  if ( cnt != 0 && *s != '.' && *s != 'e' && *s != 'E' ) {
    *outptr = s;
    return PyLong_FromLongLong(ch == '-' ? -l : l);
  }

  d = (double) l;
  while (_isdigit(*s)) d = (d * 10) + (*s++ - '0');

  if (*s == '.') {
    flt = true;
    ++s;
    while (_isdigit(*s)) {
      frac =  (frac*10) + (*s++-'0');
      prec *= 10;
    }
    d += ((double)frac/prec);
  }

  if (*s == 'e' || *s == 'E') {
    flt = true;
    ++s;

    if (*s == '+')
      ++s;
    else if (*s == '-') {
      ++s;
      neg = 1;
    }

    while (_isdigit(*s)) exponent = (exponent * 10) + (*s++ - '0');

    if ( neg ) {
      double power = 1.0;
      static double btab[9] = { 0.1, 0.01, 0.0001, 1e-8, 1e-16, 1e-32, 1e-64, 1e-128, 1e-256 };
      int num = 0;

      if ( exponent & ~0x1FF ) { *outptr = s; return PyFloat_FromDouble( 0 ); } // 1e-512 is 0
  
      for (; exponent; exponent >>= 1, num += 1 )
        if (exponent & 1) power *= btab[num];
      d *= power;

    } else {
      double power = 1.0;
      static double btab[9] = { 10.0, 100.0, 10000.0, 1e8, 1e16, 1e32, 1e64, 1e128, 1e256 };
      int num = 0;
   
#if __STDC_VERSION__ >= 199901L 
      if ( exponent & ~0x1FF ) { *outptr = s; return PyFloat_FromDouble( INFINITY ); } // 1e512 is infinity?
#else
      if ( exponent & ~0x1FF ) { *outptr = s; return PyFloat_FromDouble( HUGE_VAL ); } // 1e512 is infinity?
#endif
  
      for (; exponent; exponent >>= 1, num += 1 )
        if (exponent & 1) power *= btab[num];
      d *= power;

    }
  }

  *outptr = s;
  if ( flt ) return PyFloat_FromDouble(ch == '-' ? -d : d);
  else {
    ch = *s;
    *s = 0;
    ret = PyLong_FromString(st, NULL, 10); 
    *s = ch;
    return ret;
  }
}

#ifdef __AVX2__
//#ifdef _MSC_VER
//// Gets the first non zero bit
//static unsigned long TZCNT(unsigned long long inp) {
  //unsigned long res;
  //__asm{
    //tzcnt rax, inp
    //mov res, rax
  //}
  //return res;
//}
//#else
// Gets the first non zero bit
static unsigned long TZCNT(unsigned long long in) {
  unsigned long res;
  asm("tzcnt %1, %0\n\t" : "=r"(res) : "r"(in));
  return res;
}
//#endif
#endif

static PyObject* SetError(const char *message)
{
  PyErr_Format (PyExc_ValueError, "%s", message);
  return NULL;
}
static PyObject* SetErrorInt(const char *message, int pos)
{
  char pstr[32];
  sprintf( pstr, "%d", pos );
  strcpy( errmsg, message );
  strcat( errmsg, pstr );
  PyErr_Format (PyExc_ValueError, "%s", errmsg);
  return NULL;
}

PyObject* jsonParse(char *s, char **endptr, size_t len) {
  PyObject* tails[JSON_MAX_DEPTH];
  JsonTag tags[JSON_MAX_DEPTH];
  PyObject* keys[JSON_MAX_DEPTH];
  char *string_start;
  char *s_start = s;
  char *end = s+len;
  int pos = -1;
  bool separator = true;
  PyObject *o = NULL;

  char tmpmsg[128] = "Unexpected escape character 'Z' at pos ";
  char *it;
  int i;

  *endptr = s;
  while (*s) {
    while (_isspace(*s)) {
      ++s;
      if (!*s) break;
    }
    *endptr = s++;
    switch (**endptr) {
      case '-':
        if (s[0] == 'I') {
          if (!(s[1] == 'n' && s[2] == 'f' && ((end-s)==8 || _isdelim(s[8])))) return SetErrorInt("Expecting '-Infinity' at pos ",s-s_start-1);
          o = PyFloat_FromDouble(-Py_HUGE_VAL);
          s += 8;
          break;
        }
        if (!_isdigit(*s) && *s != '.') {
          *endptr = s;
          return SetErrorInt("Saw a - without a number following it at pos ", s-s_start-1);
        }
      case '0': case '1': case '2': case '3': case '4':
      case '5': case '6': case '7': case '8': case '9':
        o = parseNumber(*endptr, &s);
        if (!_isdelim(*s)) {
          *endptr = s;
          return SetErrorInt("A number must be followed by a delimiter at pos ", s-s_start-1);
        }
        break;
      case ']':
        if (pos == -1)               return SetErrorInt("Closing bracket ']' without an opening bracket at pos ", s-s_start-1);
        if (tags[pos] != JSON_ARRAY) return SetErrorInt("Mismatched closing bracket, expected a } at pos ", s-s_start-1);
        o = tails[pos];
        pos--;
        break;
      case '[':
        if (++pos == JSON_MAX_DEPTH) return SetErrorInt("Too many nested objects, the max depth is ", JSON_MAX_DEPTH);
        tails[pos] = PyList_New(0);
        tags[pos]  = JSON_ARRAY;
        keys[pos]  = NULL;
        separator  = true;
        continue;
      case '}':
        if (pos == -1)                return SetErrorInt("Closing bracket '}' without an opening bracket at pos ", s-s_start-1);
        if (tags[pos] != JSON_OBJECT) return SetErrorInt("Closing bracket '}' without an opening bracket at pos ", s-s_start-1);
        if (keys[pos] != NULL)        return SetErrorInt("Expected a \"key\":value, but got a closing bracket '}' at pos ", s-s_start-1);
        o = tails[pos];
        pos--;
        break;
      case '{':
        if (++pos == JSON_MAX_DEPTH) return SetErrorInt("Too many nested objects, the max depth is ", JSON_MAX_DEPTH);
        tails[pos] = PyDict_New();
        tags[pos] = JSON_OBJECT;
        keys[pos] = NULL;
        separator = true;
        continue;
      case ':':
        if (separator || keys[pos] == NULL) return SetErrorInt("Unexpected character ':' while parsing JSON string at pos ", s-s_start-1);
        separator = true;
        continue;
      case '"':
        o = NULL;
        string_start = s;
        for (it = s; *s; ++it, ++s) {
            int c = *it = *s;
            if (c == '\\') {
                c = *++s;
                switch (c) {
                case '\\':
                case '"':
                case '/':
                    *it = c;
                    break;
                case 'b':
                    *it = '\b';
                    break;
                case 'f':
                    *it = '\f';
                    break;
                case 'n':
                    *it = '\n';
                    break;
                case 'r':
                    *it = '\r';
                    break;
                case 't':
                    *it = '\t';
                    break;
                case 'u':
                   c = 0;
                    // \ud8XX\udcXX
                    if ( s[1] == 'd' && s[2] == '8' ) {
                      unsigned long tmp,wc;
                      if ( s[5] != '\\' ) return SetErrorInt("Invalid lonely surrogate \\ud800 at ", s-s_start-2);
                      tmp = (char2int(s[8])<<8 | char2int(s[9])<<4 | char2int(s[10])) - 0xC00;
                      wc = 0x10000 | char2int(s[3])<<14 | char2int(s[4])<<10 | tmp;
                      *it++ = ( 0xf0 | ((wc >> 18)&0xF) );
                      *it++ = ( 0x80 | ((wc >> 12) & 0x3f) );
                      *it++ = ( 0x80 | ((wc >> 6) & 0x3f) );
                      *it   = ( 0x80 | (wc & 0x3f) );
                      s += 10;
                      break;
                    }
                    for (i = 0; i < 4; ++i) {
                        if (_isxdigit(*++s)) {
                            c = c * 16 + char2int(*s);
                        } else {
                            *endptr = s;
                            return SetErrorInt("Unicode escape malformed at pos ", s-s_start-5);
                        }
                    }
                    if (c < 0x80) {
                        *it = c;
                    } else if (c < 0x800) {
                        *it++ = 0xC0 | (c >> 6);
                        *it = 0x80 | (c & 0x3F);
                    } else {
                        *it++ = 0xE0 | (c >> 12);
                        *it++ = 0x80 | ((c >> 6) & 0x3F);
                        *it = 0x80 | (c & 0x3F);
                    }
                    break;

                default:
                    *endptr = s;
                    tmpmsg[29] = c;
                    return SetErrorInt(tmpmsg, s-s_start-1);
                }
            } else if ((unsigned int)c < ' ' || c == '\x7F') {
              *endptr = s;
              return SetErrorInt("Invalid control characters in string at pos ", s-s_start-1);
            } else if (c == '"') {
              *it = 0;
              o = PyUnicode_FromStringAndSize(string_start, (it - string_start));
              if ( o == NULL ) return NULL;
              ++s;
              break;
            }
        }
        if ( o == NULL ) return SetErrorInt("Unterminated string starting at ", string_start-s_start-1);
        break;
      case 't':
        if (!(s[0] == 'r' && s[1] == 'u' && s[2] == 'e' && ((end-s)==3 || _isdelim(s[3])))) return SetErrorInt("Expecting 'true' at pos ",s-s_start-1);
        o = Py_True; Py_INCREF(Py_True);
        s += 3;
        break;
      case 'f':
        if (!(s[0] == 'a' && s[1] == 'l' && s[2] == 's' && s[3] == 'e' && ((end-s)==4 || _isdelim(s[4])))) return SetErrorInt("Expecting 'false' at pos ",s-s_start-1);
        o = Py_False; Py_INCREF(Py_False);
        s += 4;
        break;
      case 'n':
        if (!(s[0] == 'u' && s[1] == 'l' && s[2] == 'l' && ((end-s)==3 || _isdelim(s[3])))) return SetErrorInt("Expecting 'null' at pos ",s-s_start-1);
        o = Py_None; Py_INCREF(Py_None);
        s += 3;
        break;
      case 'N':
        if (!(s[0] == 'a' && s[1] == 'N' && ((end-s)==2 || _isdelim(s[2])))) return SetErrorInt("Expecting 'NaN' at pos ",s-s_start-1);
        o = PyFloat_FromDouble(Py_NAN);
        s += 2;
        break;
      case 'I': // Infinity
        if (!(s[0] == 'n' && s[1] == 'f' && ((end-s)==7 || _isdelim(s[7])))) return SetErrorInt("Expecting 'Infinity' at pos ",s-s_start-1);
        o = PyFloat_FromDouble(Py_HUGE_VAL);
        s += 7;
        break;
      case ',':
        if (separator || keys[pos] != NULL) return SetErrorInt("Unexpected character ',' at pos ", s-s_start-1);
        separator = true;
        continue;
      case '\0':
        continue;
      default:
        if ( keys[pos] != NULL && !separator ) return SetErrorInt("Expecting a : after an object key at pos ",s-s_start-1);
        return SetErrorInt("Unexpected character at pos ",s-s_start-1);
    }
    separator = false;
    if (pos == -1) {
      *endptr = s;
      return o;
    }
    if (tags[pos] == JSON_OBJECT) {
      if (!keys[pos]) {
        keys[pos] = o;
        continue;
      }
      PyDict_SetItem (tails[pos], keys[pos], o);
      Py_DECREF( (PyObject *) keys[pos]);
      Py_DECREF( (PyObject *) o);
      keys[pos] = NULL;
    } else {
      PyList_Append(tails[pos], o);
      Py_DECREF( (PyObject *) o);
    }
  }
  return SetError("Unexpected end of json string - could be a bad utf-8 encoding or check your [,{,\"");
}

#ifdef __AVX2__
PyObject* jParse(char *s, char **endptr, size_t len) {
  PyObject* tails[JSON_MAX_DEPTH];
  JsonTag tags[JSON_MAX_DEPTH];
  PyObject* keys[JSON_MAX_DEPTH];
  char *string_start;
  int pos = -1;
  bool separator = true;
  char *s_start = s;
  *endptr = s;
  PyObject *o = NULL;
  char tmpmsg[128] = "Unexpected escape character 'Z' at pos ";
  char *it;
  int i;

  unsigned long *quoteBitMap    = (unsigned long *) malloc( (len/64+2) * sizeof(unsigned long) );
  unsigned long *nonAsciiBitMap = (unsigned long *) malloc( (len/64+2) * sizeof(unsigned long) );

  __m256i b0, b1, b2, b3;
  unsigned char tmpbuf[32];
  int bmidx=0;
  int dist;
  char *ostart = s;
  char *buf = s;
  char *end = s+len;
  const __m256i rrq    = _mm256_set1_epi8(0x22);
  const __m256i rr0    = _mm256_set1_epi8(0);
  const __m256i rr_esc = _mm256_set1_epi8(0x5C);
  while( buf < end ) {
    if((dist = end - buf) < 128) {
      for (i=0; i < (dist & 31); i++) tmpbuf[i] = buf[ (dist & (-32)) + i];
      if (dist >= 96) {
        b0 = _mm256_loadu_si256((__m256i *)(buf + 32*0));
        b1 = _mm256_loadu_si256((__m256i *)(buf + 32*1));
        b2 = _mm256_loadu_si256((__m256i *)(buf + 32*2));
        b3 = _mm256_loadu_si256((__m256i *) (tmpbuf));
      } else if (dist >= 64) {
        b0 = _mm256_loadu_si256((__m256i *)(buf + 32*0));
        b1 = _mm256_loadu_si256((__m256i *)(buf + 32*1));
        b2 = _mm256_loadu_si256((__m256i *) (tmpbuf));
        b3 = _mm256_setzero_si256();
      } else {
        if(dist < 32) {
          b0 = _mm256_loadu_si256((__m256i *) (tmpbuf));
          quoteBitMap[bmidx] = _mm256_movemask_epi8(_mm256_cmpeq_epi8(rrq, b0));
          __m256i is0_or_esc0 = _mm256_or_si256(_mm256_cmpeq_epi8(rr0, b0),_mm256_cmpeq_epi8(rr_esc, b0));
          __m256i q0 = _mm256_or_si256(_mm256_cmpgt_epi8(rr0, b0), is0_or_esc0);
          nonAsciiBitMap[bmidx] = _mm256_movemask_epi8(q0);
          break;
        } else {
          b0 = _mm256_loadu_si256((__m256i *)(buf + 32*0));
          b1 = _mm256_loadu_si256((__m256i *) (tmpbuf));
        
          quoteBitMap[bmidx] = (_mm256_movemask_epi8(_mm256_cmpeq_epi8(rrq, b0))&0xffffffffull) | ((unsigned long)_mm256_movemask_epi8(_mm256_cmpeq_epi8(rrq, b1))<<32);
          __m256i is0_or_esc0 = _mm256_or_si256(_mm256_cmpeq_epi8(rr0, b0),_mm256_cmpeq_epi8(rr_esc, b0));
          __m256i is0_or_esc1 = _mm256_or_si256(_mm256_cmpeq_epi8(rr0, b1),_mm256_cmpeq_epi8(rr_esc, b1));
          __m256i q0 = _mm256_or_si256(_mm256_cmpgt_epi8(rr0, b0), is0_or_esc0);
          __m256i q1 = _mm256_or_si256(_mm256_cmpgt_epi8(rr0, b1), is0_or_esc1);
          nonAsciiBitMap[bmidx] = _mm256_movemask_epi8(q0) ^ ((unsigned long)_mm256_movemask_epi8(q1)<<32);
          break;
        }
      }
    } else {
      /* Load 128 bytes */
      b0 = _mm256_loadu_si256((__m256i *)(buf + 32*0));
      b1 = _mm256_loadu_si256((__m256i *)(buf + 32*1));
      b2 = _mm256_loadu_si256((__m256i *)(buf + 32*2));
      b3 = _mm256_loadu_si256((__m256i *)(buf + 32*3));
    }
    buf += 128;

    unsigned int r0,r1,r2,r3;
    __m256i q0 = _mm256_cmpeq_epi8(rrq, b0);
    __m256i q1 = _mm256_cmpeq_epi8(rrq, b1);
    __m256i q2 = _mm256_cmpeq_epi8(rrq, b2);
    __m256i q3 = _mm256_cmpeq_epi8(rrq, b3);
    r0 = _mm256_movemask_epi8(q0);
    r1 = _mm256_movemask_epi8(q1);
    r2 = _mm256_movemask_epi8(q2);
    r3 = _mm256_movemask_epi8(q3);
    quoteBitMap[bmidx]   = r0 ^ ((unsigned long)r1 << 32);
    quoteBitMap[bmidx+1] = r2 ^ ((unsigned long)r3 << 32);   

    __m256i is0_or_esc0 = _mm256_or_si256(_mm256_cmpeq_epi8(rr0, b0),_mm256_cmpeq_epi8(rr_esc, b0));
    __m256i is0_or_esc1 = _mm256_or_si256(_mm256_cmpeq_epi8(rr0, b1),_mm256_cmpeq_epi8(rr_esc, b1));
    __m256i is0_or_esc2 = _mm256_or_si256(_mm256_cmpeq_epi8(rr0, b2),_mm256_cmpeq_epi8(rr_esc, b2));
    __m256i is0_or_esc3 = _mm256_or_si256(_mm256_cmpeq_epi8(rr0, b3),_mm256_cmpeq_epi8(rr_esc, b3));

    q0 = _mm256_or_si256(_mm256_cmpgt_epi8(rr0, b0), is0_or_esc0);
    q1 = _mm256_or_si256(_mm256_cmpgt_epi8(rr0, b1), is0_or_esc1);
    q2 = _mm256_or_si256(_mm256_cmpgt_epi8(rr0, b2), is0_or_esc2);
    q3 = _mm256_or_si256(_mm256_cmpgt_epi8(rr0, b3), is0_or_esc3);

    r0 = _mm256_movemask_epi8(q0);
    r1 = _mm256_movemask_epi8(q1);
    r2 = _mm256_movemask_epi8(q2);
    r3 = _mm256_movemask_epi8(q3);
    nonAsciiBitMap[bmidx]   = r0 ^ ((unsigned long)r1 << 32);
    nonAsciiBitMap[bmidx+1] = r2 ^ ((unsigned long)r3 << 32);
    bmidx += 2;
  }

  int off;
  int shft;
  int bmOff;
  unsigned long qbm, skipbm, skiptz, tz;
  int slen;
  while (*s) {
    while (_isspace(*s)) {
      ++s;
      if (!*s) break;
    }
    //printf(">%.*s\n",4, s);
    *endptr = s++;
    switch (**endptr) {
      case '-':
        if (s[0] == 'I') {
          if (!(s[1] == 'n' && s[2] == 'f' && ((end-s)==8 || _isdelim(s[8])))) return SetErrorInt("Expecting '-Infinity' at pos ",s-s_start-1);
          //o = PyFloat_FromString("-Inf");
          o = PyFloat_FromDouble(-Py_HUGE_VAL);
          s += 8;
          break;
        }
        if (!_isdigit(*s) && *s != '.') {
          *endptr = s;
          free(quoteBitMap); free(nonAsciiBitMap);
          return SetErrorInt("Saw a - without a number following it at pos ", s-s_start-1);
        }
      case '0': case '1': case '2': case '3': case '4':
      case '5': case '6': case '7': case '8': case '9':
        o = parseNumber(*endptr, &s);
        if (!_isdelim(*s)) {
          *endptr = s;
          free(quoteBitMap); free(nonAsciiBitMap);
          return SetErrorInt("A number must be followed by a delimiter at pos ", s-s_start-1);
        }
        break;
      case ']':
        if (pos == -1)               return SetErrorInt("Closing bracket ']' without an opening bracket at pos ", s-s_start-1);
        if (tags[pos] != JSON_ARRAY) return SetErrorInt("Mismatched closing bracket, expected a } at pos ", s-s_start-1);
        o = tails[pos];
        pos--;
        break;
      case '[':
        if (++pos == JSON_MAX_DEPTH) return SetErrorInt("Too many nested objects, the max depth is ", JSON_MAX_DEPTH);
        tails[pos] = PyList_New(0);
        tags[pos]  = JSON_ARRAY;
        keys[pos]  = NULL;
        separator  = true;
        continue;
      case '}':
        if (pos == -1)                return SetErrorInt("Closing bracket '}' without an opening bracket at pos ", s-s_start-1);
        if (tags[pos] != JSON_OBJECT) return SetErrorInt("Closing bracket '}' without an opening bracket at pos ", s-s_start-1);
        if (keys[pos] != NULL)        return SetErrorInt("Expected a \"key\":value, but got a closing bracket '}' at pos ", s-s_start-1);
        o = tails[pos];
        pos--;
        break;
      case '{':
        if (++pos == JSON_MAX_DEPTH) return SetErrorInt("Too many nested objects, the max depth is ", JSON_MAX_DEPTH);
        tails[pos] = PyDict_New();
        tags[pos] = JSON_OBJECT;
        keys[pos] = NULL;
        separator = true;
        continue;
      case ':':
        if (separator || keys[pos] == NULL) return SetErrorInt("Unexpected character ':' while parsing JSON string at pos ", s-s_start-1);
        separator = true;
        continue;
      case '"':
        off = s-ostart;
        shft = off&0x3F;
        bmOff = off/64;
        tz = 64;
        slen = 0;
        while ( 1 ) { // TODO FIX infinite loop
          qbm    = quoteBitMap[ bmOff ];
          skipbm = nonAsciiBitMap[ bmOff ];
          qbm >>= shft;
          skipbm >>= shft;
          tz = TZCNT(qbm);
          skiptz = TZCNT(skipbm);
          if ( skiptz < tz ) break;
          if ( tz != 64 ) {
            slen += tz;
            break;
          }
          bmOff += 1;
          slen += 64 - shft;
          shft = 0;
        }

        if ( tz < skiptz ) {
          *(s+slen) = 0;
#if PY_MAJOR_VERSION >= 3
          o = PyUnicode_FromKindAndData(PyUnicode_1BYTE_KIND, s, slen);
#else
          o = PyUnicode_FromStringAndSize(s, slen);
#endif
          s += slen+1;
          break;
        }
        o = NULL;
        string_start = s;
        for (it = s; *s; ++it, ++s) {
            int c = *it = *s;
            if (c == '\\') {
                c = *++s;
                switch (c) {
                case '\\':
                case '"':
                case '/':
                    *it = c;
                    break;
                case 'b':
                    *it = '\b';
                    break;
                case 'f':
                    *it = '\f';
                    break;
                case 'n':
                    *it = '\n';
                    break;
                case 'r':
                    *it = '\r';
                    break;
                case 't':
                    *it = '\t';
                    break;
                case 'u':
                    c = 0;
                    // \ud8XX\udcXX
                    if ( s[1] == 'd' && s[2] == '8' ) {
                      if ( s[5] != '\\' ) return SetErrorInt("Invalid lonely surrogate \\ud800 at ", s-s_start-2); 
                      unsigned long tmp = (char2int(s[8])<<8 | char2int(s[9])<<4 | char2int(s[10])) - 0xC00;
                      unsigned long wc = 0x10000 | char2int(s[3])<<14 | char2int(s[4])<<10 | tmp;
                      *it++ = ( 0xf0 | (wc >> 18) );
                      *it++ = ( 0x80 | ((wc >> 12) & 0x3f) );
                      *it++ = ( 0x80 | ((wc >> 6) & 0x3f) );
                      *it   = ( 0x80 | (wc & 0x3f) );
                      s += 10;
                      break;
                    }
                    for (i = 0; i < 4; ++i) {
                        if (_isxdigit(*++s)) {
                            c = c * 16 + char2int(*s);
                        } else {
                            *endptr = s;
                            free(quoteBitMap); free(nonAsciiBitMap);
                            return SetErrorInt("Unicode escape malformed at pos ", s-s_start-5);
                        }
                    }
                    if (c < 0x80) {
                        *it = c;
                    } else if (c < 0x800) {
                        *it++ = 0xC0 | (c >> 6);
                        *it = 0x80 | (c & 0x3F);
                    } else {
                        *it++ = 0xE0 | (c >> 12);
                        *it++ = 0x80 | ((c >> 6) & 0x3F);
                        *it = 0x80 | (c & 0x3F);
                    }
                    break;
                default:
                    *endptr = s;
                    free(quoteBitMap); free(nonAsciiBitMap);
                    tmpmsg[29] = c;
                    return SetErrorInt(tmpmsg, s-s_start-1);
                }
            } else if ((unsigned int)c < ' ' || c == '\x7F') {
              *endptr = s;
              free(quoteBitMap); free(nonAsciiBitMap);
              return SetErrorInt("Invalid control characters in string at pos ", s-s_start-1);
            } else if (c == '"') {
              *it = 0;
              o = PyUnicode_FromStringAndSize(string_start, (it - string_start));
              if ( o == NULL ) return NULL;
              ++s;
              break;
            }
        }
        if ( o == NULL ) return SetErrorInt("Unterminated string starting at ", string_start-s_start-1);
        break;
      case 't':
        if (!(s[0] == 'r' && s[1] == 'u' && s[2] == 'e' && ((end-s)==3 || _isdelim(s[3])))) return SetErrorInt("Expecting 'true' at pos ",s-s_start-1);
        o = Py_True; Py_INCREF(Py_True);
        s += 3;
        break;
      case 'f':
        if (!(s[0] == 'a' && s[1] == 'l' && s[2] == 's' && s[3] == 'e' && ((end-s)==4 || _isdelim(s[4])))) return SetErrorInt("Expecting 'false' at pos ",s-s_start-1);
        o = Py_False; Py_INCREF(Py_False);
        s += 4;
        break;
      case 'n':
        if (!(s[0] == 'u' && s[1] == 'l' && s[2] == 'l' && ((end-s)==3 || _isdelim(s[3])))) return SetErrorInt("Expecting 'null' at pos ",s-s_start-1);
        o = Py_None; Py_INCREF(Py_None);
        s += 3;
        break;
      case 'N':
        if (!(s[0] == 'a' && s[1] == 'N' && ((end-s)==2 || _isdelim(s[2])))) return SetErrorInt("Expecting 'NaN' at pos ",s-s_start-1);
        o = PyFloat_FromDouble(Py_NAN);
        s += 2;
        break;
      case 'I': // Infinity
        if (!(s[0] == 'n' && s[1] == 'f' && ((end-s)==7 || _isdelim(s[7])))) return SetErrorInt("Expecting 'Infinity' at pos ",s-s_start-1);
        o = PyFloat_FromDouble(Py_HUGE_VAL);
        s += 7;
        break;
      case ',':
        if (separator || keys[pos] != NULL) return SetErrorInt("Unexpected character ',' at pos ", s-s_start-1);
        separator = true;
        continue;
      case '\0':
        continue;
      default:
        free(quoteBitMap); free(nonAsciiBitMap);
        if ( keys[pos] != NULL && !separator ) return SetErrorInt("Expecting a : after an object key at pos ",s-s_start-1);
        return SetErrorInt("Unexpected character at pos ",s-s_start-1);
    }
    separator = false;
    if (pos == -1) {
      *endptr = s;
      free(quoteBitMap); free(nonAsciiBitMap);
      return o;
    }
    if (tags[pos] == JSON_OBJECT) {
      if (!keys[pos]) {
        keys[pos] = o; 
        continue;
      }
      PyDict_SetItem (tails[pos], keys[pos], o);
      Py_DECREF( (PyObject *) keys[pos]);
      Py_DECREF( (PyObject *) o);
      keys[pos] = NULL;
    } else {
      PyList_Append(tails[pos], o);
      Py_XDECREF( (PyObject *) o);
    }
  }
  free(quoteBitMap); free(nonAsciiBitMap);
  return SetError("Unexpected end of json string - could be a bad utf-8 encoding or check your [,{,\"");
}
#endif



PyObject* loads(PyObject* self, PyObject *args, PyObject *kwargs)
{
  static char *kwlist[] = {"obj", NULL};
  PyObject *ret;
  PyObject *sarg;
  PyObject *arg;
  char *endptr;

  if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O", kwlist, &arg)) return NULL;

  if (PyString_Check(arg))
  {
      sarg = arg;
  }
  else if (PyUnicode_Check(arg))
  {
    sarg = PyUnicode_AsUTF8String(arg);
    if (sarg == NULL) return NULL;
  }
  else
  {
    PyErr_Format(PyExc_TypeError, "Expected String or Unicode");
    return NULL;
  }

#ifdef __AVX2__
  ret = jParse(PyString_AS_STRING(sarg), &endptr, PyString_GET_SIZE(sarg));
#else
  ret = jsonParse(PyString_AS_STRING(sarg), &endptr, PyString_GET_SIZE(sarg));
#endif

  if (sarg != arg) Py_DECREF(sarg);

  return ret;
}

PyObject* loadb(PyObject* self, PyObject *args, PyObject *kwargs) {
  static char *kwlist[] = {"obj", NULL};
  PyObject *arg;
  PyObject *argtuple;
  if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O", kwlist, &arg)) return NULL;

  if (!PyBytes_Check(arg)) {
    PyErr_Format(PyExc_TypeError, "Expected bytes, use loads for a unicode string");
    return NULL;
  }

  arg = PyUnicode_FromEncodedObject( arg, "utf-8", "strict" );
  argtuple = PyTuple_Pack(1, arg);
  return loads(self, argtuple, kwargs);
}

PyObject* load(PyObject* self, PyObject *args, PyObject *kwargs)
{ 
  PyObject *read;
  PyObject *string;
  PyObject *ret;
  PyObject *file = NULL;
  PyObject *argtuple;
  
  if (!PyArg_ParseTuple (args, "O", &file)) { return NULL; }
  
  if (!PyObject_HasAttrString (file, "read"))
  { 
    PyErr_Format (PyExc_TypeError, "expected file");
    return NULL;
  }
  
  read = PyObject_GetAttrString (file, "read");
  
  if (!PyCallable_Check (read)) {
    Py_XDECREF(read);
    PyErr_Format (PyExc_TypeError, "expected file");
    return NULL;
  }
  
  string = PyObject_CallObject (read, NULL);
  Py_XDECREF(read);
  
  if (string == NULL) { return NULL; }
  
  argtuple = PyTuple_Pack(1, string);
 
  ret = loads (self, argtuple, kwargs); 
  
  Py_XDECREF(argtuple);
  Py_XDECREF(string);
  
  if (ret == NULL) { return NULL; }
  
  return ret;
}

