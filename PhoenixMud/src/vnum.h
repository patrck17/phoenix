/***************************************************************************
 *  File: vnum.h                                        Part of PhoenixMud *
 *  Usage: Vnum of special object and rooms in the mud that the code uses  *
 *         Mostly these are used for spells.                               *
 *                                                                         *
 *  All rights reserved.  See license.doc for complete information.        *
 *  AAM Apr 98                                                             *
 *                                                                         *
 *  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
 *  PhoenixMUD is based on CircleMUD, Copyright (C) 1996-98.               *
 ***************************************************************************/



/*
 * creation spells 
 */
#define VNUM_CREATE_FOOD  1	/* mushroom */
#define VNUM_CREATE_WATER 3019	/* bottle */
#define VNUM_CREATE_LIGHT 16	/* ball of light */
#define VNUM_CONT_LIGHT   17	/* glowstaff */
#define VNUM_PORTAL       35	/* Object # of Portal */ 
#define VNUM_GOODBERRY    40	/* goodberry */
 
/* 
 * These Mobiles do not exist 
 */
#define MOB_MONSUM_I    130	/*  */
#define MOB_MONSUM_II   140	/*  */
#define MOB_MONSUM_III  150	/*  */
#define MOB_GATE_I      160	/*  */
#define MOB_GATE_II     170	/*  */
#define MOB_GATE_III    180	/*  */

/* 
 * Defined mobiles 
 */
#define MOB_INFANTRY        2	/* Conjure Infantry */
#define MOB_SUMNMOUNT       3	/* Summon Mount */
#define MOB_CLONE          10	/* Clone */
#define MOB_AERIALSERVANT  11	/*  */
#define MOB_ELEMENTAL_BASE 20	/*  */
#define MOB_ZOMBIE         30	/* Animate Dead */

/* 
 * Material Components
 */
#define BIT_WEB          320000 /* web spell */


/*
 * Mining Vnums.
 */
#define MINE_DIRT      760
#define MINE_IRON      761
#define MINE_COPPER    762
#define MINE_SILVER    763
#define MINE_GOLD      764
#define MINE_SOFTCOAL  765
#define MINE_MITHRIL   766
#define MINE_PLATINUM  767
#define MINE_TITANIUM  768
#define MINE_TIN       769

#define MINE_OPAL      775
#define MINE_DIAMOND   776
#define MINE_RUBY      777
#define MINE_EMERALD   778
#define MINE_TOPAZ     779
#define MINE_JADE      780
#define MINE_GARNET    781
#define MINE_SAPPHIRE  782
#define MINE_AMETHYST  783
#define MINE_QUARTZ    784
#define MINE_FIRE_OPAL 785


/* 
 * Othere objs
 */
#define DEFAULT_TICKET  320001
#define ROULETTE_WHEEL  18700


/*
 * Fish to catch
 */

/*
#define FRESHWATER_FISH_BASE       32570
#define NUM_FW_FISH                10
#define SALTWATER_FISH_BASE        FRESHWATER_FISH_BASE + NUM_FW_FISH
#define NUM_SW_FISH                10
#define FISHING_JUNK_BASE          SALTWATER_FISH_BASE + NUM_SW_FISH
#define NUM_FISH_JUNK              10
*/

/*
#define FRESHWATER_FISH_BASE       66100
#define NUM_FW_FISH                16
#define SALTWATER_FISH_BASE        66100
#define NUM_SW_FISH                16
#define FISHING_JUNK_BASE          66110
#define NUM_FISH_JUNK              3
*/

#define FRESHWATER_FISH_BASE       26600
#define NUM_FW_FISH                16
#define SALTWATER_FISH_BASE        26600
#define NUM_SW_FISH                16
#define FISHING_JUNK_BASE          26610
#define NUM_FISH_JUNK              3
