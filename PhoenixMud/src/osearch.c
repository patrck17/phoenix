/*
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//  File : osearch.c
//  Usage : Parameter-based object prototype searches.
//  Author : Brian Langenfeld (Cipher)
//  Created : May 14th, 1998
//  Last update : September 20th, 1998
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
*/
#include "../localHeader/conf.h"
#include "../localHeader/sysdep.h"
#include "structs.h"
#include "comm.h"
#include "interpreter.h"
#include "handler.h"
#include "db.h"
#include "utils.h"

const char *osearch_usage = 
"Usage: osearch\r\n"
"  [-n <name substring|\"name phrase\">]\r\n"
"  [-t <item_type>]\r\n"
"  [-p <wear_bits>]\r\n"
"  [-l <base level>[-top level]]\r\n"
"  [-v <base vnum>[-top vnum]]\r\n"
"  [-a <apply_types>]\r\n"
"  [-avgdam <average damage>]\r\n"
"  [-res <\"any\"|immunity_names>]\r\n"
"  [-sus <\"any\"|immunity_names>]\r\n"
"  [-imm <\"any\"|immunity_names>]\r\n"
"  [-sort <\"level\"|apply1[,apply2,apply3,...]>]\r\n"
"  [-show <apply1[,apply2,apply3,...]>]\r\n"
"  [-exclude <zone1,[zone2,zone3,...]>]\r\n"
"\r\n"
"To see possible values for a field, give that field a bogus value.\r\n";

extern const char *item_types[];
extern const char *wear_bits[];
extern const char *apply_types[];
extern const char *immunity_names[];
extern struct obj_data *obj_proto;
extern struct index_data *obj_index;
extern int top_of_objt;

extern int is_abbrevc(const char *arg1, const char *arg2);
extern char *str_str(char *cs, char *ct);

/* REQUIRES string_list END with "\n"! */
int find_abbrev_in_list(char *substring, const char *string_list[])
{
  int i = 0;
  while (*string_list[i] != '\n' && !is_abbrevc(substring, string_list[i])) {
    i++;
  }
  if (*string_list[i] == '\n') {
    return -1;
  }
  return i;
}

int osearch_compare_objects(struct obj_data *o1, struct obj_data *o2, int *keys, int num_keys)
{
  int i, j1, j2;
  for (i = 0; i < num_keys; i++) {
    if (keys[i] == 1000) {
      if (GET_OBJ_LR(o1) > GET_OBJ_LR(o2)) {
	return 1;
      } else if (GET_OBJ_LR(o1) < GET_OBJ_LR(o2)) {
	return -1;
      }
      continue;
    }
    for (j1 = 0; j1 < MAX_OBJ_AFFECT; j1++) {
      if (o1->affected[j1].location == keys[i]) {
	break;
      }
    }
    for (j2 = 0; j2 < MAX_OBJ_AFFECT; j2++) {
      if (o2->affected[j2].location == keys[i]) {
	break;
      }
    }
    if (j1 == MAX_OBJ_AFFECT && j2 == MAX_OBJ_AFFECT) {
      continue;
    } else if (j1 == MAX_OBJ_AFFECT && j2 != MAX_OBJ_AFFECT && o2->affected[j2].modifier > 0) {
      return -1;
    } else if (j2 == MAX_OBJ_AFFECT && j1 != MAX_OBJ_AFFECT && o1->affected[j1].modifier > 0) {
      return 1;
    }
    if (o1->affected[j1].modifier == o2->affected[j2].modifier) {
      continue;
    } else if (o1->affected[j1].modifier < o2->affected[j2].modifier) {
      return -1;
    } else if (o1->affected[j1].modifier > o2->affected[j2].modifier) {
      return 1;
    }
  }
  return 0;
}

/* list MUST end with "\n"! */
void concatenate_list(const char *list[], char *output)
{
  output[0] = '\x0';
  int i, first = 1;
  char line[128] = {'\x0'};
  for (i = 0; *list[i] != '\n'; i++) {
    if (strlen(list[i]) == 0 || !strcmp(list[i], "UNUSED") || !strcmp(list[i], "UNDEFINED") || strstr(list[i], "(R)") || !strcmp(list[i], "NONE")) {
      continue;
    }
    if (!first && line[0]) {
      strcat(line, ", ");
    }
    if (strlen(line) + strlen(list[i]) < 77) {
      strcat(line, list[i]);
    } else {
      strcat(output, line);
      strcat(output, "\r\n");
      line[0] = '\x0';
    }
    first = 0;
  }
  strcat(output, line);
  strcat(output, "\r\n");
}

ACMD(do_osearch)
{
  char *name_substring = NULL;
  int item_type = -1;
  int wear_type = -1;
  int level_base = 0, level_top = LVL_IMPL;
  int vnum_base = 0, vnum_top = INT_MAX;
  int applies[64], num_applies = 0;
  float avgdam = -1.0f;
  int res[64], num_res = 0;
  int sus[64], num_sus = 0;
  int imm[64], num_imm = 0;
  int sort[64], num_sort = 0;
  int exclude[64], num_exclude = 0;
  int show[64], num_show = 0;

  char output_buffer[128000] = {'\x0'};

  int i, j, k;
  char buf[4096], buf2[4096], buf3[4096];

  memset(applies, 0, 64*sizeof(int));
  memset(res, 0, 64*sizeof(int));
  memset(sus, 0, 64*sizeof(int));
  memset(imm, 0, 64*sizeof(int));

  if (!argument || !*argument) {
    send_to_char(ch, "%s", osearch_usage);
    return;
  }
  skip_spaces(&argument);
  if (!argument || !*argument) {
    send_to_char(ch, "%s", osearch_usage);
    return;
  }

  strcpy(buf, argument);

  int first = 1;
  while (TRUE) {
    char *option = strtok(first ? buf : NULL, " ");
    if (!option) {
      break;
    }
    first = 0;

    char *value = NULL;
    if (!str_cmp(option, "-n") && strstr(argument, "\"")) {
      value = strtok(NULL, "\"");
    } else {
      value = strtok(NULL, " ");
    }

    if (!value) {
      send_to_char(ch, "No value given for option \"%s\".\r\n", option);
      return;
    }
    
    if (!str_cmp(option, "-n")) {
      name_substring = value;
    } else if (!str_cmp(option, "-t")) {
      i = find_abbrev_in_list(value, item_types);
      if (i == -1) {
	concatenate_list(item_types, buf2);
	send_to_char(ch, "Unrecognized item type \"%s\".  Valid types are:\r\n\r\n%s\r\n", value, buf2);
	return;
      }
      item_type = i;
    } else if (!str_cmp(option, "-p")) {
      i = find_abbrev_in_list(value, wear_bits);
      if (i == -1) {
	concatenate_list(wear_bits, buf2);
	send_to_char(ch, "Unrecognized item type \"%s\".  Valid types are:\r\n\r\n%s\r\n", value, buf2);
	return;
      }
      wear_type = i;
    } else if (!str_cmp(option, "-a")) {
      int first2 = 1;
      char *buf4 = get_buffer(1024); char *tmp = buf4;
      while (TRUE) {
	char *apply = strtok_r(first2 ? value : NULL, ",", &buf4);
	if (!apply) {
	  break;
	}
	i = find_abbrev_in_list(apply, apply_types);
	if (i == -1) {
	  concatenate_list(apply_types, buf2);
	  send_to_char(ch, "Unrecognized apply \"%s\".  Valid applies are:\r\n\r\n%s\r\n", value, buf2);
	  release_buffer(tmp);
	  return;
	}
	applies[num_applies++] = i;
	first2 = 0;
      }
      release_buffer(tmp);
    } else if (!str_cmp(option, "-res") && is_abbrev(value, "any")) {
      res[num_res++] = -1;
    } else if (!str_cmp(option, "-res")) {
      int first2 = 1;
      char *buf4 = get_buffer(1024); char *tmp = buf4;
      while (TRUE) {
	char *resist = strtok_r(first2 ? value : NULL, ",", &buf4);
	if (!resist) {
	  break;
	}
	i = find_abbrev_in_list(resist, immunity_names);
	if (i == -1) {
	  concatenate_list(immunity_names, buf2);
	  send_to_char(ch, "Unrecognized immunity type \"%s\".  Valid types are:\r\n\r\n%s\r\n", value, buf2);
	  release_buffer(tmp);
	  return;
	}
	res[num_res++] = i;
	first2 = 0;
      }
      release_buffer(tmp);
    } else if (!str_cmp(option, "-sus") && is_abbrev(value, "any")) {
      sus[num_sus++] = -1;
    } else if (!str_cmp(option, "-sus")) {
      int first2 = 1;
      char *buf4 = get_buffer(1024); char *tmp = buf4;
      while (TRUE) {
	char *suscept = strtok_r(first2 ? value : NULL, ",", &buf4);
	if (!suscept) {
	  break;
	}
	i = find_abbrev_in_list(suscept, immunity_names);
	if (i == -1) {
	  concatenate_list(immunity_names, buf2);
	  send_to_char(ch, "Unrecognized immunity type \"%s\".  Valid types are:\r\n\r\n%s\r\n", value, buf2);
	  release_buffer(tmp);
	  return;
	}
	sus[num_sus++] = i;
	first2 = 0;
      }
      release_buffer(tmp);
    } else if (!str_cmp(option, "-imm") && is_abbrev(value, "any")) {
      imm[num_imm++] = -1;
    } else if (!str_cmp(option, "-imm")) {
      int first2 = 1;
      char *buf4 = get_buffer(1024); char *tmp = buf4;
      while (TRUE) {
	char *immune = strtok_r(first2 ? value : NULL, ",", &buf4);
	if (!immune) {
	  break;
	}
	i = find_abbrev_in_list(immune, immunity_names);
	if (i == -1) {
	  concatenate_list(immunity_names, buf2);
	  send_to_char(ch, "Unrecognized immunity type \"%s\".  Valid types are:\r\n\r\n%s\r\n", value, buf2);
	  release_buffer(tmp);
	  return;
	}
	imm[num_imm++] = i;
	first2 = 0;
      }
      release_buffer(tmp);
    } else if (!str_cmp(option, "-avgdam")) {
      avgdam = atof(value);
    } else if (!str_cmp(option, "-l")) {
      char *buf4 = get_buffer(1024); char *tmp = buf4;
      char *base = strtok_r(value, "-", &buf4);
      char *top = strtok_r(NULL, "-", &buf4);
      level_base = MAX(0, MIN(LVL_IMPL, atoi(base)));
      if (top) {
	level_top = MAX(level_base, MIN(LVL_IMPL, atoi(top)));
      } else {
	level_top = LVL_IMPL;
      }
      release_buffer(tmp);
    } else if (!str_cmp(option, "-v")) {
      char *buf4 = get_buffer(1024); char *tmp = buf4;
      char *base = strtok_r(value, "-", &buf4);
      char *top = strtok_r(NULL, "-", &buf4);
      vnum_base = MAX(0, atoi(base));
      if (top) {
	vnum_top = MAX(vnum_base, atoi(top));
      } else {
	vnum_top = INT_MAX;
      }
      release_buffer(tmp);
    } else if (!str_cmp(option, "-sort")) {
      int first2 = 1;
      char *buf4 = get_buffer(1024); char *tmp = buf4;
      while (TRUE) {
	char *key = strtok_r(first2 ? value : NULL, ",", &buf4);
	if (!key) {
	  break;
	} else if (!str_cmp(key, "level")) {
	  i = 1000;
	} else {
	  i = find_abbrev_in_list(key, apply_types);
	  if (i == -1) {
	    send_to_char(ch, "Unknown sort key \"%s\".", key);
	    release_buffer(tmp);
	    return;
	  }
	}
	sort[num_sort++] = i;
	first2 = 0;	
      }
      release_buffer(tmp);
    } else if (!str_cmp(option, "-exclude")) {
      int first2 = 1;
      char *buf4 = get_buffer(1024); char *tmp = buf4;
      while (TRUE) {
	char *key = strtok_r(first2 ? value : NULL, ",", &buf4);
	if (!key) {
	  break;
	}
	exclude[num_exclude++] = atoi(key);
	first2 = 0;	
      }
      release_buffer(tmp);
    } else if (!str_cmp(option, "-show")) {
      int first2 = 1;
      char *buf4 = get_buffer(1024); char *tmp = buf4;
      while (TRUE) {
	char *field = strtok_r(first2 ? value : NULL, ",", &buf4);
	if (!field) {
	  break;
	}
	if ((i = find_abbrev_in_list(field, immunity_names)) != -1) {
	  i += 1000;
	} else if ((i = find_abbrev_in_list(field, apply_types)) != -1) {
	  i += 2000;
	} else {
	  send_to_char(ch, "Unknown show field \"%s\".\r\n", value);
	  release_buffer(tmp);
	  return;
	}
	show[num_show++] = i;
	first2 = 0;
      }
      release_buffer(tmp);
    } else {
      send_to_char(ch, "Unknown option, \"%s\".\r\n\r\n", option);
      send_to_char(ch, "%s", osearch_usage);
      return;
    }
  }

  long ids[10000];
  int num_ids = 0;
  int found;

  for (i = 0; i < top_of_objt; i++) {
    struct obj_data *obj = &obj_proto[i];
    if (GET_OBJ_LR(obj) < level_base || GET_OBJ_LR(obj) > level_top) {
      continue;
    } else if (GET_OBJ_VNUM(obj) < vnum_base || GET_OBJ_VNUM(obj) > vnum_top) {
      continue;
    } else if (item_type != -1 && GET_OBJ_TYPE(obj) != item_type) {
      continue;
    } else if (wear_type != -1 && !CAN_WEAR(obj, 1 << wear_type)) {
      continue;
    } else if (name_substring && !str_str(GET_OBJ_NAME(obj), name_substring)) {
      continue;
    } else if (GET_OBJ_TYPE(obj) == ITEM_WEAPON && avgdam != -1.0f && avgdam >= (GET_OBJ_VAL(obj, 1) * (1.0f+GET_OBJ_VAL(obj, 2))/2.0f)) {
      continue;
    }

    /* Check excludes. */
    found = 0;
    for (j = 0; j < num_exclude; j++) {
      if (GET_OBJ_VNUM(obj) >= 100*exclude[j] && GET_OBJ_VNUM(obj) < 100*(1+exclude[j])) {
	found = 1;
	break;
      }
    }
    if (found) {
      continue;
    }    
    
    /* Check applies. */
    found = 0;
    for (j = 0; j < num_applies; j++) {
      for (k = 0; k < MAX_OBJ_AFFECT; k++) {
	if (obj->affected[k].location == applies[j]) {
	  found = 1;
	  break;
	}
      }
    }
    if (num_applies > 0 && !found) {
      continue;
    }

    /* Check resistances. */
    found = 0;
    for (j = 0; j < num_res; j++) {
      for (k = 0; k < MAX_OBJ_AFFECT; k++) {
	if (obj->affected[k].location == APPLY_RESIST) {
	  if (res[j] == -1 || (obj->affected[k].modifier & (1 << res[j]))) {
	    found = 1;
	    break;
	  }
	}
      }
    }
    if (num_res > 0 && !found) {
      continue;
    }

    /* Check susceptibilities. */
    found = 0;
    for (j = 0; j < num_sus; j++) {
      for (k = 0; k < MAX_OBJ_AFFECT; k++) {
	if (obj->affected[k].location == APPLY_SUSC) {
	  if (sus[j] == -1 || (obj->affected[k].modifier & (1 << sus[j]))) {
	    found = 1;
	    break;
	  }
	}
      }
    }
    if (num_sus > 0 && !found) {
      continue;
    }

    /* Check immunities. */
    found = 0;
    for (j = 0; j < num_imm; j++) {
      for (k = 0; k < MAX_OBJ_AFFECT; k++) {
	if (obj->affected[k].location == APPLY_IMMUNE) {
	  if (imm[j] == -1 || (obj->affected[k].modifier & (1 << imm[j]))) {
	    found = 1;
	    break;
	  }
	}
      }
    }
    if (num_imm > 0 && !found) {
      continue;
    }

    /* You match. */
    ids[num_ids++] = i;
  }

  /* SORTING EVERYTHING. */
  if (num_sort > 0) {
    for (i = 0; i < num_ids; i++) {
      for (j = 0; j < num_ids-1; j++) {
	if (osearch_compare_objects(&obj_proto[ids[j]], &obj_proto[ids[j+1]], sort, num_sort) < 0) {
	  long t = ids[j];
	  ids[j] = ids[j+1];
	  ids[j+1] = t;
	}
      }
    }
  }

  /* Now print the output. */
  sprintf(buf, "\r\n%d object%s matched your query.\r\n\r\n", num_ids, num_ids!=1 ? "s" : "");
  strcat(output_buffer, buf);

  for (i = 0; i < num_ids; i++) {
    struct obj_data *obj = &obj_proto[ids[i]];
    /* strip the fucking color out! */
    char name[1024];
    if (!strstr(GET_OBJ_NAME(obj), "&")) {
      strcpy(name, GET_OBJ_NAME(obj));
    } else {
      k = 0;
      for (j = 0; j < strlen(GET_OBJ_NAME(obj)); j++) {
	if (GET_OBJ_NAME(obj)[j] != '&') {
	  name[k++] = GET_OBJ_NAME(obj)[j];
	} else {
	  j++;
	}
      }
      name[k++] = '\x0';
    }    
    sprintf(buf, "%5i. [%5ld] %-25.25s %4d ", i+1, GET_OBJ_VNUM(obj), name, GET_OBJ_LR(obj));

    /* Weapon average damage. */
    if (avgdam != -1.0f && GET_OBJ_TYPE(obj) == ITEM_WEAPON) {
      sprintf(buf2, "%5.1f ", (GET_OBJ_VAL(obj, 1) * (1+GET_OBJ_VAL(obj, 2))/2.0f));
      strcat(buf, buf2);
    }

    /* All the applies column. */
    for (j = 0; j < num_applies; j++) {
      for (k = 0; k < MAX_OBJ_AFFECT; k++) {
	int loc = obj->affected[k].location;
	if (loc == applies[j]) {
	  if (loc == APPLY_RESIST || loc == APPLY_SUSC || loc == APPLY_IMMUNE) {
	    strcat(buf, "  Y ");
	  } else {
	    sprintf(buf2, "%3d ", obj->affected[k].modifier);
	    strcat(buf, buf2);
	  }
	  break;
	}
      }
      if (k == MAX_OBJ_AFFECT) {
	strcat(buf, "  . ");
      }
    }

    /* The resist column. */
    found = 0;
    for (j = 0; j < num_res; j++) {
      for (k = 0; k < MAX_OBJ_AFFECT; k++) {
	if (res[j] != -1 && obj->affected[k].modifier & (1 << res[j])) {
	  strcat(buf, "  Y ");
	  break;
	}
      }
      if (k == MAX_OBJ_AFFECT) {
	strcat(buf, "  . ");
      }
    }

    /* The suscept column. */
    found = 0;
    for (j = 0; j < num_sus; j++) {
      for (k = 0; k < MAX_OBJ_AFFECT; k++) {
	if (sus[j] != -1 && obj->affected[k].modifier & (1 << sus[j])) {
	  strcat(buf, "  Y ");
	  break;
	}
      }
      if (k == MAX_OBJ_AFFECT) {
	strcat(buf, "  . ");
      }
    }

    found = 0;
    for (j = 0; j < num_imm; j++) {
      for (k = 0; k < MAX_OBJ_AFFECT; k++) {
	if (imm[j] != -1 && obj->affected[k].modifier & (1 << imm[j])) {
	  strcat(buf, "  Y ");
	  break;
	}
      }
      if (k == MAX_OBJ_AFFECT) {
	strcat(buf, "  . ");
      }
    }

    /* Now all the optional, extraneous fields. */
    for (j = 0; j < num_show; j++) {
      for (k = 0; k < MAX_OBJ_AFFECT; k++) {
	int loc = obj->affected[k].location;
	if (show[j] >= 1000 && show[j] < 2000) {
	  if (loc == APPLY_RESIST || loc == APPLY_SUSC || loc == APPLY_IMMUNE) {
	    if (obj->affected[k].modifier & (1 << (show[j]-1000))) {
	      strcat(buf, "  Y ");
	      break;
	    }
	  }
	} else if (show[j] >= 2000 && show[j] < 3000 && loc == show[j]-2000) {
	  sprintf(buf2, "%3d ", obj->affected[k].modifier);
	  strcat(buf, buf2);
	  break;
	}
      }
      if (k == MAX_OBJ_AFFECT) {
	strcat(buf, "  . ");
      }
    }

    strcat(buf, "\r\n");
    if (strlen(output_buffer) + strlen(buf) < 127950) {
      strcat(output_buffer, buf);
    } else {
      strcat(output_buffer, "\r\n\r\nToo many objects matched your query.\r\n");
      break;
    }
  }

  page_string(ch->desc, output_buffer, TRUE, "");
}

































#define MAX_HEADER2_COLUMNS 10
#define OSEARCH_FORMAT \
"Format : osearch [<range>] [-a <applies>] [-n <name>]\r\n" \
"            [-p <position>] [-t <type>] [-l <base>[-top]]\r\n"

#define VALID_APPLIES \
"   str       class     hit       damroll   illegal\r\n" \
"   dex       level     move      para      combat\r\n" \
"   int       age       gold      rod       stats\r\n" \
"   wis       weight    exp       petri     points\r\n" \
"   con       height    ac        breath    spell_fail\r\n" \
"   cha       mana      hitroll   spell     avgdam\r\n"

#define VALID_WEAR \
"finger    neck    body    head    legs    feet    hands\r\n" \
"shield    arms    about   waist   wrist   wield   hold\r\n" \
"ear       face    back\r\n"

#define VALID_TYPES \
"light          scroll  wand    staff   weapon     treasure     armor\r\n"\
"potion         worn    other   trash   trap       container    note\r\n"\
"liq container  key     food    money   pen        boat         fountain\r\n"\
"fuel           pill    throw   grenade bow        sling        crossbow\r\n"\
"bolt           arrow   rock    portal  furniture  mount ticket\r\n"

//extern struct obj_data *obj_proto;
extern struct index_data *obj_index;
//extern int top_of_objt;
//extern char *item_types[];
//extern char *wear_bits[];
ACMD(do_osearch);
bool applyColumns(int N, int applyBit, char *sBuf);

char *sHeader2Bits[] = {
                          "NIL", "Str", "Dex", "Int", "Wis", "Con", // 00-05
                          "Cha", "Cla", "Lev", "Age", "Wei",  // 06-10
                          "Hei", "MMP", "MHP", "MVP", "Gol",  // 11-15
                          "Exp", " AC", "Hit", "Dam", "SPa",  // 16-20
                          "SRo", "SPe", "SBr", "SSp", "Cha", // 21-25 
                          "Lig", "Imm", "Res", "Suc", "SpF","\n"//26-31
                       };

ACMD(do_osearch2)
   {
   bool bName, bPosition, bType, bApplies, bMet, bRanged;
   char sApplies[MAX_INPUT_LENGTH], sHeader[MAX_STRING_LENGTH],
   sHeader2[MAX_STRING_LENGTH], sName[MAX_INPUT_LENGTH],
   sObjList[128000], sPosition[MAX_INPUT_LENGTH],
   sType[MAX_STRING_LENGTH], cFlag;
   char buf[MAX_STRING_LENGTH];
   char buf1[MAX_STRING_LENGTH];
   char buf2[MAX_STRING_LENGTH];
   char arg[MAX_STRING_LENGTH];
   int iFound = 0, iList, iPosition = 0, iType = 0, applyNum,
                                      iHigh = 32000, iLow = 0, iSwap;
   long lApplies = 0;
   int baseLevel = 0, topLevel = LVL_IMPL;
   char *token1, *token2;

   int showAvgDam = 0;

   bName = bPosition = bType = bApplies = bMet = bRanged = FALSE;
   sApplies[0] = sHeader[0] = sHeader2[0] = sName[0] = sObjList[0] =
                                 sPosition[0] = sType[0] = '\0';

   skip_spaces(&argument);
   strcpy(buf, argument);

   if(!(*argument) || !(argument))
      {
      send_to_char(ch, OSEARCH_FORMAT);
      return;
      }

   while(*buf)
      {
      half_chop(buf, arg, buf1);
      if(isdigit((int)*arg))
         {
         bRanged = TRUE;
         sscanf(arg, "%d-%d", &iLow, &iHigh);
         strcpy(buf, buf1);
         }
      else if(*arg == '-')
         {
         cFlag = *(arg + 1);
         switch(cFlag)
            {
         case 'a':
            bApplies = TRUE;
            half_chop(buf1, sApplies, buf);
            if(strlen(sApplies) < 1)
               {
               send_to_char(ch, "The -a switch requires arguments.  Valid arguments are:\r\n");
               send_to_char(ch, VALID_APPLIES);
               return;
               }
            while(isalpha((int)*sApplies))
               {
               if(!strcmp(sApplies, "str"))
                  SET_BIT(lApplies, (1 << APPLY_STR));
               else if(!strcmp(sApplies, "dex"))
                  SET_BIT(lApplies, (1 << APPLY_DEX));
               else if(!strcmp(sApplies, "int"))
                  SET_BIT(lApplies, (1 << APPLY_INT));
               else if(!strcmp(sApplies, "wis"))
                  SET_BIT(lApplies, (1 << APPLY_WIS));
               else if(!strcmp(sApplies, "con"))
                  SET_BIT(lApplies, (1 << APPLY_CON));
               else if(!strcmp(sApplies, "cha"))
                  SET_BIT(lApplies, (1 << APPLY_CHA));
               else if(!strcmp(sApplies, "class"))
                  SET_BIT(lApplies, (1 << APPLY_CLASS));
               else if(!strcmp(sApplies, "level"))
                  SET_BIT(lApplies, (1 << APPLY_LEVEL));
               else if(!strcmp(sApplies, "age"))
                  SET_BIT(lApplies, (1 << APPLY_AGE));
               else if(!strcmp(sApplies, "weight"))
                  SET_BIT(lApplies, (1 << APPLY_CHAR_WEIGHT));
               else if(!strcmp(sApplies, "height"))
                  SET_BIT(lApplies, (1 << APPLY_CHAR_HEIGHT));
               else if(!strcmp(sApplies, "mana"))
                  SET_BIT(lApplies, (1 << APPLY_MANA));
               else if(!strcmp(sApplies, "hit"))
                  SET_BIT(lApplies, (1 << APPLY_HIT));
               else if(!strcmp(sApplies, "move"))
                  SET_BIT(lApplies, (1 << APPLY_MOVE));
               else if(!strcmp(sApplies, "gold"))
                  SET_BIT(lApplies, (1 << APPLY_GOLD));
               else if(!strcmp(sApplies, "exp"))
                  SET_BIT(lApplies, (1 << APPLY_EXP));
               else if(!strcmp(sApplies, "ac"))
                  SET_BIT(lApplies, (1 << APPLY_AC));
               else if(!strcmp(sApplies, "hitroll"))
                  SET_BIT(lApplies, (1 << APPLY_HITROLL));
               else if(!strcmp(sApplies, "damroll"))
                  SET_BIT(lApplies, (1 << APPLY_DAMROLL));
               else if(!strcmp(sApplies, "para"))
                  SET_BIT(lApplies, (1 << APPLY_SAVING_PARA));
               else if(!strcmp(sApplies, "rod"))
                  SET_BIT(lApplies, (1 << APPLY_SAVING_ROD));
               else if(!strcmp(sApplies, "petri"))
                  SET_BIT(lApplies, (1 << APPLY_SAVING_PETRI));
               else if(!strcmp(sApplies, "breath"))
                  SET_BIT(lApplies, (1 << APPLY_SAVING_BREATH));
               else if(!strcmp(sApplies, "spell"))
                  SET_BIT(lApplies, (1 << APPLY_SAVING_SPELL));
               else if(!strcmp(sApplies, "spell_fail"))
                  SET_BIT(lApplies, (1 << APPLY_SPELL_FAIL));
               else if(!strcmp(sApplies, "combat"))
                  {
                  SET_BIT(lApplies, (1 << APPLY_STR));
                  SET_BIT(lApplies, (1 << APPLY_CON));
                  SET_BIT(lApplies, (1 << APPLY_DEX));
                  SET_BIT(lApplies, (1 << APPLY_AC));
                  SET_BIT(lApplies, (1 << APPLY_HITROLL));
                  SET_BIT(lApplies, (1 << APPLY_DAMROLL));
                  }
               else if(!strcmp(sApplies, "illegal"))
                  {
                  SET_BIT(lApplies, (1 << APPLY_CLASS));
                  SET_BIT(lApplies, (1 << APPLY_EXP));
                  SET_BIT(lApplies, (1 << APPLY_GOLD));
                  SET_BIT(lApplies, (1 << APPLY_LEVEL));
                  }
               else if(!strcmp(sApplies, "points"))
                  {
                  SET_BIT(lApplies, (1 << APPLY_MANA));
                  SET_BIT(lApplies, (1 << APPLY_HIT));
                  SET_BIT(lApplies, (1 << APPLY_MOVE));
                  }
               else if(!strcmp(sApplies, "saves"))
                  {
                  SET_BIT(lApplies, (1 << APPLY_SAVING_BREATH));
                  SET_BIT(lApplies, (1 << APPLY_SAVING_PARA));
                  SET_BIT(lApplies, (1 << APPLY_SAVING_PETRI));
                  SET_BIT(lApplies, (1 << APPLY_SAVING_ROD));
                  SET_BIT(lApplies, (1 << APPLY_SAVING_SPELL));
                  }
               else if(!strcmp(sApplies, "stats"))
                  {
                  SET_BIT(lApplies, (1 << APPLY_STR));
                  SET_BIT(lApplies, (1 << APPLY_DEX));
                  SET_BIT(lApplies, (1 << APPLY_INT));
                  SET_BIT(lApplies, (1 << APPLY_WIS));
                  SET_BIT(lApplies, (1 << APPLY_CON));
                  SET_BIT(lApplies, (1 << APPLY_CHA));
                  }
	       else if (!strcmp(sApplies, "avgdam")) {
		 showAvgDam = 1;
	       }
               else
                  {
                  send_to_char(ch, "You have declared an invalid apply.  Valid applies are:\r\n");
                  send_to_char(ch, VALID_APPLIES);
                  return;
                  }
               half_chop(buf, sApplies, buf);
               }
            strcpy(buf1, buf);
            strcpy(buf, sApplies);
            if(*buf)
               strcat(buf, " ");
            strcat(buf, buf1);
            break;
         case 'n':
            bName = TRUE;
            half_chop(buf1, sName, buf);
            break;
         case 'p':
            bPosition = TRUE;
            half_chop(buf1, sPosition, buf);
            break;
         case 't':
            bType = TRUE;
            half_chop(buf1, sType, buf);
            break;
         case 'l':
	   strcpy(buf2, buf1);
	   token1 = strtok(buf2, "-");
	   token2 = strtok(NULL, " ");
	   baseLevel = token1 ? atoi(token1) : 0;
	   topLevel = token2 ? atoi(token2) : LVL_IMPL;
	   half_chop(buf1, buf2, buf);
	   break;
         default:
            send_to_char(ch, OSEARCH_FORMAT);
            return;
            }
         }
      else
         {
         send_to_char(ch, OSEARCH_FORMAT);
         return;
         }
      }

   if(bRanged && !(bName || bPosition || bType || bApplies))
      {
      send_to_char(ch, "Ranged searches require further parameters.\r\n");
      return;
      }

   if((bName && strlen(sName) < 1) ||
           (bPosition && strlen(sPosition) < 1) ||
           (bType && strlen(sType) < 1))
      {
      send_to_char(ch, OSEARCH_FORMAT);
      return;
      }

   /* Time to build the header.  */

   if(bApplies)
      sprintbit(lApplies, sHeader2Bits, sHeader2);

   sprintf(sHeader, "       vnum    ");
   if(strlen(sHeader2) <= (MAX_HEADER2_COLUMNS * 4))
      strcat(sHeader, "      object name             ");

   if (showAvgDam) {
     strcat(sHeader, " Avg ");
   }

   strcat(sHeader, sHeader2);
   strcat(sHeader, "\r\n");

   if(bPosition)
      {
      for(iPosition = 1; *wear_bits[iPosition] != '\n'; iPosition++)
         if(isname(sPosition, wear_bits[iPosition]))
            break;
      if(*wear_bits[iPosition] == '\n')
         {
         send_to_char(ch, "You have specified an invalid position.  Valid positions are:\r\n");
         send_to_char(ch, VALID_WEAR);
         return;
         }
      else
         iPosition = (1 << iPosition);
      }

   if(bType)
      {
      for(iType = 0; *item_types[iType] != '\n'; iType++)
         if(isname(sType, item_types[iType]))
            break;
      if(*item_types[iType] == '\n')
         {
         send_to_char(ch, "You have specified an invalid item type. Valid types are:\r\n");
         send_to_char(ch, VALID_TYPES);
         return;
         }
      }

   if(bRanged)
      {
      if(iLow > iHigh)
         {
         iSwap = iLow;
         iLow = iHigh;
         iHigh = iSwap;
         }
      }






   int ids[10000];
   iFound = 0;
   for(iList = 0; iList <= top_of_objt; iList++)
      {
      if((obj_index[iList].vnum < iLow) || (obj_index[iList].vnum > iHigh))
         continue;

      buf[0] = buf2[0] = '\0';
      bMet = FALSE;

      int lr = GET_OBJ_LR(&obj_proto[iList]);
      if (lr < baseLevel || lr > topLevel) {
	continue;
      }


      /*
        //------------------------------------------------------------------------------------
        // Name, position, and type are ALWAYS exclusive if declared as a search parameter.
        // If we've specified one of these criteria and the object failed, boot it and move on.
        // Otherwise, mark the object as good to go and keep going.
        */
      if(bName)
         {
         if(isname(sName, obj_proto[iList].name))
            bMet = TRUE;
         else
            continue;
         }
      if(bPosition)
         {
         if(CAN_WEAR(&obj_proto[iList], iPosition))
            bMet = TRUE;
         else
            continue;
         }
      if(bType)
         {
         if(obj_proto[iList].obj_flags.type_flag == iType)
            bMet = TRUE;
         else
            continue;
         }

      /* The base upon which the rest of the information buffer is built.*/
      sprintf(buf, "%3d. [%5ld] ",
              (iFound + 1), obj_index[iList].vnum);

      /* If we have less than MAX_HEADER2_COLUMNS columns to display, add the name.*/
      if(strlen(sHeader2) <= (MAX_HEADER2_COLUMNS * 4))
         {
         sprintf(buf2, "%-25.25s", obj_proto[iList].short_description);
         strcat(buf, buf2);
         }

      sprintf(buf+strlen(buf)," %4ld",obj_proto[iList].obj_flags.value[4]);

      if (showAvgDam) {
	struct obj_data *obj = &obj_proto[iList];
	if (GET_OBJ_TYPE(obj) == ITEM_WEAPON) {
	  float avgDam = GET_OBJ_VAL(obj, 1) * (1+GET_OBJ_VAL(obj, 2))/2.0f;
	  sprintf(buf2, "%6.1f", avgDam);
	  strcat(buf, buf2);
	  bMet = TRUE;
	} else {
	  continue;
	}
      }

      /* Now for the apply columns.*/
      if(bApplies)
         {
	   if (!showAvgDam) {
	     bMet=FALSE;
	   }
         for(applyNum = 1; applyNum < TOP_APPLY1_NUM; applyNum++)
            {
            if(!IS_SET(lApplies, (1 << applyNum)))
               continue;
            if(applyColumns(iList, applyNum, buf))
               bMet = TRUE;
            }
         }
      strcat(buf, "\r\n");
      if(!bMet)
         continue;
      if((strlen(sObjList) + (strlen(buf) * 2)) > 128000)
         {
         strcat(sObjList, "Too many matches; try limiting your search or setting a vnum range.\r\n");
         break;
         }
      else
         strcat(sObjList, buf);
      ids[iFound++] = iList;
      }
   
   int i;
   sObjList[0] = '\x0';

   if (showAvgDam) {
     int j;
     for (i = 0; i < iFound; i++) {
       for (j = 0; j < iFound-1; j++) {
	struct obj_data *o1 = &obj_proto[ids[j]];
	struct obj_data *o2 = &obj_proto[ids[j+1]];
	float d1 = GET_OBJ_VAL(o1, 1) * (1+GET_OBJ_VAL(o1, 2))/2.0f;
	float d2 = GET_OBJ_VAL(o2, 1) * (1+GET_OBJ_VAL(o2, 2))/2.0f;
	if (d1 < d2) {
	  int k = ids[j];
	  ids[j] = ids[j+1];
	  ids[j+1] = k;
	}
       }
     }
   }

   log("ifound=%d", iFound);

   for(i = 0; i < iFound; i++)
      {
	iList = ids[i];

      if((obj_index[iList].vnum < iLow) || (obj_index[iList].vnum > iHigh))
         continue;

      buf[0] = buf2[0] = '\0';
      bMet = FALSE;

      int lr = GET_OBJ_LR(&obj_proto[iList]);
      if (lr < baseLevel || lr > topLevel) {
	continue;
      }


      /*
        //------------------------------------------------------------------------------------
        // Name, position, and type are ALWAYS exclusive if declared as a search parameter.
        // If we've specified one of these criteria and the object failed, boot it and move on.
        // Otherwise, mark the object as good to go and keep going.
        */
      if(bName)
         {
         if(isname(sName, obj_proto[iList].name))
            bMet = TRUE;
         else
            continue;
         }
      if(bPosition)
         {
         if(CAN_WEAR(&obj_proto[iList], iPosition))
            bMet = TRUE;
         else
            continue;
         }
      if(bType)
         {
         if(obj_proto[iList].obj_flags.type_flag == iType)
            bMet = TRUE;
         else
            continue;
         }

      /* The base upon which the rest of the information buffer is built.*/
      sprintf(buf, "%3d. [%5ld] ",
              (i + 1), obj_index[iList].vnum);

      /* If we have less than MAX_HEADER2_COLUMNS columns to display, add the name.*/
      if(strlen(sHeader2) <= (MAX_HEADER2_COLUMNS * 4))
         {
         sprintf(buf2, "%-25.25s", obj_proto[iList].short_description);
         strcat(buf, buf2);
         }

      sprintf(buf+strlen(buf)," %4ld",obj_proto[iList].obj_flags.value[4]);

      if (showAvgDam) {
	struct obj_data *obj = &obj_proto[iList];
	float avgDam = GET_OBJ_VAL(obj, 1) * (1+GET_OBJ_VAL(obj, 2))/2.0f;
	sprintf(buf2, "%6.1f", avgDam);
	strcat(buf, buf2);
	bMet = TRUE;
      }

      /* Now for the apply columns.*/
      if(bApplies)
         {
	   bMet=FALSE;
         for(applyNum = 1; applyNum < TOP_APPLY1_NUM; applyNum++)
            {
            if(!IS_SET(lApplies, (1 << applyNum)))
               continue;
            if(applyColumns(iList, applyNum, buf))
               bMet = TRUE;
            }
         }
      strcat(buf, "\r\n");
      /*      if(!bMet)
	      continue;*/
      if((strlen(sObjList) + (strlen(buf) * 2)) > 128000)
         {
         strcat(sObjList, "Too many matches; try limiting your search or setting a vnum range.\r\n");
         break;
         }
      else
         strcat(sObjList, buf);
      }

   if(iFound)
      page_string(ch->desc, sObjList, 1, sHeader);
   else
      send_to_char(ch, "No objects found with specified parameters.\r\n");
   }

bool applyColumns(int N, int applyBit, char *sBuf)
   {
   if(IS_SET(obj_proto[N].obj_flags.lApplyBits, (1 << applyBit)))
      {
      char *buf2=get_buffer(128);
      sprintf(buf2, "%4d", obj_proto[N].obj_flags.iApplyMods[applyBit]);
      strcat(sBuf, buf2);
      release_buffer(buf2);
      return(TRUE);
      }
   strcat(sBuf, "   .");
   return(FALSE);
   }
