/*
==============================================================================

			SOURCE FOR GLOBALVARS_T C STRUCTURE
			MUST NOT BE MODIFIED, OR CRC ERRORS WILL APPEAR

==============================================================================
*/

//
// system globals
//
entity		self;
entity		other;
entity		world;
float		time;
float		frametime;

float		force_retouch;		// force all entities to touch triggers
								// next frame.  this is needed because
								// non-moving things don't normally scan
								// for triggers, and when a trigger is
								// created (like a teleport trigger), it
								// needs to catch everything.
								// decremented each frame, so set to 2
								// to guarantee everything is touched
string		mapname;

float		deathmatch;
float		coop;
float		teamplay;

float		serverflags;		// propagated from level to level, used to
								// keep track of completed episodes

float		total_secrets;
float		total_monsters;

float		found_secrets;		// number of secrets found
float		killed_monsters;	// number of monsters killed


// spawnparms are used to encode information about clients across server
// level changes
float		parm1, parm2, parm3, parm4, parm5, parm6, parm7, parm8, parm9, parm10, parm11, parm12, parm13, parm14, parm15, parm16;

//
// global variables set by built in functions
//
vector		v_forward, v_up, v_right;	// set by makevectors()

// set by traceline / tracebox
float		trace_allsolid;
float		trace_startsolid;
float		trace_fraction;
vector		trace_endpos;
vector		trace_plane_normal;
float		trace_plane_dist;
entity		trace_ent;
float		trace_inopen;
float		trace_inwater;

entity		msg_entity;				// destination of single entity writes

//
// required prog functions
//
void() 		main;						// only for testing

void()		StartFrame;

void() 		PlayerPreThink;
void() 		PlayerPostThink;

void()		ClientKill;
void()		ClientConnect;
void() 		PutClientInServer;		// call after setting the parm1... parms
void()		ClientDisconnect;

void()		SetNewParms;			// called when a client first connects to
									// a server. sets parms so they can be
									// saved off for restarts

void()		SetChangeParms;			// call to set parms for self so they can
									// be saved for a level transition


//================================================
void		end_sys_globals;		// flag for structure dumping
//================================================

/*
==============================================================================

			SOURCE FOR ENTVARS_T C STRUCTURE
			MUST NOT BE MODIFIED, OR CRC ERRORS WILL APPEAR

==============================================================================
*/

//
// system fields (*** = do not set in prog code, maintained by C code)
//
.float		modelindex;		// *** model index in the precached list
.vector		absmin, absmax;	// *** origin + mins / maxs

.float		ltime;			// local time for entity
.float		movetype;
.float		solid;

.vector		origin;			// ***
.vector		oldorigin;		// ***
.vector		velocity;
.vector		angles;
.vector		avelocity;

.vector		punchangle;		// temp angle adjust from damage or recoil

.string		classname;		// spawn function
.string		model;
.float		frame;
.float		skin;
.float		effects;

.vector		mins, maxs;		// bounding box extents reletive to origin
.vector		size;			// maxs - mins

.void()		touch;
.void()		use;
.void()		think;
.void()		blocked;		// for doors or plats, called when can't push other

.float		nextthink;
.entity		groundentity;

// stats
.float		health;
.float		frags;
.float		weapon;			// one of the IT_SHOTGUN, etc flags
.string		weaponmodel;
.float		weaponframe;
.float		currentammo;
.float		ammo_shells, ammo_nails, ammo_rockets, ammo_cells;

.float		items;			// bit flags

.float		takedamage;
.entity		chain;
.float		deadflag;

.vector		view_ofs;			// add to origin to get eye point


.float		button0;		// fire
.float		button1;		// use
.float		button2;		// jump

.float		impulse;		// weapon changes

.float		fixangle;
.vector		v_angle;		// view / targeting angle for players
.float		idealpitch;		// calculated pitch angle for lookup up slopes


.string		netname;

.entity 	enemy;

.float		flags;

.float		colormap;
.float		team;

.float		max_health;		// players maximum health is stored here

.float		teleport_time;	// don't back up

.float		armortype;		// save this fraction of incoming damage
.float		armorvalue;

.float		waterlevel;		// 0 = not in, 1 = feet, 2 = wast, 3 = eyes
.float		watertype;		// a contents value

.float		ideal_yaw;
.float		yaw_speed;

.entity		aiment;

.entity 	goalentity;		// a movetarget or an enemy

.float		spawnflags;

.string		target;
.string		targetname;

// damage is accumulated through a frame. and sent as one single
// message, so the super shotgun doesn't generate huge messages
.float		dmg_take;
.float		dmg_save;
.entity		dmg_inflictor;

.entity		owner;		// who launched a missile
.vector		movedir;	// mostly for doors, but also used for waterjump

.string		message;		// trigger messages

.float		sounds;		// either a cd track number or sound number

.string		noise, noise1, noise2, noise3;	// contains names of wavs to play

//================================================
void		end_sys_fields;			// flag for structure dumping
//================================================

/*
==============================================================================

				CONSTANT DEFINITIONS

==============================================================================
*/


//
// constants
//

float	FALSE					= 0;
float 	TRUE					= 1;

// edict.flags
float	FL_FLY					= 1;
float	FL_SWIM					= 2;
float	FL_CLIENT				= 8;	// set for all client edicts
float	FL_INWATER				= 16;	// for enter / leave water splash
float	FL_MONSTER				= 32;
float	FL_GODMODE				= 64;	// player cheat
float	FL_NOTARGET				= 128;	// player cheat
float	FL_ITEM					= 256;	// extra wide size for bonus items
float	FL_ONGROUND				= 512;	// standing on something
float	FL_PARTIALGROUND		= 1024;	// not all corners are valid
float	FL_WATERJUMP			= 2048;	// player jumping out of water
float	FL_JUMPRELEASED			= 4096;	// for jump debouncing

// edict.movetype values
float	MOVETYPE_NONE			= 0;	// never moves
//float	MOVETYPE_ANGLENOCLIP	= 1;
//float	MOVETYPE_ANGLECLIP		= 2;
float	MOVETYPE_WALK			= 3;	// players only
float	MOVETYPE_STEP			= 4;	// discrete, not real time unless fall
float	MOVETYPE_FLY			= 5;
float	MOVETYPE_TOSS			= 6;	// gravity
float	MOVETYPE_PUSH			= 7;	// no clip to world, push and crush
float	MOVETYPE_NOCLIP			= 8;
float	MOVETYPE_FLYMISSILE		= 9;	// fly with extra size against monsters
float	MOVETYPE_BOUNCE			= 10;
float	MOVETYPE_BOUNCEMISSILE	= 11;	// bounce with extra size

// edict.solid values
float	SOLID_NOT				= 0;	// no interaction with other objects
float	SOLID_TRIGGER			= 1;	// touch on edge, but not blocking
float	SOLID_BBOX				= 2;	// touch on edge, block
float	SOLID_SLIDEBOX			= 3;	// touch on edge, but not an onground
float	SOLID_BSP				= 4;	// bsp clip, touch on edge, block

// range values
float	RANGE_MELEE				= 0;
float	RANGE_NEAR				= 1;
float	RANGE_MID				= 2;
float	RANGE_FAR				= 3;

// deadflag values

float	DEAD_NO					= 0;
float	DEAD_DYING				= 1;
float	DEAD_DEAD				= 2;
float	DEAD_RESPAWNABLE		= 3;
float	DEAD_RESPAWNING			= 4; // dead, waiting for buttons to be released

// takedamage values

float	DAMAGE_NO				= 0;
float	DAMAGE_YES				= 1;
float	DAMAGE_AIM				= 2;

// items
float	IT_AXE					= 4096;
float	IT_SHOTGUN				= 1;
float	IT_SUPER_SHOTGUN		= 2;
float	IT_NAILGUN				= 4;
float	IT_SUPER_NAILGUN		= 8;
float	IT_GRENADE_LAUNCHER		= 16;
float	IT_ROCKET_LAUNCHER		= 32;
float	IT_LIGHTNING			= 64;
float	IT_EXTRA_WEAPON			= 128;

float	IT_SHELLS				= 256;
float	IT_NAILS				= 512;
float	IT_ROCKETS				= 1024;
float	IT_CELLS				= 2048;

float	IT_ARMOR1				= 8192;
float	IT_ARMOR2				= 16384;
float	IT_ARMOR3				= 32768;
float	IT_SUPERHEALTH			= 65536;

float	IT_KEY1					= 131072;
float	IT_KEY2					= 262144;

float	IT_INVISIBILITY			= 524288;
float	IT_INVULNERABILITY		= 1048576;
float	IT_SUIT					= 2097152;
float	IT_QUAD					= 4194304;

// point content values

float	CONTENT_EMPTY			= -1;
float	CONTENT_SOLID			= -2;
float	CONTENT_WATER			= -3;
float	CONTENT_SLIME			= -4;
float	CONTENT_LAVA			= -5;
float	CONTENT_SKY				= -6;

float	STATE_TOP		= 0;
float	STATE_BOTTOM	= 1;
float	STATE_UP		= 2;
float	STATE_DOWN		= 3;

vector	VEC_ORIGIN = '0 0 0';
vector	VEC_HULL_MIN = '-16 -16 -24';
vector	VEC_HULL_MAX = '16 16 32';

vector	VEC_HULL2_MIN = '-32 -32 -24';
vector	VEC_HULL2_MAX = '32 32 64';

// protocol bytes
float	SVC_TEMPENTITY		= 23;
float	SVC_KILLEDMONSTER	= 27;
float	SVC_FOUNDSECRET		= 28;
float	SVC_INTERMISSION	= 30;
float	SVC_FINALE			= 31;
float	SVC_CDTRACK			= 32;
float	SVC_SELLSCREEN		= 33;


float	TE_SPIKE		= 0;
float	TE_SUPERSPIKE	= 1;
float	TE_GUNSHOT		= 2;
float	TE_EXPLOSION	= 3;
float	TE_TAREXPLOSION	= 4;
float	TE_LIGHTNING1	= 5;
float	TE_LIGHTNING2	= 6;
float	TE_WIZSPIKE		= 7;
float	TE_KNIGHTSPIKE	= 8;
float	TE_LIGHTNING3	= 9;
float	TE_LAVASPLASH	= 10;
float	TE_TELEPORT		= 11;

// sound channels
// channel 0 never willingly overrides
// other channels (1-7) allways override a playing sound on that channel
float	CHAN_AUTO		= 0;
float	CHAN_WEAPON		= 1;
float	CHAN_VOICE		= 2;
float	CHAN_ITEM		= 3;
float	CHAN_BODY		= 4;

float	ATTN_NONE		= 0;
float	ATTN_NORM		= 1;
float	ATTN_IDLE		= 2;
float	ATTN_STATIC		= 3;

// update types

float	UPDATE_GENERAL	= 0;
float	UPDATE_STATIC	= 1;
float	UPDATE_BINARY	= 2;
float	UPDATE_TEMP		= 3;

// entity effects

float	EF_BRIGHTFIELD	= 1;
float	EF_MUZZLEFLASH 	= 2;
float	EF_BRIGHTLIGHT 	= 4;
float	EF_DIMLIGHT 	= 8;


// messages
float	MSG_BROADCAST	= 0;		// unreliable to all
float	MSG_ONE			= 1;		// reliable to one (msg_entity)
float	MSG_ALL			= 2;		// reliable to all
float	MSG_INIT		= 3;		// write to the init string

//================================================

//
// globals
//
float	movedist;
float	gameover;		// set when a rule exits

string	string_null;	// null string, nothing should be held here
//float	empty_float;

entity	newmis;			// launch_spike sets this after spawning it

entity	activator;		// the entity that activated a trigger or brush

entity	damage_attacker;	// set by T_Damage
float	framecount;

float		skill;

//================================================

//
// world fields (FIXME: make globals)
//
//.string		wad;
.string 	map;
.float		worldtype;	// 0=medieval 1=metal 2=base

//================================================

.string		killtarget;

//
// quakeed fields
//
.float		light_lev;		// not used by game, but parsed by light util
.float		style;


//
// monster ai
//
.void()		th_stand;
.void()		th_walk;
.void()		th_run;
.float()	th_missile; // LordHavoc: changed from void() to float(), returns true if attacking
.void()		th_melee;
.void(entity attacker, float damage, float damgtype, string dethtype)		th_pain;
.void()		th_die;

.entity		oldenemy;		// mad at this player before taking damage

.float		speed;

.float	lefty;

.float	search_time;
.float	attack_state;

float	AS_STRAIGHT		= 1;
float	AS_SLIDING		= 2;
float	AS_MELEE		= 3;
float	AS_MISSILE		= 4;

//
// player only fields
//
//.float		walkframe;

.float 		attack_finished;
.float		pain_finished;

.float		invincible_finished;
.float		invisible_finished;
.float		super_damage_finished;
.float		radsuit_finished;
.float		spawnshieldtime;

.float		invincible_time, invincible_sound;
.float		invisible_time, invisible_sound;
.float		super_time, super_sound;
.float		rad_time;
.float		fly_sound;

//.float		axhitme;

.float		show_hostile;	// set to time+0.2 whenever a client fires a
							// weapon or takes damage.  Used to alert
							// monsters that otherwise would let the player go
.float		jump_flag;		// player jump flag
.float		swim_flag;		// player swimming sound flag
.float		air_finished;	// when time > air_finished, start drowning
.float		bubble_count;	// keeps track of the number of bubbles
.string		deathtype;		// keeps track of how the player died

//
// object stuff
//
.string		mdl;
.vector		mangle;			// angle at start

.vector		oldorigin;		// only used by secret door

.float		t_length, t_width;


//
// doors, etc
//
.vector		dest, dest1, dest2;
.float		wait;			// time from firing to restarting
.float		delay;			// time from activation to firing
.entity		trigger_field;	// door's trigger entity
.string		noise4;

//
// monsters
//
.float 		pausetime;
.entity 	movetarget;


//
// doors
//
.float		aflag;
.float		dmg;			// damage done by door when hit

//
// misc
//
.float		cnt; 			// misc flag

//
// subs
//
.void()		think1;
//.vector		finaldest, finalangle;

//
// triggers
//
.float		count;			// for counting triggers


//
// plats / doors / buttons
//
.float		lip;
.float		state;
.vector		pos1, pos2;		// top and bottom positions
.float		height;

//
// sounds
//
.float		waitmin, waitmax;
//.float		distance;
//.float		volume;




//===========================================================================


//
// builtin functions
//

void(vector ang)	makevectors		= #1;		// sets v_forward, etc globals
void(entity e, vector o) setorigin	= #2;
void(entity e, string m) setmodel	= #3;		// set movetype and solid first
void(entity e, vector min, vector max) setsize = #4;
// #5 was removed
void() break						= #6;
float() random						= #7;		// returns 0 - 1
void(entity e, float chan, string samp, float vol, float atten) sound = #8;
vector(vector v) normalize			= #9;
void(string e, ...) error				= #10;
void(string e, ...) objerror				= #11;
float(vector v) vlen				= #12;
float(vector v) vectoyaw			= #13;
entity() spawn						= #14;
void(entity e) remove				= #15;

// sets trace_* globals
// nomonsters can be:
// An entity will also be ignored for testing if forent == test,
// forent->owner == test, or test->owner == forent
// a forent of world is ignored
void(vector v1, vector v2, float nomonsters, entity forent) traceline = #16;

entity() checkclient				= #17;	// returns a client to look for
entity(entity start, .string fld, string match) find = #18;
string(string s) precache_sound		= #19;
string(string s) precache_model		= #20;
void(entity client, string s, ...)stuffcmd = #21;
entity(vector org, float rad) findradius = #22;
void(string s, ...) bprint				= #23;
void(entity client, string s, ...) sprint = #24;
void(string s, ...) dprint				= #25;
string(float f) ftos				= #26;
string(vector v) vtos				= #27;
void() coredump						= #28;		// prints all edicts
void() traceon						= #29;		// turns statment trace on
void() traceoff						= #30;
void(entity e) eprint				= #31;		// prints an entire edict
float(float yaw, float dist) walkmove	= #32;	// returns TRUE or FALSE
// #33 was removed
float() droptofloor= #34;	// TRUE if landed on floor
void(float style, string value) lightstyle = #35;
float(float v) rint					= #36;		// round to nearest int
float(float v) floor				= #37;		// largest integer <= v
float(float v) ceil					= #38;		// smallest integer >= v
// #39 was removed
float(entity e) checkbottom			= #40;		// true if self is on ground
float(vector v) pointcontents		= #41;		// returns a CONTENT_*
// #42 was removed
float(float f) fabs = #43;
vector(entity e, float speed) aim = #44;		// returns the shooting vector
float(string s) cvar = #45;						// return cvar.value
void(string s, ...) localcmd = #46;					// put string into local que
entity(entity e) nextent = #47;					// for looping through all ents
void(vector o, vector d, float color, float count) particle = #48;// start a particle effect
void() ChangeYaw = #49;						// turn towards self.ideal_yaw
											// at self.yaw_speed
// #50 was removed
vector(vector v) vectoangles			= #51;

//
// direct client message generation
//
void(float to, float f) WriteByte		= #52;
void(float to, float f) WriteChar		= #53;
void(float to, float f) WriteShort		= #54;
void(float to, float f) WriteLong		= #55;
void(float to, float f) WriteCoord		= #56;
void(float to, float f) WriteAngle		= #57;
void(float to, string s, ...) WriteString	= #58;
void(float to, entity s) WriteEntity	= #59;

//
// broadcast client message generation
//

// void(float f) bWriteByte		= #59;
// void(float f) bWriteChar		= #60;
// void(float f) bWriteShort		= #61;
// void(float f) bWriteLong		= #62;
// void(float f) bWriteCoord		= #63;
// void(float f) bWriteAngle		= #64;
// void(string s) bWriteString	= #65;
// void(entity e) bWriteEntity = #66;

void(float step) movetogoal				= #67;

string(string s) precache_file		= #68;	// no effect except for -copy
void(entity e) makestatic		= #69;
void(string s) changelevel = #70;

//#71 was removed

void(string var, string val) cvar_set = #72;	// sets cvar.value

void(entity client, string s, ...) centerprint = #73;	// sprint, but in middle

void(vector pos, string samp, float vol, float atten) ambientsound = #74;

string(string s) precache_model2	= #75;		// registered version only
string(string s) precache_sound2	= #76;		// registered version only
string(string s) precache_file2		= #77;		// registered version only

void(entity e) setspawnparms		= #78;		// set parm1... to the
												// values at level start
												// for coop respawn

//============================================================================

//
// subs.qc
//
void(vector tdest, float tspeed, void() func) SUB_CalcMove;
void(entity ent, vector tdest, float tspeed, void() func) SUB_CalcMoveEnt;
void(vector destangle, float tspeed, void() func) SUB_CalcAngleMove;
void()  SUB_CalcMoveDone;
void() SUB_CalcAngleMoveDone;
void() SUB_Null;
void() SUB_UseTargets;
void() SUB_Remove;

//
//	combat.qc
//



float(entity targ, entity inflictor) CanDamage;





//void(vector org, entity en, vector dir, float holetype) newbullethole;


//void(float msg, vector a) WriteVec =
//{
//	WriteCoord(msg, a_x);
//	WriteCoord(msg, a_y);
//	WriteCoord(msg, a_z);
//};

float() crandom =
{
	return 2*(random() - 0.5);
};

vector(vector m1, vector m2) randompos =
{
	local vector v;
	m2 = m2 - m1;
	v_x = m2_x * random() + m1_x;
	v_y = m2_y * random() + m1_y;
	v_z = m2_z * random() + m1_z;
	return  v;
};

// LordHavoc: made this a builtin function
/*
vector() randomvec =
{
	local vector v;
	do
	{
		v_x = random() - 0.5; // scaled up in the return()
		v_y = random() - 0.5;
		v_z = random() - 0.5;
	}
	while (vlen(v) > 0.25);
	return(v * 2);
};
*/

vector() randomvec2 =
{
	local vector v;
	v_x = random() - 0.5; // scale doesn't matter because it's normalized
	v_y = random() - 0.5;
	v_z = random() - 0.5;
	v = normalize(v);
	return(v);
};

vector() randomdirvec =
{
	local vector v;
	do
	{
		do
		{
			v_x = random() - 0.5;
			v_y = random() - 0.5;
			v_z = random() - 0.5;
		}
		while (vlen(v) > 0.25);
	}
	while (vlen(v) < 0.1);
	return normalize(v);
};

/*
float(float a) sqrt = // now this IS silly (using vlen to do square root)
{
	local vector v;
	v_x = a;
	return (vlen(v));
};
*/

// returns an empty point near the location if it's in a solid
// (useful for explosions)
vector (vector org) findbetterlocation =
{
	local vector v;
	local float c;
	if (pointcontents(org) != CONTENT_SOLID)
		return org;
	c = 0;
	while (c < 20)
	{
		c = c + 1;
// 4.1 to get it to choose +2 occasionally (rather than a 1 in 32768 chance)
		v_x = org_x + random() * 4.1 - 2;
		v_y = org_y + random() * 4.1 - 2;
		v_z = org_z + random() * 4.1 - 2;
		if (pointcontents(v) != CONTENT_SOLID)
			return v;
	}
	return org; // failed to find an empty point
};

// adjusts the entity's origin to move it out of a solid
// (useful for projectiles with no size, so they explode in open air)
// note this is mostly useless on entities bigger than that
// (unless they happen to have their origin on the surface of a wall)
void (entity e) findbetterlocation2 =
{
	local vector v, org;
	local float c;
	if (pointcontents(e.origin) != CONTENT_SOLID)
		return;
	org = e.origin;
	c = 0;
	while (c < 20)
	{
		c = c + 1;
// 4.1 to get it to choose 4 occasionally (rather than a 1 in 32768 chance)
		v_x = org_x + random() * 4.1 - 2;
		v_y = org_y + random() * 4.1 - 2;
		v_z = org_z + random() * 4.1 - 2;
		if (pointcontents(v) != CONTENT_SOLID)
		{
			setorigin(e, v);
			return;
		}
	}
	return; // failed to find an empty point
};

// returns a point at least 12 units away from walls
// (useful for explosion animations, although the blast is performed where it really happened)
vector (vector org) findbetterlocation3 =
{
	local vector v2;
	traceline(org, org - '12 0 0' , TRUE, world);if (trace_fraction < 1) {v2 = trace_endpos;traceline(v2, v2 + '12 0 0' , TRUE, world);if (trace_fraction >= 1) org = v2 + '12 0 0' ;}
	traceline(org, org - '-12 0 0', TRUE, world);if (trace_fraction < 1) {v2 = trace_endpos;traceline(v2, v2 + '-12 0 0', TRUE, world);if (trace_fraction >= 1) org = v2 + '-12 0 0';}
	traceline(org, org - '0 12 0' , TRUE, world);if (trace_fraction < 1) {v2 = trace_endpos;traceline(v2, v2 + '0 12 0' , TRUE, world);if (trace_fraction >= 1) org = v2 + '0 12 0' ;}
	traceline(org, org - '0 -12 0', TRUE, world);if (trace_fraction < 1) {v2 = trace_endpos;traceline(v2, v2 + '0 -12 0', TRUE, world);if (trace_fraction >= 1) org = v2 + '0 -12 0';}
	traceline(org, org - '0 0 12' , TRUE, world);if (trace_fraction < 1) {v2 = trace_endpos;traceline(v2, v2 + '0 0 12' , TRUE, world);if (trace_fraction >= 1) org = v2 + '0 0 12' ;}
	traceline(org, org - '0 0 -12', TRUE, world);if (trace_fraction < 1) {v2 = trace_endpos;traceline(v2, v2 + '0 0 -12', TRUE, world);if (trace_fraction >= 1) org = v2 + '0 0 -12';}
	return org;
};

vector (vector org) findbetterlocation4 =
{
	local vector v2;
	traceline(org, org - '5 0 0' , TRUE, world);if (trace_fraction < 1) {v2 = trace_endpos;traceline(v2, v2 + '5 0 0' , TRUE, world);if (trace_fraction >= 1) org = v2 + '5 0 0' ;}
	traceline(org, org - '-5 0 0', TRUE, world);if (trace_fraction < 1) {v2 = trace_endpos;traceline(v2, v2 + '-5 0 0', TRUE, world);if (trace_fraction >= 1) org = v2 + '-5 0 0';}
	traceline(org, org - '0 5 0' , TRUE, world);if (trace_fraction < 1) {v2 = trace_endpos;traceline(v2, v2 + '0 5 0' , TRUE, world);if (trace_fraction >= 1) org = v2 + '0 5 0' ;}
	traceline(org, org - '0 -5 0', TRUE, world);if (trace_fraction < 1) {v2 = trace_endpos;traceline(v2, v2 + '0 -5 0', TRUE, world);if (trace_fraction >= 1) org = v2 + '0 -5 0';}
	traceline(org, org - '0 0 5' , TRUE, world);if (trace_fraction < 1) {v2 = trace_endpos;traceline(v2, v2 + '0 0 5' , TRUE, world);if (trace_fraction >= 1) org = v2 + '0 0 5' ;}
	traceline(org, org - '0 0 -5', TRUE, world);if (trace_fraction < 1) {v2 = trace_endpos;traceline(v2, v2 + '0 0 -5', TRUE, world);if (trace_fraction >= 1) org = v2 + '0 0 -5';}
	return org;
};

/*
void(vector v1, vector v2, vector m1, vector m2) lh_traceagainstbox =
{
	local vector b, dir, delta, dironx, dirony, dironz;
	local float completedist, r;
	trace_endpos = v2;
	trace_fraction = 1;
	if (vlen(v2 - v1) < 0.1) // too short, might do a divide by zero
		return;
	// throw out the complete misses
	if (v1_x <= m1_x) if (v2_x <= m1_x) return;
	if (v1_x >= m2_x) if (v2_x >= m2_x) return;
	if (v1_y <= m1_y) if (v2_y <= m1_y) return;
	if (v1_y >= m2_y) if (v2_y >= m2_y) return;
	if (v1_z <= m1_z) if (v2_z <= m1_z) return;
	if (v1_z >= m2_z) if (v2_z >= m2_z) return;
	// starting inside the box?
	if (v1_x >= m1_x) if (v1_x < m2_x) if (v1_y >= m1_y) if (v1_y < m2_y) if (v1_z >= m1_z) if (v1_z < m2_z) return;
	delta = v2 - v1;
	dir = normalize(delta);
	completedist = vlen(v2 - v1);
	if (delta_x < -0.001 || delta_x > 0.001)
	{
		dironx_x = 1;if (delta_x < 0) dironx_x = -1;
		dironx_y = delta_y / delta_x;
		dironx_z = delta_z / delta_x;
	}
	else
	{
		dironx_x = 1;if (delta_x < 0) dironx_x = -1;
		dironx_y = 0;
		dironx_z = 0;
	}
	if (delta_y < -0.001 || delta_y > 0.001)
	{
		dirony_x = delta_x / delta_y;
		dirony_y = 1;if (delta_y < 0) dirony_y = -1;
		dirony_z = delta_z / delta_y;
	}
	else
	{
		dirony_x = 0;
		dirony_y = 1;if (delta_y < 0) dirony_y = -1;
		dirony_z = 0;
	}
	if (delta_z < -0.001 || delta_z > 0.001)
	{
		dironz_x = delta_x / delta_z;
		dironz_y = delta_y / delta_z;
		dironz_z = 1;if (delta_z < 0) dironz_z = -1;
	}
	else
	{
		dironz_x = 0;
		dironz_y = 0;
		dironz_z = 1;if (delta_z < 0) dironz_z = -1;
	}
	b = v1;
	r = floor(random() * 1000);
	if (dir_x > 0)
	{
		if (b_x < m1_x)
		{
			b = b + dironx * (m1_x - b_x);
			trace_fraction = vlen(b - v1) / completedist;
		}
	}
	else
	{
		if (b_x > m2_x)
		{
			b = b - dironx * (m2_x - b_x);
			trace_fraction = vlen(b - v1) / completedist;
		}
	}
	trace_endpos = b;
	if (trace_fraction > 1) trace_fraction = 1;
	if (dir_y > 0)
	{
		if (b_y < m1_y)
		{
			b = b + dirony * (m1_y - b_y);
			trace_fraction = vlen(b - v1) / completedist;
		}
	}
	else
	{
		if (b_y > m2_y)
		{
			b = b - dirony * (m2_y - b_y);
			trace_fraction = vlen(b - v1) / completedist;
		}
	}
	trace_endpos = b;
	if (trace_fraction > 1) trace_fraction = 1;
	if (dir_z > 0)
	{
		if (b_z < m1_z)
		{
			b = b + dironz * (m1_z - b_z);
			trace_fraction = vlen(b - v1) / completedist;
		}
	}
	else
	{
		if (b_z > m2_z)
		{
			b = b - dironz * (m2_z - b_z);
			trace_fraction = vlen(b - v1) / completedist;
		}
	}
	trace_endpos = b;
	if (trace_fraction > 1) trace_fraction = 1;
};
*/

.float cantrigger;

void(float n) bprintfloat =
{
	local string s;
	s = ftos(n);
	bprint(s);
};

void(vector n) bprintvector =
{
	local string s;
	s = vtos(n);
	bprint(s);
};

void(float n) dprintfloat =
{
	local string s;
	s = ftos(n);
	dprint(s);
};

void(vector n) dprintvector =
{
	local string s;
	s = vtos(n);
	dprint(s);
};

string deathstring1, deathstring2, deathstring3, deathstring4;

float DTYPE_OTHER = 0;
float DTYPE_PLAYER = 1;
float DTYPE_WORLD = 2;
float DTYPE_TEAMKILL = 3;
float DTYPE_SUICIDE = 4;

void(entity targ, entity attacker, string dmsg, float dtype) Obituary_Generic =
{
	deathstring1 = targ.netname;
	deathstring2 = dmsg;
	deathstring3 = "";
	deathstring4 = "";
};

// called by various obit functions to give a generic message for cases they do not handle
void(entity targ, entity attacker, string dmsg, float dtype) Obituary_Fallback =
{
	if (dtype == DTYPE_OTHER)
	{
		deathstring1 = targ.netname;
		deathstring2 = dmsg;
		deathstring3 = "";
		deathstring4 = "";
	}
	else if (dtype == DTYPE_SUICIDE)
	{
		deathstring1 = targ.netname;
		deathstring2 = " became bored with life";
		deathstring3 = "";
		deathstring4 = "";
	}
	else if (dtype == DTYPE_PLAYER)
	{
		deathstring1 = targ.netname;
		deathstring2 = " was killed by ";
		deathstring3 = attacker.netname;
		deathstring4 = "";
	}
	else if (dtype == DTYPE_TEAMKILL)
	{
		deathstring1 = targ.netname;
		deathstring2 = " was mowed down by his teammate ";
		deathstring3 = attacker.netname;
		deathstring4 = "";
	}
	else if (dtype == DTYPE_WORLD)
	{
		deathstring1 = targ.netname;
		deathstring2 = " died of unknown causes";
		deathstring3 = "";
		deathstring4 = "";
	}
};

float GAME_QUAKE = 0;
float GAME_NEXUIZ = 1;
float game;

float darkmode;

float IT_WEAPON1                      = 4096;
float IT_WEAPON2                      = 1;
float IT_WEAPON3                      = 2;
float IT_WEAPON4                      = 4;
float IT_WEAPON5                      = 8;
float IT_WEAPON6                      = 16;
float IT_WEAPON7                      = 32;
float IT_WEAPON8                      = 64;
float IT_WEAPON9                      = 128;
float IT_WEAPON10                     = 8388608;

// see modecheck.qc for deathmatch and teamplay settings

// added for Dark Places
float CHAN_SPEECH             = 5;
float CHAN_FOOT               = 6;
float CHAN_WEAPON2            = 7;

// .gravity field added in Quake 1.07 (Scourge of Armagon)
.float gravity;

.float		bodyhealth;		// used by corpse code
.float		iscorpse;		// used by corpse code

.vector dest, dest1, dest2, dest3, dest4;//, dest5;


.entity flame;			// the flame burning this thing

.float doobits;		// set if this should do obit on death

//.vector bodymins, bodymaxs, headmins, headmaxs;

.vector rotate;
.string group;

// counts of how many keys this player/bot has
.float keys_silver;
.float keys_gold;

.float havocattack;
.float havocpickup;
.float(entity player, entity item) pickupevalfunc; // returns rating for item considering player's condition (don't pick up health if it's not needed, etc)
.float shoulddodge;
.float dangerrating;

// called whenever damage is done, if not supplied there is no visible effect.
.void(vector org, float bodydamage, float armordamage, vector vel, float damgtype) bleedfunc;

// several counters
.float count1, count2, count3, count4, count5, count6, cnt1, cnt2;

.void(entity t, entity a, string m, float dtyp) obitfunc1;
.entity realowner;

float maxclients; // set by worldspawn code
float numdecors;
float maxdecors;
.float createdtime;
.void() th_gib;

//void(vector org, entity en, vector dir, float splattype, float importance) newbloodsplat;
void(vector org, float bodydamage, float armordamage, vector vel, float damgtype) genericbleedfunc;




// damage stuff

void(entity targ, entity inflictor, entity attacker, float damage, float bdamage, string dt, float damgtype, vector damgpoint, vector force, void(entity t, entity a, string m, float dtyp) obitfunc) T_Damage;
void(entity inflictor, entity attacker, float damage, float force, float radius, entity ignore, string dethtype, float damgtype, void(entity t, entity a, string m, float dtyp) obitfunc) T_RadiusDamage;


// resistances to 4 kinds of damage:
.float resist_bullet, resist_explosive, resist_energy, resist_fire, resist_ice, resist_axe;
// every monster can have resistances
// note: -1 means immune, as 0.0 can't be used

// kill message
.string deathtype;
// copied from .deathtype for the attacker
// a map entity can gain alot of meaning by
// adding a kill message in the deathtype key
// such as ' was chopped to bits'
// or ' fell into the abyss'
// if you set a deathtype for a monster
// in a map key it will override the normal
// kill message for that monster
// so you could have some hell knights that
// say ' was slain by one of the palace guards'
// and many other ways to add meaning to a map
.string deathmsg;

.float regenthink; // next time player will regen some health
.float isdecor;
.float radiusdamage_amount; // damage counter
.vector radiusdamage_force; // direction and power of blast kick
.float radiusdamage_hit; // 1 if it was hit, reset to 0 afterward
.vector radiusdamage_lasthit; // location
.float radiusdamage_ownerdamagescale; // if set on inflictor this will scale damage to owner (overriding default of 0.5)
vector raddamage_lasthit;

.float frozen;
.float thawtime;
.float thawedeffects;
.void() thawedtouch;
.float thawedmovetype;
.void() thawedthink;
.float thawedthinkdelay;

.void() knockedloosefunc; // called when kinetically disturbed
.float forcescale; // used for damage force calculations (shambler has lower forcescale than player for instance)

// how much bleeding 'matters' to this entity, a percentage between
// bodydamage (-1 = 0% as 0 doesn't work) and healthdamage (1.0 = 100%)
// for instance a player is 100% and a zombie is 0% (bleeding doesn't matter)
.float bleedratio;
.float(entity e, float healthdamage, float damage, float damgtype, string dethtype) damagemodifier;

.void() th_gib;

.float health; // if this goes below 1 the thing is dead
.float bodyhealth; // used for gibbing
.float healthregen; // regenerate this much health per second
.float healthlostthisframe; // reset by iscreature code each frame

// adjustable monster damage settings
float monsterdamagescale;
float monsterresistancescale;
// adjustable player damage settings
float playerdamagescale;

void(entity targ, entity attacker, string dmsg, float dtype, void(entity t, entity a, string m, float dtyp) obitfunc) ClientObituary;

void() monster_death_use;
