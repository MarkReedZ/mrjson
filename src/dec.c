#include "dec.h"
#include "py_defines.h"
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#ifdef __AVX2__
#include <immintrin.h>
#endif

#define JSON_ZONE_SIZE 4096
#define JSON_STACK_SIZE 32

//int numStrings, numEscaped, stringLen;

static inline bool _isspace(char c) {
    return (bool)(c == ' ') || (c >= '\t' && c <= '\r');
}

static inline bool isdelim(char c) {
    return c == ',' || c == ':' || c == ']' || c == '}' || _isspace(c) || !c;
}

static inline bool _isdigit(char c) {
    return c >= '0' && c <= '9';
}

static inline bool _isxdigit(char c) {
    return (c >= '0' && c <= '9') || ((c & ~' ') >= 'A' && (c & ~' ') <= 'F');
}

static inline int char2int(char c) {
    if (c <= '9')
        return c - '0';
    return (c & ~' ') - 'A' + 10;
}

// Reads a number from s and sets outptr to the character following the number
static PyObject* parseNumber(char *s, char **outptr) {
  bool flt = false;
  char *st = s;
  char ch = *s;
  //printf("parseNumber %.*s\n", 16, st);
  if (ch == '-') ++s;

  long l = 0;
  double d;
  int cnt = 16;
  while (_isdigit(*s) && (--cnt != 0)) l = (l * 10) + (*s++ - '0');
  //printf("cnt %d\n",cnt);
  //printf("YAY >%c<\n",*s);
  if ( cnt != 0 && *s != '.' && *s != 'e' && *s != 'E' ) {
    *outptr = s;
    return PyLong_FromLong(ch == '-' ? -l : l);
  }
  //printf("%c\n",*s);
  //int dlen;
  //d = dconv_s2d(st, 100, &dlen);
  //printf("dbl %f\n",d);
  //s = st + dlen;
  //*endptr = s;
  //return PyFloat_FromDouble(d);

  d = (double) l;
  while (_isdigit(*s)) d = (d * 10) + (*s++ - '0');

  if (*s == '.') {
    //printf("parseNumber %.*s\n", 32, st);
    flt = true;
    ++s;
    long frac = 0;
    long prec = 1;
    while (_isdigit(*s)) {
      frac =  (frac*10) + (*s++-'0');
      //fraction /= 10;
      //printf("%f\n", fraction);
      //printf("%f\n", (*s-'0')*fraction);
      //d += (*s++ - '0') * fraction;
      //printf("%f\n", d);
      prec *= 10;
    }
    //long tmp = (long)(d*prec);
    //printf("tmp %d\n", tmp);
    //d = (double)tmp/(prec);
    //printf("%f\n", d);
    d += ((double)frac/prec);
    //printf("%f\n", d);
  }

  if (*s == 'e' || *s == 'E') {
    flt = true;
    ++s;
    ++s;

    double base = 10;
    if (*s == '+')
      ++s;
    else if (*s == '-') {
      ++s;
      base = 0.1;
    }

    unsigned int exponent = 0;
    while (_isdigit(*s)) exponent = (exponent * 10) + (*s++ - '0');

    double power = 1;
    for (; exponent; exponent >>= 1, base *= base)
        if (exponent & 1) power *= base;

    d *= power;
  }

  //printf("end >%c<\n",*s);
  *outptr = s;
  if ( flt ) return PyFloat_FromDouble(ch == '-' ? -d : d);
  else {
    ch = *s;
    *s = 0;
    PyObject *ret = PyLong_FromString(st, NULL, 10);
    *s = ch;
    return ret;
    //return PyLong_FromDouble (ch == '-' ? -d : d);
  }
}

#ifdef __AVX2__
static unsigned long TZCNT(unsigned long long in) {
  unsigned long res;
  asm("tzcnt %1, %0\n\t" : "=r"(res) : "r"(in));
  return res;
}
#endif

static JSOBJ SetError(const char *message)
{
  PyErr_Format (PyExc_ValueError, "%s", message);
  //printf("%s\n", message);
  return NULL;
}

static JSOBJ newTrue(void)
{
  Py_RETURN_TRUE;
}

static JSOBJ newFalse(void)
{
  Py_RETURN_FALSE;
}

static JSOBJ newNull(void)
{
  Py_RETURN_NONE;
}


JSOBJ jsonParse(char *s, char **endptr, size_t len) {
  JSOBJ tails[JSON_STACK_SIZE];
  JsonTag tags[JSON_STACK_SIZE];
  JSOBJ keys[JSON_STACK_SIZE];
  char *string_start;
  int pos = -1;
  bool separator = true;
  *endptr = s;
  PyObject *o = NULL;
  while (*s) {
    while (_isspace(*s)) {
      ++s;
      if (!*s) break;
    }
    *endptr = s++;
    switch (**endptr) {
      case '-':
        if (!_isdigit(*s) && *s != '.') {
          *endptr = s;
          return SetError("JSON_BAD_NUMBER;");
        }
      case '0':
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
      case '8':
      case '9':
        o = parseNumber(*endptr, &s);
        //o = PyFloat_FromDouble(string2double(*endptr, &s));
        if (!isdelim(*s)) {
          *endptr = s;
          return SetError("JSON_BAD_NUMBER;");
        }
        //printf("end >%c<\n", *s);
        break;
      case ']':
        if (pos == -1) return SetError("JSON_STACK_UNDERFLOW;");
        if (tags[pos] != JSON_ARRAY) return SetError("JSON_MISMATCH_BRACKET;");
        o = tails[pos];
        pos--;
        break;
      case '[':
        if (++pos == JSON_STACK_SIZE) return SetError("JSON_STACK_OVERFLOW;");
        tails[pos] = PyList_New(0);
        tags[pos]  = JSON_ARRAY;
        keys[pos]  = NULL;
        separator  = true;
        continue;
      case '}':
        if (pos == -1) return SetError("JSON_STACK_UNDERFLOW;");
        if (tags[pos] != JSON_OBJECT) return SetError("JSON_MISMATCH_BRACKET;");
        if (keys[pos] != NULL) return SetError("JSON_UNEXPECTED_CHARACTER}");
        o = tails[pos];
        pos--;
        break;
      case '{':
        if (++pos == JSON_STACK_SIZE) return SetError("JSON_STACK_OVERFLOW;");
        tails[pos] = PyDict_New();
        tags[pos] = JSON_OBJECT;
        keys[pos] = NULL;
        separator = true;
        continue;
      case ':':
        if (separator || keys[pos] == NULL) return SetError("JSON_UNEXPECTED_CHARACTER:");
        separator = true;
        continue;
      case '"':
        //o = JsonValue(JSON_STRING, s);
        string_start = s;
        for (char *it = s; *s; ++it, ++s) {
            int c = *it = *s;
            //printf("%c\n",c);
            if (c == '\\') {
                //*(s+32) = 0;
                //printf(">%s<\n",s);
                //printf("backslash\n");
                //printf(">%c<\n",*s);
                c = *++s;
                //printf("%d\n",c);
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
                    //printf("UUU %c %c \n",s[1],s[2]);
                    if ( s[1] == 'd' && s[2] == '8' ) {
                      unsigned long tmp = (char2int(s[8])<<8 | char2int(s[9])<<4 | char2int(s[10])) - 0xC00;
                      unsigned long wc = 0x10000 | char2int(s[3])<<14 | char2int(s[4])<<10 | tmp;
                      //printf("YAY 0x%02x\n",( 0xf0 | (wc >> 18) ));
                      //printf("YAY 0x%02x\n",( 0x80 | ((wc >> 12) & 0x3f) ));
                      //printf("YAY 0x%02x\n",( 0x80 | ((wc >> 6) & 0x3f) ));
                      //printf("YAY 0x%02x\n",( 0x80 | (wc & 0x3f) ));
                      *it++ = ( 0xf0 | (wc >> 18) );
                      *it++ = ( 0x80 | ((wc >> 12) & 0x3f) );
                      *it++ = ( 0x80 | ((wc >> 6) & 0x3f) );
                      *it   = ( 0x80 | (wc & 0x3f) );
                      s += 10;
                      break;
                    }
                    for (int i = 0; i < 4; ++i) {
                        if (_isxdigit(*++s)) {
                            c = c * 16 + char2int(*s);
                        } else {
                            *endptr = s;
                            return SetError("JSON_BAD_STRING;");
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
                    return SetError("JSON_BAD_STRING;");
                }
            } else if ((unsigned int)c < ' ' || c == '\x7F') {
              *endptr = s;
              return SetError("JSON_BAD_STRING;");
            } else if (c == '"') {
              *it = 0;
              //printf("%s\n", string_start);
              o = PyUnicode_FromStringAndSize(string_start, (it - string_start));
              //o = PyBytes_FromString(string_start);
              //printf("%s\n", string_start);
              ++s;
              break;
            }
        }
        if (!isdelim(*s)) {
            *endptr = s;
            return SetError("JSON_BAD_STRING;");
        }
        break;
      case 't':
        if (!(s[0] == 'r' && s[1] == 'u' && s[2] == 'e' && isdelim(s[3]))) return SetError("JSON_BAD_IDENTIFIER;");
        o = newTrue();
        s += 3;
        break;
      case 'f':
        if (!(s[0] == 'a' && s[1] == 'l' && s[2] == 's' && s[3] == 'e' && isdelim(s[4]))) SetError("return JSON_BAD_IDENTIFIER;");
        o = newFalse();
        s += 4;
        break;
      case 'n':
        if (!(s[0] == 'u' && s[1] == 'l' && s[2] == 'l' && isdelim(s[3]))) return SetError("JSON_BAD_IDENTIFIER;");
        o = newNull();
        s += 3;
        break;
      case ',':
        if (separator || keys[pos] != NULL) return SetError("JSON_UNEXPECTED_CHARACTER,");
        separator = true;
        continue;
      case '\0':
        continue;
      default:
        //printf("end >%c<\n", *s);
        return SetError("JSON_UNEXPECTED_CHARACTER;");
    }
    separator = false;
    if (pos == -1) {
      *endptr = s;
      return o;
    }
    if (tags[pos] == JSON_OBJECT) {
      if (!keys[pos]) {
        //if (o.getTag() != JSON_STRING) return JSON_UNQUOTED_KEY;
        keys[pos] = o; //PyUnicode_FromStringAndSize (start, (end - start));
        continue;
      }
      //printf("setitem BEF\n");
      PyDict_SetItem (tails[pos], keys[pos], o);
      //printf("setitem BEF\n");
      Py_DECREF( (PyObject *) keys[pos]);
      Py_DECREF( (PyObject *) o);
      keys[pos] = NULL;
    } else {
      PyList_Append(tails[pos], o);
      Py_DECREF( (PyObject *) o);
    }
    //tails[pos]->value = o;
  }
  return SetError("JSON_UNEXPECTED_END");
}

#ifdef __AVX2__
JSOBJ jParse(char *s, char **endptr, size_t len) {
  JSOBJ tails[JSON_STACK_SIZE];
  JsonTag tags[JSON_STACK_SIZE];
  JSOBJ keys[JSON_STACK_SIZE];
  char *string_start;
  int pos = -1;
  bool separator = true;
  *endptr = s;
  PyObject *o = NULL;

  unsigned long *quoteBitMap    = (unsigned long *) malloc( (len/64+2) * sizeof(unsigned long) );
  unsigned long *nonAsciiBitMap = (unsigned long *) malloc( (len/64+2) * sizeof(unsigned long) );

  __m256i b0, b1, b2, b3;
  unsigned char tmpbuf[32];
  int i, bmidx=0;
  int dist;
  char *ostart = s;
  char *buf = s;
  char *end = s+len;
  const __m256i rrq    = _mm256_set1_epi8(0x22);
  const __m256i rr0    = _mm256_set1_epi8(0);
  const __m256i rr_esc = _mm256_set1_epi8(0x5C);
  //printf("bmidx %d",bmidx);
  while( buf < end ) {
    if((dist = end - buf) < 128) {
      //memcpy(tmpbuf, buf + (dist & (-32)), dist & 31);
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
          quoteBitMap[bmidx] = _mm256_movemask_epi8(_mm256_cmpeq_epi8(rrq, b0)) ^ ((unsigned long)_mm256_movemask_epi8(_mm256_cmpeq_epi8(rrq, b1))<<32);
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
    //quoteBitMap.push_back( r0 ^ ((unsigned long)r1 << 32));
    //quoteBitMap.push_back( r2 ^ ((unsigned long)r3 << 32));   

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
    //printf("%.*s\n", 32, s);
    while (_isspace(*s)) {
      ++s;
      if (!*s) break;
    }
    *endptr = s++;
    switch (**endptr) {
      case '-':
        if (!_isdigit(*s) && *s != '.') {
          *endptr = s;
          free(quoteBitMap); free(nonAsciiBitMap);
          return SetError("JSON_BAD_NUMBER;");
        }
      case '0':
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
      case '8':
      case '9':
        //o = PyFloat_FromDouble(string2double(*endptr, &s));
        o = parseNumber(*endptr, &s);
        if (!isdelim(*s)) {
          //printf("%.*s\n", 3, s);
          *endptr = s;
          free(quoteBitMap); free(nonAsciiBitMap);
          return SetError("JSON_BAD_NUMBER;");
        }
        break;
      case ']':
        if (pos == -1) return SetError("JSON_STACK_UNDERFLOW;");
        if (tags[pos] != JSON_ARRAY) return SetError("JSON_MISMATCH_BRACKET;");
        o = tails[pos];
        pos--;
        break;
      case '[':
        if (++pos == JSON_STACK_SIZE) return SetError("JSON_STACK_OVERFLOW;");
        tails[pos] = PyList_New(0);
        tags[pos]  = JSON_ARRAY;
        keys[pos]  = NULL;
        separator  = true;
        continue;
      case '}':
        if (pos == -1) return SetError("JSON_STACK_UNDERFLOW;");
        if (tags[pos] != JSON_OBJECT) return SetError("JSON_MISMATCH_BRACKET;");
        if (keys[pos] != NULL) return SetError("JSON_UNEXPECTED_CHARACTER}");
        o = tails[pos];
        pos--;
        break;
      case '{':
        if (++pos == JSON_STACK_SIZE) return SetError("JSON_STACK_OVERFLOW;");
        tails[pos] = PyDict_New();
        tags[pos] = JSON_OBJECT;
        keys[pos] = NULL;
        separator = true;
        continue;
      case ':':
        if (separator || keys[pos] == NULL) return SetError("JSON_UNEXPECTED_CHARACTER:");
        separator = true;
        continue;
      case '"':
        //numStrings += 1;
        //o = JsonValue(JSON_STRING, s);
        off = s-ostart;
        shft = off&0x3F;
        bmOff = off/64;
        tz = 64;
        slen = 0;
        while ( 1 ) {
          qbm    = quoteBitMap[ bmOff ];
          skipbm = nonAsciiBitMap[ bmOff ];
          //printf(" bm %016lu\n", bm );
          qbm >>= shft;
          skipbm >>= shft;
          tz = TZCNT(qbm);
          skiptz = TZCNT(skipbm);
          //printf(" shft %d off %d tz %lu skiptz %lu\n", shft, off, tz, skiptz );
          //printf("%.*s\n", 33, start);
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
          //printf("%s\n", s);
          //o = PyUnicode_FromStringAndSize(s, slen);
#if PY_MAJOR_VERSION >= 3
          o = PyUnicode_FromKindAndData(PyUnicode_1BYTE_KIND, s, slen);
#else
          o = PyUnicode_FromStringAndSize(s, slen);
#endif
          //printf("%s\n", s);
          s += slen+1;
          //stringLen += slen;
          //printf("end >%c<\n", *s);
          break;
        }
        //numEscaped += 1;
        string_start = s;
        for (char *it = s; *s; ++it, ++s) {
            int c = *it = *s;
            //printf("%c\n",c);
            if (c == '\\') {
                //*(s+32) = 0;
                //printf(">%s<\n",s);
                //printf("backslash\n");
                //printf(">%c<\n",*s);
                c = *++s;
                //printf("%d\n",c);
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
                    //printf("UUU %c %c \n",s[1],s[2]);
                    if ( s[1] == 'd' && s[2] == '8' ) {
                      unsigned long tmp = (char2int(s[8])<<8 | char2int(s[9])<<4 | char2int(s[10])) - 0xC00;
                      unsigned long wc = 0x10000 | char2int(s[3])<<14 | char2int(s[4])<<10 | tmp;
                      //printf("YAY 0x%02x\n",( 0xf0 | (wc >> 18) ));
                      //printf("YAY 0x%02x\n",( 0x80 | ((wc >> 12) & 0x3f) ));
                      //printf("YAY 0x%02x\n",( 0x80 | ((wc >> 6) & 0x3f) ));
                      //printf("YAY 0x%02x\n",( 0x80 | (wc & 0x3f) ));
                      *it++ = ( 0xf0 | (wc >> 18) );
                      *it++ = ( 0x80 | ((wc >> 12) & 0x3f) );
                      *it++ = ( 0x80 | ((wc >> 6) & 0x3f) );
                      *it   = ( 0x80 | (wc & 0x3f) );
                      s += 10;
                      break;
                    }
                    for (int i = 0; i < 4; ++i) {
                        if (_isxdigit(*++s)) {
                            c = c * 16 + char2int(*s);
                        } else {
                            *endptr = s;
                            free(quoteBitMap); free(nonAsciiBitMap);
                            return SetError("JSON_BAD_STRING;");
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
                    return SetError("JSON_BAD_STRING;");
                }
            } else if ((unsigned int)c < ' ' || c == '\x7F') {
              *endptr = s;
              free(quoteBitMap); free(nonAsciiBitMap);
              return SetError("JSON_BAD_STRING;");
            } else if (c == '"') {
              *it = 0;
              //printf("%s\n", string_start);
              o = PyUnicode_FromStringAndSize(string_start, (it - string_start));
              //stringLen += (it - string_start);
              ++s;
              break;
            }
        }
        if (!isdelim(*s)) {
            *endptr = s;
            return SetError("JSON_BAD_STRING;");
        }
        break;
      case 't':
        if (!(s[0] == 'r' && s[1] == 'u' && s[2] == 'e' && isdelim(s[3]))) return SetError("JSON_BAD_IDENTIFIER;");
        o = newTrue();
        s += 3;
        break;
      case 'f':
        if (!(s[0] == 'a' && s[1] == 'l' && s[2] == 's' && s[3] == 'e' && isdelim(s[4]))) return SetError("JSON_BAD_IDENTIFIER;");
        o = newFalse();
        s += 4;
        break;
      case 'n':
        if (!(s[0] == 'u' && s[1] == 'l' && s[2] == 'l' && isdelim(s[3]))) return SetError("JSON_BAD_IDENTIFIER;");
        o = newNull();
        s += 3;
        break;
      case ',':
        if (separator || keys[pos] != NULL) return SetError("JSON_UNEXPECTED_CHARACTER,");
        separator = true;
        continue;
      case '\0':
        continue;
      default:
        //printf("end >%c<\n", *s);
        free(quoteBitMap); free(nonAsciiBitMap);
        return SetError("JSON_UNEXPECTED_CHARACTER;");
    }
    separator = false;
    if (pos == -1) {
      *endptr = s;
      free(quoteBitMap); free(nonAsciiBitMap);
      return o;
    }
    if (tags[pos] == JSON_OBJECT) {
      if (!keys[pos]) {
        //if (o.getTag() != JSON_STRING) return JSON_UNQUOTED_KEY;
        keys[pos] = o; //PyUnicode_FromStringAndSize (start, (end - start));
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
    //tails[pos]->value = o;
  }
  free(quoteBitMap);
  free(nonAsciiBitMap);
  return SetError("JSON_UNEXPECTED_END");
}
#endif


PyObject* fromJson(PyObject* self, PyObject *args, PyObject *kwargs)
{
  static char *g_kwlist[] = {"obj", NULL};
  PyObject *ret;
  PyObject *sarg;
  PyObject *arg;

  if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O", g_kwlist, &arg)) return NULL;

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

  //numStrings = numEscaped = 0;
  char *endptr;
#ifdef __AVX2__
  ret = jParse(PyString_AS_STRING(sarg), &endptr, PyString_GET_SIZE(sarg));
#else
  ret = jsonParse(PyString_AS_STRING(sarg), &endptr, PyString_GET_SIZE(sarg));
#endif

  //printf("strings %d escaped %d\n", numStrings, numEscaped);
  //printf("Average length %d", (stringLen/numStrings));

  if (sarg != arg)
  {
    Py_DECREF(sarg);
  }

  return ret;
}

PyObject* fromJsonFile(PyObject* self, PyObject *args, PyObject *kwargs)
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
 
  ret = fromJson (self, argtuple, kwargs); 
  
  Py_XDECREF(argtuple);
  Py_XDECREF(string);
  
  if (ret == NULL) { return NULL; }
  
  return ret;
}

