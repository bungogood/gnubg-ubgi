#include "config.h"

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>

#include "backgammon.h"
#include "dice.h"
#include "eval.h"
#include "format.h"
#include "multithread.h"
#include "positionid.h"
#include "ubgi.h"

extern player ap[2];

typedef struct {
    TanBoard anBoard;
    int fHavePosition;
    int anDice[2];
    int fHaveDice;
    int nMatchTo;
    int anScore[2];
    int nCube;
    int fCubeOwner;
    int fCrawford;
    int fJacoby;
} ubgistate;

static void
ubgi_reply(const char *sz)
{
    g_print("%s\n", sz);
    fflush(stdout);
}

static void
ubgi_error(const char *szCode, const char *szMessage)
{
    g_print("error %s %s\n", szCode, szMessage);
    fflush(stdout);
}

static void
ubgi_reset_state(ubgistate *pus)
{
    memset(pus, 0, sizeof(*pus));
    pus->nCube = 1;
    pus->fCubeOwner = -1;
    pus->fJacoby = TRUE;
}

static int
ubgi_parse_dice(const char *sz, int anDice[2])
{
    int d0, d1;
    char ch;

    if (sscanf(sz, "%d %d %c", &d0, &d1, &ch) != 2)
        return FALSE;

    if (d0 < 1 || d0 > 6 || d1 < 1 || d1 > 6)
        return FALSE;

    anDice[0] = d0;
    anDice[1] = d1;
    return TRUE;
}

static int
ubgi_parse_bool_value(const char *sz, int *pf)
{
    if (!g_ascii_strcasecmp(sz, "on") || !g_ascii_strcasecmp(sz, "true") || !strcmp(sz, "1")) {
        *pf = TRUE;
        return TRUE;
    }

    if (!g_ascii_strcasecmp(sz, "off") || !g_ascii_strcasecmp(sz, "false") || !strcmp(sz, "0")) {
        *pf = FALSE;
        return TRUE;
    }

    return FALSE;
}

static int
ubgi_copy_trimmed(char *szDst, size_t cchDst, const char *szSrc, size_t cchSrc)
{
    const char *pszBegin = szSrc;
    const char *pszEnd = szSrc + cchSrc;
    size_t cch;

    while (pszBegin < pszEnd && isspace((unsigned char) *pszBegin))
        pszBegin++;
    while (pszEnd > pszBegin && isspace((unsigned char) *(pszEnd - 1)))
        pszEnd--;

    cch = (size_t) (pszEnd - pszBegin);
    if (!cch || cch >= cchDst)
        return FALSE;

    memcpy(szDst, pszBegin, cch);
    szDst[cch] = 0;
    return TRUE;
}

static int
ubgi_parse_setoption(const char *sz, char *szName, size_t cchName, char *szValue, size_t cchValue)
{
    const char *psz;
    const char *pszValueTag;

    if (!g_str_has_prefix(sz, "setoption "))
        return FALSE;

    psz = sz + strlen("setoption ");
    if (!g_str_has_prefix(psz, "name "))
        return FALSE;
    psz += 5;

    pszValueTag = strstr(psz, " value ");
    if (!pszValueTag)
        return FALSE;

    if (!ubgi_copy_trimmed(szName, cchName, psz, (size_t) (pszValueTag - psz)))
        return FALSE;
    if (!ubgi_copy_trimmed(szValue, cchValue, pszValueTag + 7, strlen(pszValueTag + 7)))
        return FALSE;

    return TRUE;
}

static int
ubgi_apply_setoption(const char *szName, const char *szValue)
{
    evalcontext *pecChequer = &ap[0].esChequer.ec;
    evalcontext *pecCube = &ap[0].esCube.ec;

    if (!g_ascii_strcasecmp(szName, "Threads")) {
        char *pszEnd;
        unsigned long n = strtoul(szValue, &pszEnd, 10);
        if (*szValue == 0 || *pszEnd || n < 1 || n > MAX_NUMTHREADS)
            return FALSE;
        MT_SetNumThreads((unsigned int) n);
        return TRUE;
    }

    if (!g_ascii_strcasecmp(szName, "Seed")) {
#if defined(HAVE_LIBGMP)
        if (InitRNGSeedLong((char *) szValue, rngCurrent, rngctxCurrent))
            return FALSE;
#else
        char *pszEnd;
        unsigned long n = strtoul(szValue, &pszEnd, 10);
        if (*szValue == 0 || *pszEnd || n > UINT_MAX)
            return FALSE;
        InitRNGSeed((unsigned int) n, rngCurrent, rngctxCurrent);
#endif
        return TRUE;
    }

    if (!g_ascii_strcasecmp(szName, "Deterministic")) {
        int f;
        if (!ubgi_parse_bool_value(szValue, &f))
            return FALSE;
        pecChequer->fDeterministic = f;
        pecCube->fDeterministic = f;
        return TRUE;
    }

    if (!g_ascii_strcasecmp(szName, "EvalPrune")) {
        int f;
        if (!ubgi_parse_bool_value(szValue, &f))
            return FALSE;
        pecChequer->fUsePrune = f;
        pecCube->fUsePrune = f;
        return TRUE;
    }

    if (!g_ascii_strcasecmp(szName, "EvalPlies")) {
        char *pszEnd;
        unsigned long n = strtoul(szValue, &pszEnd, 10);
        if (*szValue == 0 || *pszEnd || n > 7)
            return FALSE;
        pecChequer->nPlies = (unsigned int) n;
        pecCube->nPlies = (unsigned int) n;
        return TRUE;
    }

    if (!g_ascii_strcasecmp(szName, "EvalMode")) {
        if (!g_ascii_strcasecmp(szValue, "cubeful")) {
            pecChequer->fCubeful = TRUE;
            pecCube->fCubeful = TRUE;
            return TRUE;
        }
        if (!g_ascii_strcasecmp(szValue, "cubeless")) {
            pecChequer->fCubeful = FALSE;
            pecCube->fCubeful = FALSE;
            return TRUE;
        }
        return FALSE;
    }

    return FALSE;
}

static int
ubgi_go_role(const char *sz)
{
    const char *pszRole;

    if (!strcmp(sz, "go"))
        return 0;

    if (!g_str_has_prefix(sz, "go "))
        return -1;

    pszRole = strstr(sz, " role ");
    if (!pszRole)
        return 0;

    pszRole += 6;

    if (!strncmp(pszRole, "chequer", 7) && (!pszRole[7] || isspace((unsigned char) pszRole[7])))
        return 0;
    if (!strncmp(pszRole, "cube", 4) && (!pszRole[4] || isspace((unsigned char) pszRole[4])))
        return 1;
    if (!strncmp(pszRole, "turn", 4) && (!pszRole[4] || isspace((unsigned char) pszRole[4])))
        return 2;

    return -1;
}

void
run_ubgi_cl(void)
{
    ubgistate us;
    char sz[2048];

    ubgi_reset_state(&us);

    while (fgets(sz, sizeof(sz), stdin) != NULL) {
        int anMove[8];
        cubeinfo ci;
        char *pch;
        char szMove[FORMATEDMOVESIZE];

        if ((pch = strchr(sz, '\n')))
            *pch = 0;
        if ((pch = strchr(sz, '\r')))
            *pch = 0;

        if (!*sz)
            continue;

        if (!strcmp(sz, "ubgi")) {
            char szOpt[128];
            ubgi_reply("id name GNU Backgammon");
            ubgi_reply("id author GNU Backgammon AUTHORS");
            ubgi_reply("id version " VERSION);
            g_snprintf(szOpt, sizeof(szOpt), "option name Threads type spin default %u min 1 max %u",
                       MT_GetNumThreads(), MAX_NUMTHREADS);
            ubgi_reply(szOpt);
            ubgi_reply("option name Seed type spin default 0 min 0 max 4294967295");
            g_snprintf(szOpt, sizeof(szOpt), "option name Deterministic type check default %s",
                       ap[0].esChequer.ec.fDeterministic ? "true" : "false");
            ubgi_reply(szOpt);
            g_snprintf(szOpt, sizeof(szOpt), "option name EvalPlies type spin default %u min 0 max 7",
                       ap[0].esChequer.ec.nPlies);
            ubgi_reply(szOpt);
            g_snprintf(szOpt, sizeof(szOpt), "option name EvalPrune type check default %s",
                       ap[0].esChequer.ec.fUsePrune ? "true" : "false");
            ubgi_reply(szOpt);
            g_snprintf(szOpt, sizeof(szOpt), "option name EvalMode type combo default %s var cubeless var cubeful",
                       ap[0].esChequer.ec.fCubeful ? "cubeful" : "cubeless");
            ubgi_reply(szOpt);
            ubgi_reply("ubgiok");
            continue;
        }

        if (!strcmp(sz, "isready")) {
            ubgi_reply("readyok");
            continue;
        }

        if (!strcmp(sz, "newgame")) {
            ubgi_reset_state(&us);
            continue;
        }

        if (g_str_has_prefix(sz, "setoption ")) {
            char szName[64];
            char szValue[256];
            if (!ubgi_parse_setoption(sz, szName, sizeof(szName), szValue, sizeof(szValue))) {
                ubgi_error("bad_argument", "setoption");
                continue;
            }
            if (!ubgi_apply_setoption(szName, szValue))
                ubgi_error("bad_argument", "setoption");
            continue;
        }

        if (g_str_has_prefix(sz, "position gnubgid ")) {
            const char *pszID = sz + strlen("position gnubgid ");
            if (!*pszID) {
                ubgi_error("bad_argument", "invalid_position");
                continue;
            }
            if (!PositionFromID(us.anBoard, pszID)) {
                ubgi_error("bad_argument", "invalid_position");
                continue;
            }
            us.fHavePosition = TRUE;
            continue;
        }

        if (!strcmp(sz, "position xgid") || g_str_has_prefix(sz, "position xgid ")) {
            ubgi_error("unsupported_feature", "position_xgid");
            continue;
        }

        if (g_str_has_prefix(sz, "dice ")) {
            if (!ubgi_parse_dice(sz + 5, us.anDice)) {
                ubgi_error("bad_argument", "dice");
                continue;
            }
            us.fHaveDice = TRUE;
            continue;
        }

        if (g_str_has_prefix(sz, "newsession ")) {
            const char *pszType = strstr(sz, " type ");
            const char *pszLength = strstr(sz, " length ");
            const char *pszJacoby = strstr(sz, " jacoby ");
            const char *pszCrawford = strstr(sz, " crawford ");

            if (pszType) {
                pszType += 6;
                if (!strncmp(pszType, "match", 5) && (!pszType[5] || isspace((unsigned char) pszType[5]))) {
                    us.nMatchTo = 1;
                } else if (!strncmp(pszType, "money", 5) && (!pszType[5] || isspace((unsigned char) pszType[5]))) {
                    us.nMatchTo = 0;
                } else {
                    ubgi_error("bad_argument", "newsession");
                    continue;
                }
            }

            if (pszLength) {
                int nLength;
                pszLength += 8;
                if (sscanf(pszLength, "%d", &nLength) != 1 || nLength < 1) {
                    ubgi_error("bad_argument", "newsession");
                    continue;
                }
                us.nMatchTo = nLength;
            }

            if (pszJacoby) {
                pszJacoby += 8;
                if (!strncmp(pszJacoby, "on", 2) && (!pszJacoby[2] || isspace((unsigned char) pszJacoby[2])))
                    us.fJacoby = TRUE;
                else if (!strncmp(pszJacoby, "off", 3) && (!pszJacoby[3] || isspace((unsigned char) pszJacoby[3])))
                    us.fJacoby = FALSE;
                else {
                    ubgi_error("bad_argument", "newsession");
                    continue;
                }
            }

            if (pszCrawford) {
                pszCrawford += 10;
                if (!strncmp(pszCrawford, "on", 2) && (!pszCrawford[2] || isspace((unsigned char) pszCrawford[2])))
                    us.fCrawford = TRUE;
                else if (!strncmp(pszCrawford, "off", 3) && (!pszCrawford[3] || isspace((unsigned char) pszCrawford[3])))
                    us.fCrawford = FALSE;
                else {
                    ubgi_error("bad_argument", "newsession");
                    continue;
                }
            }

            continue;
        }

        if (g_str_has_prefix(sz, "setscore ")) {
            int n0, n1;
            char ch;
            if (sscanf(sz + 9, "%d %d %c", &n0, &n1, &ch) != 2 || n0 < 0 || n1 < 0) {
                ubgi_error("bad_argument", "setscore");
                continue;
            }
            us.anScore[0] = n0;
            us.anScore[1] = n1;
            continue;
        }

        if (g_str_has_prefix(sz, "setcube ")) {
            int nCube;
            const char *pszValue = strstr(sz, " value ");
            const char *pszOwner = strstr(sz, " owner ");

            if (!pszValue || !pszOwner) {
                ubgi_error("bad_argument", "setcube");
                continue;
            }

            pszValue += 7;
            pszOwner += 7;

            if (sscanf(pszValue, "%d", &nCube) != 1 || nCube < 1) {
                ubgi_error("bad_argument", "setcube");
                continue;
            }

            if (!strncmp(pszOwner, "center", 6) && (!pszOwner[6] || isspace((unsigned char) pszOwner[6])))
                us.fCubeOwner = -1;
            else if (!strncmp(pszOwner, "p0", 2) && (!pszOwner[2] || isspace((unsigned char) pszOwner[2])))
                us.fCubeOwner = 0;
            else if (!strncmp(pszOwner, "p1", 2) && (!pszOwner[2] || isspace((unsigned char) pszOwner[2])))
                us.fCubeOwner = 1;
            else {
                ubgi_error("bad_argument", "setcube");
                continue;
            }

            us.nCube = nCube;
            continue;
        }

        if (g_str_has_prefix(sz, "setturn ")) {
            if (!strcmp(sz + 8, "p0") || !strcmp(sz + 8, "p1")) {
                continue;
            }
            ubgi_error("bad_argument", "setturn");
            continue;
        }

        if (!strcmp(sz, "go") || g_str_has_prefix(sz, "go ")) {
            TanBoard anBoard;
            TanBoard anBoardBeforeMove;
            int nRole = ubgi_go_role(sz);

            if (nRole < 0) {
                ubgi_error("bad_argument", "go");
                continue;
            }

            if (nRole == 1) {
                ubgi_error("unsupported_feature", "go_role_cube");
                continue;
            }

            if (nRole == 2) {
                ubgi_error("unsupported_feature", "go_role_turn");
                continue;
            }

            if (!us.fHavePosition) {
                ubgi_error("missing_context", "position");
                continue;
            }

            if (!us.fHaveDice) {
                ubgi_error("missing_context", "dice");
                continue;
            }

            if (us.nMatchTo > 0)
                SetCubeInfo(&ci, us.nCube, us.fCubeOwner, 0,
                            us.nMatchTo, us.anScore, us.fCrawford, us.fJacoby, nBeavers, bgvDefault);
            else
                SetCubeInfoMoney(&ci, us.nCube, us.fCubeOwner, 0, us.fJacoby, nBeavers, bgvDefault);

            memcpy(anBoard, us.anBoard, sizeof(anBoard));
            memcpy(anBoardBeforeMove, us.anBoard, sizeof(anBoardBeforeMove));

            if (FindBestMove(anMove, us.anDice[0], us.anDice[1], anBoard, &ci, &ap[0].esChequer.ec, defaultFilters) < 0) {
                ubgi_error("internal", "find_best_move_failed");
                continue;
            }

            if (anMove[0] < 0) {
                TanBoard anBoardAfterMove;
                memcpy(anBoardAfterMove, us.anBoard, sizeof(anBoardAfterMove));
                SwapSides(anBoardAfterMove);
                g_print("bestmoveid %s\n", PositionID((ConstTanBoard) anBoardAfterMove));
                fflush(stdout);
                us.fHaveDice = FALSE;
                continue;
            }

            FormatMovePlain(szMove, (ConstTanBoard) anBoardBeforeMove, anMove);
            {
                size_t cch = strlen(szMove);
                while (cch && isspace((unsigned char) szMove[cch - 1]))
                    szMove[--cch] = 0;
            }
            if (ApplyMove(anBoardBeforeMove, anMove, TRUE) < 0) {
                ubgi_error("internal", "apply_move_failed");
                continue;
            }

            SwapSides(anBoardBeforeMove);

            g_print("bestmoveid %s\n", PositionID((ConstTanBoard) anBoardBeforeMove));
            fflush(stdout);
            us.fHaveDice = FALSE;
            continue;
        }

        if (!strcmp(sz, "quit"))
            return;

        ubgi_error("unknown_command", "unknown");
    }
}
