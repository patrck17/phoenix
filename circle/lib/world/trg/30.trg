#3000
Newbie Town Set~
0 n 100
~
* Nomikos, Active, 07-10-2002
* If you want to add more cities, do so in the manner below.
wait 1
set my_room %self.room%
eval my_zone %my_room% / 100
switch %my_zone%
  case 30
    set StartingCity Heliopolis
    break
  case 176
    set StartingCity Naraka
    break
  case 120
  case 121
  case 122
  case 123
    set StartingCity Silverport
    break
  case 10
  case 11
    set StartingCity Aethelfyrd
    break
  case 101
    set StartingCity Mordilnia
    break
  case 91
  case 92
    set StartingCity New Haven
    break
  case 140
  case 141
    set StartingCity Keorc Koilos
    break
  case 54
  case 55
    set StartingCity New Thalos
  case 233
    set StartingCity Ixchal
    break
  default
    set StartingCity BoogerVille
done
global StartingCity
~
#3001
Atlon Sample~
0 g 30
~
* Unknown/Alta, Active, 10/08/2004
if %actor.vnum% == -1
  if %actor.level% >= 50
    wait 1
    %echoaround% %actor% Atlon shakes %actor.name%'s hand.
    %send% %actor% Atlon shakes your hand.
    wait 1
    say How may I help you?
  else
    wait 2
    %echoaround% Atlon offers %actor.name% a free sample of his fine pastry.
    mload obj 3087
    give crumb %actor.name%
    say Now, little one, how may I help you?
  end 
end
~
#3002
Helper Greet~
0 e 0
has entered the game.~
* Unknown/Nomikos, Active, 07/10/2002
if %actor.level% == 1
  mforce %actor% color complete
end
wait 4
if ("%actor.name%" != "") && (%actor.level% <= 16)
  say Greetings %actor.name%, I'm here to help you.  Just (say help) if you find yourself in need.
end
~
#3003
Helper Trig2~
0 d 100
help~
* Unknown/Nomikos, Active, 07/10/2002
if (%actor.level% <= 16) & (%actor.vnum% == -1)
  wait 1
  say I can provide you with some starting help, %actor.name%.
  wait 2
  say Welcome to the realm of the Phoenix, and the city of %StartingCity%,
  say although I'm sure you know the name by now.  In your journeys, you will
  say need to work hard and defeat many foes to gain in expertise and levels.
  say %StartingCity% has a Guild of %actor.class%s.  Just (look map) to see
  say where your guild is.
  wait 5
  say I know of several hidden NEWBIE places where one such as yourself
  say might gain experience, just (say areas) if you wish to know more.
  wait 5
  say I can familiarize you with some of our help files.  Just (say rules), and
  say I will gladly bring you up to date.  If you say need some common advice,
  say please (say advice).
  wait 2
  say Finally, if you have any questions, you can ask other players by using the "newbie" command and channel.
elseif %actor.vnum% == -1
  wait 1
  %echoaround% %actor% %self.name% smirks at %actor.name%, saying "You need mental help, that's what you need."
  %send% %actor% %self.name% says "Come now, %actor.name%.  You no longer need my advice."
end
~
#3004
Helper Rules~
0 d 100
rules~
* Unknown/Nomikos, Active, 07/10/2002
if (%actor.level% <= 16) && (%actor.vnum% == -1)
  wait 1
  say Here are some help files you should read closely.
  wait 2
  mechoaround %actor% %self.name% shows %actor.name% a list of help files.
  msend %actor% To ensure your name is legal, type (help name).
  msend %actor% You should read the policy to learn the rules for the mud. (policy)
  msend %actor% To learn about recent additions to the mud, type (news).
  msend %actor% To see a list of common commands, type (commands).
  msend %actor% Typing (help) will show a list of topics.  You can then type (help topic)
  msend %actor% If you need further help, feel free to ask an immortal for guidance (commune).
elseif %actor.vnum% == -1
  msend %actor% You should have learned this long before now.   
  msend %actor% Perhaps, I should find someone to demote you to level 1!
  mechoaround %actor% %self.name% thinks %actor.name% should be demoted to level 1!
end
~
#3005
Helper Areas~
0 d 100
areas~
* Nomikos, Active, 07/10/2002
if %actor.vnum% == -1
wait 2
if (%actor.level% >= 11) && (%actor.level% <= 16)
  say I can take you to the village of Briarhurst, which is hidden
  say in a remote corner of the world.  Just &M(say take me there)&n.
  say if you wish to go.
elseif (%actor.level% >= 6) && (%actor.level% <= 10)
  say There is a special library in the realms that only I can take
  say you to.  If you wish to go, &M(say take me there)&n.
elseif (%actor.level% >= 1) && (%actor.level% <= 5)
  say There is a courtyard and woodland that I can take you to.
  say Just &M(say take me there)&n, and it will be done.
else
  say Go away %actor.name%, you big oaf!
end
end
~
#3006
Helper Transport~
0 d 0
take me there~
* Unknown/Nomikos, Active, 07/10/2002
wait 2
if (%actor.level% >= 1) && (%actor.level% <= 5)
  set dest 20100
elseif (%actor.level% >= 6) && (%actor.level% <= 10)
  set dest 20000
elseif (%actor.level% >= 11) && (%actor.level% <= 16)
  set dest 20152
else
  halt
end
msend %actor% %self.name% chants for a moment, creating a portal, which immediately draws you inward.
mechoaround %actor% %self.name% chants for a moment, creating a portal, which immediately surrounds %actor.name%.
wait 1
mteleport %actor% %dest%
mforce %actor% look
wait 1
msend %actor% Be watchful for hidden doorways into the woodland.
msend %actor% Read carefully, and you may find many hidden 'treasures' in this place.
msend %actor% Type recall when you wish to return to the temple.
~
#3007
Helper Advice~
0 d 100
advice~
* Unknown/Nomikos, Active, 07/10/2002
if (%actor.level% <= 16) && (%actor.vnum% == -1)
  wait 1
  say Here are some bits of advice to remember when beginning your adventure.
  wait 2
  mechoaround %actor% %self.name% gives %actor.name% a bit of friendly advice.
  msend %actor% If you get lost, type &M(recall)&n to return to this spot (only til level 15).
  msend %actor% If you need to rest, the temple north of this room is a good choice.
  msend %actor% If you need to leave the game &M(camp)&n, the Phoenix to the south will watch over you.
  msend %actor% If you want to increase your level, find your guildmaster on your map, and type &M(gain)&n.
  msend %actor% The donation room, to the east, may provide you with some basic equipment.
  msend %actor% Lastly, always remember the things your mother taught you."
elseif %actor.vnum% == -1
  wait 1
  msend %actor% %self.name% says, 'Even if you took advice, you cant teach an old dog new tricks.
  mechoaround %actor% %self.name% gives %actor.name% some fatherly advice.
end
~
#3008
Atlon Randoms~
0 ab 100
25~
* Unknown/Alta, Active, 10/08/2004
switch %random.4%
case 1
  : takes a tray of rolls from the oven, and places it on the counter.
  break
case 2
  : pops a tray of tasty-looking pastries into the oven.
  break
case 3
  : pats the flour from his hands onto his apron.
  break
case 4
  : takes a wooden broom and sweeps up some spilled flour.
break
  sneeze
  break
done
~
#3009
Atlon Talk~
0 d 1
job work~
* Unknown/Alta, Active, 10/08/2004
say Well %actor.name%, i'm the best baker in Heliopolis.
wait 1
say I bake fresh breads and pastries for the townsfolk.
wait 1
say If you'd like to buy some baked goods, type 'list' for a menu.
~
#3010
Church Bells~
2 ab 100
~
if %time.hour% == 6
  %echo% The temple bells chime, heralding in a new day.
  wasound The temple bells chime, heralding in a new day.
  wait 4
  %echo% *ding* *ding* 
  wasound *ding* *ding*
  wait 2
  %echo% *dong* *clang*
  wasound *dong* *clang*
  wait 2
  %echo% *bong* .... *bong*
  wasound *bong* .... *bong*
  wait 80
end
~
#3011
Bard Songs~
0 bm 15
~
switch %random.4%
  case 1
    %echo% The Bard begins a tune...
    wait 2
    %echo% "Oh bird of flame, from time long ago"
    wait 2
    %echo% "From paradise you come, but to where do you go?"
    wait 2
    %echo% "Your children are here, and wait for that day"
    wait 2 
    %echo% "When you will return, and show them the way"
    wait 100
    break
  case 2
    %echo% The bard starts to strum his harp
    wait 2
    %echo% "When the moon lights the land, and the stars dance above"
    wait 2
    %echo% "Young couples grasp tightly, and thoughts turn to love"
    wait 2
    %echo% "Not dragons nor demons or haunts of the land"
    wait 2
    %echo% "Can threaten the power of a union so grand"
    wait 100
    break
  case 3
    %echo% The bard clears his throat and starts to sing
    wait 2
    %echo% "Sweet misery, where have you gone?"
    wait 2
    %echo% "My days are filled with happiness, and I can't go on"
    wait 2
    %echo% "If only hard times would come again"
    wait 2
    %echo% "Then I could dispatch with this silly grin"
    wait 100
    break
  case 4
    %echo% The bard begins to tap his foot, setting a beat.
    wait 2
    %echo% "The Knights are marching two by two"   
    wait 2
    %echo% "hoo-ra   hoo-ra!"
    wait 2
    %echo% "They're fighting evil day and night "
    wait 2
    %echo% "hoo-ra   hoo-ra!"
    wait 2
    %echo% "When chaos rears it's evil head"
    wait 1
    %echo% "They'll lift their swords and smite them dead!"
    wait 2
    %echo% "Hoo-ra  Hoo-ra!"
    wait 100
    break
  default
    %echo% The bard tunes his harp in prepartation for his next song.
    break
done
~
#3012
Malebolge !disable~
0 c 100
sweep swee swe trip tri bash bas stun stu~
if (%actor.position% != fighting)
switch %arg%
  case tank
  case target
  case malebolge
  case malebolg
  case malebol
  case malebo
  case maleb
  case male
  case mal
  case man
  case ma
  case m
  case old
  case ol
  case o
    emote laughs wickedly!
    say Who do you think you are dealing with?  Ha ha ha!
    break
  default
    return 0
    break
done
else
  emote laughs at %actor.name%.
  say Who do you think you are dealing with?  Ha ha ha!
end
~
#3013
Potion Seller Script~
0 j 100
~
context 1
if (%object.name% /= potion) && (%object.val0% == 10)
  wait 1
  set index 1
  extract current_vnum %index% %vnum_list%
while %current_vnum%
    if %current_vnum% == %object.vnum%
      set found_vnum %object.vnum%
    end
    eval index %index% + 1
    extract current_vnum %index% %vnum_list%
  done
  if %found_vnum%
    context %found_vnum%
    if %Oamount% >= 10
      set Too_many true
    else
      eval Oamount %Oamount% + 1
      global Oamount
    end
    context 1
  else
    set vnum_list %vnum_list% %object.vnum%
    global vnum_list
    context %object.vnum%
    set Oname %object.shortdesc%
    global Oname
    set Oamount 1
    global Oamount
    context 1
  end
  context %object.vnum%
  if %Too_many%
    say I am overstocked with those potions.  Please, come back later.
    drop %object.name%
    unset Too_many
  else
    say Ahh, %object.shortdesc%.  I offer 100 coins for that!
    nop %actor.gold(100)%
    wait 2
    say Come back with other interesting potions you may find.
  end
elseif %object.val0% < 10
  wait 1
  say What is this??  I accept only the BEST of potions.  Come back when you have the goods, small stuff!
  drop %object.name%
else
  wait 1
  say What would I want with this garbage?  %object.shortdesc%?  It's not even a potion!
  drop %object.name%
end
~
#3014
Potion List Script~
0 c 100
list~
context 1
if !%vnum_list%
  msend %actor% Currently, there is nothing for sale.
else
  msend %actor% ##   Available   Item                                              Cost
  msend %actor% -------------------------------------------------------------------------
  set index 1
  extract current_item %index% %vnum_list%
while %current_item%
    set display %index%)
    context %current_item%
      set n 1
      eval length 50 - %Oname.strlen%
      set spacer .
while %n% < %length%
        set spacer %spacer%.
        eval n %n% + 1
      done
      if %index% < 10
        set pre_spacer ..
      else
        set pre_spacer .
      end
      msend %actor% %pre_spacer%%display%     %Oamount%     %Oname% %spacer% 100
    context 1
    eval index %index% + 1
    extract current_item %index% %vnum_list%
  done
  msend %actor% -------------------------------------------------------------------------
end
~
#3015
Malebolge React to Throw~
0 e 0
flies in from the~
if (%arg% == malebolge || malebolg || malebol || malebo || male || mal || man || ma || m || old || ol || o)
mecho Malebolge begins an evil laugh!
mecho Malebolge says, 'Just who do you think you are dealing with?  Ha ha ha!'
end
~
#3016
Guildmaster Newbie Direct~
0 c 100
g ga gai gain~
*Unknown/Alta, Active, 02/20/2005
if (%actor.vnum% == -1) && ((%actor.class% == %self.class%) && (%actor.class% != Deva) && (%actor.class% != Assassin) && (%actor.class% != Kensai) && (%actor.class% != Necromancer))
  if (%actor.level% > 10) && (%actor.level% <= 20)
    say I have taught you all I know, %actor.name%.  Seek out the guildmaster in Sundhaven, to the east.
  elseif (%actor.level% > 20) && (%actor.level% <= 30)
    say You should be practicing in Mordilnia now, %actor.name%.
    wait 1
    emote shakes his head, muttering something about foolish %actor.race%s.
  elseif (%actor.level% > 30) && (%actor.level% < 104)
    say I can teach you nothing, %actor.name%.  You must seek out a more skilled guildmaster.
  else
    return 0
  end
else
  return 0
end
~
#3019
Guild Guard (mage/cleric)~
0 c 100
n no nor nort north e ea eas east~
* Nomikos, Active, 06/28/2002
* Make sure we're not invis or visible imms
if ("%actor.level%" != "") && (%actor.level% < 104)
* For clerics
  if %cmd.mudcommand% == north
    if (%self.vnum% == 3025) && (%self.room% == 3004) && (%actor.class% != cleric)
      say This be the guild of Clerics.  Be off with ye, %actor.class%!
    else
      return 0
    end
* Mage Guild
  elseif %cmd.mudcommand% == east
    if (%self.vnum% == 3024) && (%self.room% == 3017) && (%actor.class% != magic user)
      say Only mages are permitted entry to this place, silly %actor.class%!
    else
      return 0
    end
* Anyone else
  else
    return 0
  end
else
  return 0
end
~
#3020
Feather Mantle Fly~
1 b 2
~
* Nomikos, Active, 8-29-2002
set player %self.worn_by%
if (%player.vnum% == -1)
  osend %player% The weight of the mantle lifts from your shoulders.
  oechoaround %player% The weight of the Mantle lifts from @%player.name% shoulders.
  dg_cast 10'fly' %player.name%
end
~
#3021
Malebolge Shoot~
2 c 100
sho shoo shoot~
* Unknown, Active, 02/20/2005
if %arg%
  %echo% %actor.name%, I don't think Malebolge would appreciate that...
end
~
#3022
Malebolge Throw~
2 c 100
th thr thro throw~
* Unknown, Active, 02/20/2005
if %arg%
  %echo% %actor.name%, I don't think Malebolge would appreciate that...
end
~
#3023
Phoenix Fight~
0 k 20
~
* Alighieri/Nomikos, Active, 08/13/2001
eval odds %random.6%
switch %odds%
  case 1
    * See how much damage the phoenix has taken
    * If she has more then 25% of hit points, attack, otherwise heal
    eval hitPercent %self.hitp% * 100 / %self.maxhitp
    if hitPercent < 25
      dg_cast 6 'heal' phoenix
    else
      dg_cast 8 'wrath' %actor.name%
    end
    break
  case 2
    if %self.room% == 3014
      * This is the load room for the Phoenix
      eval dir %random.4%
      * Toss the tank one of four directions
      switch %dir%
        case 1
          * Teleport north
          mteleport %actor% 3001
          break
        case 2
          * Teleport west
          mteleport %actor% 3012
          break
        case 3
          * Teleport south
          mteleport %actor% 3025
          break
        case 4
          * Teleport north
          mteleport %actor% 3016
          break
        default
          * Should never happen
      done
      dg_cast 6 'heal' phoenix
      %echo% With a mighty flex of her great talons, the Phoenix hurls %actor.name% from the room!
      msend %actor% The Phoenix clamps you in her mighty talons, and hurls you from the battle!
    else
      dg_cast 8 'wrath' %actor.name%
    end
    break
  case 3
  case 4
  case 5
    set preflist Cleric Magic Necromancer Deva Druid Bard Ranger Monk Paladin Thief
    set my_room %self.room%
    set current_opponent %self.fighting%
    set i 1
    extract current_class %i% %preflist%
    set found no
    while (%current_class% != ) && (%found% == no)
      eval current_victim %my_room.people%
      while (%current_victim% != ) && (%found% == no)
        if (%current_victim.vnum% == -1) && (%current_victim% != %current_opponent%) && (%current_victim.position% == fighting)
          set victim_class %current_victim.class%
          if %victim_class.car% == %current_class%
            set enemy %current_victim.fighting%
            if %enemy.name% == %self.name%
              set found yes
              redirect %current_victim.name%
              mtransform %self.vnum%
            end
          end
        end
        eval current_victim %current_victim.next_in_room%
      done
      eval i %i% + 1
      extract current_class %i% %preflist%
    done
    if %found% == no
      dg_cast 8 'wrath' %actor.name%
    end
    break
  default
    * should be auto-cast on tank
    dg_cast 8 'wrath' %actor.name%
done
~
#3024
Player Battlefield 1~
0 d 100
battle~
* Unknown, Active, 02/20/2005
eval amt %actor.level%*10
wait 1
emote sizes up %actor.name%.
wait 1
say For someone of your caliber, %actor.name%, it shall cost you %amt% to battle.
end
~
#3025
Player Battlefield 2~
0 m 100
10~
* Unknown, Active, 02/20/2005
eval bribe %actor.level%*10
eval bribe %actor.level%*10
if %amount% < %bribe%
  wait 1
  say Nice try little one.  I'll be drinking on you tonight!
else 
  wait 1
  say Thank you, %actor.name%.  Prepare  for battle!
  wait 2
  %teleport% %actor% 787
  %echo% The room shimmers for a moment.
end
~
#3026
Player Stealing~
0 c 100
ste stea steal~
* Aeneas: removed st from arguements
wait 1
emote looks at you and scowls.
say Now, that was a mistake!
%damage% %actor.name% 5
end
~
#3027
Take Battlefield Ticket~
0 j 100
~
* Unknown, Active, 02/20/2005
if %object.vnum% == 1263
  wait 1
  say Thank you.  Prepare yourself for battle.
  %echo% The room shimmers for a moment.
  %teleport% %actor.name% 787
  junk ticket
end
~
#3028
Pew Load~
2 f 100
~
* Manok/Apia, Active, 02/08/2004
if %self.contents%
  set pew %self.contents%
  while %pew%
  if %pew.vnum%==17800
    %purge% obj 17800
    %load% obj 17800
  end
  set pew %pew.next_content%
done
else
  %load% obj 17800
end
~
#3047
Heart of a Noob~
1 b 100
~
set wearer %self.worn_by%
if (!%wearer%) || (%wearer.level% > 20)
  %echo% No longer needed, the heart of a noob vanishes in a puff of smoke!
  wait 2
  %echo% Congratulations!
  %purge% self
end
~
#3080
Brush Mount Before Stabling~
0 c 100
brush bru brus stabl stable~
* Unknown, Active, 02/20/2005
context %actor.id%
if (%arg%&&(legs/=%arg%)&&(brush/=%cmd%))
  %send% %actor% You begin to brush your mount's legs.
  %echoaround% %actor% %actor.name% brushes $s mount's legs.
  set brush2 yep
  global brush2
elseif (%arg%&&(brush/=%cmd%) && ((feet/=%arg%) || (hooves/=%arg%)|| (hoofs/=%arg%)))
  %send% %actor% You begin to brush your mount's feet.
  %echoaround% %actor% %actor.name% brushes $s mount's feet.
  set brush3 yep
  global brush3
elseif brush/=%cmd%
  %send% %actor% You begin to brush your mount.
  %echoaround% %actor% %actor.name% brushes $s mount.
  set brush1 yep
  global brush1
end
if %cmd.mudcommand%==stable
  if %brush1%&&%brush2%&&%brush3%
    return 0
    if %arg%
      unset brush3
      unset brush2
      unset brush1
    end
  elseif %brush1%&&%brush2%
    sa You forgot the feet, silly!
  elseif %brush1%
    sa What about the legs?
  elseif
    sa You need to brush down your mount before I can stable it!
    wait 1
    sa Silly!
  end
end
~
#3081
unset for Velvet~
0 g 100
~
say My trigger commandlist is not complete!
~
#3090
follow new master~
0 c 100
pet~
* Apia, Active, 12/2001
return 0
set mast %self.master%
if %mast.vnum%!=(-1)
  wait 1
  %echoaround% %actor% %self.name% rubs gently against %actor.name%'s leg.
  %send% %actor% %self.name% rubs gently against your leg.
  wait 1
  follow %actor.name%
end
~
#3093
Narakan Portalmaster Ask Price~
0 d 100
portal~
wait 1
eval amount %actor.level% * 90
say %actor.name%, it will cost you %amount% coins for a trip through the portal.
~
#3094
Bribe Narakan Portalmaster~
0 m 1
~
if %actor.vnum% > -1
  halt
end
wait 1
eval total %actor.level% * 90
* Check that they paid enough.
if %amount% < %total%
  sigh
  wait 2
  say %actor.name%, you underpaid... try again?
else
  say Thank you, %actor.name%.  In you go!
  wait 2
  emote pushes %actor.name% through the portal.
  wait 1
  * Portal works for players under level 20 or those who roll under 99...
  eval roll %random.100%
  if (%actor.level% < 20) || (%roll% < 99)
    set target 17600
  else
    * Oops!  Pick a random room along the way to Naraka...
    %echo% Whoops!
    eval choice %random.12%
    switch %choice%
    case 1
      set target 5616
      break
    case 2
      set target 3249
      break
    case 3
      set target 1054
      break
    case 4
      set target 4224
      break
    case 5
      set target 18262
      break
    case 6
      set target 18114
      break
    case 7
      set target 9417
      break
    case 8
      set target 12271
      break
    case 9
      set target 14480
      break
    case 10
      set target 17984
      break
    case 11
      set target 17832
      break
    case 12
      set target 15996
      break
    default
      set target 3097
      break
    done
  end
  %teleport% %actor% %target%
  %force% %actor% look
  DG_log %actor.name% paid %amount% to room %target%.
end
~
#3095
Coachman trigger~
0 abcm 50
l lo loo look~
*****in progress Apia 1/05/04
if %amount%
say Why thankee Gov, step handy now...
%send% %actor% The Coachman helps you into the Carriage
%echoaround% %actor% %actor.name% gets into the Sundhaven Post Carriage.
%teleport% %actor% 3098
%echoaround% %actor% %actor.name% has boarded the coach.
else
if (%time.hour%==4 || %time.hour%==8 || %time.hour%==12 || %time.hour%==16 || %time.hour%==20)
say Last Call for the Post Carriage!
wait 10
%echo% The Coachman swings himself up onto the Carriage and takes the reins.
wait 1
say Cha!
wait 1
%echo% The Sundhaven Post Carriage starts on its journey.
set start_room %self.room%
%teleport% %self% 3098
%echo% The Coachman sticks his head in the window.
if %start_room%==3016
init 1
elseif %start_room%==8440
init 26
end
startcarriage
sa All aboard?  Let's roll!
%purge% %self%
else
eval speak %random.5%
if %speak%==1 || ((coachman/=%arg%) && %arg%)
sa Anyone for the Sundhaven Post Coach? Only 50 coins!
end
end
end
return 0
~
#3096
carriage commands~
2 c 100
*~
*****in progress Apia 1/5/04
if %cmd.mudcommand%==look
if !%arg%
return 0
elseif window/=%arg%
%send% %actor% Outside the Coach window you see....
%teleport% %actor% %current_room%
%force% %actor% look
%teleport% %actor% 3098
elseif north/=%arg% || south/=%arg% || east/=%arg% || west/=%arg%
%send% %actor% Outside the Coach window you see....
%teleport% %actor% %current_room%
%force% %actor% l %arg%
%teleport% %actor% 3098
else
return 0
end
elseif %cmd.mudcommand%==north || %cmd.mudcommand%==south
if %actor.position%==standing
%echoaround% %actor% %actor.name% hops out of the coach.
%teleport% %actor% %current_room%
%echoaround% %actor% %actor.name% hops out of the Sundhaven Post Coach
%send% %actor% You hop out of the coach.
wait 1
%force% %actor% l
else
%send% %actor% Maybe you should get on your feet first?
end
else
return 0
end
~
#3097
temp carriage init~
2 f 100
~
*****in progress Apia 1/05/04
if %Coachman% || %Coach%
halt
else
eval stop 1
global stop
wat 3016 %load% m 3085
eval current_room 3016
global current_room
end
~
#3098
carriage route~
2 c 100
startcarriage~
****in progress Apia 1/5/04****
set Coach running
global Coach
unset coachman
set path 3016 3041 3053 3022 3018 7402 7401 7400 5100 5101 5102 5103 5104 5105 5108 5109 5106 5110 5111 5112 5107 7548 8527 8521 8445 8440 
if %stop%==1
while %stop%<26
wait 8
eval stop (%stop%+1)
extract current_room %stop% %path%
%echo% The Coach rumbles along....
wat %current_room% %echo% The Sundhaven Post rumbles by on its journey to Sundhaven....
global current_room
%echo% The Coachman yells....&Y'passing %current_room.name%.'&n
global stop
done
%load% m 3085
set Coachman loaded
global Coachman
%echo% The Coachman appears in the window.
%force% Coachman sa Here we are! Sundhaven!
%teleport% Coachman %current_room%
wat %current_room% %echo% The Sundhaven Post Coach rumbles to a stop and the Coachman steps down.
   elseif %stop%==26
while %stop%>1
wait 8
eval stop (%stop%-1)
extract current_room %stop% %path%
global current_room
%echo% The coach rumbles along....
wat %current_room% %echo% The Sundhaven Post rumbles by on its way to Heliopolis....
%echo% The Coachman yells....&Y'passing %current_room.name%.'&n
global stop
done
%load% m 3085
set Coachman loaded
global Coachman
%echo% The Coachman sticks his head in the window.
%force% Coachman sa Here we be...last stop! Heliopolis!
%teleport% Coachman %current_room%
wat %current_room% %echo% The Sundhaven Post Coach rumbles to a stop and the Coachman steps down.
end
unset Coach
~
#3099
new trigger~
0 g 100
~
dg_cast 'conj' 10
~
$~
