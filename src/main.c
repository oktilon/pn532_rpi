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
            uint8_t i23:1;
            uint8_t i22:1;
            uint8_t i21:1;
            uint8_t i20:1;
            uint8_t i13:1;
            uint8_t i12:1;
            uint8_t i11:1;
            uint8_t i10:1;
        } bits;
        uint8_t b6;
    } b6; // Access bits byte 6
    union {
        struct {
            uint8_t c13:1;
            uint8_t c12:1;
            uint8_t c11:1;
            uint8_t c10:1;
            uint8_t i33:1;
            uint8_t i32:1;
            uint8_t i31:1;
            uint8_t i30:1;
        } bits;
        uint8_t b7;
    } b7; // Access bits byte 7
    union {
        struct {
            uint8_t c33:1;
            uint8_t c32:1;
            uint8_t c31:1;
            uint8_t c30:1;
            uint8_t c23:1;
            uint8_t c22:1;
            uint8_t c21:1;
            uint8_t c20:1;
        } bits;
        uint8_t b8;
    } b8; // Access bits byte 8
    uint8_t b9; // User data
} AccessBits;

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

// Long command line options
const struct option longOptions[] = { // vqxk:r:w:h
    {"help",     no_argument,       0, 'h'},
    {"verbose",  no_argument,       0, 'v'},
    {"quiet",    no_argument,       0, 'q'},
    {"extended", no_argument,       0, 'x'},
    {"key",      required_argument, 0, 'k'},
    {"read",     required_argument, 0, 'r'},
    {"write",    required_argument, 0, 'w'}
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
        return "wA=A,  rACC=A,        rwB=A";
    } else if(c1 == 0 && c2 == 1 && c3 == 0) {
        return "  -A,  rACC=A,         rB=A";
    } else if(c1 == 1 && c2 == 0 && c3 == 0) {
        return "wA=B,  rACC=AB,        wB=B";
    } else if(c1 == 1 && c2 == 1 && c3 == 0) {
        return "  -A,  rACC=AB,          -B";
    } else if(c1 == 0 && c2 == 0 && c3 == 1) {
        return "wA=A, rwACC=A,        rwB=A (*)";
    } else if(c1 == 0 && c2 == 1 && c3 == 1) {
        return "wA=B,  rACC=AB(w=B),   wB=B";
    } else if(c1 == 1 && c2 == 0 && c3 == 1) {
        return "  -A,  rACC=AB(w=B),     -B";
    } else if(c1 == 1 && c2 == 1 && c3 == 1) {
        return "  -A,  rACC=AB,          -B";
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

int parseAccessBits(AccessBits *ab, uint8_t blk,  char *str) {
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
        return snprintf(str, 256, " (b%d=%s, b%d=%s, b%d=%s, tr%d=%s)"
            , blk - 3, blockAccess(ab->b7.bits.c10, ab->b8.bits.c20, ab->b8.bits.c30)
            , blk - 2, blockAccess(ab->b7.bits.c11, ab->b8.bits.c21, ab->b8.bits.c31)
            , blk - 1, blockAccess(ab->b7.bits.c12, ab->b8.bits.c22, ab->b8.bits.c32)
            , blk, trailerAccess(ab->b7.bits.c13, ab->b8.bits.c23, ab->b8.bits.c33));
    }
    return 0;
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

/**
 * @brief Parse cmdline arguments
 *
 * @param argc args count
 * @param argv args list
 */
int parseArguments (int argc, char **argv) {
    int i, iExit = 0;
    size_t s, ix, ofs;
    long pH, pL;
    uint8_t v;
    char bByte[] = { 0, 0, 0 };
    Key key;

    while ((i = getopt_long (argc, argv, "vqxk:r:w:h", longOptions, NULL)) != -1) {
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

            case 'k': // key
                memset (key.key, 0, 6);
                s = strlen(optarg);
                if (s > 12) {
                    log_wrn ("Key size(%ld) is too long", s);
                }
                for (ix = 0; ix < 6; ix++) {
                    ofs = 5-ix;
                    pL = s - ix * 2 - 1;
                    pH = pL - 1;
                    bByte[0] = pH >= 0 ? optarg[pH] : '0';
                    bByte[1] = pL >= 0 ? optarg[pL] : '0';
                    v = (uint8_t) strtol (bByte, NULL, 16);
                    key.key[ofs] = v;
                }
                if (gKeyCount < (KEYS_SZ - 1)) {
                    memcpy(keys[gKeyCount++].key, key.key, 6);
                } else {
                    log_wrn ("Key count exceeded %d skip: %s", KEYS_SZ, optarg);
                }
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
    char accessInfo[256] = {0};
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

    if (block_number % 4 == 3) {
        ab.b6.b6 = buff[6];
        ab.b7.b7 = buff[7];
        ab.b8.b8 = buff[8];
        parseAccessBits(&ab, block_number, accessInfo);
    }

    log_all ("\033[90mBLK \033[32m%02d:\033[0m %s%s", block_number, dumpHexData(buff, 16, 1), accessInfo);
    return PN532_ERROR_NONE;
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
    if (!gBlocks && gWriteCount == 0) {
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
                // Write data
                for (ix = 0; ix < gWriteCount; ix++) {
                    BlockData *blk = &gWriteData[ix];
                    if (writeBlock(&pn532, uid, uid_len, blk) == PN532_ERROR_NONE) {
                        log_inf ("BLK %02d: %s WRITTEN", gWriteData[ix].block, dumpHexData(gWriteData[ix].data, 16, 1));
                    }
                }
            }
        }
    } else {
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
    }

    return 0;
}
