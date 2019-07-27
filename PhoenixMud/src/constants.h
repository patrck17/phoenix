/* ************************************************************************
 *  File: constants.h                                   Part of CircleMUD *
 * Usage: Numeric and string contants used by the MUD                     *
 *                                                                        *
 * All rights reserved.  See license.doc for complete information.        *
 *                                                                        *
 * Copyright (C) 1993, 94 by the Trustees of the Johns Hopkins University *
 * CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
 ************************************************************************ */

/* constants.c */
extern char   *circlemud_version;
extern char   *dirs[];
extern char   *armor_types[];
extern char   *item_condition[];
extern char   *item_condition_no_color[];
extern char   *item_wear[];
extern char   *item_wear_no_color[];
extern char   *room_bits[];
extern char   *room2_bits[];
extern char   *teleport_bits[];
extern char   *zone_bits[];
extern char   *zone_status[];
extern char   *zone_source[];
extern char   *zone_continent[][2];
extern char   *exit_bits[];
extern char   *sector_types[];
extern char   *genders[];
extern char   *wound_types[];
extern char   *position_types[];
extern char   *char_size[];
extern int     item_size_limits[][2];
extern char   *player_bits[];
extern char   *action_bits[];
extern char   *action2_bits[];
extern char   *preference_bits[];
extern char   *preference2_bits[];
extern char   *affected_bits[];
extern char   *affected2_bits[];
extern char   *room_affect_bits[];
extern char   *connected_types[];
extern char   *where[];
extern char   *equipment_types[];
extern int     wear_check[];
extern float   wear_dam_adjust[];
extern struct  obj_imm_type obj_immunity[];
extern int     hand_position[];
extern char   *item_types[];
extern char   *fuels[];
extern char   *wear_bits[];
extern char   *wear_strings[];
extern char   *material_types[];
extern char    *material_types_lower[];
extern struct  obj_material_affs material_affs[];
extern char   *extra_bits[];
extern char   *extra_bits2[];
extern char   *extra_bits_id[];
extern char   *extra_bits2_id[];
extern char   *anti_bits[];
extern char   *anti_bits_id[];
extern char   *immunity_names[];
extern char   *apply_types[];
extern char   *container_bits[];
extern char   *drinks[];
extern char   *drinknames[];
extern char   *color_liquid[];
extern char   *fullness[];
/*extern char   *spell_wear_off_msg[];*/
extern char   *spell_targets[];
extern char   *spell_routines[];
extern char   *npc_class_types[];
extern char   *WizLevels[];
extern char   *weekdays[];
extern char   *month_name[];
extern int     sharp[];
extern char   *mobprog_types[];
extern char   *hyper_soc[];
extern char   *happy_soc[];
extern char   *cheery_soc[];
extern char   *neutral_soc[];
extern char   *sad_soc[];
extern char   *grouchy_soc[];
extern char   *homicidal_soc[];
extern char   *opp_sex_soc[];
extern char   *reset_string[];
extern char   *MYERRORSTRING[];
extern int     movement_loss[];
extern int     rev_dir[];
extern char   *str_strings[];
extern char   *dex_strings[];
extern char   *con_strings[];
extern char   *int_strings[];
extern char   *wis_strings[];
extern char   *cha_strings[];
extern struct  str_app_type str_app[];
extern struct  dex_skill_type dex_app_skill[];
extern struct  dex_app_type dex_app[];
extern struct  con_app_type con_app[];
extern struct  int_app_type int_app[];
extern struct  wis_app_type wis_app[];
extern struct  cha_app_type cha_app[];
extern struct  point_gain_type point_gain[];
extern struct  mob_defaults mob_def_stats[];
extern int     drink_aff[][3];
extern char   *race_trait_string[];
extern struct  obj_ore_types ore_types[];
extern struct  npc_class_mana npc_class_mult[];
/* Race.c */
extern struct racial_data racial_info[];
extern struct trait_data trait_info[];
extern struct race_size_data race_size_info[];
extern struct race_stat_data race_stat_info[];
extern long race_max_stats[][6];
extern char *AssemblyTypes[];
extern int  CHECK;

/* Fight.c */
/* The larger this is, the less exp people get per kill.  Default
 * value is 2.7.
 */
extern float fight_group_exp_divisor;
/* The larger this is, the more level difference matters when you
 * try to hit someone (see dodge_check).  Default value is 4.
 */
extern int fight_ldiff_dodge_multiplier;
/* A large group has a greater chance of landing attacks than a
 * person running solo.  The larger this number is, the greater
 * the advantage of running in groups.  Default value is 0.
 */
extern int fight_grouped_dodge_multiplier;
/* See dodge_check.  Dodging attacks is done by choosing a random
 * number between 0 and fight_dodge_random_bound.  If the random
 * number is too high, the attack is dodged.  The lower this
 * number, the better chance someone has of landing attacks.
 * Default value is 190.
 */
extern int fight_dodge_random_bound;

