/* dod_types.h - shared data blocks of the game core.
 *
 * These mirror the original 6809 control blocks (CD.ASM / COMDAT.ASM);
 * field names keep the original P_* symbols so the annotated disassembly
 * and the C++ port cross-reference directly.  Widths are the ORIGINAL 8/16
 * bit ones (the C++ port widened several to int; we narrow them back so an
 * 8-bit target's RAM budget matches the original ~3 KB).
 */
#ifndef DOD_TYPES_H
#define DOD_TYPES_H

#include <stdint.h>
#include "platform.h"

typedef uint8_t  dodBYTE;
typedef uint16_t dodSHORT;

/* index sentinels (indices are small: OCBLND=72, CCBLND=32 entries) */
#define DOD_NONE ((int8_t)-1)

/* Attack Block - shared player/creature attack workspace */
typedef struct {
    dodSHORT P_ATPOW;
    dodBYTE  P_ATMGO;
    dodBYTE  P_ATMGD;
    dodBYTE  P_ATPHO;
    dodBYTE  P_ATPHD;
    dodSHORT P_ATXX1;
    dodSHORT P_ATXX2;
    dodSHORT P_ATDAM;
} ATB;

/* Creature Control Block */
typedef struct {
    dodSHORT P_CCPOW;
    dodBYTE  P_CCMGO;
    dodBYTE  P_CCMGD;
    dodBYTE  P_CCPHO;
    dodBYTE  P_CCPHD;
    jiffy_t  P_CCTMV;   /* move period, jiffies */
    jiffy_t  P_CCTAT;   /* attack period, jiffies */
    int8_t   P_CCOBJ;   /* carried object index, DOD_NONE if none */
    dodSHORT P_CCDAM;
    dodBYTE  P_CCUSE;
    dodBYTE  creature_id;
    dodBYTE  P_CCDIR;
    dodBYTE  P_CCROW;
    dodBYTE  P_CCCOL;
} CCB;

/* Creature Definition Block (per creature type) */
typedef struct {
    dodSHORT P_CDPOW;
    dodBYTE  P_CDMGO;
    dodBYTE  P_CDMGD;
    dodBYTE  P_CDPHO;
    dodBYTE  P_CDPHD;
    jiffy_t  P_CDTMV;
    jiffy_t  P_CDTAT;
} CDB;

/* Object Control Block */
typedef struct {
    int8_t   P_OCPTR;   /* next object in chain, DOD_NONE terminates */
    dodBYTE  P_OCROW;
    dodBYTE  P_OCCOL;
    dodBYTE  P_OCLVL;
    dodBYTE  P_OCOWN;
    dodSHORT P_OCXX0;
    dodSHORT P_OCXX1;
    dodSHORT P_OCXX2;
    dodBYTE  obj_id;
    dodBYTE  obj_type;
    dodBYTE  obj_reveal_lvl;
    dodBYTE  P_OCMGO;
    dodBYTE  P_OCPHO;
} OCB;

/* Object Definition Block */
typedef struct {
    dodBYTE P_ODCLS;
    dodBYTE P_ODREV;
    dodBYTE P_ODMGO;
    dodBYTE P_ODPHO;
} ODB;

/* Extra Definition Block (torch timers / ring charges / shield defense) */
typedef struct {
    int8_t   P_OXIDX;
    dodSHORT P_OXXX0;
    dodSHORT P_OXXX1;
    dodSHORT P_OXXX2;
} XDB;

/* Scheduler task control block: 60 Hz jiffy deadlines
 * (the C++ port used Uint32 milliseconds; the original used jiffies).
 * prev_time is only read by the CLOCK task's elapsed calculation. */
typedef struct {
    int8_t  type;       /* task type, DOD_NONE = free slot */
    int8_t  data;       /* creature id for creature tasks */
    jiffy_t frequency;  /* period in jiffies */
    jiffy_t next_time;  /* next due jiffy (delta arithmetic!) */
    jiffy_t prev_time;
} Task;

/* 32x32 maze coordinates packed the original way */
typedef struct {
    dodBYTE row;
    dodBYTE col;
} RowCol;

#endif /* DOD_TYPES_H */
