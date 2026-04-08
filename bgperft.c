/*
 * Minimal isolated movegen/perft benchmark for GNU Backgammon core.
 */

#include "config.h"

#include <glib.h>
#include <locale.h>
#include <stdio.h>
#include <string.h>

#include "eval.h"
#include "glib-ext.h"
#include "multithread.h"
#include "positionid.h"

extern void
MT_CloseThreads(void)
{
    return;
}

typedef struct {
    int d0;
    int d1;
    guint64 weight;
} dice_roll;

static const dice_roll all21[21] = {
    {1, 1, 1},
    {1, 2, 2},
    {1, 3, 2},
    {1, 4, 2},
    {1, 5, 2},
    {1, 6, 2},
    {2, 2, 1},
    {2, 3, 2},
    {2, 4, 2},
    {2, 5, 2},
    {2, 6, 2},
    {3, 3, 1},
    {3, 4, 2},
    {3, 5, 2},
    {3, 6, 2},
    {4, 4, 1},
    {4, 5, 2},
    {4, 6, 2},
    {5, 5, 1},
    {5, 6, 2},
    {6, 6, 1},
};

static void
swap_sides(TanBoard anBoard)
{
    int i;
    for (i = 0; i < 25; ++i) {
        unsigned int tmp = anBoard[0][i];
        anBoard[0][i] = anBoard[1][i];
        anBoard[1][i] = tmp;
    }
}

static guint64
perft_all_rolls(const TanBoard anBoard, const int depth, const int weighted)
{
    int i;
    if (depth == 0)
        return 1;

    guint64 total = 0;
    for (i = 0; i < 21; ++i) {
        movelist ml;
        guint64 mult = weighted ? all21[i].weight : 1;
        int cMoves = GenerateMoves(&ml, anBoard, all21[i].d0, all21[i].d1, FALSE);

        if (cMoves == 0) {
            TanBoard passed;
            guint64 sub;
            memcpy(passed, anBoard, sizeof(TanBoard));
            swap_sides(passed);
            sub = perft_all_rolls((ConstTanBoard) passed, depth - 1, weighted);
            total += mult * sub;
            continue;
        }

        for (int m = 0; m < cMoves; ++m) {
            TanBoard child;
            positionkey key = ml.amMoves[m].key;
            guint64 sub;
            PositionFromKeySwapped(child, &key);
            sub = perft_all_rolls((ConstTanBoard) child, depth - 1, weighted);
            total += mult * sub;
        }
    }

    return total;
}

int
main(int argc, char **argv)
{
    TanBoard board;
    const char *position_id = "4HPwATDgc/ABMA";
    int depth = 2;
    int iterations = 10;
    int weighted = 0;
    int die0 = 0;
    int die1 = 0;

    GOptionEntry entries[] = {
        {"position-id", 'p', 0, G_OPTION_ARG_STRING, &position_id,
         "GNUbg position ID (default: start)", "ID"},
        {"depth", 'd', 0, G_OPTION_ARG_INT, &depth, "Perft depth", "N"},
        {"iterations", 'n', 0, G_OPTION_ARG_INT, &iterations, "Number of repeated runs", "N"},
        {"weighted", 'w', 0, G_OPTION_ARG_NONE, &weighted, "Use 36-roll weighting", NULL},
        {"die0", 0, 0, G_OPTION_ARG_INT, &die0, "Single-roll benchmark die 1", "N"},
        {"die1", 0, 0, G_OPTION_ARG_INT, &die1, "Single-roll benchmark die 2", "N"},
        {NULL, 0, 0, (GOptionArg) 0, NULL, NULL, NULL}
    };

    GError *error = NULL;
    GOptionContext *context;

    glib_ext_init();
    MT_InitThreads();
    setlocale(LC_ALL, "");

    context = g_option_context_new("- isolated GNUbg movegen/perft benchmark");
    g_option_context_add_main_entries(context, entries, PACKAGE);
    g_option_context_parse(context, &argc, &argv, &error);
    g_option_context_free(context);

    if (error) {
        g_printerr("%s\n", error->message);
        return 1;
    }

    if (!PositionFromID(board, position_id)) {
        g_printerr("invalid position id: %s\n", position_id);
        return 2;
    }

    if (iterations < 1)
        iterations = 1;

    if (die0 > 0 || die1 > 0) {
        gint64 t0;
        gint64 t1;
        guint64 total_moves = 0;

        if (die0 < 1 || die0 > 6 || die1 < 1 || die1 > 6) {
            g_printerr("--die0 and --die1 must both be in 1..6\n");
            return 3;
        }

        t0 = g_get_monotonic_time();
        for (int i = 0; i < iterations; ++i) {
            movelist ml;
            total_moves += (guint64) GenerateMoves(&ml, (ConstTanBoard) board, die0, die1, FALSE);
        }
        t1 = g_get_monotonic_time();

        double secs = (double) (t1 - t0) / 1000000.0;
        double it_per_sec = (double) iterations / (secs > 0.0 ? secs : 1e-9);
        printf("mode=single-roll position_id=%s dice=%d,%d iterations=%d\n", position_id, die0, die1, iterations);
        printf("total_moves=%llu avg_moves=%.3f\n",
               (unsigned long long) total_moves,
               (double) total_moves / (double) iterations);
        printf("time_s=%.6f it_per_sec=%.2f\n", secs, it_per_sec);
        return 0;
    }

    {
        gint64 t0;
        gint64 t1;
        guint64 nodes = 0;

        t0 = g_get_monotonic_time();
        for (int i = 0; i < iterations; ++i)
            nodes = perft_all_rolls((ConstTanBoard) board, depth, weighted);
        t1 = g_get_monotonic_time();

        double secs = (double) (t1 - t0) / 1000000.0;
        double nps = (double) nodes * (double) iterations / (secs > 0.0 ? secs : 1e-9);
        printf("mode=perft position_id=%s depth=%d weighted=%d iterations=%d\n",
               position_id, depth, weighted, iterations);
        printf("nodes_per_run=%llu\n", (unsigned long long) nodes);
        printf("time_s=%.6f nodes_per_sec=%.2f\n", secs, nps);
    }

    return 0;
}
