level = the level at which the spell is cast, not the level of the caster

magic_missile:
mage  LVLd8 + 1
other LVLd6 + 1
PIERCE

chill_touch:
mage  LVLd8 + 1
other LVLd6 + 1
-1 str accum, duration LVL
COLD

burning_hands:
mage  (2+LVL)d8 + 3
other (2+LVL)d6 + 3
FIRE

shocking_grasp:
mage  (3+LVL)d8 + 5
other (3+LVL)d6 + 5
ELECTRICITY

lightning_bolt:
mage  (5+LVL)d8 + 7
other (5+LVL)d6 + 7
ELECTRICITY

color_spray:
mage  (7+LVL)d8 + 9
other (7+LVL)d6 + 9
ENERGY

fireball:
mage  (9+LVL)d8 + 11
other (9+LVL)d6 + 11
FIRE

fire_song:
all   (12+LVL)d14 + 15
FIRE

gas_blast:
all   (18+LVL)d20 + 20
POISON

frost_blast:
all   (13+LVL)d15 + 20
COLD

flame_strike:
all   (12+LVL)d14 + 20
FIRE

acid_blast:
all   (14+LVL)d16 + 20
ACID

dispel_evil:
evil  (vict_hp/(11-level)) -1
neut  (6+LVL)d8 + 6
good  0
DRAIN

dispel_good:
good  (vict_hp/(11-level)) -1
neut  (6+LVL)d8 + 6
evil  0
DRAIN

call_lightning:
all   (5+LVL)d8 + 7
ELECTRICITY

harm:
all   (8+LVL)d8 + 8
ENERGY

enfeeble:
all   (12+LVL)d14 + 20
DRAIN

wrath_of_god:
all   (17+LVL)d19 + 19
FIRE

earthquake:
all    (1+LVL)d8 + LVL
ENERGY (?)
area affect

fire_breath:
all    (14+LVL)d14 + 15
FIRE
area affect

frost_breath:
all    (14+LVL)d14 + 15
COLD

gas_breath:
all    (14+LVL)d14 + 15
POISON
area affect

lightning_breath:
all    (14+LVL)d14 + 15
ELECTRICITY

acid_breath:
all    (14+LVL)d14 + 15
ACID
area affect

armor:
af[0].location = APPLY_AC; 
af[0].modifier = -4*level; 
af[0].duration = 5*level; 
accum_duration = FALSE; 

shield:
af[0].location = APPLY_AC; 
af[0].duration =  5*level; 
af[0].modifier = -4*level; 
accum_duration=FALSE; 

stone_skin:
af[0].location = APPLY_AC; 
af[0].duration = 5*level; 
af[0].modifier = -5*level; 
af[1].location = APPLY_DEX;
af[1].duration = 5*level;
af[1].modifier = -1*(level/2+1);
accum_duration=FALSE; 

bark_skin:
af[0].location = APPLY_AC; 
af[0].duration =  5*level; 
af[0].modifier = -4*level; 
af[1].location = APPLY_DEX;
af[1].duration = 5*level;
af[1].modifier = -1*(level/4+1);
accum_duration=FALSE; 

bless:
af[0].location = APPLY_HITROLL; 
af[0].modifier = level/3+1; 
af[0].duration = 5*level; 
af[1].location = APPLY_SAVING_SPELL; 
af[1].modifier = -1*(1+level/3); 
af[1].duration = 5*level; 
accum_duration = FALSE; 

blindness:
af[0].location = APPLY_HITROLL; 
af[0].modifier = -4; 
af[0].duration = level/2+1; 
af[0].bitvector = AFF_BLIND; 
 
af[1].location = APPLY_AC; 
af[1].modifier = 40; 
af[1].duration = level/2+1; 
af[1].bitvector = AFF_BLIND; 

curse:
af[0].location = APPLY_HITROLL; 
af[0].duration = 1 + level*2; 
af[0].modifier = -1*(1+level/2); 
af[0].bitvector = AFF_CURSE; 
 
af[1].location = APPLY_DAMROLL; 
af[1].duration = 1 + level*2; 
af[1].modifier = -1*(1+level/2); 
af[1].bitvector = AFF_CURSE; 
 
accum_duration = TRUE; 
accum_affect = TRUE; 

detect_align:
af[0].duration =level*5; 
af[0].bitvector = AFF_DETECT_ALIGN;
accum_duration = FALSE; 

detect_invis:
af[0].duration =level*5; 
af[0].bitvector = AFF_DETECT_INVIS;
accum_duration = FALSE; 

detect_magic:
af[0].duration =level*5; 
af[0].bitvector = AFF_DETECT_MAGIC;
accum_duration = FALSE; 

infravision: 
af[0].duration = level*5; 
af[0].bitvector = AFF_INFRAVISION; 
accum_duration = FALSE;

invisible:
af[0].duration = level*5; 
af[0].modifier = -40; 
af[0].location = APPLY_AC; 
af[0].bitvector = AFF_INVISIBLE; 
accum_duration = FALSE; 

poison:
af[0].location = APPLY_STR; 
af[0].duration = level*3; 
af[0].modifier = -1*(1+level/3); 
af[0].bitvector = AFF_POISON; 

protection_from_evil:
af[0].duration = level*2; 
af[0].bitvector = AFF_PROTECT_EVIL; 
accum_duration = FALSE; 

sanctuary:
af[0].duration = 1+level; 
af[0].bitvector = AFF_SANCTUARY; 
accum_duration = FALSE; 

haste:
af[0].duration = level; 
af[0].bitvector = AFF_HASTE; 
accum_duration = FALSE; 

blur:
af[0].bitvector = AFF_HASTE; 
af[0].duration = level+3; 
accum_duration = FALSE; 

slow: 
af[0].bitvector = AFF_SLOW; 
af[0].duration = level; 

depression: 
af[0].bitvector = AFF_SLOW; 
af[0].duration = level+3; 

wither: 
af[0].bitvector = AFF_SLOW; 
af[0].duration = level+3; 
af[1].bitvector = AFF_SLOW; 
af[1].location = APPLY_STR; 
af[1].duration = level+3; 
af[1].modifier = -1*(1+level/2); 

sleep:
af[0].duration = level; 
af[0].bitvector = AFF_SLEEP; 

lullaby:
af[0].duration = 4 + level; 
af[0].bitvector = AFF_SLEEP; 

strength: 
af[0].location = APPLY_STR; 
af[0].duration = level*3; 
af[0].modifier = (level/3)+1; 
accum_duration = FALSE; 
accum_affect = FALSE; 

enhanced_strength:
af[0].location = APPLY_STR; 
af[0].duration = level*2; 
af[0].modifier = level+3; 
accum_duration = FALSE; 
accum_affect = FALSE; 

sense_life:
af[0].duration = level*5; 
af[0].bitvector = AFF_SENSE_LIFE; 
accum_duration = FALSE; 

waterwalk:
af[0].duration = level*5; 
af[0].bitvector = AFF_WATERWALK; 
accum_duration = FALSE; 

water_breathe:
af[0].bitvector = AFF_WATER_BREATHE; 
af[0].duration = level*5; 
accum_duration = FALSE; 

levitate:
af[0].bitvector = AFF_LEV; 
af[0].duration = level*5; 
accum_duration = FALSE; 

fly:
af[0].location = APPLY_FLY; 
af[0].duration = level*5; 
af[0].modifier = PRF2_FLY;
accum_duration = FALSE; 

dream_sight:
af[0].bitvector = AFF_DREAM; 
af[0].duration = level*5; 

dragon: 
af[0].location = APPLY_HITROLL; 
af[0].duration = 5; 
af[0].modifier = 3+level/2; 
af[1].location = APPLY_DAMROLL; 
af[1].duration = 5; 
af[1].modifier = 3+level/2; 

eagle_claw: 
af[0].location = APPLY_HITROLL; 
af[0].duration = 5; 
af[0].modifier = 4+level/2; 
af[1].location = APPLY_DAMROLL; 
af[1].duration = 5; 
af[1].modifier = 2+level/2; 

inspire:
af[0].location = APPLY_DAMROLL; 
af[0].duration = level*3; 

pixie_dust: 
af[0].location = APPLY_HITROLL; 
af[0].duration = level; 
af[0].modifier = -1*level; 
af[1].location = APPLY_DAMROLL; 
af[1].duration = level; 
af[1].modifier = -1*level; 

dispel_magic:
allowable if(lvl_caster+LVL > lvl_vict)

pass_door: 
af[0].bitvector = AFF_PASS_DOOR; 
af[0].duration = level; 

plague: 
af[0].bitvector = AFF_PLAGUE; 
af[0].duration = 80; 
accum_duration=TRUE; (!!!!!!)

cure_light:
+hp LVLd8+1

cure_serious:
+hp (2*LVL)d8 + 20

cure_critic:
+hp (3*LVL)d8 + 30

heal:
+hp (4*LVL)d8 + 100

give_life:
+hp   ch_mana/(11-LVL)
-mana ch_mana/(11-LVL)

refresh:
+mv  LVL*10

energy:
+mana LVL*10

energy_drain:
+mana ch_mana (LVL*10)
-mana vict_mana (LVL*10)

create_food:
LVL mushrooms

create_water:
LVL bottles

create_light: 
LVL*4 hrs of light
ball of light

continual_light: 
LVL*12 hrs of light
glow staf

portal:
((lvl-1)*2) +1 hrs duration

summon:
ch_level+(2*LVL) is max level summonable

locate_object:
level*2 items listed

charm:
(LVL*10) is max level charmable

enchant_weapon:
hitroll (LVL+1)/2
damroll (LVL)/2
+5+5 is best

enchant_armor:
LVL1  -1
LVL2  -2
LVL3  -3
LVL4  -3
LVL5  -4
LVL6  -4
LVL7  -5
LVL8  -5
LVL9  -6
LVL10 -7
