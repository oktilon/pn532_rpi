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

int     gLogLevel       = LOG_LEVEL_WARNING; // Logging level
int     gLogExtended    = 0;                 // Logging with file:line function
uint8_t gFirstBlock     = 0;
uint8_t gLastBlock      = 63;
int     gBlocksCnt      = 0;
uint8_t *gBlocks        = NULL;
char    *gBlocksName    = NULL;
Key     defaultKey      = {.key={0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}};
Key     keys[KEYS_SZ];
int     gKeyCount       = 0;

// Long command line options
const struct option longOptions[] = {
    {"verbose",     no_argument,        0,  'v'},
    {"quiet",       no_argument,        0,  'q'},
    {"extended",    no_argument,        0,  'x'},
    {"key",         required_argument,  0,  'k'},
    {"start",       required_argument,  0,  's'},
    {"end",         required_argument,  0,  'e'},
    {"blocks",      required_argument,  0,  'b'}
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
                printf("[%s] %s%s\033[0m [%s:%d] in %s\n", logLevelHeaders[logIx], logLevelColor[logIx], msg, file, line, func);
            else
                printf("[%s] %s%s\033[0m\n", logLevelHeaders[logIx], logLevelColor[logIx], msg);
            fflush(stdout);
        }
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
void parseArguments (int argc, char **argv) {
    int i;
    size_t s, ix, ofs;
    long pH, pL;
    uint8_t v;
    char bByte[] = { 0, 0, 0 };
    Key key;

    while ((i = getopt_long (argc, argv, "vqxk:s:e:b:", longOptions, NULL)) != -1) {
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

            case 's': // start
                gFirstBlock = atoi(optarg);
                break;

            case 'e': // end
                gLastBlock = atoi(optarg);
                break;

            case 'b': // blocks
                parseBlocks(optarg);
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
}

int readBlock(PN532 *pReader, uint8_t *uid, uint8_t uid_len, Key *key, uint8_t block_number) {
    uint32_t pn532_error = PN532_ERROR_NONE;
    uint8_t buff[255];

    log_dbg ("Auth block %hhu by key %s...", block_number, dumpHexData(key->key, 6, 0));
    pn532_error = PN532_MifareClassicAuthenticateBlock(pReader, uid, uid_len,
            block_number, MIFARE_CMD_AUTH_A, key->key);
    if (pn532_error != PN532_ERROR_NONE) {
        log_wrn ("Auth block %hhu error 0x%X", block_number, pn532_error);
        return -2;
    }

    pn532_error = PN532_MifareClassicReadBlock(pReader, buff, block_number);
    if (pn532_error != PN532_ERROR_NONE) {
        log_wrn ("Read block %hhu error 0x%X", block_number, pn532_error);
        return pn532_error;
    }

    log_all ("\033[90mBLK \033[32m%02d:\033[0m %s", block_number, dumpHexData(buff, 16, 1));
    return PN532_ERROR_NONE;
}

int main(int argc, char** argv) {
    uint8_t buff[255], doRead = 1, block_number;
    uint8_t uid[MIFARE_UID_MAX_LENGTH];
    int32_t uid_len = 0, ix, ik, r;
    PN532 pn532;
    memset(keys, 0, KEYS_SZ*sizeof(Key));

    parseArguments (argc, argv);
    if (gKeyCount == 0) {
        memcpy(&(keys[0]), &defaultKey, sizeof(Key));
        gKeyCount++;
    }

    log_all ("App %s version %s log level %s with keys: %s", PROJECT, VERSION, logLevelHeaders[gLogLevel], dumpKeys());

    PN532_SPI_Init(&pn532);
    // PN532_I2C_Init(&pn532);
    //PN532_UART_Init(&pn532);
    if (PN532_GetFirmwareVersion(&pn532, buff) == PN532_STATUS_OK) {
        log_inf ("Found PN532 with firmware version: %hhu.%hhu", buff[1], buff[2]);
    } else {
        log_err ("Didn't find PN53x chip");
        return -1;
    }
    PN532_SamConfiguration(&pn532);
    while (doRead) {
        log_all ("Scan your RFID/NFC card...");
        memset (uid, 0, MIFARE_UID_MAX_LENGTH);
        while (doRead) {
            // Check if a card is available to read
            uid_len = PN532_ReadPassiveTarget(&pn532, uid, PN532_MIFARE_ISO14443A, 1000);
            if (uid_len != PN532_STATUS_ERROR) {
                log_all ("Found card with UID: \033[96m%s\033[0m", dumpHexData(uid, uid_len, 0));
                break;
            }
        }
        if (!doRead) break;
        if (gBlocks) {
            log_inf ("Reading blocks [%s]...", gBlocksName);
            for (ix = 0; ix < gBlocksCnt; ix ++) {
                block_number = gBlocks[ix];
                for (ik = 0; ik < gKeyCount; ik++) {
                    r = readBlock (&pn532, uid, uid_len, keys + ix, block_number);
                    if (r == PN532_ERROR_NONE) continue;
                    if (r == -2) {
                        uid_len = PN532_ReadPassiveTarget(&pn532, uid, PN532_MIFARE_ISO14443A, 1000);
                        if (uid_len != PN532_STATUS_ERROR) {
                            continue;
                        }
                        break;
                    }
                    break;
                }
            }
        } else {
            log_inf ("Reading blocks [%hhu - %hhu]...", gFirstBlock, gLastBlock);
            for (block_number = gFirstBlock; block_number <= gLastBlock; block_number++) {
                for (ik = 0; ik < gKeyCount; ik++) {
                    r = readBlock (&pn532, uid, uid_len, keys + ix, block_number);
                    if (r == -2) continue;
                    if (r == PN532_ERROR_NONE) continue;
                    break;
                }
            }
        }
        sleep(1);
    }

    return 0;
}
