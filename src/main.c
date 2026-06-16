#include <getopt.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "lib/pn532.h"
#include "lib/pn532_rpi.h"

#include "config.h"
#include "main.h"

#define DUMP_BUF_SZ     2048
#define DUMP_TXT_SZ     128
#define LIST_BLK_SZ     512
#define KEYS_SZ         10

typedef struct key_str {
    uint8_t key[6];
} Key;

typedef struct access_bits {
    union {
        struct {
            uint8_t i10:1;
            uint8_t i11:1;
            uint8_t i12:1;
            uint8_t i13:1;
            uint8_t i20:1;
            uint8_t i21:1;
            uint8_t i22:1;
            uint8_t i23:1;
        } bits;
        uint8_t b6;
    } b6; // Access bits byte 6
    union {
        struct {
            uint8_t i30:1;
            uint8_t i31:1;
            uint8_t i32:1;
            uint8_t i33:1;
            uint8_t c10:1;
            uint8_t c11:1;
            uint8_t c12:1;
            uint8_t c13:1;
        } bits;
        uint8_t b7;
    } b7; // Access bits byte 7
    union {
        struct {
            uint8_t c20:1;
            uint8_t c21:1;
            uint8_t c22:1;
            uint8_t c23:1;
            uint8_t c30:1;
            uint8_t c31:1;
            uint8_t c32:1;
            uint8_t c33:1;
        } bits;
        uint8_t b8;
    } b8; // Access bits byte 8
    uint8_t b9; // User data
} AccessBits;

#define BYTE_TO_BINARY_PATTERN "%c%c%c%c%c%c%c%c"
#define BYTE_TO_BINARY(byte)  \
  ((byte) & 0x80 ? '1' : '0'), \
  ((byte) & 0x40 ? '1' : '0'), \
  ((byte) & 0x20 ? '1' : '0'), \
  ((byte) & 0x10 ? '1' : '0'), \
  ((byte) & 0x08 ? '1' : '0'), \
  ((byte) & 0x04 ? '1' : '0'), \
  ((byte) & 0x02 ? '1' : '0'), \
  ((byte) & 0x01 ? '1' : '0') 


uint8_t crc8_lookup[] = {
    0x00, 0x1D, 0x3A, 0x27, 0x74, 0x69, 0x4E, 0x53,   0xE8, 0xF5, 0xD2, 0xCF, 0x9C, 0x81, 0xA6, 0xBB,
    0xCD, 0xD0, 0xF7, 0xEA, 0xB9, 0xA4, 0x83, 0x9E,   0x25, 0x38, 0x1F, 0x02, 0x51, 0x4C, 0x6B, 0x76,
    0x87, 0x9A, 0xBD, 0xA0, 0xF3, 0xEE, 0xC9, 0xD4,   0x6F, 0x72, 0x55, 0x48, 0x1B, 0x06, 0x21, 0x3C,
    0x4A, 0x57, 0x70, 0x6D, 0x3E, 0x23, 0x04, 0x19,   0xA2, 0xBF, 0x98, 0x85, 0xD6, 0xCB, 0xEC, 0xF1,
    0x13, 0x0E, 0x29, 0x34, 0x67, 0x7A, 0x5D, 0x40,   0xFB, 0xE6, 0xC1, 0xDC, 0x8F, 0x92, 0xB5, 0xA8,
    0xDE, 0xC3, 0xE4, 0xF9, 0xAA, 0xB7, 0x90, 0x8D,   0x36, 0x2B, 0x0C, 0x11, 0x42, 0x5F, 0x78, 0x65,
    0x94, 0x89, 0xAE, 0xB3, 0xE0, 0xFD, 0xDA, 0xC7,   0x7C, 0x61, 0x46, 0x5B, 0x08, 0x15, 0x32, 0x2F,
    0x59, 0x44, 0x63, 0x7E, 0x2D, 0x30, 0x17, 0x0A,   0xB1, 0xAC, 0x8B, 0x96, 0xC5, 0xD8, 0xFF, 0xE2,
    0x26, 0x3B, 0x1C, 0x01, 0x52, 0x4F, 0x68, 0x75,   0xCE, 0xD3, 0xF4, 0xE9, 0xBA, 0xA7, 0x80, 0x9D,
    0xEB, 0xF6, 0xD1, 0xCC, 0x9F, 0x82, 0xA5, 0xB8,   0x03, 0x1E, 0x39, 0x24, 0x77, 0x6A, 0x4D, 0x50,
    0xA1, 0xBC, 0x9B, 0x86, 0xD5, 0xC8, 0xEF, 0xF2,   0x49, 0x54, 0x73, 0x6E, 0x3D, 0x20, 0x07, 0x1A,
    0x6C, 0x71, 0x56, 0x4B, 0x18, 0x05, 0x22, 0x3F,   0x84, 0x99, 0xBE, 0xA3, 0xF0, 0xED, 0xCA, 0xD7,
    0x35, 0x28, 0x0F, 0x12, 0x41, 0x5C, 0x7B, 0x66,   0xDD, 0xC0, 0xE7, 0xFA, 0xA9, 0xB4, 0x93, 0x8E,
    0xF8, 0xE5, 0xC2, 0xDF, 0x8C, 0x91, 0xB6, 0xAB,   0x10, 0x0D, 0x2A, 0x37, 0x64, 0x79, 0x5E, 0x43,
    0xB2, 0xAF, 0x88, 0x95, 0xC6, 0xDB, 0xFC, 0xE1,   0x5A, 0x47, 0x60, 0x7D, 0x2E, 0x33, 0x14, 0x09,
    0x7F, 0x62, 0x45, 0x58, 0x0B, 0x16, 0x31, 0x2C,   0x97, 0x8A, 0xAD, 0xB0, 0xE3, 0xFE, 0xD9, 0xC4
};

typedef struct block_data {
    int block;
    uint8_t data[16];
} BlockData;

int       gLogLevel       = LOG_LEVEL_WARNING; // Logging level
int       gLogExtended    = 0;                 // Logging with file:line function
int       gBlocksCnt      = 0;
int       gAuthSector     = -1;
uint8_t   *gBlocks        = NULL;
char      *gBlocksName    = NULL;
Key       defaultKey      = {.key={0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}};
Key       keys[KEYS_SZ];
int       gKeyCount       = 0;
BlockData *gWriteData     = NULL;
uint8_t   gWriteCount     = 0;
int       gReadMADKey     = -1;
BlockData gMADData[4]     = {0};

// Long command line options
const struct option longOptions[] = { // hvqxk:r:w:m::
    {"help",     no_argument,       0, 'h'},
    {"verbose",  no_argument,       0, 'v'},
    {"quiet",    no_argument,       0, 'q'},
    {"extended", no_argument,       0, 'x'},
    {"key",      required_argument, 0, 'k'},
    {"read",     required_argument, 0, 'r'},
    {"write",    required_argument, 0, 'w'},
    {"mad",      optional_argument, 0, 'm'},
    {NULL, 0, 0, '\0'}
};

const char *helpOptions[] = {
      "Show this help"
    , "Increase debug level (see below)"
    , "Minimal debug level (see below)"
    , "Extended logs with: file name, line number, function name"
    , "Custom 6-bytes key in hex format (default is FFFFFFFFFFFF)"
    , "Comma separated list of blocks for read, could be interval in format START-END (default 0-63)"
    , "Data to write in hex format (16 bytes) prefixed by block number\n"
      "\tseparated by : (e.g. 1:00112233445566778899AABBCCDDEEFF)\n"
      "\tTrailing bytes will be ignored, missing bytes will be filled with 0"
    , "Read MIFARE Application Directory (MAD) list"
};

const char *logLevelHeaders[] = {
    "\033[1;31mERR\033[0m",  // LOG_LEVEL_ERROR     // q = quiet
    "\033[1;91mWRN\033[0m",  // LOG_LEVEL_WARNING   //   = default
    "\033[1;37mINF\033[0m",  // LOG_LEVEL_INFO      // v = verbose
    "\033[1;36mDBG\033[0m",  // LOG_LEVEL_DEBUG     // vvv = verbose++
    "\033[1;33mTRC\033[0m",  // LOG_LEVEL_TRACE     // vv = verbose+
    "APP"   // LOG_LEVEL_ALL
};

const char *logLevelColor[] = {
    "\033[0;31m",  // LOG_LEVEL_ERROR   #BC1B27
    "\033[0;91m",  // LOG_LEVEL_WARNING #F15E42
    "\033[0;37m",  // LOG_LEVEL_INFO    #D0CFCC
    "\033[0;36m",  // LOG_LEVEL_DEBUG   #2AA1B3
    "\033[0;33m",  // LOG_LEVEL_TRACE   #A2734C
    "\033[0m"   // LOG_LEVEL_ALL <no-color>
};

void logger (const char *file, int line, const char *func, int lvl, const char* fmt, ...) {
    char *msg = NULL;
    int logIx = lvl < 0 ? LOG_LEVEL_MAX+1 : (lvl > LOG_LEVEL_MAX ? LOG_LEVEL_MAX : lvl);

    if (lvl <= gLogLevel) {
        // Format log message
        va_list arglist;
        va_start (arglist, fmt);
        int r = vasprintf (&msg, fmt, arglist);
        va_end (arglist);

        if (r) {
            if (gLogExtended)
                printf("%s%s\033[0m [%s:%d] in %s\n", logLevelColor[logIx], msg, file, line, func);
            else
                printf("%s%s\033[0m\n", logLevelColor[logIx], msg);
            fflush(stdout);
        }
    }
}

const char *pn532_errorstr(uint8_t error) {
    switch (error) {
        case PN532_ERROR_NONE:                     return "No error";
        case PN532_ERROR_TIMEOUT:                  return "Time Out";
        case PN532_ERROR_CRC:                      return "CRC error";
        case PN532_ERROR_PARITY:                   return "Parity error";
        case PN532_ERROR_COLLISION_BITCOUNT:       return "Collision bitcount";
        case PN532_ERROR_MIFARE_FRAMING:           return "Framing error";
        case PN532_ERROR_COLLISION_BITCOLLISION:   return "Bit collision";
        case PN532_ERROR_NOBUFS:                   return "Buffer error";
        case PN532_ERROR_RFNOBUFS:                 return "Buffer overflow";
        case PN532_ERROR_ACTIVE_TOOSLOW:           return "Active too slow";
        case PN532_ERROR_RFPROTO:                  return "RF Protocol error";
        case PN532_ERROR_TOOHOT:                   return "Temperature error";
        case PN532_ERROR_INTERNAL_NOBUFS:          return "Internal buffer overflow";
        case PN532_ERROR_INVAL:                    return "Invalid parameter";
        case PN532_ERROR_DEP_INVALID_COMMAND:      return "Invalid DEP command";
        case PN532_ERROR_DEP_BADDATA:              return "Invalid DEP data";
        case PN532_ERROR_MIFARE_AUTH:              return "Authentication error";
        case PN532_ERROR_NOSECURE:                 return "No NFC Security";
        case PN532_ERROR_I2CBUSY:                  return "I2C bus is Busy";
        case PN532_ERROR_UIDCHECKSUM:              return "UID Check byte is wrong";
        case PN532_ERROR_DEPSTATE:                 return "Invalid device state";
        case PN532_ERROR_HCIINVAL:                 return "Invalid HCI parameter";
        case PN532_ERROR_CONTEXT:                  return "Invalid context";
        case PN532_ERROR_RELEASED:                 return "Release error";
        case PN532_ERROR_CARDSWAPPED:              return "Card swapped";
        case PN532_ERROR_NOCARD:                   return "No card";
        case PN532_ERROR_MISMATCH:                 return "Mismatch";
        case PN532_ERROR_OVERCURRENT:              return "Over-current";
        case PN532_ERROR_NONAD:                    return "Missing NAD";
        case 0xFF:                                 return "Error";
        // case PN532_STATUS_OK:                   return "OK";
        default: break;
    }
    return "Unknown error";
}

void showHelp(char *cmd) {
    printf ("Usage: %s [OPTIONS] [-k KEY] (-r BLOCKS | -w BLK:DATA) \n", cmd);
    printf (" where OPTIONS are:\n");
    for (int i = 0; i < sizeof(longOptions)/sizeof(struct option); i++) {
        printf ("  -%c, --%-10s %s\n", longOptions[i].val, longOptions[i].name, helpOptions[i]);
    }
}

const char *dumpHexData (uint8_t *data, size_t sz, uint8_t withText) {
    static char _buf[DUMP_BUF_SZ];
    char _txt[DUMP_TXT_SZ] = {0};
    size_t i, cnt = 0, ctx = 0;

    memset(_buf, 0, DUMP_BUF_SZ);
    for (i = 0; i < sz && cnt < DUMP_BUF_SZ; i++, cnt+=3) {
        snprintf (_buf + cnt, DUMP_BUF_SZ - cnt, "%02hhX ", data[i]);
        if (withText && ctx < (DUMP_TXT_SZ - 2)) {
            ctx += sprintf(_txt + ctx, "%c", data[i] <= 0x1F ? '.' : (char)data[i]);
        }
    }
    if (withText && (cnt + ctx + 3) < DUMP_BUF_SZ) {
        strcat(_buf, "  ");
        strcat(_buf, _txt);
    }
    return _buf;
}

const char *dumpHexDataCopy (uint8_t *data, size_t sz, uint8_t withText) {
    return strdup (dumpHexData(data, sz, withText));
}

const char *dumpKeys() {
    static char _buf[DUMP_BUF_SZ];
    size_t ofs, add = 0;

    memset(_buf, 0, DUMP_BUF_SZ);
    for (int i = 0; i < gKeyCount; i++) {
        if (i > 0) {
            ofs = i*12 + add;
            add += snprintf(_buf + ofs, DUMP_BUF_SZ - ofs, ", ");
        }
        for (int j = 0; j < 6; j++) {
            ofs = i*12 + add + j*2;
            snprintf (_buf + ofs, DUMP_BUF_SZ - ofs, "%02hhX", keys[i].key[j]);
        }
    }
    return _buf;
}

const char *trailerAccess(uint8_t c1, uint8_t c2, uint8_t c3) {
    if(c1 == 0 && c2 == 0 && c3 == 0) {
        return "\033[96mkA=wA\033[0m, \033[31mkB=rwA,\033[0m \033[92mAccess=rA\033[0m";
    } else if(c1 == 0 && c2 == 1 && c3 == 0) {
        return "\033[92mkA-\033[0m, \033[91mkB=rA,\033[0m \033[92mAccess=rA\033[0m";
    } else if(c1 == 1 && c2 == 0 && c3 == 0) {
        return "\033[96mkA=wB\033[0m, \033[96mkB=wB,\033[0m \033[92mAccess=rAB\033[0m";
    } else if(c1 == 1 && c2 == 1 && c3 == 0) {
        return "\033[92mkA-\033[0m, \033[92mkB-,\033[0m \033[92mAccess=rAB\033[0m";
    } else if(c1 == 0 && c2 == 0 && c3 == 1) {
        return "\033[96mkA=wA\033[0m, \033[31mkB=rwA,\033[0m \033[93mAccess=rwA\033[0m (*)";
    } else if(c1 == 0 && c2 == 1 && c3 == 1) {
        return "\033[96mkA=wA\033[0m, \033[96mkB=wB,\033[0m \033[93mAccess=rwA\033[0m (*)";
    } else if(c1 == 1 && c2 == 0 && c3 == 1) {
        return "\033[92mkA-\033[0m, \033[92mkB-,\033[0m \033[92mAccess=rAB\033[0m";
    } else if(c1 == 1 && c2 == 1 && c3 == 1) {
        return "\033[92mkA-\033[0m, \033[92mkB-,\033[0m \033[92mAccess=rAB\033[0m";
    }
    return "ERR";
}

const char *blockAccess(uint8_t c1, uint8_t c2, uint8_t c3) {
    if(c1 == 0 && c2 == 0 && c3 == 0) {
        return "r=AB, w=AB, inc=AB, dec=AB (*)";
    } else if(c1 == 0 && c2 == 1 && c3 == 0) {
        return "r=AB, w-,   inc-,   dec-";
    } else if(c1 == 1 && c2 == 0 && c3 == 0) {
        return "r=AB, w=B,  inc-,   dec-";
    } else if(c1 == 1 && c2 == 1 && c3 == 0) {
        return "r=AB, w=B,  inc=B,  dec=B  (val)";
    } else if(c1 == 0 && c2 == 0 && c3 == 1) {
        return "r=AB, w-,   inc-,   dec=AB (val)";
    } else if(c1 == 0 && c2 == 1 && c3 == 1) {
        return "r=B,  w=B,  inc-,   dec-";
    } else if(c1 == 1 && c2 == 0 && c3 == 1) {
        return "r=B,  w-,   inc-,   dec-";
    } else if(c1 == 1 && c2 == 1 && c3 == 1) {
        return "r-,   w-,   inc-,   dec-";
    }
    return "ERR";
}

int dumpAccessBits(AccessBits *ab, uint8_t blk) {
    if (ab->b6.bits.i13 == !ab->b7.bits.c13 &&
        ab->b6.bits.i12 == !ab->b7.bits.c12 &&
        ab->b6.bits.i11 == !ab->b7.bits.c11 &&
        ab->b6.bits.i10 == !ab->b7.bits.c10 &&
        ab->b6.bits.i23 == !ab->b8.bits.c23 &&
        ab->b6.bits.i22 == !ab->b8.bits.c22 &&
        ab->b6.bits.i21 == !ab->b8.bits.c21 &&
        ab->b6.bits.i20 == !ab->b8.bits.c20 &&
        ab->b7.bits.i33 == !ab->b8.bits.c33 &&
        ab->b7.bits.i32 == !ab->b8.bits.c32 &&
        ab->b7.bits.i31 == !ab->b8.bits.c31 &&
        ab->b7.bits.i30 == !ab->b8.bits.c30
    ) {
        log_all("\033[90mACC \033[32m%02d:\033[0m %d%d%d %s", blk - 3, ab->b7.bits.c10, ab->b8.bits.c20, ab->b8.bits.c30,blockAccess(ab->b7.bits.c10, ab->b8.bits.c20, ab->b8.bits.c30));
        log_all("\033[90mACC \033[32m%02d:\033[0m %d%d%d %s", blk - 2, ab->b7.bits.c11, ab->b8.bits.c21, ab->b8.bits.c31, blockAccess(ab->b7.bits.c11, ab->b8.bits.c21, ab->b8.bits.c31));
        log_all("\033[90mACC \033[32m%02d:\033[0m %d%d%d %s", blk - 1, ab->b7.bits.c12, ab->b8.bits.c22, ab->b8.bits.c32, blockAccess(ab->b7.bits.c12, ab->b8.bits.c22, ab->b8.bits.c32));
        log_all("\033[90mTRL \033[32m%02d:\033[0m %d%d%d %s", blk, ab->b7.bits.c13, ab->b8.bits.c23, ab->b8.bits.c33, trailerAccess(ab->b7.bits.c13, ab->b8.bits.c23, ab->b8.bits.c33));
    } else {
        log_err("Invalid access bits:\n"
            "BLK0: C1=%d C2=%d C3=%d [I1=%d I2=%d I3=%d]\n"
            "BLK1: C1=%d C2=%d C3=%d [I1=%d I2=%d I3=%d]\n"
            "BLK2: C1=%d C2=%d C3=%d [I1=%d I2=%d I3=%d]\n"
            "TRL3: C1=%d C2=%d C3=%d [I1=%d I2=%d I3=%d]\n",
            ab->b7.bits.c10, ab->b8.bits.c20, ab->b8.bits.c30,
            ab->b6.bits.i10, ab->b6.bits.i20, ab->b7.bits.i30,
            ab->b7.bits.c11, ab->b8.bits.c21, ab->b8.bits.c31,
            ab->b6.bits.i11, ab->b6.bits.i21, ab->b7.bits.i31,
            ab->b7.bits.c12, ab->b8.bits.c22, ab->b8.bits.c32,
            ab->b6.bits.i12, ab->b6.bits.i22, ab->b7.bits.i32,
            ab->b7.bits.c13, ab->b8.bits.c23, ab->b8.bits.c33,
            ab->b6.bits.i13, ab->b6.bits.i23, ab->b7.bits.i33
        );
    }
    return 0;
}

void dumpMADSector() {
    uint8_t ix, ib;
    uint8_t crc = 0xc7; // MAD CRC8 Init value
    // CRC8
    ib = 1;
    for (ix = 1; ix < 32; ix++) {
        if(ix > 15) ib = 2;
        crc = crc8_lookup[crc ^ gMADData[ib].data[ix - (ib - 1) * 16]];
    }
    log_all("CRC8 Calc=%02hhX, Read=%02hhX", crc, gMADData[1].data[0]);
    // Sector 1
    for (ix = 2; ix < 16; ix++) {
        //
    }
}

void parseWriteData (const char *list) {
    char *p = strchr (list, ':');
    if (!p) {
        log_wrn ("Invalid write data format: %s", list);
        return;
    }
    *p = 0;
    int blkNum = atoi(list);
    if (blkNum < 0 || blkNum > 63) {
        log_wrn ("Invalid block number in write data: %s", list);
        return;
    }
    p++;
    if (strlen(p) % 2 != 0) {
        p--;
        *p = '0';
    }
    size_t ix, sz = strlen(p) / 2;
    if (sz > 16) {
        sz = 16;
    }

    gWriteData = (BlockData *) realloc (gWriteData, sizeof(BlockData) * (gWriteCount + 1));
    if (!gWriteData) {
        log_wrn ("Memory allocation failed for write data");
        return;
    }
    gWriteData[gWriteCount].block = blkNum;
    memset(gWriteData[gWriteCount].data, 0, 16);
    
    for(ix = 0; ix < sz; ix++) {
        char dig[3] = {0};
        dig[0] = p[ix*2];
        dig[1] = p[ix*2+1];
        gWriteData[gWriteCount].data[ix] = (uint8_t) strtol(dig, NULL, 16);
    }

    gWriteCount++;  
}

void parseBlocks (const char *list)  {
    uint8_t sectors[LIST_BLK_SZ];
    char dig[10] = {0};
    memset(sectors, 0, LIST_BLK_SZ);
    int beg = -1, end = -1, lst = 0, v;
    size_t i, sz = strlen(list), ixDig = 0;
    for (i = 0; i < sz; i++) {
        char c = list[i];
        // log_trc ("char=%c beg=%d end=%d ixDig=%d", c, beg, end, ixDig);
        if ('0' <= c && c <= '9') {
            dig[ixDig++] = c;
            dig[ixDig] = 0;
        } else if (c == '-') {
            if (i == 0) {
                beg = 0;
                ixDig = 0;
                continue;
            } else if (beg < 0 && ixDig == 0) {
                log_wrn ("Invalid list: %s", list);
                return;
            }
            beg = atoi(dig);
            dig[0] = 0;
            ixDig = 0;
        } else if (c == ',') {
            if (beg >= 0 && ixDig) {
                end = atoi(dig);
                if (end >= beg) {
                    for (v = beg; v <= end; v++) {
                        sectors[lst++] = v;
                        if (lst >= LIST_BLK_SZ) {
                            log_wrn ("No space for list: %s", list);
                            return;
                        }
                    }
                } else {
                    for (v = end; v <= beg; v++) {
                        sectors[lst++] = v;
                        if (lst >= LIST_BLK_SZ) {
                            log_wrn ("No space for list: %s", list);
                            return;
                        }
                    }
                }
                beg = -1;
                end = -1;
                dig[0] = 0;
                ixDig = 0;
            } else if (beg < 0 && ixDig) {
                v = atoi(dig);
                sectors[lst++] = v;
                if (lst >= LIST_BLK_SZ) {
                    log_wrn ("No space for list: %s", list);
                    return;
                }
                dig[0] = 0;
                ixDig = 0;
            } else {
                log_wrn ("Invalid list: %s", list);
                return;
            }
        }
    }
    // log_trc ("FIN beg=%d end=%d ixDig=%d", beg, end, ixDig);
    if (ixDig) {
        if (beg < 0) {
            if (lst < LIST_BLK_SZ) {
                v = atoi(dig);
                sectors[lst++] = v;
            } else {
                log_wrn ("No space for list: %s", list);
            }
        } else {
            end = atoi(dig);
            if (end >= beg) {
                for (v = beg; v <= end; v++) {
                    sectors[lst++] = v;
                    if (lst >= LIST_BLK_SZ) {
                        log_wrn ("No space for list: %s", list);
                        break;
                    }
                }
            } else {
                for (v = end; v <= beg; v++) {
                    sectors[lst++] = v;
                    if (lst >= LIST_BLK_SZ) {
                        log_wrn ("No space for list: %s", list);
                        break;
                    }
                }
            }
        }
    }
    if (lst > 0) {
        if (gBlocks) {
            free(gBlocks);
        }
        gBlocks = malloc (lst + 1);
        memset (gBlocks, 0, lst + 1);
        memcpy (gBlocks, sectors, lst);
        gBlocksCnt = lst;
        gBlocksName = strdup (list);
    }
}

int addKey(char *arg) {
    int ret = -1;
    size_t s, ix, ofs;
    long pH, pL;
    uint8_t v;
    char bByte[] = { 0, 0, 0 };
    Key key;

    if (!arg) return -1;

    memset (&key, 0, sizeof(key));
    s = strlen(arg);
    if (s > 12) {
        log_wrn ("Key size(%ld) is too long", s);
    }
    for (ix = 0; ix < 6; ix++) {
        ofs = 5-ix;
        pL = s - ix * 2 - 1;
        pH = pL - 1;
        bByte[0] = pH >= 0 ? arg[pH] : '0';
        bByte[1] = pL >= 0 ? arg[pL] : '0';
        v = (uint8_t) strtol (bByte, NULL, 16);
        key.key[ofs] = v;
    }
    if (gKeyCount < (KEYS_SZ - 1)) {
        ret = gKeyCount;
        memcpy(keys[gKeyCount++].key, key.key, 6);
    } else {
        log_wrn ("Key count exceeded %d skip: %s", KEYS_SZ, arg);
    }

    return ret;
}


/**
 * @brief Parse cmdline arguments
 *
 * @param argc args count
 * @param argv args list
 */
int parseArguments (int argc, char **argv) {
    int i, iExit = 0;
    

    while ((i = getopt_long (argc, argv, "hvqxk:r:w:m::", longOptions, NULL)) != -1) {
        printf("Opt: %d [%c], ix:%d, optarg:%s\n", i, (char)i, optind, optarg ? optarg : "NULL");
        switch (i) {
            case 'v': // verbose
                gLogLevel++;
                if (gLogLevel > LOG_LEVEL_MAX) {
                    gLogLevel = LOG_LEVEL_MAX;
                }
                break;

            case 'q': // quiet
                gLogLevel = LOG_LEVEL_ERROR;
                break;

            case 'x': // quiet
                gLogExtended = 1;
                break;

            case 'r': // read
                parseBlocks(optarg);
                break;

            case 'w': // write
                parseWriteData(optarg);
                break;

            case 'h': // help
                showHelp(argv[0]);
                iExit = 1;
                break;

            case 'm': // MAD (MiFare Application Directory)
                if (optarg) {
                    gReadMADKey = addKey(optarg);
                } else {
                    gReadMADKey = addKey("a0a1a2a3a4a5");
                }
                break;

            case 'k': // key
                addKey(optarg);
                break;

            default:
                break;
        }
    }

    return iExit;
}

int writeBlock(PN532 *pReader, uint8_t *uid, uint8_t uid_len, BlockData *blk) {
    uint8_t pn532_error = PN532_ERROR_NONE, uid_len_repeat = 0;
    int ik;

    int sector = blk->block / 4;
    if (sector != gAuthSector) {
        for (ik = 0; ik < gKeyCount; ik++) {
            log_dbg ("Auth block %hhu [Sector: %d Prev: %d] by key[%d] %s...", blk->block, sector, gAuthSector, ik, dumpHexData(keys[ik].key, 6, 0));
            pn532_error = PN532_MifareClassicAuthenticateBlock(pReader, uid, uid_len,
                    blk->block, MIFARE_CMD_AUTH_A, keys[ik].key);

            if (pn532_error == PN532_ERROR_NONE) break;
            log_wrn ("Auth block %hhu error 0x%hhX (%s)", blk->block, pn532_error, pn532_errorstr(pn532_error));
            sleep(1);
            uid_len_repeat = PN532_ReadPassiveTarget(pReader, uid, PN532_MIFARE_ISO14443A, 1000);
            if (uid_len_repeat != PN532_STATUS_ERROR) {
                continue;
            }
            break;
        }

        if (pn532_error != PN532_ERROR_NONE) {
            gAuthSector = -1;
            return pn532_error;
        }

        gAuthSector = sector;
    }

    pn532_error = PN532_MifareClassicWriteBlock(pReader, blk->data, blk->block);
    if (pn532_error != PN532_ERROR_NONE) {
        log_wrn ("Write block %hhu error 0x%X (%s)", blk->block, pn532_error, pn532_errorstr(pn532_error));
        return pn532_error;
    }

    log_all ("\033[90mBLK \033[32m%02d:\033[0m %s written", blk->block, dumpHexData(blk->data, 16, 1));
    return PN532_ERROR_NONE;
}


int readBlock(PN532 *pReader, uint8_t *uid, uint8_t uid_len, uint8_t block_number) {
    uint8_t pn532_error = PN532_ERROR_NONE;
    uint8_t buff[255], uid_len_repeat = 0;
    AccessBits ab;
    int ik;

    int sector = block_number / 4;
    if (sector != gAuthSector) {
        for (ik = 0; ik < gKeyCount; ik++) {
            log_dbg ("Auth block %hhu [Sector: %d Prev: %d] by key[%d] %s...", block_number, sector, gAuthSector, ik, dumpHexData(keys[ik].key, 6, 0));
            pn532_error = PN532_MifareClassicAuthenticateBlock(pReader, uid, uid_len,
                    block_number, MIFARE_CMD_AUTH_A, keys[ik].key);

            if (pn532_error == PN532_ERROR_NONE) break;
            log_wrn ("Auth block %hhu error 0x%hhX (%s)", block_number, pn532_error, pn532_errorstr(pn532_error));
            sleep(1);
            uid_len_repeat = PN532_ReadPassiveTarget(pReader, uid, PN532_MIFARE_ISO14443A, 1000);
            if (uid_len_repeat != PN532_STATUS_ERROR) {
                continue;
            }
            break;
        }

        if (uid_len_repeat == PN532_STATUS_ERROR) {
            log_wrn ("Card is missing");
            gAuthSector = -1;
            return -2;
        }

        if (pn532_error != PN532_ERROR_NONE) {
            gAuthSector = -1;
            return pn532_error;
        }

        gAuthSector = sector;
    }

    pn532_error = PN532_MifareClassicReadBlock(pReader, buff, block_number);
    if (pn532_error != PN532_ERROR_NONE) {
        log_wrn ("Read block %hhu error 0x%X (%s)", block_number, pn532_error, pn532_errorstr(pn532_error));
        gAuthSector = -1;
        return pn532_error;
    }

    log_all ("\033[90mBLK \033[32m%02d:\033[0m %s", block_number, dumpHexData(buff, 16, 1));
    if (block_number % 4 == 3) {
        ab.b6.b6 = buff[6];
        ab.b7.b7 = buff[7];
        ab.b8.b8 = buff[8];
        dumpAccessBits(&ab, block_number);
    }

    return PN532_ERROR_NONE;
}

int readMadBlock(PN532 *pReader, uint8_t *uid, uint8_t uid_len, uint8_t block_number) {
    uint8_t pn532_error = PN532_ERROR_NONE;
    uint8_t buff[255];

    log_dbg ("Auth MAD sector by key[%d] %s...", gReadMADKey, dumpHexData(keys[gReadMADKey].key, 6, 0));
    pn532_error = PN532_MifareClassicAuthenticateBlock(pReader, uid, uid_len,
            block_number, MIFARE_CMD_AUTH_A, keys[gReadMADKey].key);

    if (pn532_error != PN532_ERROR_NONE) {
        log_wrn ("Auth block %hhu error 0x%hhX (%s)", block_number, pn532_error, pn532_errorstr(pn532_error));
        return -1;
    }

    pn532_error = PN532_MifareClassicReadBlock(pReader, buff, block_number);
    if (pn532_error != PN532_ERROR_NONE) {
        log_wrn ("Read block %hhu error 0x%X (%s)", block_number, pn532_error, pn532_errorstr(pn532_error));
        return -1;
    }

    gMADData[block_number].block = block_number;
    memcpy(gMADData[block_number].data, buff, 16);

    return 0;
}

int main(int argc, char** argv) {
    uint8_t buff[255], doLoop = 1, block_number;
    uint8_t uid[MIFARE_UID_MAX_LENGTH];
    int32_t uid_len = 0, ix;
    PN532 pn532;
    memset(keys, 0, KEYS_SZ*sizeof(Key));

    int iExit = parseArguments (argc, argv);
    if (iExit != 0)
        return iExit;
    if (gKeyCount == 0) {
        memcpy(&(keys[0]), &defaultKey, sizeof(Key));
        gKeyCount++;
    }
    if (!gBlocks && gWriteCount == 0 && gReadMADKey < 0) {
        log_err ("No blocks specified for read or write, use -r or -w options");
        return -1;
    }

    log_all ("App %s version %s log level %s with keys[%d]: %s", PROJECT, VERSION, logLevelHeaders[gLogLevel], gKeyCount, dumpKeys());

    PN532_SPI_Init(&pn532);
    // PN532_I2C_Init(&pn532);
    // PN532_UART_Init(&pn532);
    if (PN532_GetFirmwareVersion(&pn532, buff) == PN532_STATUS_OK) {
        log_inf ("Found PN532 with firmware version: %hhu.%hhu", buff[1], buff[2]);
    } else {
        log_err ("Didn't find PN53x chip");
        return -1;
    }
    PN532_SamConfiguration(&pn532);
    if (gWriteCount > 0) {
        while (doLoop) {
            log_all ("Attach your RFID/NFC card for WRITE...");
            memset (uid, 0, MIFARE_UID_MAX_LENGTH);
            while (doLoop) {
                // Check if a card is available to read
                gAuthSector = -1;
                uid_len = PN532_ReadPassiveTarget(&pn532, uid, PN532_MIFARE_ISO14443A, 1000);
                if (uid_len != PN532_STATUS_ERROR) {
                    log_all ("Found card with UID: \033[96m%s\033[0m", dumpHexData(uid, uid_len, 0));
                    break;
                }
            }
            if (!doLoop) break;
            log_inf ("Writing %d block(s)...", gWriteCount);
            for (ix = 0; ix < gWriteCount; ix++) {
                BlockData *blk = &gWriteData[ix];
                if (writeBlock(&pn532, uid, uid_len, blk) == PN532_ERROR_NONE) {
                    log_inf ("BLK %02d: %s WRITTEN", gWriteData[ix].block, dumpHexData(gWriteData[ix].data, 16, 1));
                }
            }
            doLoop = 0;
        }
    } else if (gBlocksCnt > 0) {
        while (doLoop) {
            log_all ("Scan your RFID/NFC card...");
            memset (uid, 0, MIFARE_UID_MAX_LENGTH);
            while (doLoop) {
                // Check if a card is available to read
                gAuthSector = -1;
                uid_len = PN532_ReadPassiveTarget(&pn532, uid, PN532_MIFARE_ISO14443A, 1000);
                if (uid_len != PN532_STATUS_ERROR) {
                    log_all ("Found card with UID: \033[96m%s\033[0m", dumpHexData(uid, uid_len, 0));
                    break;
                }
            }
            if (!doLoop) break;
            log_inf ("Reading blocks [%s]...", gBlocksName);
            for (ix = 0; ix < gBlocksCnt; ix ++) {
                block_number = gBlocks[ix];
                if (readBlock (&pn532, uid, uid_len, block_number) == -2)
                    break;
            }
            sleep(3);
        }
    } else if (gReadMADKey >= 0) {
        while (doLoop) {
            log_all ("Read MAD from RFID/NFC card...");
            memset (uid, 0, MIFARE_UID_MAX_LENGTH);
            while (doLoop) {
                // Check if a card is available to read
                gAuthSector = -1;
                uid_len = PN532_ReadPassiveTarget(&pn532, uid, PN532_MIFARE_ISO14443A, 1000);
                if (uid_len != PN532_STATUS_ERROR) {
                    log_all ("Found card with UID: \033[96m%s\033[0m", dumpHexData(uid, uid_len, 0));
                    break;
                }
            }
            if (!doLoop) break;
            log_inf ("Reading MAD blocks...");
            for (block_number = 0; block_number < 4; block_number ++) {
                if (readMadBlock (&pn532, uid, uid_len, block_number) < 0) {
                    doLoop = 0;
                    break;
                }
            }
            if (doLoop) {
                dumpMADSector();
                doLoop = 0;
            }
        }
    } else {
        log_all ("Nothing to do");
    }

    return 0;
}
