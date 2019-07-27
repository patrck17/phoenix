/* ************************************************************************
*   File: Guild.h                                                         *
*  Usage: GuildMaster's: Definition for everything needed by shop.c       *
*                                                                         *
* Written by Jason Goodwin.   jgoodwin@expert.cc.purdue.edu               *
************************************************************************ */
 
struct guild_master_data {
   int num;			/* number of the guild */
   int skills_and_spells[MAX_SKILLS+1];	/*array to keep track of which things we'll train */
   float charge;		/* charge*skill level=how much we'll charge */
   char *no_such_skill;		/* message when we don't teach that skill */
   char *not_enough_gold;	/* message when the student doesn't have enough gold */
   int type;			/* does GM teach skills or spells? */
   int gm;			/* GM's vnum */
   int with_who;		/* whom we dislike */
   int open, close;		/* when we will train */
   int min_lev,max_lev;		/* the levels we train */
   SPECIAL(*func);		/* secondary spec_proc for the GM */
};

/* Whom will we not train  */
#define TRAIN_NOGOOD		(1 << 0)
#define TRAIN_NOEVIL		(1 << 1)
#define TRAIN_NONEUTRAL		(1 << 2)
#define TRAIN_NOMAGE		(1 << 3)
#define TRAIN_NOCLERIC		(1 << 4)
#define TRAIN_NOTHIEF		(1 << 5)
#define TRAIN_NOWARRIOR		(1 << 6)
#define TRAIN_NORANGER          (1 << 7)
#define TRAIN_NOBARD            (1 << 8)
#define TRAIN_NOMONK            (1 << 9)
#define TRAIN_NOUNUSED          (1 << 10)
#define TRAIN_NOBARBARIAN       (1 << 11)
#define TRAIN_NOPALADIN         (1 << 12)
#define TRAIN_NOAPALADIN        (1 << 13)
#define TRAIN_NODRUID           (1 << 14)
#define TRAIN_NOMERCHANT        (1 << 15)
#define TRAIN_NOKENSAI          (1 << 16)
#define TRAIN_NOASSASSIN        (1 << 17)
#define TRAIN_NONECRO           (1 << 18)
#define TRAIN_NODEVA            (1 << 19)
#define TRAIN_NOIMMORTAL        (1 << 20)
#define TRAIN_NOGOD             (1 << 21)
#define TRAIN_NONONECLASS       (1 << 22)

#define GM_NUM(i)      (gm_index[i].num)
#define GM_CHARGE(i)   (gm_index[i].charge)
#define GM_TYPE(i)     (gm_index[i].type)
#define GM_TRAINER(i)  (gm_index[i].gm)
#define GM_WITH_WHO(i) (gm_index[i].with_who)
#define GM_OPEN(i)     (gm_index[i].open)
#define GM_CLOSE(i)    (gm_index[i].close)
#define GM_FUNC(i)     (gm_index[i].func)
#define GM_MINLVL(i)   (gm_index[i].min_lev)
#define GM_MAXLVL(i)   (gm_index[i].max_lev)


#define NOTRAIN_GOOD(i)		(IS_SET((GM_WITH_WHO(i)), TRAIN_NOGOOD))
#define NOTRAIN_EVIL(i)		(IS_SET((GM_WITH_WHO(i)), TRAIN_NOEVIL))
#define NOTRAIN_NEUTRAL(i)	(IS_SET((GM_WITH_WHO(i)), TRAIN_NONEUTRAL))
#define NOTRAIN_MAGE(i)		(IS_SET((GM_WITH_WHO(i)), TRAIN_NOMAGE))
#define NOTRAIN_CLERIC(i)	(IS_SET((GM_WITH_WHO(i)), TRAIN_NOCLERIC))
#define NOTRAIN_THIEF(i)	(IS_SET((GM_WITH_WHO(i)), TRAIN_NOTHIEF))
#define NOTRAIN_WARRIOR(i)	(IS_SET((GM_WITH_WHO(i)), TRAIN_NOWARRIOR))
#define NOTRAIN_RANGER(i)       (IS_SET((GM_WITH_WHO(i)), TRAIN_NORANGER))
#define NOTRAIN_BARD(i)         (IS_SET((GM_WITH_WHO(i)), TRAIN_NOBARD))
#define NOTRAIN_MONK(i)         (IS_SET((GM_WITH_WHO(i)), TRAIN_NOMONK))
#define NOTRAIN_UNUSED(i)       (IS_SET((GM_WITH_WHO(i)), TRAIN_NOUNUSED))
#define NOTRAIN_BARBARIAN(i)    (IS_SET((GM_WITH_WHO(i)), TRAIN_NOBARBARIAN))
#define NOTRAIN_PALADIN(i)      (IS_SET((GM_WITH_WHO(i)), TRAIN_NOPALADIN))
#define NOTRAIN_APALADIN(i)     (IS_SET((GM_WITH_WHO(i)), TRAIN_NOAPALADIN))
#define NOTRAIN_DRUID(i)        (IS_SET((GM_WITH_WHO(i)), TRAIN_NODRUID))
#define NOTRAIN_MERCHANT(i)     (IS_SET((GM_WITH_WHO(i)), TRAIN_NOMERCHANT))
#define NOTRAIN_KENSAI(i)       (IS_SET((GM_WITH_WHO(i)), TRAIN_NOKENSAI))
#define NOTRAIN_ASSASSIN(i)     (IS_SET((GM_WITH_WHO(i)), TRAIN_NOASSASSIN))
#define NOTRAIN_NECRO(i)        (IS_SET((GM_WITH_WHO(i)), TRAIN_NONECRO))
#define NOTRAIN_DEVA(i)         (IS_SET((GM_WITH_WHO(i)), TRAIN_NODEVA))
#define NOTRAIN_IMMORTAL(i)     (IS_SET((GM_WITH_WHO(i)), TRAIN_NOIMMORTAL))
#define NOTRAIN_GOD(i)          (IS_SET((GM_WITH_WHO(i)), TRAIN_NOGOD))
#define NOTRAIN_NONECLASS(i)    (IS_SET((GM_WITH_WHO(i)), TRAIN_NONONECLASS))

#define MSG_TRAINER_NOT_OPEN 	  "I'm busy! Come back later!"
#define MSG_TRAINER_NO_SEE_CH 	  "I don't train someone I can't see!"
#define MSG_TRAINER_DISLIKE_ALIGN "Get out of here before I get angry!"
#define MSG_TRAINER_DISLIKE_CLASS "I don't like your kind!"

#define LEARNED_LEVEL   0       /* % known which is considered "learned" */
#define MAX_PER_PRAC    1       /* max percent gain in skill per practice */
#define MIN_PER_PRAC    2       /* min percent gain in skill per practice */


#define LEARNED(ch) (prac_params[LEARNED_LEVEL][(int)GET_CLASS(ch)])
#define MINGAIN(ch) (prac_params[MIN_PER_PRAC][(int)GET_CLASS(ch)])
#define MAXGAIN(ch) (prac_params[MAX_PER_PRAC][(int)GET_CLASS(ch)])

