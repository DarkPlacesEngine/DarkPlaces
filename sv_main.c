/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// sv_main.c -- server main program

#include "quakedef.h"
#include "sv_demo.h"
#include "libcurl.h"
#include "csprogs.h"

static void SV_SaveEntFile_f(void);
static void SV_StartDownload_f(void);
static void SV_Download_f(void);
static void SV_VM_Setup(void);
extern cvar_t net_connecttimeout;

void VM_CustomStats_Clear (void);
void VM_SV_UpdateCustomStats (client_t *client, prvm_edict_t *ent, sizebuf_t *msg, int *stats);

cvar_t sv_worldmessage = {CVAR_READONLY, "sv_worldmessage", "", "title of current level"};
cvar_t sv_worldname = {CVAR_READONLY, "sv_worldname", "", "name of current worldmodel"};
cvar_t sv_worldnamenoextension = {CVAR_READONLY, "sv_worldnamenoextension", "", "name of current worldmodel without extension"};
cvar_t sv_worldbasename = {CVAR_READONLY, "sv_worldbasename", "", "name of current worldmodel without maps/ prefix or extension"};

cvar_t coop = {0, "coop","0", "coop mode, 0 = no coop, 1 = coop mode, multiple players playing through the singleplayer game (coop mode also shuts off deathmatch)"};
cvar_t deathmatch = {0, "deathmatch","0", "deathmatch mode, values depend on mod but typically 0 = no deathmatch, 1 = normal deathmatch with respawning weapons, 2 = weapons stay (players can only pick up new weapons)"};
cvar_t fraglimit = {CVAR_NOTIFY, "fraglimit","0", "ends level if this many frags is reached by any player"};
cvar_t gamecfg = {0, "gamecfg", "0", "unused cvar in quake, can be used by mods"};
cvar_t noexit = {CVAR_NOTIFY, "noexit","0", "kills anyone attempting to use an exit"};
cvar_t nomonsters = {0, "nomonsters", "0", "unused cvar in quake, can be used by mods"};
cvar_t pausable = {0, "pausable","1", "allow players to pause or not"};
cvar_t pr_checkextension = {CVAR_READONLY, "pr_checkextension", "1", "indicates to QuakeC that the standard quakec extensions system is available (if 0, quakec should not attempt to use extensions)"};
cvar_t samelevel = {CVAR_NOTIFY, "samelevel","0", "repeats same level if level ends (due to timelimit or someone hitting an exit)"};
cvar_t skill = {0, "skill","1", "difficulty level of game, affects monster layouts in levels, 0 = easy, 1 = normal, 2 = hard, 3 = nightmare (same layout as hard but monsters fire twice)"};
cvar_t slowmo = {0, "slowmo", "1.0", "controls game speed, 0.5 is half speed, 2 is double speed"};

cvar_t sv_accelerate = {0, "sv_accelerate", "10", "rate at which a player accelerates to sv_maxspeed"};
cvar_t sv_aim = {CVAR_SAVE, "sv_aim", "2", "maximum cosine angle for quake's vertical autoaim, a value above 1 completely disables the autoaim, quake used 0.93"};
cvar_t sv_airaccel_qw = {0, "sv_airaccel_qw", "1", "ratio of QW-style air control as opposed to simple acceleration; when < 0, the speed is clamped against the maximum allowed forward speed after the move"};
cvar_t sv_airaccel_qw_stretchfactor = {0, "sv_airaccel_qw_stretchfactor", "0", "when set, the maximum acceleration increase the player may get compared to forward-acceleration when strafejumping"};
cvar_t sv_airaccel_sideways_friction = {0, "sv_airaccel_sideways_friction", "", "anti-sideways movement stabilization (reduces speed gain when zigzagging); when < 0, only so much friction is applied that braking (by accelerating backwards) cannot be stronger"};
cvar_t sv_airaccelerate = {0, "sv_airaccelerate", "-1", "rate at which a player accelerates to sv_maxairspeed while in the air, if less than 0 the sv_accelerate variable is used instead"};
cvar_t sv_airstopaccelerate = {0, "sv_airstopaccelerate", "0", "when set, replacement for sv_airaccelerate when moving backwards"};
cvar_t sv_airspeedlimit_nonqw = {0, "sv_airspeedlimit_nonqw", "0", "when set, this is a soft speed limit while in air when using airaccel_qw not equal to 1"};
cvar_t sv_airstrafeaccelerate = {0, "sv_airstrafeaccelerate", "0", "when set, replacement for sv_airaccelerate when just strafing"};
cvar_t sv_maxairstrafespeed = {0, "sv_maxairstrafespeed", "0", "when set, replacement for sv_maxairspeed when just strafing"};
cvar_t sv_airstrafeaccel_qw = {0, "sv_airstrafeaccel_qw", "0", "when set, replacement for sv_airaccel_qw when just strafing"};
cvar_t sv_aircontrol = {0, "sv_aircontrol", "0", "CPMA-style air control"};
cvar_t sv_aircontrol_power = {0, "sv_aircontrol_power", "2", "CPMA-style air control exponent"};
cvar_t sv_aircontrol_penalty = {0, "sv_aircontrol_penalty", "0", "deceleration while using CPMA-style air control"};
cvar_t sv_allowdownloads = {0, "sv_allowdownloads", "1", "whether to allow clients to download files from the server (does not affect http downloads)"};
cvar_t sv_allowdownloads_archive = {0, "sv_allowdownloads_archive", "0", "whether to allow downloads of archives (pak/pk3)"};
cvar_t sv_allowdownloads_config = {0, "sv_allowdownloads_config", "0", "whether to allow downloads of config files (cfg)"};
cvar_t sv_allowdownloads_dlcache = {0, "sv_allowdownloads_dlcache", "0", "whether to allow downloads of dlcache files (dlcache/)"};
cvar_t sv_allowdownloads_inarchive = {0, "sv_allowdownloads_inarchive", "0", "whether to allow downloads from archives (pak/pk3)"};
cvar_t sv_areagrid_mingridsize = {CVAR_NOTIFY, "sv_areagrid_mingridsize", "128", "minimum areagrid cell size, smaller values work better for lots of small objects, higher values for large objects"};
cvar_t sv_checkforpacketsduringsleep = {0, "sv_checkforpacketsduringsleep", "0", "uses select() function to wait between frames which can be interrupted by packets being received, instead of Sleep()/usleep()/SDL_Sleep() functions which do not check for packets"};
cvar_t sv_clmovement_enable = {0, "sv_clmovement_enable", "1", "whether to allow clients to use cl_movement prediction, which can cause choppy movement on the server which may annoy other players"};
cvar_t sv_clmovement_minping = {0, "sv_clmovement_minping", "0", "if client ping is below this time in milliseconds, then their ability to use cl_movement prediction is disabled for a while (as they don't need it)"};
cvar_t sv_clmovement_minping_disabletime = {0, "sv_clmovement_minping_disabletime", "1000", "when client falls below minping, disable their prediction for this many milliseconds (should be at least 1000 or else their prediction may turn on/off frequently)"};
cvar_t sv_clmovement_inputtimeout = {0, "sv_clmovement_inputtimeout", "0.2", "when a client does not send input for this many seconds, force them to move anyway (unlike QuakeWorld)"};
cvar_t sv_cullentities_nevercullbmodels = {0, "sv_cullentities_nevercullbmodels", "0", "if enabled the clients are always notified of moving doors and lifts and other submodels of world (warning: eats a lot of network bandwidth on some levels!)"};
cvar_t sv_cullentities_pvs = {0, "sv_cullentities_pvs", "1", "fast but loose culling of hidden entities"};
cvar_t sv_cullentities_stats = {0, "sv_cullentities_stats", "0", "displays stats on network entities culled by various methods for each client"};
cvar_t sv_cullentities_trace = {0, "sv_cullentities_trace", "0", "somewhat slow but very tight culling of hidden entities, minimizes network traffic and makes wallhack cheats useless"};
cvar_t sv_cullentities_trace_delay = {0, "sv_cullentities_trace_delay", "1", "number of seconds until the entity gets actually culled"};
cvar_t sv_cullentities_trace_delay_players = {0, "sv_cullentities_trace_delay_players", "0.2", "number of seconds until the entity gets actually culled if it is a player entity"};
cvar_t sv_cullentities_trace_enlarge = {0, "sv_cullentities_trace_enlarge", "0", "box enlargement for entity culling"};
cvar_t sv_cullentities_trace_prediction = {0, "sv_cullentities_trace_prediction", "1", "also trace from the predicted player position"};
cvar_t sv_cullentities_trace_prediction_time = {0, "sv_cullentities_trace_prediction_time", "0.2", "how many seconds of prediction to use"};
cvar_t sv_cullentities_trace_entityocclusion = {0, "sv_cullentities_trace_entityocclusion", "0", "also check if doors and other bsp models are in the way"};
cvar_t sv_cullentities_trace_samples = {0, "sv_cullentities_trace_samples", "2", "number of samples to test for entity culling"};
cvar_t sv_cullentities_trace_samples_extra = {0, "sv_cullentities_trace_samples_extra", "2", "number of samples to test for entity culling when the entity affects its surroundings by e.g. dlight"};
cvar_t sv_cullentities_trace_samples_players = {0, "sv_cullentities_trace_samples_players", "8", "number of samples to test for entity culling when the entity is a player entity"};
cvar_t sv_debugmove = {CVAR_NOTIFY, "sv_debugmove", "0", "disables collision detection optimizations for debugging purposes"};
cvar_t sv_echobprint = {CVAR_SAVE, "sv_echobprint", "1", "prints gamecode bprint() calls to server console"};
cvar_t sv_edgefriction = {0, "edgefriction", "1", "how much you slow down when nearing a ledge you might fall off, multiplier of sv_friction (Quake used 2, QuakeWorld used 1 due to a bug in physics code)"};
cvar_t sv_entpatch = {0, "sv_entpatch", "1", "enables loading of .ent files to override entities in the bsp (for example Threewave CTF server pack contains .ent patch files enabling play of CTF on id1 maps)"};
cvar_t sv_fixedframeratesingleplayer = {0, "sv_fixedframeratesingleplayer", "1", "allows you to use server-style timing system in singleplayer (don't run faster than sys_ticrate)"};
cvar_t sv_freezenonclients = {CVAR_NOTIFY, "sv_freezenonclients", "0", "freezes time, except for players, allowing you to walk around and take screenshots of explosions"};
cvar_t sv_friction = {CVAR_NOTIFY, "sv_friction","4", "how fast you slow down"};
cvar_t sv_gameplayfix_blowupfallenzombies = {0, "sv_gameplayfix_blowupfallenzombies", "1", "causes findradius to detect SOLID_NOT entities such as zombies and corpses on the floor, allowing splash damage to apply to them"};
cvar_t sv_gameplayfix_consistentplayerprethink = {0, "sv_gameplayfix_consistentplayerprethink", "0", "improves fairness in multiplayer by running all PlayerPreThink functions (which fire weapons) before performing physics, then running all PlayerPostThink functions"};
cvar_t sv_gameplayfix_delayprojectiles = {0, "sv_gameplayfix_delayprojectiles", "1", "causes entities to not move on the same frame they are spawned, meaning that projectiles wait until the next frame to perform their first move, giving proper interpolation and rocket trails, but making weapons harder to use at low framerates"};
cvar_t sv_gameplayfix_droptofloorstartsolid = {0, "sv_gameplayfix_droptofloorstartsolid", "1", "prevents items and monsters that start in a solid area from falling out of the level (makes droptofloor treat trace_startsolid as an acceptable outcome)"};
cvar_t sv_gameplayfix_droptofloorstartsolid_nudgetocorrect = {0, "sv_gameplayfix_droptofloorstartsolid_nudgetocorrect", "1", "tries to nudge stuck items and monsters out of walls before droptofloor is performed"};
cvar_t sv_gameplayfix_easierwaterjump = {0, "sv_gameplayfix_easierwaterjump", "1", "changes water jumping to make it easier to get out of water (exactly like in QuakeWorld)"};
cvar_t sv_gameplayfix_findradiusdistancetobox = {0, "sv_gameplayfix_findradiusdistancetobox", "1", "causes findradius to check the distance to the corner of a box rather than the center of the box, makes findradius detect bmodels such as very large doors that would otherwise be unaffected by splash damage"};
cvar_t sv_gameplayfix_gravityunaffectedbyticrate = {0, "sv_gameplayfix_gravityunaffectedbyticrate", "0", "fix some ticrate issues in physics."};
cvar_t sv_gameplayfix_grenadebouncedownslopes = {0, "sv_gameplayfix_grenadebouncedownslopes", "1", "prevents MOVETYPE_BOUNCE (grenades) from getting stuck when fired down a downward sloping surface"};
cvar_t sv_gameplayfix_multiplethinksperframe = {0, "sv_gameplayfix_multiplethinksperframe", "1", "allows entities to think more often than the server framerate, primarily useful for very high fire rate weapons"};
cvar_t sv_gameplayfix_noairborncorpse = {0, "sv_gameplayfix_noairborncorpse", "1", "causes entities (corpses, items, etc) sitting ontop of moving entities (players) to fall when the moving entity (player) is no longer supporting them"};
cvar_t sv_gameplayfix_noairborncorpse_allowsuspendeditems = {0, "sv_gameplayfix_noairborncorpse_allowsuspendeditems", "1", "causes entities sitting ontop of objects that are instantaneously remove to float in midair (special hack to allow a common level design trick for floating items)"};
cvar_t sv_gameplayfix_nudgeoutofsolid = {0, "sv_gameplayfix_nudgeoutofsolid", "0", "attempts to fix physics errors (where an object ended up in solid for some reason)"};
cvar_t sv_gameplayfix_nudgeoutofsolid_separation = {0, "sv_gameplayfix_nudgeoutofsolid_separation", "0.03125", "keep objects this distance apart to prevent collision issues on seams"};
cvar_t sv_gameplayfix_q2airaccelerate = {0, "sv_gameplayfix_q2airaccelerate", "0", "Quake2-style air acceleration"};
cvar_t sv_gameplayfix_nogravityonground = {0, "sv_gameplayfix_nogravityonground", "0", "turn off gravity when on ground (to get rid of sliding)"};
cvar_t sv_gameplayfix_setmodelrealbox = {0, "sv_gameplayfix_setmodelrealbox", "1", "fixes a bug in Quake that made setmodel always set the entity box to ('-16 -16 -16', '16 16 16') rather than properly checking the model box, breaks some poorly coded mods"};
cvar_t sv_gameplayfix_slidemoveprojectiles = {0, "sv_gameplayfix_slidemoveprojectiles", "1", "allows MOVETYPE_FLY/FLYMISSILE/TOSS/BOUNCE/BOUNCEMISSILE entities to finish their move in a frame even if they hit something, fixes 'gravity accumulation' bug for grenades on steep slopes"};
cvar_t sv_gameplayfix_stepdown = {0, "sv_gameplayfix_stepdown", "0", "attempts to step down stairs, not just up them (prevents the familiar thud..thud..thud.. when running down stairs and slopes)"};
cvar_t sv_gameplayfix_stepwhilejumping = {0, "sv_gameplayfix_stepwhilejumping", "1", "applies step-up onto a ledge even while airborn, useful if you would otherwise just-miss the floor when running across small areas with gaps (for instance running across the moving platforms in dm2, or jumping to the megahealth and red armor in dm2 rather than using the bridge)"};
cvar_t sv_gameplayfix_stepmultipletimes = {0, "sv_gameplayfix_stepmultipletimes", "0", "applies step-up onto a ledge more than once in a single frame, when running quickly up stairs"};
cvar_t sv_gameplayfix_nostepmoveonsteepslopes = {0, "sv_gameplayfix_nostepmoveonsteepslopes", "0", "grude fix which prevents MOVETYPE_STEP (not swimming or flying) to move on slopes whose angle is bigger than 45 degree"};
cvar_t sv_gameplayfix_swiminbmodels = {0, "sv_gameplayfix_swiminbmodels", "1", "causes pointcontents (used to determine if you are in a liquid) to check bmodel entities as well as the world model, so you can swim around in (possibly moving) water bmodel entities"};
cvar_t sv_gameplayfix_upwardvelocityclearsongroundflag = {0, "sv_gameplayfix_upwardvelocityclearsongroundflag", "1", "prevents monsters, items, and most other objects from being stuck to the floor when pushed around by damage, and other situations in mods"};
cvar_t sv_gameplayfix_downtracesupportsongroundflag = {0, "sv_gameplayfix_downtracesupportsongroundflag", "1", "prevents very short moves from clearing onground (which may make the player stick to the floor at high netfps)"};
cvar_t sv_gameplayfix_q1bsptracelinereportstexture = {0, "sv_gameplayfix_q1bsptracelinereportstexture", "1", "enables mods to get accurate trace_texture results on q1bsp by using a surface-hitting traceline implementation rather than the standard solidbsp method, q3bsp always reports texture accurately"};
cvar_t sv_gravity = {CVAR_NOTIFY, "sv_gravity","800", "how fast you fall (512 = roughly earth gravity)"};
cvar_t sv_idealpitchscale = {0, "sv_idealpitchscale","0.8", "how much to look up/down slopes and stairs when not using freelook"};
cvar_t sv_jumpstep = {CVAR_NOTIFY, "sv_jumpstep", "0", "whether you can step up while jumping (sv_gameplayfix_stepwhilejumping must also be 1)"};
cvar_t sv_jumpvelocity = {0, "sv_jumpvelocity", "270", "cvar that can be used by QuakeC code for jump velocity"};
cvar_t sv_maxairspeed = {0, "sv_maxairspeed", "30", "maximum speed a player can accelerate to when airborn (note that it is possible to completely stop by moving the opposite direction)"};
cvar_t sv_maxrate = {CVAR_SAVE | CVAR_NOTIFY, "sv_maxrate", "1000000", "upper limit on client rate cvar, should reflect your network connection quality"};
cvar_t sv_maxspeed = {CVAR_NOTIFY, "sv_maxspeed", "320", "maximum speed a player can accelerate to when on ground (can be exceeded by tricks)"};
cvar_t sv_maxvelocity = {CVAR_NOTIFY, "sv_maxvelocity","2000", "universal speed limit on all entities"};
cvar_t sv_nostep = {CVAR_NOTIFY, "sv_nostep","0", "prevents MOVETYPE_STEP entities (monsters) from moving"};
cvar_t sv_playerphysicsqc = {CVAR_NOTIFY, "sv_playerphysicsqc", "1", "enables QuakeC function to override player physics"};
cvar_t sv_progs = {0, "sv_progs", "progs.dat", "selects which quakec progs.dat file to run" };
cvar_t sv_protocolname = {0, "sv_protocolname", "DP7", "selects network protocol to host for (values include QUAKE, QUAKEDP, NEHAHRAMOVIE, DP1 and up)"};
cvar_t sv_random_seed = {0, "sv_random_seed", "", "random seed; when set, on every map start this random seed is used to initialize the random number generator. Don't touch it unless for benchmarking or debugging"};
cvar_t sv_ratelimitlocalplayer = {0, "sv_ratelimitlocalplayer", "0", "whether to apply rate limiting to the local player in a listen server (only useful for testing)"};
cvar_t sv_sound_land = {0, "sv_sound_land", "demon/dland2.wav", "sound to play when MOVETYPE_STEP entity hits the ground at high speed (empty cvar disables the sound)"};
cvar_t sv_sound_watersplash = {0, "sv_sound_watersplash", "misc/h2ohit1.wav", "sound to play when MOVETYPE_FLY/TOSS/BOUNCE/STEP entity enters or leaves water (empty cvar disables the sound)"};
cvar_t sv_stepheight = {CVAR_NOTIFY, "sv_stepheight", "18", "how high you can step up (TW_SV_STEPCONTROL extension)"};
cvar_t sv_stopspeed = {CVAR_NOTIFY, "sv_stopspeed","100", "how fast you come to a complete stop"};
cvar_t sv_wallfriction = {CVAR_NOTIFY, "sv_wallfriction", "1", "how much you slow down when sliding along a wall"};
cvar_t sv_wateraccelerate = {0, "sv_wateraccelerate", "-1", "rate at which a player accelerates to sv_maxspeed while in the air, if less than 0 the sv_accelerate variable is used instead"};
cvar_t sv_waterfriction = {CVAR_NOTIFY, "sv_waterfriction","-1", "how fast you slow down, if less than 0 the sv_friction variable is used instead"};
cvar_t sv_warsowbunny_airforwardaccel = {0, "sv_warsowbunny_airforwardaccel", "1.00001", "how fast you accelerate until you reach sv_maxspeed"};
cvar_t sv_warsowbunny_accel = {0, "sv_warsowbunny_accel", "0.1585", "how fast you accelerate until after reaching sv_maxspeed (it gets harder as you near sv_warsowbunny_topspeed)"};
cvar_t sv_warsowbunny_topspeed = {0, "sv_warsowbunny_topspeed", "925", "soft speed limit (can get faster with rjs and on ramps)"};
cvar_t sv_warsowbunny_turnaccel = {0, "sv_warsowbunny_turnaccel", "0", "max sharpness of turns (also master switch for the sv_warsowbunny_* mode; set this to 9 to enable)"};
cvar_t sv_warsowbunny_backtosideratio = {0, "sv_warsowbunny_backtosideratio", "0.8", "lower values make it easier to change direction without losing speed; the drawback is \"understeering\" in sharp turns"};
cvar_t sv_onlycsqcnetworking = {0, "sv_onlycsqcnetworking", "0", "disables legacy entity networking code for higher performance (except on clients, which can still be legacy)"};
cvar_t sys_ticrate = {CVAR_SAVE, "sys_ticrate","0.0138889", "how long a server frame is in seconds, 0.05 is 20fps server rate, 0.1 is 10fps (can not be set higher than 0.1), 0 runs as many server frames as possible (makes games against bots a little smoother, overwhelms network players), 0.0138889 matches QuakeWorld physics"};
cvar_t teamplay = {CVAR_NOTIFY, "teamplay","0", "teamplay mode, values depend on mod but typically 0 = no teams, 1 = no team damage no self damage, 2 = team damage and self damage, some mods support 3 = no team damage but can damage self"};
cvar_t timelimit = {CVAR_NOTIFY, "timelimit","0", "ends level at this time (in minutes)"};

cvar_t saved1 = {CVAR_SAVE, "saved1", "0", "unused cvar in quake that is saved to config.cfg on exit, can be used by mods"};
cvar_t saved2 = {CVAR_SAVE, "saved2", "0", "unused cvar in quake that is saved to config.cfg on exit, can be used by mods"};
cvar_t saved3 = {CVAR_SAVE, "saved3", "0", "unused cvar in quake that is saved to config.cfg on exit, can be used by mods"};
cvar_t saved4 = {CVAR_SAVE, "saved4", "0", "unused cvar in quake that is saved to config.cfg on exit, can be used by mods"};
cvar_t savedgamecfg = {CVAR_SAVE, "savedgamecfg", "0", "unused cvar in quake that is saved to config.cfg on exit, can be used by mods"};
cvar_t scratch1 = {0, "scratch1", "0", "unused cvar in quake, can be used by mods"};
cvar_t scratch2 = {0,"scratch2", "0", "unused cvar in quake, can be used by mods"};
cvar_t scratch3 = {0, "scratch3", "0", "unused cvar in quake, can be used by mods"};
cvar_t scratch4 = {0, "scratch4", "0", "unused cvar in quake, can be used by mods"};
cvar_t temp1 = {0, "temp1","0", "general cvar for mods to use, in stock id1 this selects which death animation to use on players (0 = random death, other values select specific death scenes)"};

cvar_t nehx00 = {0, "nehx00", "0", "nehahra data storage cvar (used in singleplayer)"};
cvar_t nehx01 = {0, "nehx01", "0", "nehahra data storage cvar (used in singleplayer)"};
cvar_t nehx02 = {0, "nehx02", "0", "nehahra data storage cvar (used in singleplayer)"};
cvar_t nehx03 = {0, "nehx03", "0", "nehahra data storage cvar (used in singleplayer)"};
cvar_t nehx04 = {0, "nehx04", "0", "nehahra data storage cvar (used in singleplayer)"};
cvar_t nehx05 = {0, "nehx05", "0", "nehahra data storage cvar (used in singleplayer)"};
cvar_t nehx06 = {0, "nehx06", "0", "nehahra data storage cvar (used in singleplayer)"};
cvar_t nehx07 = {0, "nehx07", "0", "nehahra data storage cvar (used in singleplayer)"};
cvar_t nehx08 = {0, "nehx08", "0", "nehahra data storage cvar (used in singleplayer)"};
cvar_t nehx09 = {0, "nehx09", "0", "nehahra data storage cvar (used in singleplayer)"};
cvar_t nehx10 = {0, "nehx10", "0", "nehahra data storage cvar (used in singleplayer)"};
cvar_t nehx11 = {0, "nehx11", "0", "nehahra data storage cvar (used in singleplayer)"};
cvar_t nehx12 = {0, "nehx12", "0", "nehahra data storage cvar (used in singleplayer)"};
cvar_t nehx13 = {0, "nehx13", "0", "nehahra data storage cvar (used in singleplayer)"};
cvar_t nehx14 = {0, "nehx14", "0", "nehahra data storage cvar (used in singleplayer)"};
cvar_t nehx15 = {0, "nehx15", "0", "nehahra data storage cvar (used in singleplayer)"};
cvar_t nehx16 = {0, "nehx16", "0", "nehahra data storage cvar (used in singleplayer)"};
cvar_t nehx17 = {0, "nehx17", "0", "nehahra data storage cvar (used in singleplayer)"};
cvar_t nehx18 = {0, "nehx18", "0", "nehahra data storage cvar (used in singleplayer)"};
cvar_t nehx19 = {0, "nehx19", "0", "nehahra data storage cvar (used in singleplayer)"};
cvar_t cutscene = {0, "cutscene", "1", "enables cutscenes in nehahra, can be used by other mods"};

cvar_t sv_autodemo_perclient = {CVAR_SAVE, "sv_autodemo_perclient", "0", "set to 1 to enable autorecorded per-client demos (they'll start to record at the beginning of a match); set it to 2 to also record client->server packets (for debugging)"};
cvar_t sv_autodemo_perclient_nameformat = {CVAR_SAVE, "sv_autodemo_perclient_nameformat", "sv_autodemos/%Y-%m-%d_%H-%M", "The format of the sv_autodemo_perclient filename, followed by the map name, the client number and the IP address + port number, separated by underscores (the date is encoded using strftime escapes)" };
cvar_t sv_autodemo_perclient_discardable = {CVAR_SAVE, "sv_autodemo_perclient_discardable", "0", "Allow game code to decide whether a demo should be kept or discarded."};

cvar_t halflifebsp = {0, "halflifebsp", "0", "indicates the current map is hlbsp format (useful to know because of different bounding box sizes)"};

server_t sv;
server_static_t svs;

mempool_t *sv_mempool = NULL;

extern cvar_t slowmo;
extern float		scr_centertime_off;

// MUST match effectnameindex_t in client.h
static const char *standardeffectnames[EFFECT_TOTAL] =
{
	"",
	"TE_GUNSHOT",
	"TE_GUNSHOTQUAD",
	"TE_SPIKE",
	"TE_SPIKEQUAD",
	"TE_SUPERSPIKE",
	"TE_SUPERSPIKEQUAD",
	"TE_WIZSPIKE",
	"TE_KNIGHTSPIKE",
	"TE_EXPLOSION",
	"TE_EXPLOSIONQUAD",
	"TE_TAREXPLOSION",
	"TE_TELEPORT",
	"TE_LAVASPLASH",
	"TE_SMALLFLASH",
	"TE_FLAMEJET",
	"EF_FLAME",
	"TE_BLOOD",
	"TE_SPARK",
	"TE_PLASMABURN",
	"TE_TEI_G3",
	"TE_TEI_SMOKE",
	"TE_TEI_BIGEXPLOSION",
	"TE_TEI_PLASMAHIT",
	"EF_STARDUST",
	"TR_ROCKET",
	"TR_GRENADE",
	"TR_BLOOD",
	"TR_WIZSPIKE",
	"TR_SLIGHTBLOOD",
	"TR_KNIGHTSPIKE",
	"TR_VORESPIKE",
	"TR_NEHAHRASMOKE",
	"TR_NEXUIZPLASMA",
	"TR_GLOWTRAIL",
	"SVC_PARTICLE"
};

#define SV_REQFUNCS 0
#define sv_reqfuncs NULL

//#define SV_REQFUNCS (sizeof(sv_reqfuncs) / sizeof(const char *))
//static const char *sv_reqfuncs[] = {
//};

#define SV_REQFIELDS (sizeof(sv_reqfields) / sizeof(prvm_required_field_t))

prvm_required_field_t sv_reqfields[] =
{
#define PRVM_DECLARE_serverglobalfloat(x)
#define PRVM_DECLARE_serverglobalvector(x)
#define PRVM_DECLARE_serverglobalstring(x)
#define PRVM_DECLARE_serverglobaledict(x)
#define PRVM_DECLARE_serverglobalfunction(x)
#define PRVM_DECLARE_clientglobalfloat(x)
#define PRVM_DECLARE_clientglobalvector(x)
#define PRVM_DECLARE_clientglobalstring(x)
#define PRVM_DECLARE_clientglobaledict(x)
#define PRVM_DECLARE_clientglobalfunction(x)
#define PRVM_DECLARE_menuglobalfloat(x)
#define PRVM_DECLARE_menuglobalvector(x)
#define PRVM_DECLARE_menuglobalstring(x)
#define PRVM_DECLARE_menuglobaledict(x)
#define PRVM_DECLARE_menuglobalfunction(x)
#define PRVM_DECLARE_serverfieldfloat(x) {ev_float, #x},
#define PRVM_DECLARE_serverfieldvector(x) {ev_vector, #x},
#define PRVM_DECLARE_serverfieldstring(x) {ev_string, #x},
#define PRVM_DECLARE_serverfieldedict(x) {ev_entity, #x},
#define PRVM_DECLARE_serverfieldfunction(x) {ev_function, #x},
#define PRVM_DECLARE_clientfieldfloat(x)
#define PRVM_DECLARE_clientfieldvector(x)
#define PRVM_DECLARE_clientfieldstring(x)
#define PRVM_DECLARE_clientfieldedict(x)
#define PRVM_DECLARE_clientfieldfunction(x)
#define PRVM_DECLARE_menufieldfloat(x)
#define PRVM_DECLARE_menufieldvector(x)
#define PRVM_DECLARE_menufieldstring(x)
#define PRVM_DECLARE_menufieldedict(x)
#define PRVM_DECLARE_menufieldfunction(x)
#define PRVM_DECLARE_serverfunction(x)
#define PRVM_DECLARE_clientfunction(x)
#define PRVM_DECLARE_menufunction(x)
#define PRVM_DECLARE_field(x)
#define PRVM_DECLARE_global(x)
#define PRVM_DECLARE_function(x)
#include "prvm_offsets.h"
#undef PRVM_DECLARE_serverglobalfloat
#undef PRVM_DECLARE_serverglobalvector
#undef PRVM_DECLARE_serverglobalstring
#undef PRVM_DECLARE_serverglobaledict
#undef PRVM_DECLARE_serverglobalfunction
#undef PRVM_DECLARE_clientglobalfloat
#undef PRVM_DECLARE_clientglobalvector
#undef PRVM_DECLARE_clientglobalstring
#undef PRVM_DECLARE_clientglobaledict
#undef PRVM_DECLARE_clientglobalfunction
#undef PRVM_DECLARE_menuglobalfloat
#undef PRVM_DECLARE_menuglobalvector
#undef PRVM_DECLARE_menuglobalstring
#undef PRVM_DECLARE_menuglobaledict
#undef PRVM_DECLARE_menuglobalfunction
#undef PRVM_DECLARE_serverfieldfloat
#undef PRVM_DECLARE_serverfieldvector
#undef PRVM_DECLARE_serverfieldstring
#undef PRVM_DECLARE_serverfieldedict
#undef PRVM_DECLARE_serverfieldfunction
#undef PRVM_DECLARE_clientfieldfloat
#undef PRVM_DECLARE_clientfieldvector
#undef PRVM_DECLARE_clientfieldstring
#undef PRVM_DECLARE_clientfieldedict
#undef PRVM_DECLARE_clientfieldfunction
#undef PRVM_DECLARE_menufieldfloat
#undef PRVM_DECLARE_menufieldvector
#undef PRVM_DECLARE_menufieldstring
#undef PRVM_DECLARE_menufieldedict
#undef PRVM_DECLARE_menufieldfunction
#undef PRVM_DECLARE_serverfunction
#undef PRVM_DECLARE_clientfunction
#undef PRVM_DECLARE_menufunction
#undef PRVM_DECLARE_field
#undef PRVM_DECLARE_global
#undef PRVM_DECLARE_function
};

#define SV_REQGLOBALS (sizeof(sv_reqglobals) / sizeof(prvm_required_field_t))

prvm_required_field_t sv_reqglobals[] =
{
#define PRVM_DECLARE_serverglobalfloat(x) {ev_float, #x},
#define PRVM_DECLARE_serverglobalvector(x) {ev_vector, #x},
#define PRVM_DECLARE_serverglobalstring(x) {ev_string, #x},
#define PRVM_DECLARE_serverglobaledict(x) {ev_entity, #x},
#define PRVM_DECLARE_serverglobalfunction(x) {ev_function, #x},
#define PRVM_DECLARE_clientglobalfloat(x)
#define PRVM_DECLARE_clientglobalvector(x)
#define PRVM_DECLARE_clientglobalstring(x)
#define PRVM_DECLARE_clientglobaledict(x)
#define PRVM_DECLARE_clientglobalfunction(x)
#define PRVM_DECLARE_menuglobalfloat(x)
#define PRVM_DECLARE_menuglobalvector(x)
#define PRVM_DECLARE_menuglobalstring(x)
#define PRVM_DECLARE_menuglobaledict(x)
#define PRVM_DECLARE_menuglobalfunction(x)
#define PRVM_DECLARE_serverfieldfloat(x)
#define PRVM_DECLARE_serverfieldvector(x)
#define PRVM_DECLARE_serverfieldstring(x)
#define PRVM_DECLARE_serverfieldedict(x)
#define PRVM_DECLARE_serverfieldfunction(x)
#define PRVM_DECLARE_clientfieldfloat(x)
#define PRVM_DECLARE_clientfieldvector(x)
#define PRVM_DECLARE_clientfieldstring(x)
#define PRVM_DECLARE_clientfieldedict(x)
#define PRVM_DECLARE_clientfieldfunction(x)
#define PRVM_DECLARE_menufieldfloat(x)
#define PRVM_DECLARE_menufieldvector(x)
#define PRVM_DECLARE_menufieldstring(x)
#define PRVM_DECLARE_menufieldedict(x)
#define PRVM_DECLARE_menufieldfunction(x)
#define PRVM_DECLARE_serverfunction(x)
#define PRVM_DECLARE_clientfunction(x)
#define PRVM_DECLARE_menufunction(x)
#define PRVM_DECLARE_field(x)
#define PRVM_DECLARE_global(x)
#define PRVM_DECLARE_function(x)
#include "prvm_offsets.h"
#undef PRVM_DECLARE_serverglobalfloat
#undef PRVM_DECLARE_serverglobalvector
#undef PRVM_DECLARE_serverglobalstring
#undef PRVM_DECLARE_serverglobaledict
#undef PRVM_DECLARE_serverglobalfunction
#undef PRVM_DECLARE_clientglobalfloat
#undef PRVM_DECLARE_clientglobalvector
#undef PRVM_DECLARE_clientglobalstring
#undef PRVM_DECLARE_clientglobaledict
#undef PRVM_DECLARE_clientglobalfunction
#undef PRVM_DECLARE_menuglobalfloat
#undef PRVM_DECLARE_menuglobalvector
#undef PRVM_DECLARE_menuglobalstring
#undef PRVM_DECLARE_menuglobaledict
#undef PRVM_DECLARE_menuglobalfunction
#undef PRVM_DECLARE_serverfieldfloat
#undef PRVM_DECLARE_serverfieldvector
#undef PRVM_DECLARE_serverfieldstring
#undef PRVM_DECLARE_serverfieldedict
#undef PRVM_DECLARE_serverfieldfunction
#undef PRVM_DECLARE_clientfieldfloat
#undef PRVM_DECLARE_clientfieldvector
#undef PRVM_DECLARE_clientfieldstring
#undef PRVM_DECLARE_clientfieldedict
#undef PRVM_DECLARE_clientfieldfunction
#undef PRVM_DECLARE_menufieldfloat
#undef PRVM_DECLARE_menufieldvector
#undef PRVM_DECLARE_menufieldstring
#undef PRVM_DECLARE_menufieldedict
#undef PRVM_DECLARE_menufieldfunction
#undef PRVM_DECLARE_serverfunction
#undef PRVM_DECLARE_clientfunction
#undef PRVM_DECLARE_menufunction
#undef PRVM_DECLARE_field
#undef PRVM_DECLARE_global
#undef PRVM_DECLARE_function
};



//============================================================================

void SV_AreaStats_f(void)
{
	World_PrintAreaStats(&sv.world, "server");
}

/*
===============
SV_Init
===============
*/
void SV_Init (void)
{
	// init the csqc progs cvars, since they are updated/used by the server code
	// TODO: fix this since this is a quick hack to make some of [515]'s broken code run ;) [9/13/2006 Black]
	extern cvar_t csqc_progname;	//[515]: csqc crc check and right csprogs name according to progs.dat
	extern cvar_t csqc_progcrc;
	extern cvar_t csqc_progsize;

	Cvar_RegisterVariable(&sv_worldmessage);
	Cvar_RegisterVariable(&sv_worldname);
	Cvar_RegisterVariable(&sv_worldnamenoextension);
	Cvar_RegisterVariable(&sv_worldbasename);

	Cvar_RegisterVariable (&csqc_progname);
	Cvar_RegisterVariable (&csqc_progcrc);
	Cvar_RegisterVariable (&csqc_progsize);

	Cmd_AddCommand("sv_saveentfile", SV_SaveEntFile_f, "save map entities to .ent file (to allow external editing)");
	Cmd_AddCommand("sv_areastats", SV_AreaStats_f, "prints statistics on entity culling during collision traces");
	Cmd_AddCommand_WithClientCommand("sv_startdownload", NULL, SV_StartDownload_f, "begins sending a file to the client (network protocol use only)");
	Cmd_AddCommand_WithClientCommand("download", NULL, SV_Download_f, "downloads a specified file from the server");

	Cvar_RegisterVariable (&coop);
	Cvar_RegisterVariable (&deathmatch);
	Cvar_RegisterVariable (&fraglimit);
	Cvar_RegisterVariable (&gamecfg);
	Cvar_RegisterVariable (&noexit);
	Cvar_RegisterVariable (&nomonsters);
	Cvar_RegisterVariable (&pausable);
	Cvar_RegisterVariable (&pr_checkextension);
	Cvar_RegisterVariable (&samelevel);
	Cvar_RegisterVariable (&skill);
	Cvar_RegisterVariable (&slowmo);
	Cvar_RegisterVariable (&sv_accelerate);
	Cvar_RegisterVariable (&sv_aim);
	Cvar_RegisterVariable (&sv_airaccel_qw);
	Cvar_RegisterVariable (&sv_airaccel_qw_stretchfactor);
	Cvar_RegisterVariable (&sv_airaccel_sideways_friction);
	Cvar_RegisterVariable (&sv_airaccelerate);
	Cvar_RegisterVariable (&sv_airstopaccelerate);
	Cvar_RegisterVariable (&sv_airstrafeaccelerate);
	Cvar_RegisterVariable (&sv_maxairstrafespeed);
	Cvar_RegisterVariable (&sv_airstrafeaccel_qw);
	Cvar_RegisterVariable (&sv_airspeedlimit_nonqw);
	Cvar_RegisterVariable (&sv_aircontrol);
	Cvar_RegisterVariable (&sv_aircontrol_power);
	Cvar_RegisterVariable (&sv_aircontrol_penalty);
	Cvar_RegisterVariable (&sv_allowdownloads);
	Cvar_RegisterVariable (&sv_allowdownloads_archive);
	Cvar_RegisterVariable (&sv_allowdownloads_config);
	Cvar_RegisterVariable (&sv_allowdownloads_dlcache);
	Cvar_RegisterVariable (&sv_allowdownloads_inarchive);
	Cvar_RegisterVariable (&sv_areagrid_mingridsize);
	Cvar_RegisterVariable (&sv_checkforpacketsduringsleep);
	Cvar_RegisterVariable (&sv_clmovement_enable);
	Cvar_RegisterVariable (&sv_clmovement_minping);
	Cvar_RegisterVariable (&sv_clmovement_minping_disabletime);
	Cvar_RegisterVariable (&sv_clmovement_inputtimeout);
	Cvar_RegisterVariable (&sv_cullentities_nevercullbmodels);
	Cvar_RegisterVariable (&sv_cullentities_pvs);
	Cvar_RegisterVariable (&sv_cullentities_stats);
	Cvar_RegisterVariable (&sv_cullentities_trace);
	Cvar_RegisterVariable (&sv_cullentities_trace_delay);
	Cvar_RegisterVariable (&sv_cullentities_trace_delay_players);
	Cvar_RegisterVariable (&sv_cullentities_trace_enlarge);
	Cvar_RegisterVariable (&sv_cullentities_trace_entityocclusion);
	Cvar_RegisterVariable (&sv_cullentities_trace_prediction);
	Cvar_RegisterVariable (&sv_cullentities_trace_prediction_time);
	Cvar_RegisterVariable (&sv_cullentities_trace_samples);
	Cvar_RegisterVariable (&sv_cullentities_trace_samples_extra);
	Cvar_RegisterVariable (&sv_cullentities_trace_samples_players);
	Cvar_RegisterVariable (&sv_debugmove);
	Cvar_RegisterVariable (&sv_echobprint);
	Cvar_RegisterVariable (&sv_edgefriction);
	Cvar_RegisterVariable (&sv_entpatch);
	Cvar_RegisterVariable (&sv_fixedframeratesingleplayer);
	Cvar_RegisterVariable (&sv_freezenonclients);
	Cvar_RegisterVariable (&sv_friction);
	Cvar_RegisterVariable (&sv_gameplayfix_blowupfallenzombies);
	Cvar_RegisterVariable (&sv_gameplayfix_consistentplayerprethink);
	Cvar_RegisterVariable (&sv_gameplayfix_delayprojectiles);
	Cvar_RegisterVariable (&sv_gameplayfix_droptofloorstartsolid);
	Cvar_RegisterVariable (&sv_gameplayfix_droptofloorstartsolid_nudgetocorrect);
	Cvar_RegisterVariable (&sv_gameplayfix_easierwaterjump);
	Cvar_RegisterVariable (&sv_gameplayfix_findradiusdistancetobox);
	Cvar_RegisterVariable (&sv_gameplayfix_gravityunaffectedbyticrate);
	Cvar_RegisterVariable (&sv_gameplayfix_grenadebouncedownslopes);
	Cvar_RegisterVariable (&sv_gameplayfix_multiplethinksperframe);
	Cvar_RegisterVariable (&sv_gameplayfix_noairborncorpse);
	Cvar_RegisterVariable (&sv_gameplayfix_noairborncorpse_allowsuspendeditems);
	Cvar_RegisterVariable (&sv_gameplayfix_nudgeoutofsolid);
	Cvar_RegisterVariable (&sv_gameplayfix_nudgeoutofsolid_separation);
	Cvar_RegisterVariable (&sv_gameplayfix_q2airaccelerate);
	Cvar_RegisterVariable (&sv_gameplayfix_nogravityonground);
	Cvar_RegisterVariable (&sv_gameplayfix_setmodelrealbox);
	Cvar_RegisterVariable (&sv_gameplayfix_slidemoveprojectiles);
	Cvar_RegisterVariable (&sv_gameplayfix_stepdown);
	Cvar_RegisterVariable (&sv_gameplayfix_stepwhilejumping);
	Cvar_RegisterVariable (&sv_gameplayfix_stepmultipletimes);
	Cvar_RegisterVariable (&sv_gameplayfix_nostepmoveonsteepslopes);
	Cvar_RegisterVariable (&sv_gameplayfix_swiminbmodels);
	Cvar_RegisterVariable (&sv_gameplayfix_upwardvelocityclearsongroundflag);
	Cvar_RegisterVariable (&sv_gameplayfix_downtracesupportsongroundflag);
	Cvar_RegisterVariable (&sv_gameplayfix_q1bsptracelinereportstexture);
	Cvar_RegisterVariable (&sv_gravity);
	Cvar_RegisterVariable (&sv_idealpitchscale);
	Cvar_RegisterVariable (&sv_jumpstep);
	Cvar_RegisterVariable (&sv_jumpvelocity);
	Cvar_RegisterVariable (&sv_maxairspeed);
	Cvar_RegisterVariable (&sv_maxrate);
	Cvar_RegisterVariable (&sv_maxspeed);
	Cvar_RegisterVariable (&sv_maxvelocity);
	Cvar_RegisterVariable (&sv_nostep);
	Cvar_RegisterVariable (&sv_playerphysicsqc);
	Cvar_RegisterVariable (&sv_progs);
	Cvar_RegisterVariable (&sv_protocolname);
	Cvar_RegisterVariable (&sv_random_seed);
	Cvar_RegisterVariable (&sv_ratelimitlocalplayer);
	Cvar_RegisterVariable (&sv_sound_land);
	Cvar_RegisterVariable (&sv_sound_watersplash);
	Cvar_RegisterVariable (&sv_stepheight);
	Cvar_RegisterVariable (&sv_stopspeed);
	Cvar_RegisterVariable (&sv_wallfriction);
	Cvar_RegisterVariable (&sv_wateraccelerate);
	Cvar_RegisterVariable (&sv_waterfriction);
	Cvar_RegisterVariable (&sv_warsowbunny_airforwardaccel);
	Cvar_RegisterVariable (&sv_warsowbunny_accel);
	Cvar_RegisterVariable (&sv_warsowbunny_topspeed);
	Cvar_RegisterVariable (&sv_warsowbunny_turnaccel);
	Cvar_RegisterVariable (&sv_warsowbunny_backtosideratio);
	Cvar_RegisterVariable (&sv_onlycsqcnetworking);
	Cvar_RegisterVariable (&sys_ticrate);
	Cvar_RegisterVariable (&teamplay);
	Cvar_RegisterVariable (&timelimit);

	Cvar_RegisterVariable (&saved1);
	Cvar_RegisterVariable (&saved2);
	Cvar_RegisterVariable (&saved3);
	Cvar_RegisterVariable (&saved4);
	Cvar_RegisterVariable (&savedgamecfg);
	Cvar_RegisterVariable (&scratch1);
	Cvar_RegisterVariable (&scratch2);
	Cvar_RegisterVariable (&scratch3);
	Cvar_RegisterVariable (&scratch4);
	Cvar_RegisterVariable (&temp1);

	// LordHavoc: Nehahra uses these to pass data around cutscene demos
	Cvar_RegisterVariable (&nehx00);
	Cvar_RegisterVariable (&nehx01);
	Cvar_RegisterVariable (&nehx02);
	Cvar_RegisterVariable (&nehx03);
	Cvar_RegisterVariable (&nehx04);
	Cvar_RegisterVariable (&nehx05);
	Cvar_RegisterVariable (&nehx06);
	Cvar_RegisterVariable (&nehx07);
	Cvar_RegisterVariable (&nehx08);
	Cvar_RegisterVariable (&nehx09);
	Cvar_RegisterVariable (&nehx10);
	Cvar_RegisterVariable (&nehx11);
	Cvar_RegisterVariable (&nehx12);
	Cvar_RegisterVariable (&nehx13);
	Cvar_RegisterVariable (&nehx14);
	Cvar_RegisterVariable (&nehx15);
	Cvar_RegisterVariable (&nehx16);
	Cvar_RegisterVariable (&nehx17);
	Cvar_RegisterVariable (&nehx18);
	Cvar_RegisterVariable (&nehx19);
	Cvar_RegisterVariable (&cutscene); // for Nehahra but useful to other mods as well

	Cvar_RegisterVariable (&sv_autodemo_perclient);
	Cvar_RegisterVariable (&sv_autodemo_perclient_nameformat);
	Cvar_RegisterVariable (&sv_autodemo_perclient_discardable);

	Cvar_RegisterVariable (&halflifebsp);

	sv_mempool = Mem_AllocPool("server", 0, NULL);
}

static void SV_SaveEntFile_f(void)
{
	if (!sv.active || !sv.worldmodel)
	{
		Con_Print("Not running a server\n");
		return;
	}
	FS_WriteFile(va("%s.ent", sv.worldnamenoextension), sv.worldmodel->brush.entities, (fs_offset_t)strlen(sv.worldmodel->brush.entities));
}


/*
=============================================================================

EVENT MESSAGES

=============================================================================
*/

/*
==================
SV_StartParticle

Make sure the event gets sent to all clients
==================
*/
void SV_StartParticle (vec3_t org, vec3_t dir, int color, int count)
{
	int i;

	if (sv.datagram.cursize > MAX_PACKETFRAGMENT-18)
		return;
	MSG_WriteByte (&sv.datagram, svc_particle);
	MSG_WriteCoord (&sv.datagram, org[0], sv.protocol);
	MSG_WriteCoord (&sv.datagram, org[1], sv.protocol);
	MSG_WriteCoord (&sv.datagram, org[2], sv.protocol);
	for (i=0 ; i<3 ; i++)
		MSG_WriteChar (&sv.datagram, (int)bound(-128, dir[i]*16, 127));
	MSG_WriteByte (&sv.datagram, count);
	MSG_WriteByte (&sv.datagram, color);
	SV_FlushBroadcastMessages();
}

/*
==================
SV_StartEffect

Make sure the event gets sent to all clients
==================
*/
void SV_StartEffect (vec3_t org, int modelindex, int startframe, int framecount, int framerate)
{
	if (modelindex >= 256 || startframe >= 256)
	{
		if (sv.datagram.cursize > MAX_PACKETFRAGMENT-19)
			return;
		MSG_WriteByte (&sv.datagram, svc_effect2);
		MSG_WriteCoord (&sv.datagram, org[0], sv.protocol);
		MSG_WriteCoord (&sv.datagram, org[1], sv.protocol);
		MSG_WriteCoord (&sv.datagram, org[2], sv.protocol);
		MSG_WriteShort (&sv.datagram, modelindex);
		MSG_WriteShort (&sv.datagram, startframe);
		MSG_WriteByte (&sv.datagram, framecount);
		MSG_WriteByte (&sv.datagram, framerate);
	}
	else
	{
		if (sv.datagram.cursize > MAX_PACKETFRAGMENT-17)
			return;
		MSG_WriteByte (&sv.datagram, svc_effect);
		MSG_WriteCoord (&sv.datagram, org[0], sv.protocol);
		MSG_WriteCoord (&sv.datagram, org[1], sv.protocol);
		MSG_WriteCoord (&sv.datagram, org[2], sv.protocol);
		MSG_WriteByte (&sv.datagram, modelindex);
		MSG_WriteByte (&sv.datagram, startframe);
		MSG_WriteByte (&sv.datagram, framecount);
		MSG_WriteByte (&sv.datagram, framerate);
	}
	SV_FlushBroadcastMessages();
}

/*
==================
SV_StartSound

Each entity can have eight independant sound sources, like voice,
weapon, feet, etc.

Channel 0 is an auto-allocate channel, the others override anything
already running on that entity/channel pair.

An attenuation of 0 will play full volume everywhere in the level.
Larger attenuations will drop off.  (max 4 attenuation)

==================
*/
void SV_StartSound (prvm_edict_t *entity, int channel, const char *sample, int volume, float attenuation, qboolean reliable)
{
	sizebuf_t *dest;
	int sound_num, field_mask, i, ent;

	dest = (reliable ? &sv.reliable_datagram : &sv.datagram);

	if (volume < 0 || volume > 255)
	{
		Con_Printf ("SV_StartSound: volume = %i\n", volume);
		return;
	}

	if (attenuation < 0 || attenuation > 4)
	{
		Con_Printf ("SV_StartSound: attenuation = %f\n", attenuation);
		return;
	}

	if (!IS_CHAN(channel))
	{
		Con_Printf ("SV_StartSound: channel = %i\n", channel);
		return;
	}

	channel = CHAN_ENGINE2NET(channel);

	if (sv.datagram.cursize > MAX_PACKETFRAGMENT-21)
		return;

// find precache number for sound
	sound_num = SV_SoundIndex(sample, 1);
	if (!sound_num)
		return;

	ent = PRVM_NUM_FOR_EDICT(entity);

	field_mask = 0;
	if (volume != DEFAULT_SOUND_PACKET_VOLUME)
		field_mask |= SND_VOLUME;
	if (attenuation != DEFAULT_SOUND_PACKET_ATTENUATION)
		field_mask |= SND_ATTENUATION;
	if (ent >= 8192 || channel < 0 || channel > 7)
		field_mask |= SND_LARGEENTITY;
	if (sound_num >= 256)
		field_mask |= SND_LARGESOUND;

// directed messages go only to the entity they are targeted on
	MSG_WriteByte (dest, svc_sound);
	MSG_WriteByte (dest, field_mask);
	if (field_mask & SND_VOLUME)
		MSG_WriteByte (dest, volume);
	if (field_mask & SND_ATTENUATION)
		MSG_WriteByte (dest, (int)(attenuation*64));
	if (field_mask & SND_LARGEENTITY)
	{
		MSG_WriteShort (dest, ent);
		MSG_WriteChar (dest, channel);
	}
	else
		MSG_WriteShort (dest, (ent<<3) | channel);
	if ((field_mask & SND_LARGESOUND) || sv.protocol == PROTOCOL_NEHAHRABJP2)
		MSG_WriteShort (dest, sound_num);
	else
		MSG_WriteByte (dest, sound_num);
	for (i = 0;i < 3;i++)
		MSG_WriteCoord (dest, PRVM_serveredictvector(entity, origin)[i]+0.5*(PRVM_serveredictvector(entity, mins)[i]+PRVM_serveredictvector(entity, maxs)[i]), sv.protocol);

	// TODO do we have to do anything here when dest is &sv.reliable_datagram?
	if(!reliable)
		SV_FlushBroadcastMessages();
}

/*
==================
SV_StartPointSound

Nearly the same logic as SV_StartSound, except an origin
instead of an entity is provided and channel is omitted.

The entity sent to the client is 0 (world) and the channel
is 0 (CHAN_AUTO).  SND_LARGEENTITY will never occur in this
function, therefore the check for it is omitted.

==================
*/
void SV_StartPointSound (vec3_t origin, const char *sample, int volume, float attenuation)
{
	int sound_num, field_mask, i;

	if (volume < 0 || volume > 255)
	{
		Con_Printf ("SV_StartPointSound: volume = %i\n", volume);
		return;
	}

	if (attenuation < 0 || attenuation > 4)
	{
		Con_Printf ("SV_StartPointSound: attenuation = %f\n", attenuation);
		return;
	}

	if (sv.datagram.cursize > MAX_PACKETFRAGMENT-21)
		return;

	// find precache number for sound
	sound_num = SV_SoundIndex(sample, 1);
	if (!sound_num)
		return;

	field_mask = 0;
	if (volume != DEFAULT_SOUND_PACKET_VOLUME)
		field_mask |= SND_VOLUME;
	if (attenuation != DEFAULT_SOUND_PACKET_ATTENUATION)
		field_mask |= SND_ATTENUATION;
	if (sound_num >= 256)
		field_mask |= SND_LARGESOUND;

// directed messages go only to the entity they are targeted on
	MSG_WriteByte (&sv.datagram, svc_sound);
	MSG_WriteByte (&sv.datagram, field_mask);
	if (field_mask & SND_VOLUME)
		MSG_WriteByte (&sv.datagram, volume);
	if (field_mask & SND_ATTENUATION)
		MSG_WriteByte (&sv.datagram, (int)(attenuation*64));
	// Always write entnum 0 for the world entity
	MSG_WriteShort (&sv.datagram, (0<<3) | 0);
	if (field_mask & SND_LARGESOUND)
		MSG_WriteShort (&sv.datagram, sound_num);
	else
		MSG_WriteByte (&sv.datagram, sound_num);
	for (i = 0;i < 3;i++)
		MSG_WriteCoord (&sv.datagram, origin[i], sv.protocol);
	SV_FlushBroadcastMessages();
}

/*
==============================================================================

CLIENT SPAWNING

==============================================================================
*/

/*
================
SV_SendServerinfo

Sends the first message from the server to a connected client.
This will be sent on the initial connection and upon each server load.
================
*/
void SV_SendServerinfo (client_t *client)
{
	int i;
	char message[128];

	// we know that this client has a netconnection and thus is not a bot

	// edicts get reallocated on level changes, so we need to update it here
	client->edict = PRVM_EDICT_NUM((client - svs.clients) + 1);

	// clear cached stuff that depends on the level
	client->weaponmodel[0] = 0;
	client->weaponmodelindex = 0;

	// LordHavoc: clear entityframe tracking
	client->latestframenum = 0;

	// initialize the movetime, so a speedhack can't make use of the time before this client joined
	client->cmd.time = sv.time;

	if (client->entitydatabase)
		EntityFrame_FreeDatabase(client->entitydatabase);
	if (client->entitydatabase4)
		EntityFrame4_FreeDatabase(client->entitydatabase4);
	if (client->entitydatabase5)
		EntityFrame5_FreeDatabase(client->entitydatabase5);

	memset(client->stats, 0, sizeof(client->stats));
	memset(client->statsdeltabits, 0, sizeof(client->statsdeltabits));

	if (sv.protocol != PROTOCOL_QUAKE && sv.protocol != PROTOCOL_QUAKEDP && sv.protocol != PROTOCOL_NEHAHRAMOVIE && sv.protocol != PROTOCOL_NEHAHRABJP && sv.protocol != PROTOCOL_NEHAHRABJP2 && sv.protocol != PROTOCOL_NEHAHRABJP3)
	{
		if (sv.protocol == PROTOCOL_DARKPLACES1 || sv.protocol == PROTOCOL_DARKPLACES2 || sv.protocol == PROTOCOL_DARKPLACES3)
			client->entitydatabase = EntityFrame_AllocDatabase(sv_mempool);
		else if (sv.protocol == PROTOCOL_DARKPLACES4)
			client->entitydatabase4 = EntityFrame4_AllocDatabase(sv_mempool);
		else
			client->entitydatabase5 = EntityFrame5_AllocDatabase(sv_mempool);
	}

	// reset csqc entity versions
	for (i = 0;i < prog->max_edicts;i++)
	{
		client->csqcentityscope[i] = 0;
		client->csqcentitysendflags[i] = 0xFFFFFF;
		client->csqcentityglobalhistory[i] = 0;
	}
	for (i = 0;i < NUM_CSQCENTITYDB_FRAMES;i++)
	{
		client->csqcentityframehistory[i].num = 0;
		client->csqcentityframehistory[i].framenum = -1;
	}
	client->csqcnumedicts = 0;
	client->csqcentityframehistory_next = 0;

	SZ_Clear (&client->netconnection->message);
	MSG_WriteByte (&client->netconnection->message, svc_print);
	dpsnprintf (message, sizeof (message), "\nServer: %s build %s (progs %i crc)\n", gamename, buildstring, prog->filecrc);
	MSG_WriteString (&client->netconnection->message,message);

	SV_StopDemoRecording(client); // to split up demos into different files
	if(sv_autodemo_perclient.integer && client->netconnection)
	{
		char demofile[MAX_OSPATH];
		char ipaddress[MAX_QPATH];
		size_t i;

		// start a new demo file
		LHNETADDRESS_ToString(&(client->netconnection->peeraddress), ipaddress, sizeof(ipaddress), true);
		for(i = 0; ipaddress[i]; ++i)
			if(!isalnum(ipaddress[i]))
				ipaddress[i] = '-';
		dpsnprintf (demofile, sizeof(demofile), "%s_%s_%d_%s.dem", Sys_TimeString (sv_autodemo_perclient_nameformat.string), sv.worldbasename, PRVM_NUM_FOR_EDICT(client->edict), ipaddress);

		SV_StartDemoRecording(client, demofile, -1);
	}

	//[515]: init csprogs according to version of svprogs, check the crc, etc.
	if (sv.csqc_progname[0])
	{
		Con_DPrintf("sending csqc info to client (\"%s\" with size %i and crc %i)\n", sv.csqc_progname, sv.csqc_progsize, sv.csqc_progcrc);
		MSG_WriteByte (&client->netconnection->message, svc_stufftext);
		MSG_WriteString (&client->netconnection->message, va("csqc_progname %s\n", sv.csqc_progname));
		MSG_WriteByte (&client->netconnection->message, svc_stufftext);
		MSG_WriteString (&client->netconnection->message, va("csqc_progsize %i\n", sv.csqc_progsize));
		MSG_WriteByte (&client->netconnection->message, svc_stufftext);
		MSG_WriteString (&client->netconnection->message, va("csqc_progcrc %i\n", sv.csqc_progcrc));

		if(client->sv_demo_file != NULL)
		{
			int i;
			static char buf[NET_MAXMESSAGE];
			sizebuf_t sb;

			sb.data = (unsigned char *) buf;
			sb.maxsize = sizeof(buf);
			i = 0;
			while(MakeDownloadPacket(sv.csqc_progname, svs.csqc_progdata, sv.csqc_progsize, sv.csqc_progcrc, i++, &sb, sv.protocol))
				SV_WriteDemoMessage(client, &sb, false);
		}

		//[515]: init stufftext string (it is sent before svc_serverinfo)
		if (PRVM_GetString(PRVM_serverglobalstring(SV_InitCmd)))
		{
			MSG_WriteByte (&client->netconnection->message, svc_stufftext);
			MSG_WriteString (&client->netconnection->message, va("%s\n", PRVM_GetString(PRVM_serverglobalstring(SV_InitCmd))));
		}
	}

	//if (sv_allowdownloads.integer)
	// always send the info that the server supports the protocol, even if downloads are forbidden
	// only because of that, the CSQC exception can work
	{
		MSG_WriteByte (&client->netconnection->message, svc_stufftext);
		MSG_WriteString (&client->netconnection->message, "cl_serverextension_download 2\n");
	}

	// send at this time so it's guaranteed to get executed at the right time
	{
		client_t *save;
		save = host_client;
		host_client = client;
		Curl_SendRequirements();
		host_client = save;
	}

	MSG_WriteByte (&client->netconnection->message, svc_serverinfo);
	MSG_WriteLong (&client->netconnection->message, Protocol_NumberForEnum(sv.protocol));
	MSG_WriteByte (&client->netconnection->message, svs.maxclients);

	if (!coop.integer && deathmatch.integer)
		MSG_WriteByte (&client->netconnection->message, GAME_DEATHMATCH);
	else
		MSG_WriteByte (&client->netconnection->message, GAME_COOP);

	MSG_WriteString (&client->netconnection->message,PRVM_GetString(PRVM_serveredictstring(prog->edicts, message)));

	for (i = 1;i < MAX_MODELS && sv.model_precache[i][0];i++)
		MSG_WriteString (&client->netconnection->message, sv.model_precache[i]);
	MSG_WriteByte (&client->netconnection->message, 0);

	for (i = 1;i < MAX_SOUNDS && sv.sound_precache[i][0];i++)
		MSG_WriteString (&client->netconnection->message, sv.sound_precache[i]);
	MSG_WriteByte (&client->netconnection->message, 0);

// send music
	MSG_WriteByte (&client->netconnection->message, svc_cdtrack);
	MSG_WriteByte (&client->netconnection->message, (int)PRVM_serveredictfloat(prog->edicts, sounds));
	MSG_WriteByte (&client->netconnection->message, (int)PRVM_serveredictfloat(prog->edicts, sounds));

// set view
// store this in clientcamera, too
	client->clientcamera = PRVM_NUM_FOR_EDICT(client->edict);
	MSG_WriteByte (&client->netconnection->message, svc_setview);
	MSG_WriteShort (&client->netconnection->message, client->clientcamera);

	MSG_WriteByte (&client->netconnection->message, svc_signonnum);
	MSG_WriteByte (&client->netconnection->message, 1);

	client->spawned = false;		// need prespawn, spawn, etc
	client->sendsignon = 1;			// send this message, and increment to 2, 2 will be set to 0 by the prespawn command

	// clear movement info until client enters the new level properly
	memset(&client->cmd, 0, sizeof(client->cmd));
	client->movesequence = 0;
	client->movement_highestsequence_seen = 0;
	memset(&client->movement_count, 0, sizeof(client->movement_count));
#ifdef NUM_PING_TIMES
	for (i = 0;i < NUM_PING_TIMES;i++)
		client->ping_times[i] = 0;
	client->num_pings = 0;
#endif
	client->ping = 0;

	// allow the client some time to send his keepalives, even if map loading took ages
	client->netconnection->timeout = realtime + net_connecttimeout.value;
}

/*
================
SV_ConnectClient

Initializes a client_t for a new net connection.  This will only be called
once for a player each game, not once for each level change.
================
*/
void SV_ConnectClient (int clientnum, netconn_t *netconnection)
{
	client_t		*client;
	int				i;

	client = svs.clients + clientnum;

// set up the client_t
	if (sv.loadgame)
	{
		float backupparms[NUM_SPAWN_PARMS];
		memcpy(backupparms, client->spawn_parms, sizeof(backupparms));
		memset(client, 0, sizeof(*client));
		memcpy(client->spawn_parms, backupparms, sizeof(backupparms));
	}
	else
		memset(client, 0, sizeof(*client));
	client->active = true;
	client->netconnection = netconnection;

	Con_DPrintf("Client %s connected\n", client->netconnection ? client->netconnection->address : "botclient");

	if(client->netconnection && client->netconnection->crypto.authenticated)
	{
		Con_Printf("%s connection to %s has been established: client is %s@%.*s, I am %.*s@%.*s\n",
				client->netconnection->crypto.use_aes ? "Encrypted" : "Authenticated",
				client->netconnection->address,
				client->netconnection->crypto.client_idfp[0] ? client->netconnection->crypto.client_idfp : "-",
				crypto_keyfp_recommended_length, client->netconnection->crypto.client_keyfp[0] ? client->netconnection->crypto.client_keyfp : "-",
				crypto_keyfp_recommended_length, client->netconnection->crypto.server_idfp[0] ? client->netconnection->crypto.server_idfp : "-",
				crypto_keyfp_recommended_length, client->netconnection->crypto.server_keyfp[0] ? client->netconnection->crypto.server_keyfp : "-"
				);
	}

	strlcpy(client->name, "unconnected", sizeof(client->name));
	strlcpy(client->old_name, "unconnected", sizeof(client->old_name));
	client->spawned = false;
	client->edict = PRVM_EDICT_NUM(clientnum+1);
	if (client->netconnection)
		client->netconnection->message.allowoverflow = true;		// we can catch it
	// prepare the unreliable message buffer
	client->unreliablemsg.data = client->unreliablemsg_data;
	client->unreliablemsg.maxsize = sizeof(client->unreliablemsg_data);
	// updated by receiving "rate" command from client, this is also the default if not using a DP client
	client->rate = 1000000000;
	// no limits for local player
	if (client->netconnection && LHNETADDRESS_GetAddressType(&client->netconnection->peeraddress) == LHNETADDRESSTYPE_LOOP)
		client->rate = 1000000000;
	client->connecttime = realtime;

	if (!sv.loadgame)
	{
		// call the progs to get default spawn parms for the new client
		// set self to world to intentionally cause errors with broken SetNewParms code in some mods
		PRVM_serverglobaledict(self) = 0;
		PRVM_ExecuteProgram (PRVM_serverfunction(SetNewParms), "QC function SetNewParms is missing");
		for (i=0 ; i<NUM_SPAWN_PARMS ; i++)
			client->spawn_parms[i] = (&PRVM_serverglobalfloat(parm1))[i];

		// set up the entity for this client (including .colormap, .team, etc)
		PRVM_ED_ClearEdict(client->edict);
	}

	// don't call SendServerinfo for a fresh botclient because its fields have
	// not been set up by the qc yet
	if (client->netconnection)
		SV_SendServerinfo (client);
	else
		client->spawned = true;
}


/*
===============================================================================

FRAME UPDATES

===============================================================================
*/

/*
=============================================================================

The PVS must include a small area around the client to allow head bobbing
or other small motion on the client side.  Otherwise, a bob might cause an
entity that should be visible to not show up, especially when the bob
crosses a waterline.

=============================================================================
*/

static qboolean SV_PrepareEntityForSending (prvm_edict_t *ent, entity_state_t *cs, int enumber)
{
	int i;
	unsigned int sendflags;
	unsigned int version;
	unsigned int modelindex, effects, flags, glowsize, lightstyle, lightpflags, light[4], specialvisibilityradius;
	unsigned int customizeentityforclient;
	unsigned int sendentity;
	float f;
	float *v;
	vec3_t cullmins, cullmaxs;
	dp_model_t *model;

	// fast path for games that do not use legacy entity networking
	// note: still networks clients even if they are legacy
	sendentity = PRVM_serveredictfunction(ent, SendEntity);
	if (sv_onlycsqcnetworking.integer && !sendentity && enumber > svs.maxclients)
		return false;

	// this 2 billion unit check is actually to detect NAN origins
	// (we really don't want to send those)
	if (!(VectorLength2(PRVM_serveredictvector(ent, origin)) < 2000000000.0*2000000000.0))
		return false;

	// EF_NODRAW prevents sending for any reason except for your own
	// client, so we must keep all clients in this superset
	effects = (unsigned)PRVM_serveredictfloat(ent, effects);

	// we can omit invisible entities with no effects that are not clients
	// LordHavoc: this could kill tags attached to an invisible entity, I
	// just hope we never have to support that case
	i = (int)PRVM_serveredictfloat(ent, modelindex);
	modelindex = (i >= 1 && i < MAX_MODELS && PRVM_serveredictstring(ent, model) && *PRVM_GetString(PRVM_serveredictstring(ent, model)) && sv.models[i]) ? i : 0;

	flags = 0;
	i = (int)(PRVM_serveredictfloat(ent, glow_size) * 0.25f);
	glowsize = (unsigned char)bound(0, i, 255);
	if (PRVM_serveredictfloat(ent, glow_trail))
		flags |= RENDER_GLOWTRAIL;
	if (PRVM_serveredictedict(ent, viewmodelforclient))
		flags |= RENDER_VIEWMODEL;

	v = PRVM_serveredictvector(ent, color);
	f = v[0]*256;
	light[0] = (unsigned short)bound(0, f, 65535);
	f = v[1]*256;
	light[1] = (unsigned short)bound(0, f, 65535);
	f = v[2]*256;
	light[2] = (unsigned short)bound(0, f, 65535);
	f = PRVM_serveredictfloat(ent, light_lev);
	light[3] = (unsigned short)bound(0, f, 65535);
	lightstyle = (unsigned char)PRVM_serveredictfloat(ent, style);
	lightpflags = (unsigned char)PRVM_serveredictfloat(ent, pflags);

	if (gamemode == GAME_TENEBRAE)
	{
		// tenebrae's EF_FULLDYNAMIC conflicts with Q2's EF_NODRAW
		if (effects & 16)
		{
			effects &= ~16;
			lightpflags |= PFLAGS_FULLDYNAMIC;
		}
		// tenebrae's EF_GREEN conflicts with DP's EF_ADDITIVE
		if (effects & 32)
		{
			effects &= ~32;
			light[0] = (int)(0.2*256);
			light[1] = (int)(1.0*256);
			light[2] = (int)(0.2*256);
			light[3] = 200;
			lightpflags |= PFLAGS_FULLDYNAMIC;
		}
	}

	specialvisibilityradius = 0;
	if (lightpflags & PFLAGS_FULLDYNAMIC)
		specialvisibilityradius = max(specialvisibilityradius, light[3]);
	if (glowsize)
		specialvisibilityradius = max(specialvisibilityradius, glowsize * 4);
	if (flags & RENDER_GLOWTRAIL)
		specialvisibilityradius = max(specialvisibilityradius, 100);
	if (effects & (EF_BRIGHTFIELD | EF_MUZZLEFLASH | EF_BRIGHTLIGHT | EF_DIMLIGHT | EF_RED | EF_BLUE | EF_FLAME | EF_STARDUST))
	{
		if (effects & EF_BRIGHTFIELD)
			specialvisibilityradius = max(specialvisibilityradius, 80);
		if (effects & EF_MUZZLEFLASH)
			specialvisibilityradius = max(specialvisibilityradius, 100);
		if (effects & EF_BRIGHTLIGHT)
			specialvisibilityradius = max(specialvisibilityradius, 400);
		if (effects & EF_DIMLIGHT)
			specialvisibilityradius = max(specialvisibilityradius, 200);
		if (effects & EF_RED)
			specialvisibilityradius = max(specialvisibilityradius, 200);
		if (effects & EF_BLUE)
			specialvisibilityradius = max(specialvisibilityradius, 200);
		if (effects & EF_FLAME)
			specialvisibilityradius = max(specialvisibilityradius, 250);
		if (effects & EF_STARDUST)
			specialvisibilityradius = max(specialvisibilityradius, 100);
	}

	// early culling checks
	// (final culling is done by SV_MarkWriteEntityStateToClient)
	customizeentityforclient = PRVM_serveredictfunction(ent, customizeentityforclient);
	if (!customizeentityforclient && enumber > svs.maxclients && (!modelindex && !specialvisibilityradius))
		return false;

	*cs = defaultstate;
	cs->active = ACTIVE_NETWORK;
	cs->number = enumber;
	VectorCopy(PRVM_serveredictvector(ent, origin), cs->origin);
	VectorCopy(PRVM_serveredictvector(ent, angles), cs->angles);
	cs->flags = flags;
	cs->effects = effects;
	cs->colormap = (unsigned)PRVM_serveredictfloat(ent, colormap);
	cs->modelindex = modelindex;
	cs->skin = (unsigned)PRVM_serveredictfloat(ent, skin);
	cs->frame = (unsigned)PRVM_serveredictfloat(ent, frame);
	cs->viewmodelforclient = PRVM_serveredictedict(ent, viewmodelforclient);
	cs->exteriormodelforclient = PRVM_serveredictedict(ent, exteriormodeltoclient);
	cs->nodrawtoclient = PRVM_serveredictedict(ent, nodrawtoclient);
	cs->drawonlytoclient = PRVM_serveredictedict(ent, drawonlytoclient);
	cs->customizeentityforclient = customizeentityforclient;
	cs->tagentity = PRVM_serveredictedict(ent, tag_entity);
	cs->tagindex = (unsigned char)PRVM_serveredictfloat(ent, tag_index);
	cs->glowsize = glowsize;
	cs->traileffectnum = PRVM_serveredictfloat(ent, traileffectnum);

	// don't need to init cs->colormod because the defaultstate did that for us
	//cs->colormod[0] = cs->colormod[1] = cs->colormod[2] = 32;
	v = PRVM_serveredictvector(ent, colormod);
	if (VectorLength2(v))
	{
		i = (int)(v[0] * 32.0f);cs->colormod[0] = bound(0, i, 255);
		i = (int)(v[1] * 32.0f);cs->colormod[1] = bound(0, i, 255);
		i = (int)(v[2] * 32.0f);cs->colormod[2] = bound(0, i, 255);
	}

	// don't need to init cs->glowmod because the defaultstate did that for us
	//cs->glowmod[0] = cs->glowmod[1] = cs->glowmod[2] = 32;
	v = PRVM_serveredictvector(ent, glowmod);
	if (VectorLength2(v))
	{
		i = (int)(v[0] * 32.0f);cs->glowmod[0] = bound(0, i, 255);
		i = (int)(v[1] * 32.0f);cs->glowmod[1] = bound(0, i, 255);
		i = (int)(v[2] * 32.0f);cs->glowmod[2] = bound(0, i, 255);
	}

	cs->modelindex = modelindex;

	cs->alpha = 255;
	f = (PRVM_serveredictfloat(ent, alpha) * 255.0f);
	if (f)
	{
		i = (int)f;
		cs->alpha = (unsigned char)bound(0, i, 255);
	}
	// halflife
	f = (PRVM_serveredictfloat(ent, renderamt));
	if (f)
	{
		i = (int)f;
		cs->alpha = (unsigned char)bound(0, i, 255);
	}

	cs->scale = 16;
	f = (PRVM_serveredictfloat(ent, scale) * 16.0f);
	if (f)
	{
		i = (int)f;
		cs->scale = (unsigned char)bound(0, i, 255);
	}

	cs->glowcolor = 254;
	f = PRVM_serveredictfloat(ent, glow_color);
	if (f)
		cs->glowcolor = (int)f;

	if (PRVM_serveredictfloat(ent, fullbright))
		cs->effects |= EF_FULLBRIGHT;

	f = PRVM_serveredictfloat(ent, modelflags);
	if (f)
		cs->effects |= ((unsigned int)f & 0xff) << 24;

	if (PRVM_serveredictfloat(ent, movetype) == MOVETYPE_STEP)
		cs->flags |= RENDER_STEP;
	if (cs->number != sv.writeentitiestoclient_cliententitynumber && (cs->effects & EF_LOWPRECISION) && cs->origin[0] >= -32768 && cs->origin[1] >= -32768 && cs->origin[2] >= -32768 && cs->origin[0] <= 32767 && cs->origin[1] <= 32767 && cs->origin[2] <= 32767)
		cs->flags |= RENDER_LOWPRECISION;
	if (PRVM_serveredictfloat(ent, colormap) >= 1024)
		cs->flags |= RENDER_COLORMAPPED;
	if (cs->viewmodelforclient)
		cs->flags |= RENDER_VIEWMODEL; // show relative to the view

	if (PRVM_serveredictfloat(ent, sendcomplexanimation))
	{
		cs->flags |= RENDER_COMPLEXANIMATION;
		if (PRVM_serveredictfloat(ent, skeletonindex) >= 1)
			cs->skeletonobject = ent->priv.server->skeleton;
		cs->framegroupblend[0].frame = PRVM_serveredictfloat(ent, frame);
		cs->framegroupblend[1].frame = PRVM_serveredictfloat(ent, frame2);
		cs->framegroupblend[2].frame = PRVM_serveredictfloat(ent, frame3);
		cs->framegroupblend[3].frame = PRVM_serveredictfloat(ent, frame4);
		cs->framegroupblend[0].start = PRVM_serveredictfloat(ent, frame1time);
		cs->framegroupblend[1].start = PRVM_serveredictfloat(ent, frame2time);
		cs->framegroupblend[2].start = PRVM_serveredictfloat(ent, frame3time);
		cs->framegroupblend[3].start = PRVM_serveredictfloat(ent, frame4time);
		cs->framegroupblend[1].lerp = PRVM_serveredictfloat(ent, lerpfrac);
		cs->framegroupblend[2].lerp = PRVM_serveredictfloat(ent, lerpfrac3);
		cs->framegroupblend[3].lerp = PRVM_serveredictfloat(ent, lerpfrac4);
		cs->framegroupblend[0].lerp = 1.0f - cs->framegroupblend[1].lerp - cs->framegroupblend[2].lerp - cs->framegroupblend[3].lerp;
	}

	cs->light[0] = light[0];
	cs->light[1] = light[1];
	cs->light[2] = light[2];
	cs->light[3] = light[3];
	cs->lightstyle = lightstyle;
	cs->lightpflags = lightpflags;

	cs->specialvisibilityradius = specialvisibilityradius;

	// calculate the visible box of this entity (don't use the physics box
	// as that is often smaller than a model, and would not count
	// specialvisibilityradius)
	if ((model = SV_GetModelByIndex(modelindex)) && (model->type != mod_null))
	{
		float scale = cs->scale * (1.0f / 16.0f);
		if (cs->angles[0] || cs->angles[2]) // pitch and roll
		{
			VectorMA(cs->origin, scale, model->rotatedmins, cullmins);
			VectorMA(cs->origin, scale, model->rotatedmaxs, cullmaxs);
		}
		else if (cs->angles[1] || ((effects | model->effects) & EF_ROTATE))
		{
			VectorMA(cs->origin, scale, model->yawmins, cullmins);
			VectorMA(cs->origin, scale, model->yawmaxs, cullmaxs);
		}
		else
		{
			VectorMA(cs->origin, scale, model->normalmins, cullmins);
			VectorMA(cs->origin, scale, model->normalmaxs, cullmaxs);
		}
	}
	else
	{
		// if there is no model (or it could not be loaded), use the physics box
		VectorAdd(cs->origin, PRVM_serveredictvector(ent, mins), cullmins);
		VectorAdd(cs->origin, PRVM_serveredictvector(ent, maxs), cullmaxs);
	}
	if (specialvisibilityradius)
	{
		cullmins[0] = min(cullmins[0], cs->origin[0] - specialvisibilityradius);
		cullmins[1] = min(cullmins[1], cs->origin[1] - specialvisibilityradius);
		cullmins[2] = min(cullmins[2], cs->origin[2] - specialvisibilityradius);
		cullmaxs[0] = max(cullmaxs[0], cs->origin[0] + specialvisibilityradius);
		cullmaxs[1] = max(cullmaxs[1], cs->origin[1] + specialvisibilityradius);
		cullmaxs[2] = max(cullmaxs[2], cs->origin[2] + specialvisibilityradius);
	}

	// calculate center of bbox for network prioritization purposes
	VectorMAM(0.5f, cullmins, 0.5f, cullmaxs, cs->netcenter);

	// if culling box has moved, update pvs cluster links
	if (!VectorCompare(cullmins, ent->priv.server->cullmins) || !VectorCompare(cullmaxs, ent->priv.server->cullmaxs))
	{
		VectorCopy(cullmins, ent->priv.server->cullmins);
		VectorCopy(cullmaxs, ent->priv.server->cullmaxs);
		// a value of -1 for pvs_numclusters indicates that the links are not
		// cached, and should be re-tested each time, this is the case if the
		// culling box touches too many pvs clusters to store, or if the world
		// model does not support FindBoxClusters
		ent->priv.server->pvs_numclusters = -1;
		if (sv.worldmodel && sv.worldmodel->brush.FindBoxClusters)
		{
			i = sv.worldmodel->brush.FindBoxClusters(sv.worldmodel, cullmins, cullmaxs, MAX_ENTITYCLUSTERS, ent->priv.server->pvs_clusterlist);
			if (i <= MAX_ENTITYCLUSTERS)
				ent->priv.server->pvs_numclusters = i;
		}
	}

	// we need to do some csqc entity upkeep here
	// get self.SendFlags and clear them
	// (to let the QC know that they've been read)
	if (sendentity)
	{
		sendflags = (unsigned int)PRVM_serveredictfloat(ent, SendFlags);
		PRVM_serveredictfloat(ent, SendFlags) = 0;
		// legacy self.Version system
		if ((version = (unsigned int)PRVM_serveredictfloat(ent, Version)))
		{
			if (sv.csqcentityversion[enumber] != version)
				sendflags = 0xFFFFFF;
			sv.csqcentityversion[enumber] = version;
		}
		// move sendflags into the per-client sendflags
		if (sendflags)
			for (i = 0;i < svs.maxclients;i++)
				svs.clients[i].csqcentitysendflags[enumber] |= sendflags;
		// mark it as inactive for non-csqc networking
		cs->active = ACTIVE_SHARED;
	}

	return true;
}

void SV_PrepareEntitiesForSending(void)
{
	int e;
	prvm_edict_t *ent;
	// send all entities that touch the pvs
	sv.numsendentities = 0;
	sv.sendentitiesindex[0] = NULL;
	memset(sv.sendentitiesindex, 0, prog->num_edicts * sizeof(*sv.sendentitiesindex));
	for (e = 1, ent = PRVM_NEXT_EDICT(prog->edicts);e < prog->num_edicts;e++, ent = PRVM_NEXT_EDICT(ent))
	{
		if (!ent->priv.server->free && SV_PrepareEntityForSending(ent, sv.sendentities + sv.numsendentities, e))
		{
			sv.sendentitiesindex[e] = sv.sendentities + sv.numsendentities;
			sv.numsendentities++;
		}
	}
}

#define MAX_LINEOFSIGHTTRACES 64

qboolean SV_CanSeeBox(int numtraces, vec_t enlarge, vec3_t eye, vec3_t entboxmins, vec3_t entboxmaxs)
{
	float pitchsign;
	float alpha;
	float starttransformed[3], endtransformed[3];
	int blocked = 0;
	int traceindex;
	int originalnumtouchedicts;
	int numtouchedicts = 0;
	int touchindex;
	matrix4x4_t matrix, imatrix;
	dp_model_t *model;
	prvm_edict_t *touch;
	static prvm_edict_t *touchedicts[MAX_EDICTS];
	vec3_t boxmins, boxmaxs;
	vec3_t clipboxmins, clipboxmaxs;
	vec3_t endpoints[MAX_LINEOFSIGHTTRACES];

	numtraces = min(numtraces, MAX_LINEOFSIGHTTRACES);

	// expand the box a little
	boxmins[0] = (enlarge+1) * entboxmins[0] - enlarge * entboxmaxs[0];
	boxmaxs[0] = (enlarge+1) * entboxmaxs[0] - enlarge * entboxmins[0];
	boxmins[1] = (enlarge+1) * entboxmins[1] - enlarge * entboxmaxs[1];
	boxmaxs[1] = (enlarge+1) * entboxmaxs[1] - enlarge * entboxmins[1];
	boxmins[2] = (enlarge+1) * entboxmins[2] - enlarge * entboxmaxs[2];
	boxmaxs[2] = (enlarge+1) * entboxmaxs[2] - enlarge * entboxmins[2];

	VectorMAM(0.5f, boxmins, 0.5f, boxmaxs, endpoints[0]);
	for (traceindex = 1;traceindex < numtraces;traceindex++)
		VectorSet(endpoints[traceindex], lhrandom(boxmins[0], boxmaxs[0]), lhrandom(boxmins[1], boxmaxs[1]), lhrandom(boxmins[2], boxmaxs[2]));

	// calculate sweep box for the entire swarm of traces
	VectorCopy(eye, clipboxmins);
	VectorCopy(eye, clipboxmaxs);
	for (traceindex = 0;traceindex < numtraces;traceindex++)
	{
		clipboxmins[0] = min(clipboxmins[0], endpoints[traceindex][0]);
		clipboxmins[1] = min(clipboxmins[1], endpoints[traceindex][1]);
		clipboxmins[2] = min(clipboxmins[2], endpoints[traceindex][2]);
		clipboxmaxs[0] = max(clipboxmaxs[0], endpoints[traceindex][0]);
		clipboxmaxs[1] = max(clipboxmaxs[1], endpoints[traceindex][1]);
		clipboxmaxs[2] = max(clipboxmaxs[2], endpoints[traceindex][2]);
	}

	// get the list of entities in the sweep box
	if (sv_cullentities_trace_entityocclusion.integer)
		numtouchedicts = World_EntitiesInBox(&sv.world, clipboxmins, clipboxmaxs, MAX_EDICTS, touchedicts);
	if (numtouchedicts > MAX_EDICTS)
	{
		// this never happens
		Con_Printf("SV_EntitiesInBox returned %i edicts, max was %i\n", numtouchedicts, MAX_EDICTS);
		numtouchedicts = MAX_EDICTS;
	}
	// iterate the entities found in the sweep box and filter them
	originalnumtouchedicts = numtouchedicts;
	numtouchedicts = 0;
	for (touchindex = 0;touchindex < originalnumtouchedicts;touchindex++)
	{
		touch = touchedicts[touchindex];
		if (PRVM_serveredictfloat(touch, solid) != SOLID_BSP)
			continue;
		model = SV_GetModelFromEdict(touch);
		if (!model || !model->brush.TraceLineOfSight)
			continue;
		// skip obviously transparent entities
		alpha = PRVM_serveredictfloat(touch, alpha);
		if (alpha && alpha < 1)
			continue;
		if ((int)PRVM_serveredictfloat(touch, effects) & EF_ADDITIVE)
			continue;
		touchedicts[numtouchedicts++] = touch;
	}

	// now that we have a filtered list of "interesting" entities, fire each
	// ray against all of them, this gives us an early-out case when something
	// is visible (which it often is)

	for (traceindex = 0;traceindex < numtraces;traceindex++)
	{
		// check world occlusion
		if (sv.worldmodel && sv.worldmodel->brush.TraceLineOfSight)
			if (!sv.worldmodel->brush.TraceLineOfSight(sv.worldmodel, eye, endpoints[traceindex]))
				continue;
		for (touchindex = 0;touchindex < numtouchedicts;touchindex++)
		{
			touch = touchedicts[touchindex];
			model = SV_GetModelFromEdict(touch);
			if(model && model->brush.TraceLineOfSight)
			{
				// get the entity matrix
				pitchsign = SV_GetPitchSign(touch);
				Matrix4x4_CreateFromQuakeEntity(&matrix, PRVM_serveredictvector(touch, origin)[0], PRVM_serveredictvector(touch, origin)[1], PRVM_serveredictvector(touch, origin)[2], pitchsign * PRVM_serveredictvector(touch, angles)[0], PRVM_serveredictvector(touch, angles)[1], PRVM_serveredictvector(touch, angles)[2], 1);
				Matrix4x4_Invert_Simple(&imatrix, &matrix);
				// see if the ray hits this entity
				Matrix4x4_Transform(&imatrix, eye, starttransformed);
				Matrix4x4_Transform(&imatrix, endpoints[traceindex], endtransformed);
				if (!model->brush.TraceLineOfSight(model, starttransformed, endtransformed))
				{
					blocked++;
					break;
				}
			}
		}
		// check if the ray was blocked
		if (touchindex < numtouchedicts)
			continue;
		// return if the ray was not blocked
		return true;
	}

	// no rays survived
	return false;
}

void SV_MarkWriteEntityStateToClient(entity_state_t *s)
{
	int isbmodel;
	dp_model_t *model;
	prvm_edict_t *ed;
	if (sv.sententitiesconsideration[s->number] == sv.sententitiesmark)
		return;
	sv.sententitiesconsideration[s->number] = sv.sententitiesmark;
	sv.writeentitiestoclient_stats_totalentities++;

	if (s->customizeentityforclient)
	{
		PRVM_serverglobaledict(self) = s->number;
		PRVM_serverglobaledict(other) = sv.writeentitiestoclient_cliententitynumber;
		PRVM_ExecuteProgram(s->customizeentityforclient, "customizeentityforclient: NULL function");
		if(!PRVM_G_FLOAT(OFS_RETURN) || !SV_PrepareEntityForSending(PRVM_EDICT_NUM(s->number), s, s->number))
			return;
	}

	// never reject player
	if (s->number != sv.writeentitiestoclient_cliententitynumber)
	{
		// check various rejection conditions
		if (s->nodrawtoclient == sv.writeentitiestoclient_cliententitynumber)
			return;
		if (s->drawonlytoclient && s->drawonlytoclient != sv.writeentitiestoclient_cliententitynumber)
			return;
		if (s->effects & EF_NODRAW)
			return;
		// LordHavoc: only send entities with a model or important effects
		if (!s->modelindex && s->specialvisibilityradius == 0)
			return;

		isbmodel = (model = SV_GetModelByIndex(s->modelindex)) != NULL && model->name[0] == '*';
		// viewmodels don't have visibility checking
		if (s->viewmodelforclient)
		{
			if (s->viewmodelforclient != sv.writeentitiestoclient_cliententitynumber)
				return;
		}
		else if (s->tagentity)
		{
			// tag attached entities simply check their parent
			if (!sv.sendentitiesindex[s->tagentity])
				return;
			SV_MarkWriteEntityStateToClient(sv.sendentitiesindex[s->tagentity]);
			if (sv.sententities[s->tagentity] != sv.sententitiesmark)
				return;
		}
		// always send world submodels in newer protocols because they don't
		// generate much traffic (in old protocols they hog bandwidth)
		// but only if sv_cullentities_nevercullbmodels is off
		else if (!(s->effects & EF_NODEPTHTEST) && (!isbmodel || !sv_cullentities_nevercullbmodels.integer || sv.protocol == PROTOCOL_QUAKE || sv.protocol == PROTOCOL_QUAKEDP || sv.protocol == PROTOCOL_NEHAHRAMOVIE))
		{
			// entity has survived every check so far, check if visible
			ed = PRVM_EDICT_NUM(s->number);

			// if not touching a visible leaf
			if (sv_cullentities_pvs.integer && !r_novis.integer && !r_trippy.integer && sv.writeentitiestoclient_pvsbytes)
			{
				if (ed->priv.server->pvs_numclusters < 0)
				{
					// entity too big for clusters list
					if (sv.worldmodel && sv.worldmodel->brush.BoxTouchingPVS && !sv.worldmodel->brush.BoxTouchingPVS(sv.worldmodel, sv.writeentitiestoclient_pvs, ed->priv.server->cullmins, ed->priv.server->cullmaxs))
					{
						sv.writeentitiestoclient_stats_culled_pvs++;
						return;
					}
				}
				else
				{
					int i;
					// check cached clusters list
					for (i = 0;i < ed->priv.server->pvs_numclusters;i++)
						if (CHECKPVSBIT(sv.writeentitiestoclient_pvs, ed->priv.server->pvs_clusterlist[i]))
							break;
					if (i == ed->priv.server->pvs_numclusters)
					{
						sv.writeentitiestoclient_stats_culled_pvs++;
						return;
					}
				}
			}

			// or not seen by random tracelines
			if (sv_cullentities_trace.integer && !isbmodel && sv.worldmodel->brush.TraceLineOfSight && !r_trippy.integer)
			{
				int samples =
					s->number <= svs.maxclients
						? sv_cullentities_trace_samples_players.integer
						:
					s->specialvisibilityradius
						? sv_cullentities_trace_samples_extra.integer
						: sv_cullentities_trace_samples.integer;
				float enlarge = sv_cullentities_trace_enlarge.value;

				if(samples > 0)
				{
					int eyeindex;
					for (eyeindex = 0;eyeindex < sv.writeentitiestoclient_numeyes;eyeindex++)
						if(SV_CanSeeBox(samples, enlarge, sv.writeentitiestoclient_eyes[eyeindex], ed->priv.server->cullmins, ed->priv.server->cullmaxs))
							break;
					if(eyeindex < sv.writeentitiestoclient_numeyes)
						svs.clients[sv.writeentitiestoclient_clientnumber].visibletime[s->number] =
							realtime + (
								s->number <= svs.maxclients
									? sv_cullentities_trace_delay_players.value
									: sv_cullentities_trace_delay.value
							);
					else if (realtime > svs.clients[sv.writeentitiestoclient_clientnumber].visibletime[s->number])
					{
						sv.writeentitiestoclient_stats_culled_trace++;
						return;
					}
				}
			}
		}
	}

	// this just marks it for sending
	// FIXME: it would be more efficient to send here, but the entity
	// compressor isn't that flexible
	sv.writeentitiestoclient_stats_visibleentities++;
	sv.sententities[s->number] = sv.sententitiesmark;
}

#if MAX_LEVELNETWORKEYES > 0
#define MAX_EYE_RECURSION 1 // increase if recursion gets supported by portals
void SV_AddCameraEyes(void)
{
	int e, i, j, k;
	prvm_edict_t *ed;
	static int cameras[MAX_LEVELNETWORKEYES];
	static vec3_t camera_origins[MAX_LEVELNETWORKEYES];
	static int eye_levels[MAX_CLIENTNETWORKEYES];
	int n_cameras = 0;
	vec3_t mi, ma;

	for(i = 0; i < sv.writeentitiestoclient_numeyes; ++i)
		eye_levels[i] = 0;

	// check line of sight to portal entities and add them to PVS
	for (e = 1, ed = PRVM_NEXT_EDICT(prog->edicts);e < prog->num_edicts;e++, ed = PRVM_NEXT_EDICT(ed))
	{
		if (!ed->priv.server->free)
		{
			if(PRVM_serveredictfunction(ed, camera_transform))
			{
				PRVM_serverglobaledict(self) = e;
				PRVM_serverglobaledict(other) = sv.writeentitiestoclient_cliententitynumber;
				VectorCopy(sv.writeentitiestoclient_eyes[0], PRVM_serverglobalvector(trace_endpos));
				VectorCopy(sv.writeentitiestoclient_eyes[0], PRVM_G_VECTOR(OFS_PARM0));
				VectorClear(PRVM_G_VECTOR(OFS_PARM1));
				PRVM_ExecuteProgram(PRVM_serveredictfunction(ed, camera_transform), "QC function e.camera_transform is missing");
				if(!VectorCompare(PRVM_serverglobalvector(trace_endpos), sv.writeentitiestoclient_eyes[0]))
				{
					VectorCopy(PRVM_serverglobalvector(trace_endpos), camera_origins[n_cameras]);
					cameras[n_cameras] = e;
					++n_cameras;
					if(n_cameras >= MAX_LEVELNETWORKEYES)
						break;
				}
			}
		}
	}

	if(!n_cameras)
		return;

	// i is loop counter, is reset to 0 when an eye got added
	// j is camera index to check
	for(i = 0, j = 0; sv.writeentitiestoclient_numeyes < MAX_CLIENTNETWORKEYES && i < n_cameras; ++i, ++j, j %= n_cameras)
	{
		if(!cameras[j])
			continue;
		ed = PRVM_EDICT_NUM(cameras[j]);
		VectorAdd(PRVM_serveredictvector(ed, origin), PRVM_serveredictvector(ed, mins), mi);
		VectorAdd(PRVM_serveredictvector(ed, origin), PRVM_serveredictvector(ed, maxs), ma);
		for(k = 0; k < sv.writeentitiestoclient_numeyes; ++k)
		if(eye_levels[k] <= MAX_EYE_RECURSION)
		{
			if(SV_CanSeeBox(sv_cullentities_trace_samples.integer, sv_cullentities_trace_enlarge.value, sv.writeentitiestoclient_eyes[k], mi, ma))
			{
				eye_levels[sv.writeentitiestoclient_numeyes] = eye_levels[k] + 1;
				VectorCopy(camera_origins[j], sv.writeentitiestoclient_eyes[sv.writeentitiestoclient_numeyes]);
				// Con_Printf("added eye %d: %f %f %f because we can see %f %f %f .. %f %f %f from eye %d\n", j, sv.writeentitiestoclient_eyes[sv.writeentitiestoclient_numeyes][0], sv.writeentitiestoclient_eyes[sv.writeentitiestoclient_numeyes][1], sv.writeentitiestoclient_eyes[sv.writeentitiestoclient_numeyes][2], mi[0], mi[1], mi[2], ma[0], ma[1], ma[2], k);
				sv.writeentitiestoclient_numeyes++;
				cameras[j] = 0;
				i = 0;
				break;
			}
		}
	}
}
#else
void SV_AddCameraEyes(void)
{
}
#endif

void SV_WriteEntitiesToClient(client_t *client, prvm_edict_t *clent, sizebuf_t *msg, int maxsize)
{
	qboolean need_empty = false;
	int i, numsendstates, numcsqcsendstates;
	entity_state_t *s;
	prvm_edict_t *camera;
	qboolean success;
	vec3_t eye;

	// if there isn't enough space to accomplish anything, skip it
	if (msg->cursize + 25 > maxsize)
		return;

	sv.writeentitiestoclient_msg = msg;
	sv.writeentitiestoclient_clientnumber = client - svs.clients;

	sv.writeentitiestoclient_stats_culled_pvs = 0;
	sv.writeentitiestoclient_stats_culled_trace = 0;
	sv.writeentitiestoclient_stats_visibleentities = 0;
	sv.writeentitiestoclient_stats_totalentities = 0;
	sv.writeentitiestoclient_numeyes = 0;

	// get eye location
	sv.writeentitiestoclient_cliententitynumber = PRVM_EDICT_TO_PROG(clent); // LordHavoc: for comparison purposes
	camera = PRVM_EDICT_NUM( client->clientcamera );
	VectorAdd(PRVM_serveredictvector(camera, origin), PRVM_serveredictvector(clent, view_ofs), eye);
	sv.writeentitiestoclient_pvsbytes = 0;
	// get the PVS values for the eye location, later FatPVS calls will merge
	if (sv.worldmodel && sv.worldmodel->brush.FatPVS)
		sv.writeentitiestoclient_pvsbytes = sv.worldmodel->brush.FatPVS(sv.worldmodel, eye, 8, sv.writeentitiestoclient_pvs, sizeof(sv.writeentitiestoclient_pvs), sv.writeentitiestoclient_pvsbytes != 0);

	// add the eye to a list for SV_CanSeeBox tests
	VectorCopy(eye, sv.writeentitiestoclient_eyes[sv.writeentitiestoclient_numeyes]);
	sv.writeentitiestoclient_numeyes++;

	// calculate predicted eye origin for SV_CanSeeBox tests
	if (sv_cullentities_trace_prediction.integer)
	{
		vec_t predtime = bound(0, host_client->ping, sv_cullentities_trace_prediction_time.value);
		vec3_t predeye;
		VectorMA(eye, predtime, PRVM_serveredictvector(camera, velocity), predeye);
		if (SV_CanSeeBox(1, 0, eye, predeye, predeye))
		{
			VectorCopy(predeye, sv.writeentitiestoclient_eyes[sv.writeentitiestoclient_numeyes]);
			sv.writeentitiestoclient_numeyes++;
		}
		//if (!sv.writeentitiestoclient_useprediction)
		//	Con_DPrintf("Trying to walk into solid in a pingtime... not predicting for culling\n");
	}

	SV_AddCameraEyes();

	// build PVS from the new eyes
	if (sv.worldmodel && sv.worldmodel->brush.FatPVS)
		for(i = 1; i < sv.writeentitiestoclient_numeyes; ++i)
			sv.writeentitiestoclient_pvsbytes = sv.worldmodel->brush.FatPVS(sv.worldmodel, sv.writeentitiestoclient_eyes[i], 8, sv.writeentitiestoclient_pvs, sizeof(sv.writeentitiestoclient_pvs), sv.writeentitiestoclient_pvsbytes != 0);

	sv.sententitiesmark++;

	for (i = 0;i < sv.numsendentities;i++)
		SV_MarkWriteEntityStateToClient(sv.sendentities + i);

	numsendstates = 0;
	numcsqcsendstates = 0;
	for (i = 0;i < sv.numsendentities;i++)
	{
		s = &sv.sendentities[i];
		if (sv.sententities[s->number] == sv.sententitiesmark)
		{
			if(s->active == ACTIVE_NETWORK)
			{
				if (s->exteriormodelforclient)
				{
					if (s->exteriormodelforclient == sv.writeentitiestoclient_cliententitynumber)
						s->flags |= RENDER_EXTERIORMODEL;
					else
						s->flags &= ~RENDER_EXTERIORMODEL;
				}
				sv.writeentitiestoclient_sendstates[numsendstates++] = s;
			}
			else if(sv.sendentities[i].active == ACTIVE_SHARED)
				sv.writeentitiestoclient_csqcsendstates[numcsqcsendstates++] = s->number;
			else
				Con_Printf("entity %d is in sv.sendentities and marked, but not active, please breakpoint me\n", s->number);
		}
	}

	if (sv_cullentities_stats.integer)
		Con_Printf("client \"%s\" entities: %d total, %d visible, %d culled by: %d pvs %d trace\n", client->name, sv.writeentitiestoclient_stats_totalentities, sv.writeentitiestoclient_stats_visibleentities, sv.writeentitiestoclient_stats_culled_pvs + sv.writeentitiestoclient_stats_culled_trace, sv.writeentitiestoclient_stats_culled_pvs, sv.writeentitiestoclient_stats_culled_trace);

	if(client->entitydatabase5)
		need_empty = EntityFrameCSQC_WriteFrame(msg, maxsize, numcsqcsendstates, sv.writeentitiestoclient_csqcsendstates, client->entitydatabase5->latestframenum + 1);
	else
		EntityFrameCSQC_WriteFrame(msg, maxsize, numcsqcsendstates, sv.writeentitiestoclient_csqcsendstates, 0);

	if(client->num_skippedentityframes >= 10)
		need_empty = true; // force every 10th frame to be not empty (or cl_movement replay takes too long)

	if (client->entitydatabase5)
		success = EntityFrame5_WriteFrame(msg, maxsize, client->entitydatabase5, numsendstates, sv.writeentitiestoclient_sendstates, client - svs.clients + 1, client->movesequence, need_empty);
	else if (client->entitydatabase4)
	{
		success = EntityFrame4_WriteFrame(msg, maxsize, client->entitydatabase4, numsendstates, sv.writeentitiestoclient_sendstates);
		Protocol_WriteStatsReliable();
	}
	else if (client->entitydatabase)
	{
		success = EntityFrame_WriteFrame(msg, maxsize, client->entitydatabase, numsendstates, sv.writeentitiestoclient_sendstates, client - svs.clients + 1);
		Protocol_WriteStatsReliable();
	}
	else
	{
		success = EntityFrameQuake_WriteFrame(msg, maxsize, numsendstates, sv.writeentitiestoclient_sendstates);
		Protocol_WriteStatsReliable();
	}

	if(success)
		client->num_skippedentityframes = 0;
	else
		++client->num_skippedentityframes;
}

/*
=============
SV_CleanupEnts

=============
*/
static void SV_CleanupEnts (void)
{
	int		e;
	prvm_edict_t	*ent;

	ent = PRVM_NEXT_EDICT(prog->edicts);
	for (e=1 ; e<prog->num_edicts ; e++, ent = PRVM_NEXT_EDICT(ent))
		PRVM_serveredictfloat(ent, effects) = (int)PRVM_serveredictfloat(ent, effects) & ~EF_MUZZLEFLASH;
}

/*
==================
SV_WriteClientdataToMessage

==================
*/
void SV_WriteClientdataToMessage (client_t *client, prvm_edict_t *ent, sizebuf_t *msg, int *stats)
{
	int		bits;
	int		i;
	prvm_edict_t	*other;
	int		items;
	vec3_t	punchvector;
	int		viewzoom;
	const char *s;
	float	*statsf = (float *)stats;
	float gravity;

//
// send a damage message
//
	if (PRVM_serveredictfloat(ent, dmg_take) || PRVM_serveredictfloat(ent, dmg_save))
	{
		other = PRVM_PROG_TO_EDICT(PRVM_serveredictedict(ent, dmg_inflictor));
		MSG_WriteByte (msg, svc_damage);
		MSG_WriteByte (msg, (int)PRVM_serveredictfloat(ent, dmg_save));
		MSG_WriteByte (msg, (int)PRVM_serveredictfloat(ent, dmg_take));
		for (i=0 ; i<3 ; i++)
			MSG_WriteCoord (msg, PRVM_serveredictvector(other, origin)[i] + 0.5*(PRVM_serveredictvector(other, mins)[i] + PRVM_serveredictvector(other, maxs)[i]), sv.protocol);

		PRVM_serveredictfloat(ent, dmg_take) = 0;
		PRVM_serveredictfloat(ent, dmg_save) = 0;
	}

//
// send the current viewpos offset from the view entity
//
	SV_SetIdealPitch ();		// how much to look up / down ideally

// a fixangle might get lost in a dropped packet.  Oh well.
	if(PRVM_serveredictfloat(ent, fixangle))
	{
		// angle fixing was requested by global thinking code...
		// so store the current angles for later use
		memcpy(host_client->fixangle_angles, PRVM_serveredictvector(ent, angles), sizeof(host_client->fixangle_angles));
		host_client->fixangle_angles_set = TRUE;

		// and clear fixangle for the next frame
		PRVM_serveredictfloat(ent, fixangle) = 0;
	}

	if (host_client->fixangle_angles_set)
	{
		MSG_WriteByte (msg, svc_setangle);
		for (i=0 ; i < 3 ; i++)
			MSG_WriteAngle (msg, host_client->fixangle_angles[i], sv.protocol);
		host_client->fixangle_angles_set = FALSE;
	}

	// stuff the sigil bits into the high bits of items for sbar, or else
	// mix in items2
	if (gamemode == GAME_HIPNOTIC || gamemode == GAME_ROGUE)
		items = (int)PRVM_serveredictfloat(ent, items) | ((int)PRVM_serveredictfloat(ent, items2) << 23);
	else
		items = (int)PRVM_serveredictfloat(ent, items) | ((int)PRVM_serverglobalfloat(serverflags) << 28);

	VectorCopy(PRVM_serveredictvector(ent, punchvector), punchvector);

	// cache weapon model name and index in client struct to save time
	// (this search can be almost 1% of cpu time!)
	s = PRVM_GetString(PRVM_serveredictstring(ent, weaponmodel));
	if (strcmp(s, client->weaponmodel))
	{
		strlcpy(client->weaponmodel, s, sizeof(client->weaponmodel));
		client->weaponmodelindex = SV_ModelIndex(s, 1);
	}

	viewzoom = (int)(PRVM_serveredictfloat(ent, viewzoom) * 255.0f);
	if (viewzoom == 0)
		viewzoom = 255;

	bits = 0;

	if ((int)PRVM_serveredictfloat(ent, flags) & FL_ONGROUND)
		bits |= SU_ONGROUND;
	if (PRVM_serveredictfloat(ent, waterlevel) >= 2)
		bits |= SU_INWATER;
	if (PRVM_serveredictfloat(ent, idealpitch))
		bits |= SU_IDEALPITCH;

	for (i=0 ; i<3 ; i++)
	{
		if (PRVM_serveredictvector(ent, punchangle)[i])
			bits |= (SU_PUNCH1<<i);
		if (sv.protocol != PROTOCOL_QUAKE && sv.protocol != PROTOCOL_QUAKEDP && sv.protocol != PROTOCOL_NEHAHRAMOVIE && sv.protocol != PROTOCOL_NEHAHRABJP && sv.protocol != PROTOCOL_NEHAHRABJP2 && sv.protocol != PROTOCOL_NEHAHRABJP3)
			if (punchvector[i])
				bits |= (SU_PUNCHVEC1<<i);
		if (PRVM_serveredictvector(ent, velocity)[i])
			bits |= (SU_VELOCITY1<<i);
	}

	gravity = PRVM_serveredictfloat(ent, gravity);if (!gravity) gravity = 1.0f;

	memset(stats, 0, sizeof(int[MAX_CL_STATS]));
	stats[STAT_VIEWHEIGHT] = (int)PRVM_serveredictvector(ent, view_ofs)[2];
	stats[STAT_ITEMS] = items;
	stats[STAT_WEAPONFRAME] = (int)PRVM_serveredictfloat(ent, weaponframe);
	stats[STAT_ARMOR] = (int)PRVM_serveredictfloat(ent, armorvalue);
	stats[STAT_WEAPON] = client->weaponmodelindex;
	stats[STAT_HEALTH] = (int)PRVM_serveredictfloat(ent, health);
	stats[STAT_AMMO] = (int)PRVM_serveredictfloat(ent, currentammo);
	stats[STAT_SHELLS] = (int)PRVM_serveredictfloat(ent, ammo_shells);
	stats[STAT_NAILS] = (int)PRVM_serveredictfloat(ent, ammo_nails);
	stats[STAT_ROCKETS] = (int)PRVM_serveredictfloat(ent, ammo_rockets);
	stats[STAT_CELLS] = (int)PRVM_serveredictfloat(ent, ammo_cells);
	stats[STAT_ACTIVEWEAPON] = (int)PRVM_serveredictfloat(ent, weapon);
	stats[STAT_VIEWZOOM] = viewzoom;
	stats[STAT_TOTALSECRETS] = (int)PRVM_serverglobalfloat(total_secrets);
	stats[STAT_TOTALMONSTERS] = (int)PRVM_serverglobalfloat(total_monsters);
	// the QC bumps these itself by sending svc_'s, so we have to keep them
	// zero or they'll be corrected by the engine
	//stats[STAT_SECRETS] = PRVM_serverglobalfloat(found_secrets);
	//stats[STAT_MONSTERS] = PRVM_serverglobalfloat(killed_monsters);

	// movement settings for prediction
	// note: these are not sent in protocols with lower MAX_CL_STATS limits
	stats[STAT_MOVEFLAGS] = MOVEFLAG_VALID
		| (sv_gameplayfix_q2airaccelerate.integer ? MOVEFLAG_Q2AIRACCELERATE : 0)
		| (sv_gameplayfix_nogravityonground.integer ? MOVEFLAG_NOGRAVITYONGROUND : 0)
		| (sv_gameplayfix_gravityunaffectedbyticrate.integer ? MOVEFLAG_GRAVITYUNAFFECTEDBYTICRATE : 0)
	;
	statsf[STAT_MOVEVARS_TICRATE] = sys_ticrate.value;
	statsf[STAT_MOVEVARS_TIMESCALE] = slowmo.value;
	statsf[STAT_MOVEVARS_GRAVITY] = sv_gravity.value;
	statsf[STAT_MOVEVARS_STOPSPEED] = sv_stopspeed.value;
	statsf[STAT_MOVEVARS_MAXSPEED] = sv_maxspeed.value;
	statsf[STAT_MOVEVARS_SPECTATORMAXSPEED] = sv_maxspeed.value; // FIXME: QW has a separate cvar for this
	statsf[STAT_MOVEVARS_ACCELERATE] = sv_accelerate.value;
	statsf[STAT_MOVEVARS_AIRACCELERATE] = sv_airaccelerate.value >= 0 ? sv_airaccelerate.value : sv_accelerate.value;
	statsf[STAT_MOVEVARS_WATERACCELERATE] = sv_wateraccelerate.value >= 0 ? sv_wateraccelerate.value : sv_accelerate.value;
	statsf[STAT_MOVEVARS_ENTGRAVITY] = gravity;
	statsf[STAT_MOVEVARS_JUMPVELOCITY] = sv_jumpvelocity.value;
	statsf[STAT_MOVEVARS_EDGEFRICTION] = sv_edgefriction.value;
	statsf[STAT_MOVEVARS_MAXAIRSPEED] = sv_maxairspeed.value;
	statsf[STAT_MOVEVARS_STEPHEIGHT] = sv_stepheight.value;
	statsf[STAT_MOVEVARS_AIRACCEL_QW] = sv_airaccel_qw.value;
	statsf[STAT_MOVEVARS_AIRACCEL_QW_STRETCHFACTOR] = sv_airaccel_qw_stretchfactor.value;
	statsf[STAT_MOVEVARS_AIRACCEL_SIDEWAYS_FRICTION] = sv_airaccel_sideways_friction.value;
	statsf[STAT_MOVEVARS_FRICTION] = sv_friction.value;
	statsf[STAT_MOVEVARS_WATERFRICTION] = sv_waterfriction.value >= 0 ? sv_waterfriction.value : sv_friction.value;
	statsf[STAT_MOVEVARS_AIRSTOPACCELERATE] = sv_airstopaccelerate.value;
	statsf[STAT_MOVEVARS_AIRSTRAFEACCELERATE] = sv_airstrafeaccelerate.value;
	statsf[STAT_MOVEVARS_MAXAIRSTRAFESPEED] = sv_maxairstrafespeed.value;
	statsf[STAT_MOVEVARS_AIRSTRAFEACCEL_QW] = sv_airstrafeaccel_qw.value;
	statsf[STAT_MOVEVARS_AIRCONTROL] = sv_aircontrol.value;
	statsf[STAT_MOVEVARS_AIRCONTROL_POWER] = sv_aircontrol_power.value;
	statsf[STAT_MOVEVARS_AIRCONTROL_PENALTY] = sv_aircontrol_penalty.value;
	statsf[STAT_MOVEVARS_WARSOWBUNNY_AIRFORWARDACCEL] = sv_warsowbunny_airforwardaccel.value;
	statsf[STAT_MOVEVARS_WARSOWBUNNY_ACCEL] = sv_warsowbunny_accel.value;
	statsf[STAT_MOVEVARS_WARSOWBUNNY_TOPSPEED] = sv_warsowbunny_topspeed.value;
	statsf[STAT_MOVEVARS_WARSOWBUNNY_TURNACCEL] = sv_warsowbunny_turnaccel.value;
	statsf[STAT_MOVEVARS_WARSOWBUNNY_BACKTOSIDERATIO] = sv_warsowbunny_backtosideratio.value;
	statsf[STAT_MOVEVARS_AIRSPEEDLIMIT_NONQW] = sv_airspeedlimit_nonqw.value;
	statsf[STAT_FRAGLIMIT] = fraglimit.value;
	statsf[STAT_TIMELIMIT] = timelimit.value;

	if (sv.protocol == PROTOCOL_QUAKE || sv.protocol == PROTOCOL_QUAKEDP || sv.protocol == PROTOCOL_NEHAHRAMOVIE || sv.protocol == PROTOCOL_NEHAHRABJP || sv.protocol == PROTOCOL_NEHAHRABJP2 || sv.protocol == PROTOCOL_NEHAHRABJP3 || sv.protocol == PROTOCOL_DARKPLACES1 || sv.protocol == PROTOCOL_DARKPLACES2 || sv.protocol == PROTOCOL_DARKPLACES3 || sv.protocol == PROTOCOL_DARKPLACES4 || sv.protocol == PROTOCOL_DARKPLACES5)
	{
		if (stats[STAT_VIEWHEIGHT] != DEFAULT_VIEWHEIGHT) bits |= SU_VIEWHEIGHT;
		bits |= SU_ITEMS;
		if (stats[STAT_WEAPONFRAME]) bits |= SU_WEAPONFRAME;
		if (stats[STAT_ARMOR]) bits |= SU_ARMOR;
		bits |= SU_WEAPON;
		// FIXME: which protocols support this?  does PROTOCOL_DARKPLACES3 support viewzoom?
		if (sv.protocol == PROTOCOL_DARKPLACES2 || sv.protocol == PROTOCOL_DARKPLACES3 || sv.protocol == PROTOCOL_DARKPLACES4 || sv.protocol == PROTOCOL_DARKPLACES5)
			if (viewzoom != 255)
				bits |= SU_VIEWZOOM;
	}

	if (bits >= 65536)
		bits |= SU_EXTEND1;
	if (bits >= 16777216)
		bits |= SU_EXTEND2;

	// send the data
	MSG_WriteByte (msg, svc_clientdata);
	MSG_WriteShort (msg, bits);
	if (bits & SU_EXTEND1)
		MSG_WriteByte(msg, bits >> 16);
	if (bits & SU_EXTEND2)
		MSG_WriteByte(msg, bits >> 24);

	if (bits & SU_VIEWHEIGHT)
		MSG_WriteChar (msg, stats[STAT_VIEWHEIGHT]);

	if (bits & SU_IDEALPITCH)
		MSG_WriteChar (msg, (int)PRVM_serveredictfloat(ent, idealpitch));

	for (i=0 ; i<3 ; i++)
	{
		if (bits & (SU_PUNCH1<<i))
		{
			if (sv.protocol == PROTOCOL_QUAKE || sv.protocol == PROTOCOL_QUAKEDP || sv.protocol == PROTOCOL_NEHAHRAMOVIE || sv.protocol == PROTOCOL_NEHAHRABJP || sv.protocol == PROTOCOL_NEHAHRABJP2 || sv.protocol == PROTOCOL_NEHAHRABJP3)
				MSG_WriteChar(msg, (int)PRVM_serveredictvector(ent, punchangle)[i]);
			else
				MSG_WriteAngle16i(msg, PRVM_serveredictvector(ent, punchangle)[i]);
		}
		if (bits & (SU_PUNCHVEC1<<i))
		{
			if (sv.protocol == PROTOCOL_DARKPLACES1 || sv.protocol == PROTOCOL_DARKPLACES2 || sv.protocol == PROTOCOL_DARKPLACES3 || sv.protocol == PROTOCOL_DARKPLACES4)
				MSG_WriteCoord16i(msg, punchvector[i]);
			else
				MSG_WriteCoord32f(msg, punchvector[i]);
		}
		if (bits & (SU_VELOCITY1<<i))
		{
			if (sv.protocol == PROTOCOL_QUAKE || sv.protocol == PROTOCOL_QUAKEDP || sv.protocol == PROTOCOL_NEHAHRAMOVIE || sv.protocol == PROTOCOL_NEHAHRABJP || sv.protocol == PROTOCOL_NEHAHRABJP2 || sv.protocol == PROTOCOL_NEHAHRABJP3 || sv.protocol == PROTOCOL_DARKPLACES1 || sv.protocol == PROTOCOL_DARKPLACES2 || sv.protocol == PROTOCOL_DARKPLACES3 || sv.protocol == PROTOCOL_DARKPLACES4)
				MSG_WriteChar(msg, (int)(PRVM_serveredictvector(ent, velocity)[i] * (1.0f / 16.0f)));
			else
				MSG_WriteCoord32f(msg, PRVM_serveredictvector(ent, velocity)[i]);
		}
	}

	if (bits & SU_ITEMS)
		MSG_WriteLong (msg, stats[STAT_ITEMS]);

	if (sv.protocol == PROTOCOL_DARKPLACES5)
	{
		if (bits & SU_WEAPONFRAME)
			MSG_WriteShort (msg, stats[STAT_WEAPONFRAME]);
		if (bits & SU_ARMOR)
			MSG_WriteShort (msg, stats[STAT_ARMOR]);
		if (bits & SU_WEAPON)
			MSG_WriteShort (msg, stats[STAT_WEAPON]);
		MSG_WriteShort (msg, stats[STAT_HEALTH]);
		MSG_WriteShort (msg, stats[STAT_AMMO]);
		MSG_WriteShort (msg, stats[STAT_SHELLS]);
		MSG_WriteShort (msg, stats[STAT_NAILS]);
		MSG_WriteShort (msg, stats[STAT_ROCKETS]);
		MSG_WriteShort (msg, stats[STAT_CELLS]);
		MSG_WriteShort (msg, stats[STAT_ACTIVEWEAPON]);
		if (bits & SU_VIEWZOOM)
			MSG_WriteShort (msg, bound(0, stats[STAT_VIEWZOOM], 65535));
	}
	else if (sv.protocol == PROTOCOL_QUAKE || sv.protocol == PROTOCOL_QUAKEDP || sv.protocol == PROTOCOL_NEHAHRAMOVIE || sv.protocol == PROTOCOL_NEHAHRABJP || sv.protocol == PROTOCOL_NEHAHRABJP2 || sv.protocol == PROTOCOL_NEHAHRABJP3 || sv.protocol == PROTOCOL_DARKPLACES1 || sv.protocol == PROTOCOL_DARKPLACES2 || sv.protocol == PROTOCOL_DARKPLACES3 || sv.protocol == PROTOCOL_DARKPLACES4)
	{
		if (bits & SU_WEAPONFRAME)
			MSG_WriteByte (msg, stats[STAT_WEAPONFRAME]);
		if (bits & SU_ARMOR)
			MSG_WriteByte (msg, stats[STAT_ARMOR]);
		if (bits & SU_WEAPON)
		{
			if (sv.protocol == PROTOCOL_NEHAHRABJP || sv.protocol == PROTOCOL_NEHAHRABJP2 || sv.protocol == PROTOCOL_NEHAHRABJP3)
				MSG_WriteShort (msg, stats[STAT_WEAPON]);
			else
				MSG_WriteByte (msg, stats[STAT_WEAPON]);
		}
		MSG_WriteShort (msg, stats[STAT_HEALTH]);
		MSG_WriteByte (msg, stats[STAT_AMMO]);
		MSG_WriteByte (msg, stats[STAT_SHELLS]);
		MSG_WriteByte (msg, stats[STAT_NAILS]);
		MSG_WriteByte (msg, stats[STAT_ROCKETS]);
		MSG_WriteByte (msg, stats[STAT_CELLS]);
		if (gamemode == GAME_HIPNOTIC || gamemode == GAME_ROGUE || gamemode == GAME_NEXUIZ)
		{
			for (i = 0;i < 32;i++)
				if (stats[STAT_ACTIVEWEAPON] & (1<<i))
					break;
			MSG_WriteByte (msg, i);
		}
		else
			MSG_WriteByte (msg, stats[STAT_ACTIVEWEAPON]);
		if (bits & SU_VIEWZOOM)
		{
			if (sv.protocol == PROTOCOL_DARKPLACES2 || sv.protocol == PROTOCOL_DARKPLACES3 || sv.protocol == PROTOCOL_DARKPLACES4)
				MSG_WriteByte (msg, bound(0, stats[STAT_VIEWZOOM], 255));
			else
				MSG_WriteShort (msg, bound(0, stats[STAT_VIEWZOOM], 65535));
		}
	}
}

void SV_FlushBroadcastMessages(void)
{
	int i;
	client_t *client;
	if (sv.datagram.cursize <= 0)
		return;
	for (i = 0, client = svs.clients;i < svs.maxclients;i++, client++)
	{
		if (!client->spawned || !client->netconnection || client->unreliablemsg.cursize + sv.datagram.cursize > client->unreliablemsg.maxsize || client->unreliablemsg_splitpoints >= (int)(sizeof(client->unreliablemsg_splitpoint)/sizeof(client->unreliablemsg_splitpoint[0])))
			continue;
		SZ_Write(&client->unreliablemsg, sv.datagram.data, sv.datagram.cursize);
		client->unreliablemsg_splitpoint[client->unreliablemsg_splitpoints++] = client->unreliablemsg.cursize;
	}
	SZ_Clear(&sv.datagram);
}

static void SV_WriteUnreliableMessages(client_t *client, sizebuf_t *msg, int maxsize, int maxsize2)
{
	// scan the splitpoints to find out how many we can fit in
	int numsegments, j, split;
	if (!client->unreliablemsg_splitpoints)
		return;
	// always accept the first one if it's within 1024 bytes, this ensures
	// that very big datagrams which are over the rate limit still get
	// through, just to keep it working
	for (numsegments = 1;numsegments < client->unreliablemsg_splitpoints;numsegments++)
		if (msg->cursize + client->unreliablemsg_splitpoint[numsegments] > maxsize)
			break;
	// the first segment gets an exemption from the rate limiting, otherwise
	// it could get dropped consistently due to a low rate limit
	if (numsegments == 1)
		maxsize = maxsize2;
	// some will fit, so add the ones that will fit
	split = client->unreliablemsg_splitpoint[numsegments-1];
	// note this discards ones that were accepted by the segments scan but
	// can not fit, such as a really huge first one that will never ever
	// fit in a packet...
	if (msg->cursize + split <= maxsize)
		SZ_Write(msg, client->unreliablemsg.data, split);
	// remove the part we sent, keeping any remaining data
	client->unreliablemsg.cursize -= split;
	if (client->unreliablemsg.cursize > 0)
		memmove(client->unreliablemsg.data, client->unreliablemsg.data + split, client->unreliablemsg.cursize);
	// adjust remaining splitpoints
	client->unreliablemsg_splitpoints -= numsegments;
	for (j = 0;j < client->unreliablemsg_splitpoints;j++)
		client->unreliablemsg_splitpoint[j] = client->unreliablemsg_splitpoint[numsegments + j] - split;
}

/*
=======================
SV_SendClientDatagram
=======================
*/
static void SV_SendClientDatagram (client_t *client)
{
	int clientrate, maxrate, maxsize, maxsize2, downloadsize;
	sizebuf_t msg;
	int stats[MAX_CL_STATS];
	static unsigned char sv_sendclientdatagram_buf[NET_MAXMESSAGE];

	// obey rate limit by limiting packet frequency if the packet size
	// limiting fails
	// (usually this is caused by reliable messages)
	if (!NetConn_CanSend(client->netconnection))
		return;

	// PROTOCOL_DARKPLACES5 and later support packet size limiting of updates
	maxrate = max(NET_MINRATE, sv_maxrate.integer);
	if (sv_maxrate.integer != maxrate)
		Cvar_SetValueQuick(&sv_maxrate, maxrate);

	// clientrate determines the 'cleartime' of a packet
	// (how long to wait before sending another, based on this packet's size)
	clientrate = bound(NET_MINRATE, client->rate, maxrate);

	switch (sv.protocol)
	{
	case PROTOCOL_QUAKE:
	case PROTOCOL_QUAKEDP:
	case PROTOCOL_NEHAHRAMOVIE:
	case PROTOCOL_NEHAHRABJP:
	case PROTOCOL_NEHAHRABJP2:
	case PROTOCOL_NEHAHRABJP3:
	case PROTOCOL_QUAKEWORLD:
		// no packet size limit support on Quake protocols because it just
		// causes missing entities/effects
		// packets are simply sent less often to obey the rate limit
		maxsize = 1024;
		maxsize2 = 1024;
		break;
	case PROTOCOL_DARKPLACES1:
	case PROTOCOL_DARKPLACES2:
	case PROTOCOL_DARKPLACES3:
	case PROTOCOL_DARKPLACES4:
		// no packet size limit support on DP1-4 protocols because they kick
		// the client off if they overflow, and miss effects
		// packets are simply sent less often to obey the rate limit
		maxsize = sizeof(sv_sendclientdatagram_buf);
		maxsize2 = sizeof(sv_sendclientdatagram_buf);
		break;
	default:
		// DP5 and later protocols support packet size limiting which is a
		// better method than limiting packet frequency as QW does
		//
		// at very low rates (or very small sys_ticrate) the packet size is
		// not reduced below 128, but packets may be sent less often
		maxsize = (int)(clientrate * sys_ticrate.value);
		maxsize = bound(128, maxsize, 1400);
		maxsize2 = 1400;
		// csqc entities can easily exceed 128 bytes, so disable throttling in
		// mods that use csqc (they are likely to use less bandwidth anyway)
		if (sv.csqc_progsize > 0)
			maxsize = maxsize2;
		break;
	}

	if (LHNETADDRESS_GetAddressType(&host_client->netconnection->peeraddress) == LHNETADDRESSTYPE_LOOP && !sv_ratelimitlocalplayer.integer)
	{
		// for good singleplayer, send huge packets
		maxsize = sizeof(sv_sendclientdatagram_buf);
		maxsize2 = sizeof(sv_sendclientdatagram_buf);
		// never limit frequency in singleplayer
		clientrate = 1000000000;
	}

	// while downloading, limit entity updates to half the packet
	// (any leftover space will be used for downloading)
	if (host_client->download_file)
		maxsize /= 2;

	msg.data = sv_sendclientdatagram_buf;
	msg.maxsize = sizeof(sv_sendclientdatagram_buf);
	msg.cursize = 0;
	msg.allowoverflow = false;

	if (host_client->spawned)
	{
		// the player is in the game
		MSG_WriteByte (&msg, svc_time);
		MSG_WriteFloat (&msg, sv.time);

		// add the client specific data to the datagram
		SV_WriteClientdataToMessage (client, client->edict, &msg, stats);
		// now update the stats[] array using any registered custom fields
		VM_SV_UpdateCustomStats (client, client->edict, &msg, stats);
		// set host_client->statsdeltabits
		Protocol_UpdateClientStats (stats);

		// add as many queued unreliable messages (effects) as we can fit
		// limit effects to half of the remaining space
		if (client->unreliablemsg.cursize)
			SV_WriteUnreliableMessages (client, &msg, maxsize/2, maxsize2);

		// now write as many entities as we can fit, and also sends stats
		SV_WriteEntitiesToClient (client, client->edict, &msg, maxsize);
	}
	else if (realtime > client->keepalivetime)
	{
		// the player isn't totally in the game yet
		// send small keepalive messages if too much time has passed
		// (may also be sending downloads)
		client->keepalivetime = realtime + 5;
		MSG_WriteChar (&msg, svc_nop);
	}

	// if a download is active, see if there is room to fit some download data
	// in this packet
	downloadsize = min(maxsize*2,maxsize2) - msg.cursize - 7;
	if (host_client->download_file && host_client->download_started && downloadsize > 0)
	{
		fs_offset_t downloadstart;
		unsigned char data[1400];
		downloadstart = FS_Tell(host_client->download_file);
		downloadsize = min(downloadsize, (int)sizeof(data));
		downloadsize = FS_Read(host_client->download_file, data, downloadsize);
		// note this sends empty messages if at the end of the file, which is
		// necessary to keep the packet loss logic working
		// (the last blocks may be lost and need to be re-sent, and that will
		//  only occur if the client acks the empty end messages, revealing
		//  a gap in the download progress, causing the last blocks to be
		//  sent again)
		MSG_WriteChar (&msg, svc_downloaddata);
		MSG_WriteLong (&msg, downloadstart);
		MSG_WriteShort (&msg, downloadsize);
		if (downloadsize > 0)
			SZ_Write (&msg, data, downloadsize);
	}

	// reliable only if none is in progress
	if(client->sendsignon != 2 && !client->netconnection->sendMessageLength)
		SV_WriteDemoMessage(client, &(client->netconnection->message), false);
	// unreliable
	SV_WriteDemoMessage(client, &msg, false);

// send the datagram
	NetConn_SendUnreliableMessage (client->netconnection, &msg, sv.protocol, clientrate, client->sendsignon == 2);
	if (client->sendsignon == 1 && !client->netconnection->message.cursize)
		client->sendsignon = 2; // prevent reliable until client sends prespawn (this is the keepalive phase)
}

/*
=======================
SV_UpdateToReliableMessages
=======================
*/
static void SV_UpdateToReliableMessages (void)
{
	int i, j;
	client_t *client;
	const char *name;
	const char *model;
	const char *skin;
	int clientcamera;

// check for changes to be sent over the reliable streams
	for (i = 0, host_client = svs.clients;i < svs.maxclients;i++, host_client++)
	{
		// update the host_client fields we care about according to the entity fields
		host_client->edict = PRVM_EDICT_NUM(i+1);

		// DP_SV_CLIENTNAME
		name = PRVM_GetString(PRVM_serveredictstring(host_client->edict, netname));
		if (name == NULL)
			name = "";
		// always point the string back at host_client->name to keep it safe
		strlcpy (host_client->name, name, sizeof (host_client->name));
		PRVM_serveredictstring(host_client->edict, netname) = PRVM_SetEngineString(host_client->name);
		if (strcmp(host_client->old_name, host_client->name))
		{
			if (host_client->spawned)
				SV_BroadcastPrintf("%s ^7changed name to %s\n", host_client->old_name, host_client->name);
			strlcpy(host_client->old_name, host_client->name, sizeof(host_client->old_name));
			// send notification to all clients
			MSG_WriteByte (&sv.reliable_datagram, svc_updatename);
			MSG_WriteByte (&sv.reliable_datagram, i);
			MSG_WriteString (&sv.reliable_datagram, host_client->name);
			SV_WriteNetnameIntoDemo(host_client);
		}

		// DP_SV_CLIENTCOLORS
		host_client->colors = (int)PRVM_serveredictfloat(host_client->edict, clientcolors);
		if (host_client->old_colors != host_client->colors)
		{
			host_client->old_colors = host_client->colors;
			// send notification to all clients
			MSG_WriteByte (&sv.reliable_datagram, svc_updatecolors);
			MSG_WriteByte (&sv.reliable_datagram, i);
			MSG_WriteByte (&sv.reliable_datagram, host_client->colors);
		}

		// NEXUIZ_PLAYERMODEL
		model = PRVM_GetString(PRVM_serveredictstring(host_client->edict, playermodel));
		if (model == NULL)
			model = "";
		// always point the string back at host_client->name to keep it safe
		strlcpy (host_client->playermodel, model, sizeof (host_client->playermodel));
		PRVM_serveredictstring(host_client->edict, playermodel) = PRVM_SetEngineString(host_client->playermodel);

		// NEXUIZ_PLAYERSKIN
		skin = PRVM_GetString(PRVM_serveredictstring(host_client->edict, playerskin));
		if (skin == NULL)
			skin = "";
		// always point the string back at host_client->name to keep it safe
		strlcpy (host_client->playerskin, skin, sizeof (host_client->playerskin));
		PRVM_serveredictstring(host_client->edict, playerskin) = PRVM_SetEngineString(host_client->playerskin);

		// TODO: add an extension name for this [1/17/2008 Black]
		clientcamera = PRVM_serveredictedict(host_client->edict, clientcamera);
		if (clientcamera > 0)
		{
			int oldclientcamera = host_client->clientcamera;
			if (clientcamera >= prog->max_edicts || PRVM_EDICT_NUM(clientcamera)->priv.required->free)
				clientcamera = PRVM_NUM_FOR_EDICT(host_client->edict);
			host_client->clientcamera = clientcamera;

			if (oldclientcamera != host_client->clientcamera)
			{
				MSG_WriteByte(&host_client->netconnection->message, svc_setview);
				MSG_WriteShort(&host_client->netconnection->message, host_client->clientcamera);
			}
		}

		// frags
		host_client->frags = (int)PRVM_serveredictfloat(host_client->edict, frags);
		if(gamemode == GAME_NEXUIZ || gamemode == GAME_XONOTIC)
			if(!host_client->spawned && host_client->netconnection)
				host_client->frags = -666;
		if (host_client->old_frags != host_client->frags)
		{
			host_client->old_frags = host_client->frags;
			// send notification to all clients
			MSG_WriteByte (&sv.reliable_datagram, svc_updatefrags);
			MSG_WriteByte (&sv.reliable_datagram, i);
			MSG_WriteShort (&sv.reliable_datagram, host_client->frags);
		}
	}

	for (j = 0, client = svs.clients;j < svs.maxclients;j++, client++)
		if (client->netconnection && (client->spawned || client->clientconnectcalled)) // also send MSG_ALL to people who are past ClientConnect, but not spawned yet
			SZ_Write (&client->netconnection->message, sv.reliable_datagram.data, sv.reliable_datagram.cursize);

	SZ_Clear (&sv.reliable_datagram);
}


/*
=======================
SV_SendClientMessages
=======================
*/
void SV_SendClientMessages (void)
{
	int i, prepared = false;

	if (sv.protocol == PROTOCOL_QUAKEWORLD)
		Sys_Error("SV_SendClientMessages: no quakeworld support\n");

	SV_FlushBroadcastMessages();

// update frags, names, etc
	SV_UpdateToReliableMessages();

// build individual updates
	for (i = 0, host_client = svs.clients;i < svs.maxclients;i++, host_client++)
	{
		if (!host_client->active)
			continue;
		if (!host_client->netconnection)
			continue;

		if (host_client->netconnection->message.overflowed)
		{
			SV_DropClient (true);	// if the message couldn't send, kick off
			continue;
		}

		if (!prepared)
		{
			prepared = true;
			// only prepare entities once per frame
			SV_PrepareEntitiesForSending();
		}
		SV_SendClientDatagram (host_client);
	}

// clear muzzle flashes
	SV_CleanupEnts();
}

static void SV_StartDownload_f(void)
{
	if (host_client->download_file)
		host_client->download_started = true;
}

/*
 * Compression extension negotiation:
 *
 * Server to client:
 *   cl_serverextension_download 2
 *
 * Client to server:
 *   download <filename> <list of zero or more suppported compressions in order of preference>
 * e.g.
 *   download maps/map1.bsp lzo deflate huffman
 *
 * Server to client:
 *   cl_downloadbegin <compressed size> <filename> <compression method actually used>
 * e.g.
 *   cl_downloadbegin 123456 maps/map1.bsp deflate
 *
 * The server may choose not to compress the file by sending no compression name, like:
 *   cl_downloadbegin 345678 maps/map1.bsp
 *
 * NOTE: the "download" command may only specify compression algorithms if
 *       cl_serverextension_download is 2!
 *       If cl_serverextension_download has a different value, the client must
 *       assume this extension is not supported!
 */

static void Download_CheckExtensions(void)
{
	int i;
	int argc = Cmd_Argc();

	// first reset them all
	host_client->download_deflate = false;
	
	for(i = 2; i < argc; ++i)
	{
		if(!strcmp(Cmd_Argv(i), "deflate"))
		{
			host_client->download_deflate = true;
			break;
		}
	}
}

static void SV_Download_f(void)
{
	const char *whichpack, *whichpack2, *extension;
	qboolean is_csqc; // so we need to check only once

	if (Cmd_Argc() < 2)
	{
		SV_ClientPrintf("usage: download <filename> {<extensions>}*\n");
		SV_ClientPrintf("       supported extensions: deflate\n");
		return;
	}

	if (FS_CheckNastyPath(Cmd_Argv(1), false))
	{
		SV_ClientPrintf("Download rejected: nasty filename \"%s\"\n", Cmd_Argv(1));
		return;
	}

	if (host_client->download_file)
	{
		// at this point we'll assume the previous download should be aborted
		Con_DPrintf("Download of %s aborted by %s starting a new download\n", host_client->download_name, host_client->name);
		Host_ClientCommands("\nstopdownload\n");

		// close the file and reset variables
		FS_Close(host_client->download_file);
		host_client->download_file = NULL;
		host_client->download_name[0] = 0;
		host_client->download_expectedposition = 0;
		host_client->download_started = false;
	}

	is_csqc = (sv.csqc_progname[0] && strcmp(Cmd_Argv(1), sv.csqc_progname) == 0);
	
	if (!sv_allowdownloads.integer && !is_csqc)
	{
		SV_ClientPrintf("Downloads are disabled on this server\n");
		Host_ClientCommands("\nstopdownload\n");
		return;
	}

	Download_CheckExtensions();

	strlcpy(host_client->download_name, Cmd_Argv(1), sizeof(host_client->download_name));
	extension = FS_FileExtension(host_client->download_name);

	// host_client is asking to download a specified file
	if (developer_extra.integer)
		Con_DPrintf("Download request for %s by %s\n", host_client->download_name, host_client->name);

	if(is_csqc)
	{
		char extensions[MAX_QPATH]; // make sure this can hold all extensions
		extensions[0] = '\0';
		
		if(host_client->download_deflate)
			strlcat(extensions, " deflate", sizeof(extensions));
		
		Con_DPrintf("Downloading %s to %s\n", host_client->download_name, host_client->name);

		if(host_client->download_deflate && svs.csqc_progdata_deflated)
			host_client->download_file = FS_FileFromData(svs.csqc_progdata_deflated, svs.csqc_progsize_deflated, true);
		else
			host_client->download_file = FS_FileFromData(svs.csqc_progdata, sv.csqc_progsize, true);
		
		// no, no space is needed between %s and %s :P
		Host_ClientCommands("\ncl_downloadbegin %i %s%s\n", (int)FS_FileSize(host_client->download_file), host_client->download_name, extensions);

		host_client->download_expectedposition = 0;
		host_client->download_started = false;
		host_client->sendsignon = true; // make sure this message is sent
		return;
	}

	if (!FS_FileExists(host_client->download_name))
	{
		SV_ClientPrintf("Download rejected: server does not have the file \"%s\"\nYou may need to separately download or purchase the data archives for this game/mod to get this file\n", host_client->download_name);
		Host_ClientCommands("\nstopdownload\n");
		return;
	}

	// check if the user is trying to download part of registered Quake(r)
	whichpack = FS_WhichPack(host_client->download_name);
	whichpack2 = FS_WhichPack("gfx/pop.lmp");
	if ((whichpack && whichpack2 && !strcasecmp(whichpack, whichpack2)) || FS_IsRegisteredQuakePack(host_client->download_name))
	{
		SV_ClientPrintf("Download rejected: file \"%s\" is part of registered Quake(r)\nYou must purchase Quake(r) from id Software or a retailer to get this file\nPlease go to http://www.idsoftware.com/games/quake/quake/index.php?game_section=buy\n", host_client->download_name);
		Host_ClientCommands("\nstopdownload\n");
		return;
	}

	// check if the server has forbidden archive downloads entirely
	if (!sv_allowdownloads_inarchive.integer)
	{
		whichpack = FS_WhichPack(host_client->download_name);
		if (whichpack)
		{
			SV_ClientPrintf("Download rejected: file \"%s\" is in an archive (\"%s\")\nYou must separately download or purchase the data archives for this game/mod to get this file\n", host_client->download_name, whichpack);
			Host_ClientCommands("\nstopdownload\n");
			return;
		}
	}

	if (!sv_allowdownloads_config.integer)
	{
		if (!strcasecmp(extension, "cfg"))
		{
			SV_ClientPrintf("Download rejected: file \"%s\" is a .cfg file which is forbidden for security reasons\nYou must separately download or purchase the data archives for this game/mod to get this file\n", host_client->download_name);
			Host_ClientCommands("\nstopdownload\n");
			return;
		}
	}

	if (!sv_allowdownloads_dlcache.integer)
	{
		if (!strncasecmp(host_client->download_name, "dlcache/", 8))
		{
			SV_ClientPrintf("Download rejected: file \"%s\" is in the dlcache/ directory which is forbidden for security reasons\nYou must separately download or purchase the data archives for this game/mod to get this file\n", host_client->download_name);
			Host_ClientCommands("\nstopdownload\n");
			return;
		}
	}

	if (!sv_allowdownloads_archive.integer)
	{
		if (!strcasecmp(extension, "pak") || !strcasecmp(extension, "pk3"))
		{
			SV_ClientPrintf("Download rejected: file \"%s\" is an archive\nYou must separately download or purchase the data archives for this game/mod to get this file\n", host_client->download_name);
			Host_ClientCommands("\nstopdownload\n");
			return;
		}
	}

	host_client->download_file = FS_OpenVirtualFile(host_client->download_name, true);
	if (!host_client->download_file)
	{
		SV_ClientPrintf("Download rejected: server could not open the file \"%s\"\n", host_client->download_name);
		Host_ClientCommands("\nstopdownload\n");
		return;
	}

	if (FS_FileSize(host_client->download_file) > 1<<30)
	{
		SV_ClientPrintf("Download rejected: file \"%s\" is very large\n", host_client->download_name);
		Host_ClientCommands("\nstopdownload\n");
		FS_Close(host_client->download_file);
		host_client->download_file = NULL;
		return;
	}

	if (FS_FileSize(host_client->download_file) < 0)
	{
		SV_ClientPrintf("Download rejected: file \"%s\" is not a regular file\n", host_client->download_name);
		Host_ClientCommands("\nstopdownload\n");
		FS_Close(host_client->download_file);
		host_client->download_file = NULL;
		return;
	}

	Con_DPrintf("Downloading %s to %s\n", host_client->download_name, host_client->name);

	/*
	 * we can only do this if we would actually deflate on the fly
	 * which we do not (yet)!
	{
		char extensions[MAX_QPATH]; // make sure this can hold all extensions
		extensions[0] = '\0';
		
		if(host_client->download_deflate)
			strlcat(extensions, " deflate", sizeof(extensions));

		// no, no space is needed between %s and %s :P
		Host_ClientCommands("\ncl_downloadbegin %i %s%s\n", (int)FS_FileSize(host_client->download_file), host_client->download_name, extensions);
	}
	*/
	Host_ClientCommands("\ncl_downloadbegin %i %s\n", (int)FS_FileSize(host_client->download_file), host_client->download_name);

	host_client->download_expectedposition = 0;
	host_client->download_started = false;
	host_client->sendsignon = true; // make sure this message is sent

	// the rest of the download process is handled in SV_SendClientDatagram
	// and other code dealing with svc_downloaddata and clc_ackdownloaddata
	//
	// no svc_downloaddata messages will be sent until sv_startdownload is
	// sent by the client
}

/*
==============================================================================

SERVER SPAWNING

==============================================================================
*/

/*
================
SV_ModelIndex

================
*/
int SV_ModelIndex(const char *s, int precachemode)
{
	int i, limit = ((sv.protocol == PROTOCOL_QUAKE || sv.protocol == PROTOCOL_QUAKEDP || sv.protocol == PROTOCOL_NEHAHRAMOVIE || sv.protocol == PROTOCOL_NEHAHRABJP || sv.protocol == PROTOCOL_NEHAHRABJP2 || sv.protocol == PROTOCOL_NEHAHRABJP3) ? 256 : MAX_MODELS);
	char filename[MAX_QPATH];
	if (!s || !*s)
		return 0;
	// testing
	//if (precachemode == 2)
	//	return 0;
	strlcpy(filename, s, sizeof(filename));
	for (i = 2;i < limit;i++)
	{
		if (!sv.model_precache[i][0])
		{
			if (precachemode)
			{
				if (sv.state != ss_loading && (sv.protocol == PROTOCOL_QUAKE || sv.protocol == PROTOCOL_QUAKEDP || sv.protocol == PROTOCOL_NEHAHRAMOVIE || sv.protocol == PROTOCOL_NEHAHRABJP || sv.protocol == PROTOCOL_NEHAHRABJP2 || sv.protocol == PROTOCOL_NEHAHRABJP3 || sv.protocol == PROTOCOL_DARKPLACES1 || sv.protocol == PROTOCOL_DARKPLACES2 || sv.protocol == PROTOCOL_DARKPLACES3 || sv.protocol == PROTOCOL_DARKPLACES4 || sv.protocol == PROTOCOL_DARKPLACES5))
				{
					Con_Printf("SV_ModelIndex(\"%s\"): precache_model can only be done in spawn functions\n", filename);
					return 0;
				}
				if (precachemode == 1)
					Con_Printf("SV_ModelIndex(\"%s\"): not precached (fix your code), precaching anyway\n", filename);
				strlcpy(sv.model_precache[i], filename, sizeof(sv.model_precache[i]));
				sv.models[i] = Mod_ForName (sv.model_precache[i], true, false, s[0] == '*' ? sv.worldname : NULL);
				if (sv.state != ss_loading)
				{
					MSG_WriteByte(&sv.reliable_datagram, svc_precache);
					MSG_WriteShort(&sv.reliable_datagram, i);
					MSG_WriteString(&sv.reliable_datagram, filename);
				}
				return i;
			}
			Con_Printf("SV_ModelIndex(\"%s\"): not precached\n", filename);
			return 0;
		}
		if (!strcmp(sv.model_precache[i], filename))
			return i;
	}
	Con_Printf("SV_ModelIndex(\"%s\"): i (%i) == MAX_MODELS (%i)\n", filename, i, MAX_MODELS);
	return 0;
}

/*
================
SV_SoundIndex

================
*/
int SV_SoundIndex(const char *s, int precachemode)
{
	int i, limit = ((sv.protocol == PROTOCOL_QUAKE || sv.protocol == PROTOCOL_QUAKEDP || sv.protocol == PROTOCOL_NEHAHRAMOVIE || sv.protocol == PROTOCOL_NEHAHRABJP || sv.protocol == PROTOCOL_NEHAHRABJP2 || sv.protocol == PROTOCOL_NEHAHRABJP3) ? 256 : MAX_SOUNDS);
	char filename[MAX_QPATH];
	if (!s || !*s)
		return 0;
	// testing
	//if (precachemode == 2)
	//	return 0;
	strlcpy(filename, s, sizeof(filename));
	for (i = 1;i < limit;i++)
	{
		if (!sv.sound_precache[i][0])
		{
			if (precachemode)
			{
				if (sv.state != ss_loading && (sv.protocol == PROTOCOL_QUAKE || sv.protocol == PROTOCOL_QUAKEDP || sv.protocol == PROTOCOL_NEHAHRAMOVIE || sv.protocol == PROTOCOL_NEHAHRABJP || sv.protocol == PROTOCOL_NEHAHRABJP2 || sv.protocol == PROTOCOL_NEHAHRABJP3 || sv.protocol == PROTOCOL_DARKPLACES1 || sv.protocol == PROTOCOL_DARKPLACES2 || sv.protocol == PROTOCOL_DARKPLACES3 || sv.protocol == PROTOCOL_DARKPLACES4 || sv.protocol == PROTOCOL_DARKPLACES5))
				{
					Con_Printf("SV_SoundIndex(\"%s\"): precache_sound can only be done in spawn functions\n", filename);
					return 0;
				}
				if (precachemode == 1)
					Con_Printf("SV_SoundIndex(\"%s\"): not precached (fix your code), precaching anyway\n", filename);
				strlcpy(sv.sound_precache[i], filename, sizeof(sv.sound_precache[i]));
				if (sv.state != ss_loading)
				{
					MSG_WriteByte(&sv.reliable_datagram, svc_precache);
					MSG_WriteShort(&sv.reliable_datagram, i + 32768);
					MSG_WriteString(&sv.reliable_datagram, filename);
				}
				return i;
			}
			Con_Printf("SV_SoundIndex(\"%s\"): not precached\n", filename);
			return 0;
		}
		if (!strcmp(sv.sound_precache[i], filename))
			return i;
	}
	Con_Printf("SV_SoundIndex(\"%s\"): i (%i) == MAX_SOUNDS (%i)\n", filename, i, MAX_SOUNDS);
	return 0;
}

/*
================
SV_ParticleEffectIndex

================
*/
int SV_ParticleEffectIndex(const char *name)
{
	int i, argc, linenumber, effectnameindex;
	int filepass;
	fs_offset_t filesize;
	unsigned char *filedata;
	const char *text;
	const char *textstart;
	//const char *textend;
	char argv[16][1024];
	char filename[MAX_QPATH];
	if (!sv.particleeffectnamesloaded)
	{
		sv.particleeffectnamesloaded = true;
		memset(sv.particleeffectname, 0, sizeof(sv.particleeffectname));
		for (i = 0;i < EFFECT_TOTAL;i++)
			strlcpy(sv.particleeffectname[i], standardeffectnames[i], sizeof(sv.particleeffectname[i]));
		for (filepass = 0;;filepass++)
		{
			if (filepass == 0)
				dpsnprintf(filename, sizeof(filename), "effectinfo.txt");
			else if (filepass == 1)
				dpsnprintf(filename, sizeof(filename), "%s_effectinfo.txt", sv.worldnamenoextension);
			else
				break;
			filedata = FS_LoadFile(filename, tempmempool, true, &filesize);
			if (!filedata)
				continue;
			textstart = (const char *)filedata;
			//textend = (const char *)filedata + filesize;
			text = textstart;
			for (linenumber = 1;;linenumber++)
			{
				argc = 0;
				for (;;)
				{
					if (!COM_ParseToken_Simple(&text, true, false) || !strcmp(com_token, "\n"))
						break;
					if (argc < 16)
					{
						strlcpy(argv[argc], com_token, sizeof(argv[argc]));
						argc++;
					}
				}
				if (com_token[0] == 0)
					break; // if the loop exited and it's not a \n, it's EOF
				if (argc < 1)
					continue;
				if (!strcmp(argv[0], "effect"))
				{
					if (argc == 2)
					{
						for (effectnameindex = 1;effectnameindex < SV_MAX_PARTICLEEFFECTNAME;effectnameindex++)
						{
							if (sv.particleeffectname[effectnameindex][0])
							{
								if (!strcmp(sv.particleeffectname[effectnameindex], argv[1]))
									break;
							}
							else
							{
								strlcpy(sv.particleeffectname[effectnameindex], argv[1], sizeof(sv.particleeffectname[effectnameindex]));
								break;
							}
						}
						// if we run out of names, abort
						if (effectnameindex == SV_MAX_PARTICLEEFFECTNAME)
						{
							Con_Printf("%s:%i: too many effects!\n", filename, linenumber);
							break;
						}
					}
				}
			}
			Mem_Free(filedata);
		}
	}
	// search for the name
	for (effectnameindex = 1;effectnameindex < SV_MAX_PARTICLEEFFECTNAME && sv.particleeffectname[effectnameindex][0];effectnameindex++)
		if (!strcmp(sv.particleeffectname[effectnameindex], name))
			return effectnameindex;
	// return 0 if we couldn't find it
	return 0;
}

dp_model_t *SV_GetModelByIndex(int modelindex)
{
	return (modelindex > 0 && modelindex < MAX_MODELS) ? sv.models[modelindex] : NULL;
}

dp_model_t *SV_GetModelFromEdict(prvm_edict_t *ed)
{
	int modelindex;
	if (!ed || ed->priv.server->free)
		return NULL;
	modelindex = (int)PRVM_serveredictfloat(ed, modelindex);
	return (modelindex > 0 && modelindex < MAX_MODELS) ? sv.models[modelindex] : NULL;
}

/*
================
SV_CreateBaseline

================
*/
static void SV_CreateBaseline (void)
{
	int i, entnum, large;
	prvm_edict_t *svent;

	// LordHavoc: clear *all* states (note just active ones)
	for (entnum = 0;entnum < prog->max_edicts;entnum++)
	{
		// get the current server version
		svent = PRVM_EDICT_NUM(entnum);

		// LordHavoc: always clear state values, whether the entity is in use or not
		svent->priv.server->baseline = defaultstate;

		if (svent->priv.server->free)
			continue;
		if (entnum > svs.maxclients && !PRVM_serveredictfloat(svent, modelindex))
			continue;

		// create entity baseline
		VectorCopy (PRVM_serveredictvector(svent, origin), svent->priv.server->baseline.origin);
		VectorCopy (PRVM_serveredictvector(svent, angles), svent->priv.server->baseline.angles);
		svent->priv.server->baseline.frame = (int)PRVM_serveredictfloat(svent, frame);
		svent->priv.server->baseline.skin = (int)PRVM_serveredictfloat(svent, skin);
		if (entnum > 0 && entnum <= svs.maxclients)
		{
			svent->priv.server->baseline.colormap = entnum;
			svent->priv.server->baseline.modelindex = SV_ModelIndex("progs/player.mdl", 1);
		}
		else
		{
			svent->priv.server->baseline.colormap = 0;
			svent->priv.server->baseline.modelindex = (int)PRVM_serveredictfloat(svent, modelindex);
		}

		large = false;
		if (svent->priv.server->baseline.modelindex & 0xFF00 || svent->priv.server->baseline.frame & 0xFF00)
		{
			large = true;
			if (sv.protocol == PROTOCOL_NEHAHRABJP || sv.protocol == PROTOCOL_NEHAHRABJP2 || sv.protocol == PROTOCOL_NEHAHRABJP3)
				large = false;
		}

		// add to the message
		if (large)
			MSG_WriteByte (&sv.signon, svc_spawnbaseline2);
		else
			MSG_WriteByte (&sv.signon, svc_spawnbaseline);
		MSG_WriteShort (&sv.signon, entnum);

		if (large)
		{
			MSG_WriteShort (&sv.signon, svent->priv.server->baseline.modelindex);
			MSG_WriteShort (&sv.signon, svent->priv.server->baseline.frame);
		}
		else if (sv.protocol == PROTOCOL_NEHAHRABJP || sv.protocol == PROTOCOL_NEHAHRABJP2 || sv.protocol == PROTOCOL_NEHAHRABJP3)
		{
			MSG_WriteShort (&sv.signon, svent->priv.server->baseline.modelindex);
			MSG_WriteByte (&sv.signon, svent->priv.server->baseline.frame);
		}
		else
		{
			MSG_WriteByte (&sv.signon, svent->priv.server->baseline.modelindex);
			MSG_WriteByte (&sv.signon, svent->priv.server->baseline.frame);
		}
		MSG_WriteByte (&sv.signon, svent->priv.server->baseline.colormap);
		MSG_WriteByte (&sv.signon, svent->priv.server->baseline.skin);
		for (i=0 ; i<3 ; i++)
		{
			MSG_WriteCoord(&sv.signon, svent->priv.server->baseline.origin[i], sv.protocol);
			MSG_WriteAngle(&sv.signon, svent->priv.server->baseline.angles[i], sv.protocol);
		}
	}
}

/*
================
SV_Prepare_CSQC

Load csprogs.dat and comperss it so it doesn't need to be
reloaded on request.
================
*/
void SV_Prepare_CSQC(void)
{
	fs_offset_t progsize;

	if(svs.csqc_progdata)
	{
		Con_DPrintf("Unloading old CSQC data.\n");
		Mem_Free(svs.csqc_progdata);
		if(svs.csqc_progdata_deflated)
			Mem_Free(svs.csqc_progdata_deflated);
	}

	svs.csqc_progdata = NULL;
	svs.csqc_progdata_deflated = NULL;
	
	sv.csqc_progname[0] = 0;
	svs.csqc_progdata = FS_LoadFile(csqc_progname.string, sv_mempool, false, &progsize);

	if(progsize > 0)
	{
		size_t deflated_size;

		sv.csqc_progsize = (int)progsize;
		sv.csqc_progcrc = CRC_Block(svs.csqc_progdata, progsize);
		strlcpy(sv.csqc_progname, csqc_progname.string, sizeof(sv.csqc_progname));
		Con_DPrintf("server detected csqc progs file \"%s\" with size %i and crc %i\n", sv.csqc_progname, sv.csqc_progsize, sv.csqc_progcrc);

		Con_DPrint("Compressing csprogs.dat\n");
		//unsigned char *FS_Deflate(const unsigned char *data, size_t size, size_t *deflated_size, int level, mempool_t *mempool);
		svs.csqc_progdata_deflated = FS_Deflate(svs.csqc_progdata, progsize, &deflated_size, -1, sv_mempool);
		svs.csqc_progsize_deflated = (int)deflated_size;
		if(svs.csqc_progdata_deflated)
		{
			Con_DPrintf("Deflated: %g%%\n", 100.0 - 100.0 * (deflated_size / (float)progsize));
			Con_DPrintf("Uncompressed: %u\nCompressed:   %u\n", (unsigned)sv.csqc_progsize, (unsigned)svs.csqc_progsize_deflated);
		}
		else
			Con_DPrintf("Cannot compress - need zlib for this. Using uncompressed progs only.\n");
	}
}

/*
================
SV_SaveSpawnparms

Grabs the current state of each client for saving across the
transition to another level
================
*/
void SV_SaveSpawnparms (void)
{
	int		i, j;

	svs.serverflags = (int)PRVM_serverglobalfloat(serverflags);

	for (i = 0, host_client = svs.clients;i < svs.maxclients;i++, host_client++)
	{
		if (!host_client->active)
			continue;

	// call the progs to get default spawn parms for the new client
		PRVM_serverglobaledict(self) = PRVM_EDICT_TO_PROG(host_client->edict);
		PRVM_ExecuteProgram (PRVM_serverfunction(SetChangeParms), "QC function SetChangeParms is missing");
		for (j=0 ; j<NUM_SPAWN_PARMS ; j++)
			host_client->spawn_parms[j] = (&PRVM_serverglobalfloat(parm1))[j];
	}
}

/*
================
SV_SpawnServer

This is called at the start of each level
================
*/

void SV_SpawnServer (const char *server)
{
	prvm_edict_t *ent;
	int i;
	char *entities;
	dp_model_t *worldmodel;
	char modelname[sizeof(sv.worldname)];

	Con_DPrintf("SpawnServer: %s\n", server);

	dpsnprintf (modelname, sizeof(modelname), "maps/%s.bsp", server);

	if (!FS_FileExists(modelname))
	{
		dpsnprintf (modelname, sizeof(modelname), "maps/%s", server);
		if (!FS_FileExists(modelname))
		{
			Con_Printf("SpawnServer: no map file named maps/%s.bsp\n", server);
			return;
		}
	}

	if (cls.state != ca_dedicated)
	{
		SCR_BeginLoadingPlaque();
		S_StopAllSounds();
	}

	if(sv.active)
	{
		SV_VM_Begin();
		World_End(&sv.world);
		if(PRVM_serverfunction(SV_Shutdown))
		{
			func_t s = PRVM_serverfunction(SV_Shutdown);
			PRVM_serverfunction(SV_Shutdown) = 0; // prevent it from getting called again
			PRVM_ExecuteProgram(s,"SV_Shutdown() required");
		}
		SV_VM_End();
	}

	// free q3 shaders so that any newly downloaded shaders will be active
	Mod_FreeQ3Shaders();

	worldmodel = Mod_ForName(modelname, false, developer.integer > 0, NULL);
	if (!worldmodel || !worldmodel->TraceBox)
	{
		Con_Printf("Couldn't load map %s\n", modelname);
		return;
	}

	Collision_Cache_Reset(true);

	// let's not have any servers with no name
	if (hostname.string[0] == 0)
		Cvar_Set ("hostname", "UNNAMED");
	scr_centertime_off = 0;

	svs.changelevel_issued = false;		// now safe to issue another

	// make the map a required file for clients
	Curl_ClearRequirements();
	Curl_RequireFile(modelname);

//
// tell all connected clients that we are going to a new level
//
	if (sv.active)
	{
		client_t *client;
		for (i = 0, client = svs.clients;i < svs.maxclients;i++, client++)
		{
			if (client->netconnection)
			{
				MSG_WriteByte(&client->netconnection->message, svc_stufftext);
				MSG_WriteString(&client->netconnection->message, "reconnect\n");
			}
		}
	}
	else
	{
		// open server port
		NetConn_OpenServerPorts(true);
	}

//
// make cvars consistant
//
	if (coop.integer)
		Cvar_SetValue ("deathmatch", 0);
	// LordHavoc: it can be useful to have skills outside the range 0-3...
	//current_skill = bound(0, (int)(skill.value + 0.5), 3);
	//Cvar_SetValue ("skill", (float)current_skill);
	current_skill = (int)(skill.value + 0.5);

//
// set up the new server
//
	memset (&sv, 0, sizeof(sv));
	// if running a local client, make sure it doesn't try to access the last
	// level's data which is no longer valiud
	cls.signon = 0;

	Cvar_SetValue("halflifebsp", worldmodel->brush.ishlbsp);

	if(*sv_random_seed.string)
	{
		srand(sv_random_seed.integer);
		Con_Printf("NOTE: random seed is %d; use for debugging/benchmarking only!\nUnset sv_random_seed to get real random numbers again.\n", sv_random_seed.integer);
	}

	SV_VM_Setup();

	sv.active = true;

	// set level base name variables for later use
	strlcpy (sv.name, server, sizeof (sv.name));
	strlcpy(sv.worldname, modelname, sizeof(sv.worldname));
	FS_StripExtension(sv.worldname, sv.worldnamenoextension, sizeof(sv.worldnamenoextension));
	strlcpy(sv.worldbasename, !strncmp(sv.worldnamenoextension, "maps/", 5) ? sv.worldnamenoextension + 5 : sv.worldnamenoextension, sizeof(sv.worldbasename));
	//Cvar_SetQuick(&sv_worldmessage, sv.worldmessage); // set later after QC is spawned
	Cvar_SetQuick(&sv_worldname, sv.worldname);
	Cvar_SetQuick(&sv_worldnamenoextension, sv.worldnamenoextension);
	Cvar_SetQuick(&sv_worldbasename, sv.worldbasename);

	sv.protocol = Protocol_EnumForName(sv_protocolname.string);
	if (sv.protocol == PROTOCOL_UNKNOWN)
	{
		char buffer[1024];
		Protocol_Names(buffer, sizeof(buffer));
		Con_Printf("Unknown sv_protocolname \"%s\", valid values are:\n%s\n", sv_protocolname.string, buffer);
		sv.protocol = PROTOCOL_QUAKE;
	}

	SV_VM_Begin();

// load progs to get entity field count
	//PR_LoadProgs ( sv_progs.string );

	sv.datagram.maxsize = sizeof(sv.datagram_buf);
	sv.datagram.cursize = 0;
	sv.datagram.data = sv.datagram_buf;

	sv.reliable_datagram.maxsize = sizeof(sv.reliable_datagram_buf);
	sv.reliable_datagram.cursize = 0;
	sv.reliable_datagram.data = sv.reliable_datagram_buf;

	sv.signon.maxsize = sizeof(sv.signon_buf);
	sv.signon.cursize = 0;
	sv.signon.data = sv.signon_buf;

// leave slots at start for clients only
	//prog->num_edicts = svs.maxclients+1;

	sv.state = ss_loading;
	prog->allowworldwrites = true;
	sv.paused = false;

	PRVM_serverglobalfloat(time) = sv.time = 1.0;

	Mod_ClearUsed();
	worldmodel->used = true;

	sv.worldmodel = worldmodel;
	sv.models[1] = sv.worldmodel;

//
// clear world interaction links
//
	World_SetSize(&sv.world, sv.worldname, sv.worldmodel->normalmins, sv.worldmodel->normalmaxs);
	World_Start(&sv.world);

	strlcpy(sv.sound_precache[0], "", sizeof(sv.sound_precache[0]));

	strlcpy(sv.model_precache[0], "", sizeof(sv.model_precache[0]));
	strlcpy(sv.model_precache[1], sv.worldname, sizeof(sv.model_precache[1]));
	for (i = 1;i < sv.worldmodel->brush.numsubmodels && i+1 < MAX_MODELS;i++)
	{
		dpsnprintf(sv.model_precache[i+1], sizeof(sv.model_precache[i+1]), "*%i", i);
		sv.models[i+1] = Mod_ForName (sv.model_precache[i+1], false, false, sv.worldname);
	}
	if(i < sv.worldmodel->brush.numsubmodels)
		Con_Printf("Too many submodels (MAX_MODELS is %i)\n", MAX_MODELS);

//
// load the rest of the entities
//
	// AK possible hack since num_edicts is still 0
	ent = PRVM_EDICT_NUM(0);
	memset (ent->fields.vp, 0, prog->entityfields * 4);
	ent->priv.server->free = false;
	PRVM_serveredictstring(ent, model) = PRVM_SetEngineString(sv.worldname);
	PRVM_serveredictfloat(ent, modelindex) = 1;		// world model
	PRVM_serveredictfloat(ent, solid) = SOLID_BSP;
	PRVM_serveredictfloat(ent, movetype) = MOVETYPE_PUSH;
	VectorCopy(sv.world.mins, PRVM_serveredictvector(ent, mins));
	VectorCopy(sv.world.maxs, PRVM_serveredictvector(ent, maxs));
	VectorCopy(sv.world.mins, PRVM_serveredictvector(ent, absmin));
	VectorCopy(sv.world.maxs, PRVM_serveredictvector(ent, absmax));

	if (coop.value)
		PRVM_serverglobalfloat(coop) = coop.integer;
	else
		PRVM_serverglobalfloat(deathmatch) = deathmatch.integer;

	PRVM_serverglobalstring(mapname) = PRVM_SetEngineString(sv.name);

// serverflags are for cross level information (sigils)
	PRVM_serverglobalfloat(serverflags) = svs.serverflags;

	// we need to reset the spawned flag on all connected clients here so that
	// their thinks don't run during startup (before PutClientInServer)
	// we also need to set up the client entities now
	// and we need to set the ->edict pointers to point into the progs edicts
	for (i = 0, host_client = svs.clients;i < svs.maxclients;i++, host_client++)
	{
		host_client->spawned = false;
		host_client->edict = PRVM_EDICT_NUM(i + 1);
		PRVM_ED_ClearEdict(host_client->edict);
	}

	// load replacement entity file if found
	if (sv_entpatch.integer && (entities = (char *)FS_LoadFile(va("%s.ent", sv.worldnamenoextension), tempmempool, true, NULL)))
	{
		Con_Printf("Loaded %s.ent\n", sv.worldnamenoextension);
		PRVM_ED_LoadFromFile (entities);
		Mem_Free(entities);
	}
	else
		PRVM_ED_LoadFromFile (sv.worldmodel->brush.entities);


	// LordHavoc: clear world angles (to fix e3m3.bsp)
	VectorClear(PRVM_serveredictvector(prog->edicts, angles));

// all setup is completed, any further precache statements are errors
//	sv.state = ss_active; // LordHavoc: workaround for svc_precache bug
	prog->allowworldwrites = false;

// run two frames to allow everything to settle
	PRVM_serverglobalfloat(time) = sv.time = 1.0001;
	for (i = 0;i < 2;i++)
	{
		sv.frametime = 0.1;
		SV_Physics ();
	}

	if (cls.state == ca_dedicated)
		Mod_PurgeUnused();

// create a baseline for more efficient communications
	if (sv.protocol == PROTOCOL_QUAKE || sv.protocol == PROTOCOL_QUAKEDP || sv.protocol == PROTOCOL_NEHAHRAMOVIE || sv.protocol == PROTOCOL_NEHAHRABJP || sv.protocol == PROTOCOL_NEHAHRABJP2 || sv.protocol == PROTOCOL_NEHAHRABJP3)
		SV_CreateBaseline ();

	sv.state = ss_active; // LordHavoc: workaround for svc_precache bug

	// to prevent network timeouts
	realtime = Sys_DoubleTime();
	
// send serverinfo to all connected clients, and set up botclients coming back from a level change
	for (i = 0, host_client = svs.clients;i < svs.maxclients;i++, host_client++)
	{
		host_client->clientconnectcalled = false; // do NOT call ClientDisconnect if he drops before ClientConnect!
		if (!host_client->active)
			continue;
		if (host_client->netconnection)
			SV_SendServerinfo(host_client);
		else
		{
			int j;
			// if client is a botclient coming from a level change, we need to
			// set up client info that normally requires networking

			// copy spawn parms out of the client_t
			for (j=0 ; j< NUM_SPAWN_PARMS ; j++)
				(&PRVM_serverglobalfloat(parm1))[j] = host_client->spawn_parms[j];

			// call the spawn function
			host_client->clientconnectcalled = true;
			PRVM_serverglobalfloat(time) = sv.time;
			PRVM_serverglobaledict(self) = PRVM_EDICT_TO_PROG(host_client->edict);
			PRVM_ExecuteProgram (PRVM_serverfunction(ClientConnect), "QC function ClientConnect is missing");
			PRVM_ExecuteProgram (PRVM_serverfunction(PutClientInServer), "QC function PutClientInServer is missing");
			host_client->spawned = true;
		}
	}

	// update the map title cvar
	strlcpy(sv.worldmessage, PRVM_GetString(PRVM_serveredictstring(prog->edicts, message)), sizeof(sv.worldmessage)); // map title (not related to filename)
	Cvar_SetQuick(&sv_worldmessage, sv.worldmessage);

	Con_DPrint("Server spawned.\n");
	NetConn_Heartbeat (2);

	SV_VM_End();
}

/////////////////////////////////////////////////////
// SV VM stuff

static void SV_VM_CB_BeginIncreaseEdicts(void)
{
	// links don't survive the transition, so unlink everything
	World_UnlinkAll(&sv.world);
}

static void SV_VM_CB_EndIncreaseEdicts(void)
{
	int i;
	prvm_edict_t *ent;

	// link every entity except world
	for (i = 1, ent = prog->edicts;i < prog->num_edicts;i++, ent++)
		if (!ent->priv.server->free)
			SV_LinkEdict(ent);
}

static void SV_VM_CB_InitEdict(prvm_edict_t *e)
{
	// LordHavoc: for consistency set these here
	int num = PRVM_NUM_FOR_EDICT(e) - 1;

	e->priv.server->move = false; // don't move on first frame

	if (num >= 0 && num < svs.maxclients)
	{
		// set colormap and team on newly created player entity
		PRVM_serveredictfloat(e, colormap) = num + 1;
		PRVM_serveredictfloat(e, team) = (svs.clients[num].colors & 15) + 1;
		// set netname/clientcolors back to client values so that
		// DP_SV_CLIENTNAME and DP_SV_CLIENTCOLORS will not immediately
		// reset them
		PRVM_serveredictstring(e, netname) = PRVM_SetEngineString(svs.clients[num].name);
		PRVM_serveredictfloat(e, clientcolors) = svs.clients[num].colors;
		// NEXUIZ_PLAYERMODEL and NEXUIZ_PLAYERSKIN
		PRVM_serveredictstring(e, playermodel) = PRVM_SetEngineString(svs.clients[num].playermodel);
		PRVM_serveredictstring(e, playerskin) = PRVM_SetEngineString(svs.clients[num].playerskin);
		// Assign netaddress (IP Address, etc)
		if(svs.clients[num].netconnection != NULL)
		{
			// Acquire Readable Address
			LHNETADDRESS_ToString(&svs.clients[num].netconnection->peeraddress, svs.clients[num].netaddress, sizeof(svs.clients[num].netaddress), false);
			PRVM_serveredictstring(e, netaddress) = PRVM_SetEngineString(svs.clients[num].netaddress);
		}
		else
			PRVM_serveredictstring(e, netaddress) = PRVM_SetEngineString("null/botclient");
		if(svs.clients[num].netconnection != NULL && svs.clients[num].netconnection->crypto.authenticated && svs.clients[num].netconnection->crypto.client_idfp[0])
			PRVM_serveredictstring(e, crypto_idfp) = PRVM_SetEngineString(svs.clients[num].netconnection->crypto.client_idfp);
		else
			PRVM_serveredictstring(e, crypto_idfp) = 0;
		if(svs.clients[num].netconnection != NULL && svs.clients[num].netconnection->crypto.authenticated && svs.clients[num].netconnection->crypto.client_keyfp[0])
			PRVM_serveredictstring(e, crypto_keyfp) = PRVM_SetEngineString(svs.clients[num].netconnection->crypto.client_keyfp);
		else
			PRVM_serveredictstring(e, crypto_keyfp) = 0;
		if(svs.clients[num].netconnection != NULL && svs.clients[num].netconnection->crypto.authenticated && svs.clients[num].netconnection->crypto.server_keyfp[0])
			PRVM_serveredictstring(e, crypto_mykeyfp) = PRVM_SetEngineString(svs.clients[num].netconnection->crypto.server_keyfp);
		else
			PRVM_serveredictstring(e, crypto_mykeyfp) = 0;
		if(svs.clients[num].netconnection != NULL && svs.clients[num].netconnection->crypto.authenticated && svs.clients[num].netconnection->crypto.use_aes)
			PRVM_serveredictstring(e, crypto_encryptmethod) = PRVM_SetEngineString("AES128");
		else
			PRVM_serveredictstring(e, crypto_encryptmethod) = 0;
		if(svs.clients[num].netconnection != NULL && svs.clients[num].netconnection->crypto.authenticated)
			PRVM_serveredictstring(e, crypto_signmethod) = PRVM_SetEngineString("HMAC-SHA256");
		else
			PRVM_serveredictstring(e, crypto_signmethod) = 0;
	}
}

static void SV_VM_CB_FreeEdict(prvm_edict_t *ed)
{
	int i;
	int e;

	World_UnlinkEdict(ed);		// unlink from world bsp

	PRVM_serveredictstring(ed, model) = 0;
	PRVM_serveredictfloat(ed, takedamage) = 0;
	PRVM_serveredictfloat(ed, modelindex) = 0;
	PRVM_serveredictfloat(ed, colormap) = 0;
	PRVM_serveredictfloat(ed, skin) = 0;
	PRVM_serveredictfloat(ed, frame) = 0;
	VectorClear(PRVM_serveredictvector(ed, origin));
	VectorClear(PRVM_serveredictvector(ed, angles));
	PRVM_serveredictfloat(ed, nextthink) = -1;
	PRVM_serveredictfloat(ed, solid) = 0;

	VM_RemoveEdictSkeleton(ed);
	World_Physics_RemoveFromEntity(&sv.world, ed);
	World_Physics_RemoveJointFromEntity(&sv.world, ed);

	// make sure csqc networking is aware of the removed entity
	e = PRVM_NUM_FOR_EDICT(ed);
	sv.csqcentityversion[e] = 0;
	for (i = 0;i < svs.maxclients;i++)
	{
		if (svs.clients[i].csqcentityscope[e])
			svs.clients[i].csqcentityscope[e] = 1; // removed, awaiting send
		svs.clients[i].csqcentitysendflags[e] = 0xFFFFFF;
	}
}

static void SV_VM_CB_CountEdicts(void)
{
	int		i;
	prvm_edict_t	*ent;
	int		active, models, solid, step;

	active = models = solid = step = 0;
	for (i=0 ; i<prog->num_edicts ; i++)
	{
		ent = PRVM_EDICT_NUM(i);
		if (ent->priv.server->free)
			continue;
		active++;
		if (PRVM_serveredictfloat(ent, solid))
			solid++;
		if (PRVM_serveredictstring(ent, model))
			models++;
		if (PRVM_serveredictfloat(ent, movetype) == MOVETYPE_STEP)
			step++;
	}

	Con_Printf("num_edicts:%3i\n", prog->num_edicts);
	Con_Printf("active    :%3i\n", active);
	Con_Printf("view      :%3i\n", models);
	Con_Printf("touch     :%3i\n", solid);
	Con_Printf("step      :%3i\n", step);
}

static qboolean SV_VM_CB_LoadEdict(prvm_edict_t *ent)
{
	// remove things from different skill levels or deathmatch
	if (gamemode != GAME_TRANSFUSION) //Transfusion does this in QC
	{
		if (deathmatch.integer)
		{
			if (((int)PRVM_serveredictfloat(ent, spawnflags) & SPAWNFLAG_NOT_DEATHMATCH))
			{
				return false;
			}
		}
		else if ((current_skill <= 0 && ((int)PRVM_serveredictfloat(ent, spawnflags) & SPAWNFLAG_NOT_EASY  ))
			|| (current_skill == 1 && ((int)PRVM_serveredictfloat(ent, spawnflags) & SPAWNFLAG_NOT_MEDIUM))
			|| (current_skill >= 2 && ((int)PRVM_serveredictfloat(ent, spawnflags) & SPAWNFLAG_NOT_HARD  )))
		{
			return false;
		}
	}
	return true;
}

static void SV_VM_Setup(void)
{
	PRVM_Begin;
	PRVM_InitProg( PRVM_SERVERPROG );

	// allocate the mempools
	// TODO: move the magic numbers/constants into #defines [9/13/2006 Black]
	prog->progs_mempool = Mem_AllocPool("Server Progs", 0, NULL);
	prog->builtins = vm_sv_builtins;
	prog->numbuiltins = vm_sv_numbuiltins;
	prog->max_edicts = 512;
	if (sv.protocol == PROTOCOL_QUAKE)
		prog->limit_edicts = 640; // before quake mission pack 1 this was 512
	else if (sv.protocol == PROTOCOL_QUAKEDP)
		prog->limit_edicts = 2048; // guessing
	else if (sv.protocol == PROTOCOL_NEHAHRAMOVIE)
		prog->limit_edicts = 2048; // guessing!
	else if (sv.protocol == PROTOCOL_NEHAHRABJP || sv.protocol == PROTOCOL_NEHAHRABJP2 || sv.protocol == PROTOCOL_NEHAHRABJP3)
		prog->limit_edicts = 4096; // guessing!
	else
		prog->limit_edicts = MAX_EDICTS;
	prog->reserved_edicts = svs.maxclients;
	prog->edictprivate_size = sizeof(edict_engineprivate_t);
	prog->name = "server";
	prog->extensionstring = vm_sv_extensions;
	prog->loadintoworld = true;

	prog->begin_increase_edicts = SV_VM_CB_BeginIncreaseEdicts;
	prog->end_increase_edicts = SV_VM_CB_EndIncreaseEdicts;
	prog->init_edict = SV_VM_CB_InitEdict;
	prog->free_edict = SV_VM_CB_FreeEdict;
	prog->count_edicts = SV_VM_CB_CountEdicts;
	prog->load_edict = SV_VM_CB_LoadEdict;
	prog->init_cmd = VM_SV_Cmd_Init;
	prog->reset_cmd = VM_SV_Cmd_Reset;
	prog->error_cmd = Host_Error;
	prog->ExecuteProgram = SVVM_ExecuteProgram;

	PRVM_LoadProgs( sv_progs.string, SV_REQFUNCS, sv_reqfuncs, SV_REQFIELDS, sv_reqfields, SV_REQGLOBALS, sv_reqglobals);

	// some mods compiled with scrambling compilers lack certain critical
	// global names and field names such as "self" and "time" and "nextthink"
	// so we have to set these offsets manually, matching the entvars_t
	// but we only do this if the prog header crc matches, otherwise it's totally freeform
	if (prog->progs_crc == PROGHEADER_CRC || prog->progs_crc == PROGHEADER_CRC_TENEBRAE)
	{
		PRVM_ED_FindFieldOffset_FromStruct(entvars_t, modelindex);
		PRVM_ED_FindFieldOffset_FromStruct(entvars_t, absmin);
		PRVM_ED_FindFieldOffset_FromStruct(entvars_t, absmax);
		PRVM_ED_FindFieldOffset_FromStruct(entvars_t, ltime);
		PRVM_ED_FindFieldOffset_FromStruct(entvars_t, movetype);
		PRVM_ED_FindFieldOffset_FromStruct(entvars_t, solid);
		PRVM_ED_FindFieldOffset_FromStruct(entvars_t, origin);
		PRVM_ED_FindFieldOffset_FromStruct(entvars_t, oldorigin);
		PRVM_ED_FindFieldOffset_FromStruct(entvars_t, velocity);
		PRVM_ED_FindFieldOffset_FromStruct(entvars_t, angles);
		PRVM_ED_FindFieldOffset_FromStruct(entvars_t, avelocity);
		PRVM_ED_FindFieldOffset_FromStruct(entvars_t, punchangle);
		PRVM_ED_FindFieldOffset_FromStruct(entvars_t, classname);
		PRVM_ED_FindFieldOffset_FromStruct(entvars_t, model);
		PRVM_ED_FindFieldOffset_FromStruct(entvars_t, frame);
		PRVM_ED_FindFieldOffset_FromStruct(entvars_t, skin);
		PRVM_ED_FindFieldOffset_FromStruct(entvars_t, effects);
		PRVM_ED_FindFieldOffset_FromStruct(entvars_t, mins);
		PRVM_ED_FindFieldOffset_FromStruct(entvars_t, maxs);
		PRVM_ED_FindFieldOffset_FromStruct(entvars_t, size);
		PRVM_ED_FindFieldOffset_FromStruct(entvars_t, touch);
		PRVM_ED_FindFieldOffset_FromStruct(entvars_t, use);
		PRVM_ED_FindFieldOffset_FromStruct(entvars_t, think);
		PRVM_ED_FindFieldOffset_FromStruct(entvars_t, blocked);
		PRVM_ED_FindFieldOffset_FromStruct(entvars_t, nextthink);
		PRVM_ED_FindFieldOffset_FromStruct(entvars_t, groundentity);
		PRVM_ED_FindFieldOffset_FromStruct(entvars_t, health);
		PRVM_ED_FindFieldOffset_FromStruct(entvars_t, frags);
		PRVM_ED_FindFieldOffset_FromStruct(entvars_t, weapon);
		PRVM_ED_FindFieldOffset_FromStruct(entvars_t, weaponmodel);
		PRVM_ED_FindFieldOffset_FromStruct(entvars_t, weaponframe);
		PRVM_ED_FindFieldOffset_FromStruct(entvars_t, currentammo);
		PRVM_ED_FindFieldOffset_FromStruct(entvars_t, ammo_shells);
		PRVM_ED_FindFieldOffset_FromStruct(entvars_t, ammo_nails);
		PRVM_ED_FindFieldOffset_FromStruct(entvars_t, ammo_rockets);
		PRVM_ED_FindFieldOffset_FromStruct(entvars_t, ammo_cells);
		PRVM_ED_FindFieldOffset_FromStruct(entvars_t, items);
		PRVM_ED_FindFieldOffset_FromStruct(entvars_t, takedamage);
		PRVM_ED_FindFieldOffset_FromStruct(entvars_t, chain);
		PRVM_ED_FindFieldOffset_FromStruct(entvars_t, deadflag);
		PRVM_ED_FindFieldOffset_FromStruct(entvars_t, view_ofs);
		PRVM_ED_FindFieldOffset_FromStruct(entvars_t, button0);
		PRVM_ED_FindFieldOffset_FromStruct(entvars_t, button1);
		PRVM_ED_FindFieldOffset_FromStruct(entvars_t, button2);
		PRVM_ED_FindFieldOffset_FromStruct(entvars_t, impulse);
		PRVM_ED_FindFieldOffset_FromStruct(entvars_t, fixangle);
		PRVM_ED_FindFieldOffset_FromStruct(entvars_t, v_angle);
		PRVM_ED_FindFieldOffset_FromStruct(entvars_t, idealpitch);
		PRVM_ED_FindFieldOffset_FromStruct(entvars_t, netname);
		PRVM_ED_FindFieldOffset_FromStruct(entvars_t, enemy);
		PRVM_ED_FindFieldOffset_FromStruct(entvars_t, flags);
		PRVM_ED_FindFieldOffset_FromStruct(entvars_t, colormap);
		PRVM_ED_FindFieldOffset_FromStruct(entvars_t, team);
		PRVM_ED_FindFieldOffset_FromStruct(entvars_t, max_health);
		PRVM_ED_FindFieldOffset_FromStruct(entvars_t, teleport_time);
		PRVM_ED_FindFieldOffset_FromStruct(entvars_t, armortype);
		PRVM_ED_FindFieldOffset_FromStruct(entvars_t, armorvalue);
		PRVM_ED_FindFieldOffset_FromStruct(entvars_t, waterlevel);
		PRVM_ED_FindFieldOffset_FromStruct(entvars_t, watertype);
		PRVM_ED_FindFieldOffset_FromStruct(entvars_t, ideal_yaw);
		PRVM_ED_FindFieldOffset_FromStruct(entvars_t, yaw_speed);
		PRVM_ED_FindFieldOffset_FromStruct(entvars_t, aiment);
		PRVM_ED_FindFieldOffset_FromStruct(entvars_t, goalentity);
		PRVM_ED_FindFieldOffset_FromStruct(entvars_t, spawnflags);
		PRVM_ED_FindFieldOffset_FromStruct(entvars_t, target);
		PRVM_ED_FindFieldOffset_FromStruct(entvars_t, targetname);
		PRVM_ED_FindFieldOffset_FromStruct(entvars_t, dmg_take);
		PRVM_ED_FindFieldOffset_FromStruct(entvars_t, dmg_save);
		PRVM_ED_FindFieldOffset_FromStruct(entvars_t, dmg_inflictor);
		PRVM_ED_FindFieldOffset_FromStruct(entvars_t, owner);
		PRVM_ED_FindFieldOffset_FromStruct(entvars_t, movedir);
		PRVM_ED_FindFieldOffset_FromStruct(entvars_t, message);
		PRVM_ED_FindFieldOffset_FromStruct(entvars_t, sounds);
		PRVM_ED_FindFieldOffset_FromStruct(entvars_t, noise);
		PRVM_ED_FindFieldOffset_FromStruct(entvars_t, noise1);
		PRVM_ED_FindFieldOffset_FromStruct(entvars_t, noise2);
		PRVM_ED_FindFieldOffset_FromStruct(entvars_t, noise3);
		PRVM_ED_FindGlobalOffset_FromStruct(globalvars_t, self);
		PRVM_ED_FindGlobalOffset_FromStruct(globalvars_t, other);
		PRVM_ED_FindGlobalOffset_FromStruct(globalvars_t, world);
		PRVM_ED_FindGlobalOffset_FromStruct(globalvars_t, time);
		PRVM_ED_FindGlobalOffset_FromStruct(globalvars_t, frametime);
		PRVM_ED_FindGlobalOffset_FromStruct(globalvars_t, force_retouch);
		PRVM_ED_FindGlobalOffset_FromStruct(globalvars_t, mapname);
		PRVM_ED_FindGlobalOffset_FromStruct(globalvars_t, deathmatch);
		PRVM_ED_FindGlobalOffset_FromStruct(globalvars_t, coop);
		PRVM_ED_FindGlobalOffset_FromStruct(globalvars_t, teamplay);
		PRVM_ED_FindGlobalOffset_FromStruct(globalvars_t, serverflags);
		PRVM_ED_FindGlobalOffset_FromStruct(globalvars_t, total_secrets);
		PRVM_ED_FindGlobalOffset_FromStruct(globalvars_t, total_monsters);
		PRVM_ED_FindGlobalOffset_FromStruct(globalvars_t, found_secrets);
		PRVM_ED_FindGlobalOffset_FromStruct(globalvars_t, killed_monsters);
		PRVM_ED_FindGlobalOffset_FromStruct(globalvars_t, parm1);
		PRVM_ED_FindGlobalOffset_FromStruct(globalvars_t, parm2);
		PRVM_ED_FindGlobalOffset_FromStruct(globalvars_t, parm3);
		PRVM_ED_FindGlobalOffset_FromStruct(globalvars_t, parm4);
		PRVM_ED_FindGlobalOffset_FromStruct(globalvars_t, parm5);
		PRVM_ED_FindGlobalOffset_FromStruct(globalvars_t, parm6);
		PRVM_ED_FindGlobalOffset_FromStruct(globalvars_t, parm7);
		PRVM_ED_FindGlobalOffset_FromStruct(globalvars_t, parm8);
		PRVM_ED_FindGlobalOffset_FromStruct(globalvars_t, parm9);
		PRVM_ED_FindGlobalOffset_FromStruct(globalvars_t, parm10);
		PRVM_ED_FindGlobalOffset_FromStruct(globalvars_t, parm11);
		PRVM_ED_FindGlobalOffset_FromStruct(globalvars_t, parm12);
		PRVM_ED_FindGlobalOffset_FromStruct(globalvars_t, parm13);
		PRVM_ED_FindGlobalOffset_FromStruct(globalvars_t, parm14);
		PRVM_ED_FindGlobalOffset_FromStruct(globalvars_t, parm15);
		PRVM_ED_FindGlobalOffset_FromStruct(globalvars_t, parm16);
		PRVM_ED_FindGlobalOffset_FromStruct(globalvars_t, v_forward);
		PRVM_ED_FindGlobalOffset_FromStruct(globalvars_t, v_up);
		PRVM_ED_FindGlobalOffset_FromStruct(globalvars_t, v_right);
		PRVM_ED_FindGlobalOffset_FromStruct(globalvars_t, trace_allsolid);
		PRVM_ED_FindGlobalOffset_FromStruct(globalvars_t, trace_startsolid);
		PRVM_ED_FindGlobalOffset_FromStruct(globalvars_t, trace_fraction);
		PRVM_ED_FindGlobalOffset_FromStruct(globalvars_t, trace_endpos);
		PRVM_ED_FindGlobalOffset_FromStruct(globalvars_t, trace_plane_normal);
		PRVM_ED_FindGlobalOffset_FromStruct(globalvars_t, trace_plane_dist);
		PRVM_ED_FindGlobalOffset_FromStruct(globalvars_t, trace_ent);
		PRVM_ED_FindGlobalOffset_FromStruct(globalvars_t, trace_inopen);
		PRVM_ED_FindGlobalOffset_FromStruct(globalvars_t, trace_inwater);
		PRVM_ED_FindGlobalOffset_FromStruct(globalvars_t, msg_entity);
//		PRVM_ED_FindGlobalOffset_FromStruct(globalvars_t, main);
//		PRVM_ED_FindGlobalOffset_FromStruct(globalvars_t, StartFrame);
//		PRVM_ED_FindGlobalOffset_FromStruct(globalvars_t, PlayerPreThink);
//		PRVM_ED_FindGlobalOffset_FromStruct(globalvars_t, PlayerPostThink);
//		PRVM_ED_FindGlobalOffset_FromStruct(globalvars_t, ClientKill);
//		PRVM_ED_FindGlobalOffset_FromStruct(globalvars_t, ClientConnect);
//		PRVM_ED_FindGlobalOffset_FromStruct(globalvars_t, PutClientInServer);
//		PRVM_ED_FindGlobalOffset_FromStruct(globalvars_t, ClientDisconnect);
//		PRVM_ED_FindGlobalOffset_FromStruct(globalvars_t, SetNewParms);
//		PRVM_ED_FindGlobalOffset_FromStruct(globalvars_t, SetChangeParms);
	}
	else
		Con_DPrintf("%s: %s system vars have been modified (CRC %i != engine %i), will not load in other engines", PRVM_NAME, sv_progs.string, prog->progs_crc, PROGHEADER_CRC);

	// OP_STATE is always supported on server because we add fields/globals for it
	prog->flag |= PRVM_OP_STATE;

	VM_CustomStats_Clear();//[515]: csqc

	PRVM_End;

	SV_Prepare_CSQC();
}

void SV_VM_Begin(void)
{
	PRVM_Begin;
	PRVM_SetProg( PRVM_SERVERPROG );

	PRVM_serverglobalfloat(time) = (float) sv.time;
}

void SV_VM_End(void)
{
	PRVM_End;
}
