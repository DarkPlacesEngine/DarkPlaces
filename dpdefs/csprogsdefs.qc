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

float 		player_localentnum;	//the entnum
float 		player_localnum;	//the playernum
float		maxclients;	//a constant filled in by the engine. gah, portability eh?

float		clientcommandframe;	//player movement
float		servercommandframe;	//clientframe echoed off the server

string		mapname;

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

//
// required prog functions
//
void()		CSQC_Init;
void()		CSQC_Shutdown;
float(float f, float t, float n)	CSQC_InputEvent;
void(float w, float h)		CSQC_UpdateView;
float(string s)	CSQC_ConsoleCommand;

//these fields are read and set by the default player physics
vector		pmove_org;
vector		pmove_vel;
vector		pmove_mins;
vector		pmove_maxs;
//retrieved from the current movement commands (read by player physics)
float		input_timelength;
vector		input_angles;
vector		input_movevalues;	//forwards, right, up.
float		input_buttons;		//attack, use, jump (default physics only uses jump)

float		movevar_gravity;
float		movevar_stopspeed;
float		movevar_maxspeed;
float		movevar_spectatormaxspeed;	//used by NOCLIP movetypes.
float		movevar_accelerate;
float		movevar_airaccelerate;
float		movevar_wateraccelerate;
float		movevar_friction;
float		movevar_waterfriction;
float		movevar_entgravity;	//the local player's gravity field. Is a multiple (1 is the normal value)

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

.float		entnum;	// *** the ent number as on the server
.float		drawmask;
.void()		predraw;

.float		movetype;
.float		solid;

.vector		origin;			// ***
.vector		oldorigin;		// ***
.vector		velocity;
.vector		angles;
.vector		avelocity;

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

.entity		chain;

.string		netname;

.entity 	enemy;

.float		flags;

.float		colormap;

.entity		owner;		// who launched a missile

//================================================
void		end_sys_fields;			// flag for structure dumping
//================================================

/*
==============================================================================

			OPTIONAL FIELDS AND GLOBALS

==============================================================================
*/

// Additional OPTIONAL Fields and Globals
float		intermission;

vector		view_angles; // same as input_angles
vector		view_punchangle;
vector		view_punchvector;

/*
==============================================================================

			CONSTANT DEFINITIONS

==============================================================================
*/

const float MASK_ENGINE			= 1;
const float MASK_ENGINEVIEWMODELS	= 2;
const float MASK_NORMAL			= 4;

const float RF_VIEWMODEL	= 1;
const float RF_EXTERNALMODEL	= 2;
const float RF_DEPTHHACK	= 4;
const float RF_ADDATIVE		= 8;
const float RF_USEAXIS		= 16;

const float VF_MIN		= 1;	//(vector)
const float VF_MIN_X		= 2;	//(float)
const float VF_MIN_Y		= 3;	//(float)
const float VF_SIZE		= 4;	//(vector) (viewport size)
const float VF_SIZE_Y		= 5;	//(float)
const float VF_SIZE_X		= 6;	//(float)
const float VF_VIEWPORT		= 7;	//(vector, vector)
const float VF_FOV		= 8;	//(vector)
const float VF_FOVX		= 9;	//(float)
const float VF_FOVY		= 10;	//(float)
const float VF_ORIGIN		= 11;	//(vector)
const float VF_ORIGIN_X		= 12;	//(float)
const float VF_ORIGIN_Y		= 13;	//(float)
const float VF_ORIGIN_Z		= 14;	//(float)
const float VF_ANGLES		= 15;	//(vector)
const float VF_ANGLES_X		= 16;	//(float)
const float VF_ANGLES_Y		= 17;	//(float)
const float VF_ANGLES_Z		= 18;	//(float)
const float VF_DRAWWORLD	= 19;	//(float)
const float VF_DRAWENGINESBAR	= 20;	//(float)
const float VF_DRAWCROSSHAIR	= 21;	//(float)

const float VF_CL_VIEWANGLES	= 33;	//(vector)
const float VF_CL_VIEWANGLES_X	= 34;	//(float)
const float VF_CL_VIEWANGLES_Y	= 35;	//(float)
const float VF_CL_VIEWANGLES_Z	= 36;	//(float) 

const float VF_PERSPECTIVE      = 200;

const float STAT_HEALTH		= 0;
const float STAT_WEAPONMODEL	= 2;
const float STAT_AMMO		= 3;
const float STAT_ARMOR		= 4;
const float STAT_WEAPONFRAME	= 5;
const float STAT_SHELLS		= 6;
const float STAT_NAILS		= 7;
const float STAT_ROCKETS	= 8;
const float STAT_CELLS		= 9;
const float STAT_ACTIVEWEAPON	= 10;
const float STAT_TOTALSECRETS	= 11;
const float STAT_TOTALMONSTERS	= 12;
const float STAT_SECRETS		= 13;
const float STAT_MONSTERS		= 14;
const float STAT_ITEMS		= 15;
const float STAT_VIEWHEIGHT	= 16;

// Quake Sound Constants
const float CHAN_AUTO			= 0;
const float CHAN_WEAPON			= 1;
const float CHAN_VOICE			= 2;
const float CHAN_ITEM			= 3;
const float CHAN_BODY			= 4;

const float ATTN_NONE			= 0;
const float ATTN_NORM			= 1;
const float ATTN_IDLE			= 2;
const float ATTN_STATIC			= 3;

// Frik File Constants
const float FILE_READ			= 0;
const float FILE_APPEND			= 1;
const float FILE_WRITE			= 2;

// Quake Point Contents
const float CONTENT_EMPTY					= -1;
const float CONTENT_SOLID					= -2;
const float CONTENT_WATER					= -3;
const float CONTENT_SLIME					= -4;
const float CONTENT_LAVA					= -5;
const float CONTENT_SKY						= -6;

// Quake Solid Constants
const float SOLID_NOT						= 0;
const float SOLID_TRIGGER					= 1;
const float SOLID_BBOX						= 2;
const float SOLID_SLIDEBOX					= 3;
const float SOLID_BSP						= 4;
const float SOLID_CORPSE					= 5;

// Quake Move Constants
const float MOVE_NORMAL						= 0;
const float MOVE_NOMONSTERS					= 1;
const float MOVE_MISSILE					= 2;

// Boolean Constants
const float true	= 1;
const float false	= 0;
const float TRUE	= 1;
const float FALSE	= 0;

const float EXTRA_LOW = -99999999;
const float EXTRA_HIGH = 99999999;

const vector VEC_1	= '1 1 1';
const vector VEC_0	= '0 0 0';
const vector VEC_M1	= '-1 -1 -1';

const float M_PI = 3.14159265358979323846;

vector	VEC_HULL_MIN = '-16 -16 -24';
vector	VEC_HULL_MAX = '16 16 32';

// Quake Temporary Entity Constants
const float TE_SPIKE						= 0;
const float TE_SUPERSPIKE					= 1;
const float TE_GUNSHOT						= 2;
const float TE_EXPLOSION					= 3;
const float TE_TAREXPLOSION					= 4;
const float TE_LIGHTNING1					= 5;
const float TE_LIGHTNING2					= 6;
const float TE_WIZSPIKE						= 7;
const float TE_KNIGHTSPIKE					= 8;
const float TE_LIGHTNING3					= 9;
const float TE_LAVASPLASH					= 10;
const float TE_TELEPORT						= 11;
const float TE_EXPLOSION2					= 12;
	// Darkplaces Additions
	const float TE_EXPLOSIONRGB					= 53;
	const float TE_GUNSHOTQUAD					= 57;
	const float TE_EXPLOSIONQUAD 				= 70;
	const float TE_SPIKEQUAD					= 58;
	const float TE_SUPERSPIKEQUAD				= 59;
	
// PFlags for Dynamic Lights
const float PFLAGS_NOSHADOW					= 1;
const float PFLAGS_CORONA					= 2;
const float PFLAGS_FULLDYNAMIC				= 128;

const float EF_ADDITIVE		= 32;
const float EF_BLUE		= 64;
const float EF_FLAME		= 1024;
const float EF_FULLBRIGHT	= 512;
const float EF_NODEPTHTEST	= 8192;
const float EF_NODRAW		= 16;
const float EF_NOSHADOW		= 4096;
const float EF_RED		= 128;
const float EF_STARDUST		= 2048;
const float EF_SELECTABLE 	= 16384;

const float PFL_ONGROUND	= 1;
const float PFL_CROUCH		= 2;
const float PFL_DEAD		= 4;
const float PFL_GIBBED		= 8;

/*
==============================================================================

			BUILTIN DEFINITIONS
			EXTENSIONS ARE NOT ADDED HERE, BUT BELOW!

==============================================================================
*/

void(vector ang) makevectors = #1;
void(entity e, vector o) setorigin = #2;
void(entity e, string m) setmodel = #3;
void(entity e, vector min, vector max) setsize = #4;

void() break = #6;
float() random = #7;
void(entity e, float chan, string samp) sound = #8;
vector(vector v) normalize = #9;
void(string e) error = #10;
void(string e) objerror = #11;
float(vector v) vlen = #12;
float(vector v) vectoyaw = #13;
entity() spawn = #14;
void(entity e) remove = #15;
float(vector v1, vector v2, float tryents, entity ignoreentity) traceline = #16;

entity(entity start, .string fld, string match) find = #18;
void(string s) precache_sound = #19;
void(string s) precache_model = #20;

entity(vector org, float rad) findradius = #22;

void(string s, ...) dprint = #25;
string(float f) ftos = #26;
string(vector v) vtos = #27;
void() coredump = #28;
void() traceon = #29;
void() traceoff = #30;
void(entity e) eprint = #31;
// settrace optional
float(float yaw, float dist, float settrace) walkmove = #32;

float() droptofloor = #34;
void(float style, string value) lightstyle = #35;
float(float v) rint = #36;
float(float v) floor = #37;
float(float v) ceil = #38;

float(entity e) checkbottom = #40;
float(vector v) pointcontents = #41;

float(float f) fabs = #43;

float(string s) cvar = #45;
void(string s, ...) localcmd = #46;
entity(entity e) nextent = #47;
void(vector o, vector d, float color, float count) particle = #48;
void() ChangeYaw = #49;

vector(vector v) vectoangles = #51;
vector(vector v, vector w) vectoangles2 = #51;

float(float f) sin = #60;
float(float f) cos = #61;
float(float f) sqrt = #62;
void(entity ent) changepitch = #63;
void(entity e, entity ignore) tracetoss = #64;
string(entity ent) etos = #65;

string(string s) precache_file = #68;
void(entity e) makestatic = #69;

void(string var, string val) cvar_set = #72;

void(vector pos, string samp, float vol, float atten) ambientsound = #74;
string(string s) precache_model2 = #75;
string(string s) precache_sound2 = #76;
string(string s) precache_file2 = #77;

float(string s) stof = #81;


void(vector v1, vector min, vector max, vector v2, float nomonsters, entity forent) tracebox = #90;
vector() randomvec = #91;
vector(vector org) getlight = #92;
vector(vector org, float lpflags) getlight2 = #92;
const float LP_LIGHTMAP	= 1;
const float LP_RTWORLD = 2;
const float LP_DYNLIGHT = 4;
const float LP_COMPLETE = 7;

float(string name, string value) registercvar = #93;
float( float a, ... ) min = #94;
float( float b, ... ) max = #95;
float(float minimum, float val, float maximum) bound = #96;
float(float f, float f) pow = #97;
entity(entity start, .float fld, float match) findfloat = #98;
float(string s) checkextension = #99;
// FrikaC and Telejano range #100-#199

float(string filename, float mode) fopen = #110;
void(float fhandle) fclose = #111;
string(float fhandle) fgets = #112;
void(float fhandle, string s) fputs = #113;
float(string s) strlen = #114;
string(...) strcat = #115;
string(string s, float start, float length) substring = #116;
vector(string) stov = #117;
string(string s) strzone = #118;
void(string s) strunzone = #119;

// FTEQW range #200-#299

float(float number, float quantity) bitshift = #218;

//float(string str, string sub[, float startpos]) strstrofs = #221;
float(string str, string sub, float startpos) strstrofs = #221;
float(string str, float ofs) str2chr = #222;
string(float c, ...) chr2str = #223;
string(float ccase, float calpha, float cnum, string s, ...) strconv = #224;
string(float chars, string s, ...) strpad = #225;
string(string info, string key, string value, ...) infoadd = #226;
string(string info, string key) infoget = #227;
float(string s1, string s2, float len) strncmp = #228;
float(string s1, string s2) strcasecmp = #229;
float(string s1, string s2, float len) strncasecmp = #230;

// CSQC range #300-#399
void() clearscene = #300;
void(float mask) addentities = #301;
void(entity ent) addentity = #302;
float(float property, ...) setproperty = #303;
void() renderscene = #304;
void(vector org, float radius, vector lightcolours) adddynamiclight = #305;
void(vector org, float radius, vector lightcolours, float style, string cubemapname, float pflags) adddynamiclight2 = #305;
//void(string texturename, float flag[, float is2d, float lines]) R_BeginPolygon = #306;
void(string texturename, float flag, float is2d, float lines) R_BeginPolygon = #306;
void(vector org, vector texcoords, vector rgb, float alpha) R_PolygonVertex = #307;
void() R_EndPolygon = #308;
vector (vector v) cs_unproject = #310;
vector (vector v) cs_project = #311;

void(float width, vector pos1, vector pos2, float flag) drawline = #315;
float(string name) iscachedpic = #316;
string(string name, float trywad) precache_pic = #317;
string(string name) precache_cubemap = #317;
vector(string picname) draw_getimagesize = #318;
void(string name) freepic = #319;
float(vector position, float character, vector scale, vector rgb, float alpha, float flag) drawcharacter = #320;
float(vector position, string text, vector scale, vector rgb, float alpha, float flag) drawstring = #321;
float(vector position, string pic, vector size, vector rgb, float alpha, float flag) drawpic = #322;
float(vector position, vector size, vector rgb, float alpha, float flag) drawfill = #323;
void(float x, float y, float width, float height) drawsetcliparea = #324;
void(void) drawresetcliparea = #325;
float(vector position, string text, vector scale, float alpha, float flag) drawcolorcodedstring = #326;
vector(vector position, string text, vector scale, vector rgb, float alpha, float flag) drawcolorcodedstring2 = #326;

float(float stnum) getstatf = #330;
float(float stnum) getstati = #331;
string(float firststnum) getstats = #332;
void(entity e, float mdlindex) setmodelindex = #333;
string(float mdlindex) modelnameforindex = #334;
float(string effectname) particleeffectnum = #335;
void(entity ent, float effectnum, vector start, vector end) trailparticles = #336;
//void(float effectnum, vector origin [, vector dir, float count]) pointparticles = #337;
void(float effectnum, vector origin , vector dir, float count) pointparticles = #337;
void(string s, ...) centerprint = #338;
void(string s, ...) print = #339;
string(float keynum) keynumtostring = #340;
float(string keyname) stringtokeynum = #341;
string(float keynum) getkeybind = #342;
void(float usecursor) setcursormode = #343;
vector() getmousepos = #344;
float(float framenum) getinputstate = #345;
void(float sens) setsensitivityscale = #346;
void() runstandardplayerphysics = #347;
string(float playernum, string keyname) getplayerkeyvalue = #348;
float() isdemo = #349;
float() isserver = #350;
void(vector origin, vector forward, vector right, vector up) SetListener = #351;
void(string cmdname) registercommand = #352;
float(entity ent) wasfreed = #353;
string(string key) serverkey = #354;

// Use proper case; refer to the id1 Write* functions!
float() ReadByte = #360;
float() ReadChar = #361;
float() ReadShort = #362;
float() ReadLong = #363;
float() ReadCoord = #364;
float() ReadAngle = #365;
string() ReadString = #366;
float() ReadFloat = #367;

// LordHavoc's range #400-#499
void(entity from, entity to) copyentity = #400;

entity(.string fld, string match) findchain = #402;
entity(.float fld, float match) findchainfloat = #403;
void(vector org, string modelname, float startframe, float endframe, float framerate) effect = #404;
void(vector org, vector velocity, float howmany) te_blood = #405;
void(vector mincorner, vector maxcorner, float explosionspeed, float howmany) te_bloodshower = #406;
void(vector org, vector color) te_explosionrgb = #407;
void(vector mincorner, vector maxcorner, vector vel, float howmany, float color, float gravityflag, float randomveljitter) te_particlecube = #408;
void(vector mincorner, vector maxcorner, vector vel, float howmany, float color) te_particlerain = #409;
void(vector mincorner, vector maxcorner, vector vel, float howmany, float color) te_particlesnow = #410;
void(vector org, vector vel, float howmany) te_spark = #411;
void(vector org) te_gunshotquad = #412;
void(vector org) te_spikequad = #413;
void(vector org) te_superspikequad = #414;
void(vector org) te_explosionquad = #415;
void(vector org) te_smallflash = #416;
void(vector org, float radius, float lifetime, vector color) te_customflash = #417;
void(vector org) te_gunshot = #418;
void(vector org) te_spike = #419;
void(vector org) te_superspike = #420;
void(vector org) te_explosion = #421;
void(vector org) te_tarexplosion = #422;
void(vector org) te_wizspike = #423;
void(vector org) te_knightspike = #424;
void(vector org) te_lavasplash = #425;
void(vector org) te_teleport = #426;
void(vector org, float colorstart, float colorlength) te_explosion2 = #427;
void(entity own, vector start, vector end) te_lightning1 = #428;
void(entity own, vector start, vector end) te_lightning2 = #429;
void(entity own, vector start, vector end) te_lightning3 = #430;
void(entity own, vector start, vector end) te_beam = #431;
void(vector dir) vectorvectors = #432;
void(vector org) te_plasmaburn = #433;
float(entity e, float s) getsurfacenumpoints = #434;
vector(entity e, float s, float n) getsurfacepoint = #435;
vector(entity e, float s) getsurfacenormal = #436;
string(entity e, float s) getsurfacetexture = #437;
float(entity e, vector p) getsurfacenearpoint = #438;
vector(entity e, float s, vector p) getsurfaceclippedpoint = #439;

float(string s) tokenize = #441;
string(float n) argv = #442;
void(entity e, entity tagentity, string tagname) setattachment = #443;
float(string pattern, float caseinsensitive, float quiet) search_begin = #444;
void(float handle) search_end = #445;
float(float handle) search_getsize = #446;
string(float handle, float num) search_getfilename = #447;
string(string s) cvar_string = #448;
entity(entity start, .float fld, float match) findflags = #449;
entity(.float fld, float match) findchainflags = #450;
float(entity ent, string tagname) gettagindex = #451;
vector(entity ent, float tagindex) gettaginfo = #452;

void(vector org, vector vel, float howmany) te_flamejet = #457;

entity(float num) entitybyindex = #459;
float() buf_create = #460;
void(float bufhandle) buf_del = #461;
float(float bufhandle) buf_getsize = #462;
void(float bufhandle_from, float bufhandle_to) buf_copy = #463;
void(float bufhandle, float sortpower, float backward) buf_sort = #464;
string(float bufhandle, string glue) buf_implode = #465;
string(float bufhandle, float string_index) bufstr_get = #466;
void(float bufhandle, float string_index, string str) bufstr_set = #467;
float(float bufhandle, string str, float order) bufstr_add = #468;
void(float bufhandle, float string_index) bufstr_free = #469;

float(float s) asin = #471;
float(float c) acos = #472;
float(float t) atan = #473;
float(float c, float s) atan2 = #474;
float(float a) tan = #475;
float(string s) strippedstringlen = #476;
float(string s) strlennocol = #476; // This is the correct name for the function, but not removing the decolorizedstring mapping.
string(string s) decolorizedstring = #477;
string(string s) strdecolorize = #477; // This is the correct name for the function, but not removing the decolorizedstring mapping.
string(float uselocaltime, string format, ...) strftime = #478;
float(string s) tokenizebyseparator = #479;
string(string s) strtolower = #480;
string(string s) strtoupper = #481;
string(string s) cvar_defstring = #482;
void(vector origin, string sample, float volume, float attenuation) pointsound = #483;
string(string search, string replace, string subject) strreplace = #484;
string(string search, string replace, string subject) strireplace = #485;
vector(entity e, float s, float n, float a) getsurfacepointattribute = #486;
#ifdef SUPPORT_GECKO
float gecko_create( string name ) = #487;
void gecko_destroy( string name ) = #488;
void gecko_navigate( string name, string URI ) = #489;
float gecko_keyevent( string name, float key, float eventtype ) = #490;
void gecko_mousemove( string name, float x, float y ) = #491;
void gecko_resize( string name, float w, float h ) = #492;
vector gecko_get_texture_extent( string name ) = #493;
#else

#endif

/*
==============================================================================

			EXTENSION DEFINITIONS

==============================================================================
*/

// DP_CSQC_SPAWNPARTICLE
// idea: VorteX
// darkplaces implementation: VorteX
// constant definitions:
// particle base behavior:
float PT_ALPHASTATIC = 1;
float PT_STATIC = 2;
float PT_SPARK = 3;
float PT_BEAM = 4;
float PT_RAIN = 5;
float PT_RAINDECAL = 6;
float PT_SNOW = 7;
float PT_BUBBLE = 8;
float PT_BLOOD = 9;
float PT_SMOKE = 10;
float PT_DECAL = 11;
float PT_ENTITYPARTICLE = 12;
// particle blendtypes:
float PBLEND_ALPHA = 0;
float PBLEND_ADD = 1;
float PBLEND_INVMOD = 2;
// particle orientation:
float PARTICLE_BILLBOARD = 0;
float PARTICLE_SPARK = 1;
float PARTICLE_ORIENTED_DOUBLESIDED = 2;
float PARTICLE_BEAM = 3;
// global definitions:
float particle_type; // one of PT_
float particle_blendmode; // one of PBLEND_ values
float particle_orientation; // one of PARTICLE_ values
vector particle_color1; 
vector particle_color2;
float particle_tex; // number of chunk in particlefont
float particle_size;
float particle_sizeincrease; 
float particle_alpha;
float particle_alphafade;
float particle_time;
float particle_gravity;
float particle_bounce;
float particle_airfriction;
float particle_liquidfriction;
float particle_originjitter;
float particle_velocityjitter;
float particle_qualityreduction; // enable culling of this particle when FPS is low
float particle_stretch;
vector particle_staincolor1;
vector particle_staincolor2;
float particle_staintex;
float particle_stainalpha;
float particle_stainsize;
float particle_delayspawn;
float particle_delaycollision;
float particle_angle;
float particle_spin;
// builtin definitions:
float(float max_themes) initparticlespawner = #522; // check fields/globals for integration and enable particle spawner, return 1 is succeded, otherwise returns 0
void() resetparticle = #523; // reset p_ globals to default theme #0
void(float theme) particletheme = #524; // restore p_ globals from saved theme
float() particlethemesave = #525; // save p_ globals to new particletheme and return it's index
void(float theme) particlethemeupdate = #525; // save p_ globals to new particletheme and return it's index
void(float theme) particlethemefree = #526; // delete a particle theme
float(vector org, vector vel) spawnparticle = #527; // returns 0 when failed, 1 when spawned
float(vector org, vector vel, float theme) quickparticle = #527; // not reading globals, just theme, returns 0 when failed, 1 when spawned
float(vector org, vector vel, float delay, float collisiondelay) delayedparticle = #528;
float(vector org, vector vel, float delay, float collisiondelay, float theme) quickdelayedparticle = #528;
// description: this builtin provides an easy and flexible way to spawn particles, 
// it is not created as replace for DP_SV_POINTPARTICLES but as an addition to it. 
// With this extension you can create a specific particles like rain particles, or entity particles
// notes: 
// 1) 0 is default particle template, it could be changed
// 2) color vectors could have value 0-255 of each component
// restrictions: max themes could be between 4 and 2048
// warning: you should call initparticlespawner() at very beginning BEFORE all other particle spawner functions
// function to query particle info
// don't remove this function as it protects all particle_ globals from FTEQCC/FRIKQCC non-referenced removal optimisation
void() printparticle =
{
	// vortex: this also protects from 'non-referenced' optimisation on some compilers
	print("PARTICLE:\n");
	print(strcat("  type: ", ftos(particle_type), "\n"));
	print(strcat("  blendmode: ", ftos(particle_blendmode), "\n"));
	print(strcat("  orientation: ", ftos(particle_orientation), "\n"));
	print(strcat("  color1: ", vtos(particle_color1), "\n"));
	print(strcat("  color2: ", vtos(particle_color2), "\n"));
	print(strcat("  tex: ", ftos(particle_tex), "\n"));
	print(strcat("  size: ", ftos(particle_size), "\n"));
	print(strcat("  sizeincrease: ", ftos(particle_sizeincrease), "\n"));
	print(strcat("  alpha: ", ftos(particle_alpha), "\n"));
	print(strcat("  alphafade: ", ftos(particle_alphafade), "\n"));
	print(strcat("  time: ", ftos(particle_time), "\n"));
	print(strcat("  gravity: ", ftos(particle_gravity), "\n"));
	print(strcat("  bounce: ", ftos(particle_bounce), "\n"));
	print(strcat("  airfriction: ", ftos(particle_airfriction), "\n"));
	print(strcat("  liquidfriction: ", ftos(particle_liquidfriction), "\n"));
	print(strcat("  originjitter: ", ftos(particle_originjitter), "\n"));
	print(strcat("  velocityjitter: ", ftos(particle_velocityjitter), "\n"));
	print(strcat("  qualityreduction: ", ftos(particle_qualityreduction), "\n"));
	print(strcat("  stretch: ", ftos(particle_stretch), "\n"));
	print(strcat("  staincolor1: ", vtos(particle_staincolor1), "\n"));
	print(strcat("  staincolor2: ", vtos(particle_staincolor2), "\n"));
	print(strcat("  staintex: ", ftos(particle_staintex), "\n"));
	print(strcat("  stainalpha: ", ftos(particle_stainalpha), "\n"));
	print(strcat("  stainsize: ", ftos(particle_stainsize), "\n"));
	print(strcat("  delayspawn: ", ftos(particle_delayspawn), "\n"));
	print(strcat("  delaycollision: ", ftos(particle_delaycollision), "\n"));
	print(strcat("  angle: ", ftos(particle_angle), "\n"));
	print(strcat("  spin: ", ftos(particle_spin), "\n"));
}

// DP_CSQC_ENTITYTRANSPARENTSORTING_OFFSET
// idea: VorteX
// darkplaces implementation: VorteX
float RF_USETRANSPARENTOFFSET = 64; // enables transparent origin offsetting
// global definitions
float transparent_offset; // should be set before entity is added
// description: offset a model's meshes origin used for transparent sorting. Could be used to tweak sorting bugs on very large transparent entities or hacking transparent sorting order for certain objects
// example: transparent_offset = 1000000; // entity always appear on background of other transparents
// note: offset is done in view forward axis

// DP_CSQC_ENTITYNOCULL
// idea: VorteX
// darkplaces implementation: VorteX
const float RF_NOCULL = 128;
// description: when renderflag is set, engine will not use culling methods for this entity, e.g. it will always be drawn
// useful for large outdoor objects (like asteriods on sky horizont or sky models)
// also useful when culling is done at CSQC side

// DP_CSQC_SETPAUSE
// idea: VorteX
// darkplaces implementation: VorteX
// builtin definitions:
void(float ispaused) setpause = #531;
// description: provides ability to set pause in local games (similar to one set once console is activated)
// not stopping sound/cd track, useful for inventory screens, ingame menus with input etc.

// DP_CSQC_QUERYRENDERENTITY
// idea: VorteX
// darkplaces implementation: VorteX
// constant definitions:
// render entity fields:
float E_ACTIVE 	    = 0; // float 0/1
float E_ORIGIN      = 1; // vector
float E_FORWARD     = 2; // vector
float E_RIGHT       = 3; // vector
float E_UP          = 4; // vector
float E_SCALE       = 5; // float
float E_ORIGINANDVECTORS = 6; // returns origin, + sets v_* vectors to orientation
float E_ALPHA       = 7; // float
float E_COLORMOD    = 8; // vector
float E_PANTSCOLOR  = 9; // vector
float E_SHIRTCOLOR  = 10; // vector
float E_SKIN        = 11; // float
float E_MINS        = 12; // vector
float E_MAXS        = 13; // vector
float E_ABSMIN      = 14; // vector
float E_ABSMAX      = 15; // vector
float E_LIGHT       = 16; // vector - modellight
// builtin definitions:
float(float entitynum, float fldnum) getentity = #504;
vector(float entitynum, float fldnum) getentityvec = #504;
// description: allows to query parms from render entities, especially useful with attaching CSQC ents to
// server entities networked and interpolated by engine (monsters, players), number of entity is it's SVQC number
// you can send it via tempentity/CSQC entity message. Note that this builtin doesnt know about entity removing/reallocating
// so it's meaning to work for short period of time, dont use it on missiles/grenades whatever will be removed next five seconds

//DP_GFX_FONTS
//idea: Blub\0, divVerent
//darkplaces implementation: Blub\0
//console commands:
//  loadfont fontname fontmaps size1 size2 ...
//   A font can simply be gfx/tgafile (freetype fonts doent need extension), 
//   or alternatively you can specify multiple fonts and faces
//   Like this: gfx/vera-sans:2,gfx/fallback:1
//   to load face 2 of the font gfx/vera-sans and use face 1
//   of gfx/fallback as fallback font
//   You can also specify a list of font sizes to load, like this:
//   loadfont console gfx/conchars,gfx/fallback 8 12 16 24 32
//   In many cases, 8 12 16 24 32 should be a good choice.
//   for slots see:
//constant definitions:
float drawfont;            // set it before drawstring()/drawchar() calls
float FONT_DEFAULT = 0;    // 'default'
float FONT_CONSOLE = 1;    // 'console', REALLY should be fixed width (ls!)
float FONT_SBAR = 2;       // 'sbar', used on hud, must be fixed width
float FONT_NOTIFY = 3;     // 'notify', used on sprint/bprint
float FONT_CHAT = 4;       // 'chat'
float FONT_CENTERPRINT = 5;// 'centerprint'
float FONT_INFOBAR = 6;    // 'infobar'
float FONT_MENU = 7;       // 'menu', should be fixed width
float FONT_USER0 = 8;      // 'user0', userdefined fonts
float FONT_USER1 = 9;      // 'user1', userdefined fonts
float FONT_USER2 = 10;     // 'user2', userdefined fonts
float FONT_USER3 = 11;     // 'user3', userdefined fonts
float FONT_USER4 = 12;     // 'user4', userdefined fonts
float FONT_USER5 = 13;     // 'user5', userdefined fonts
float FONT_USER6 = 14;     // 'user6', userdefined fonts
float FONT_USER7 = 15;     // 'user7' slot, userdefined fonts
//builtin definitions:
float findfont(string s) = #356; // find font by fontname and return it's index
float loadfont(string fontname, string fontmaps, string sizes, float slot, float fix_scale, float fix_voffset) = #357; 
// loads font immediately so stringwidth() function can be used just after builtin call
// returns a font slotnum (which is used to set drawfont to)
// first 3 parms are identical to "loadfont" console command ones
// slot could be one of FONT_ constants or result of findfont() or -1 to not use it
// if slot is given, font will be loaded to this slotnum and fontname become new title for it
// this way you can rename user* fonts to something more usable
// fix_* parms let you fix badly made fonts by applying some transformations to them
// fix_scale : per-character center-oriented scale (doesn't change line height at all)
// fix_voffset : vertical offset for each character, it's a multiplier to character height
float stringwidth(string text, float allowColorCodes, vector size) = #327; // get a width of string with given font and char size
float stringwidth_menu(string text, float allowColorCodes, vector size) = #468; // in menu.dat it has different builtin #
//description: engine support for custom fonts in console, hud, qc etc.
// limits:
//  max 128 chars for font name
//  max 3 font fallbacks
//  max 8 sizes per font

//DP_GFX_FONTS_FREETYPE
//idea: Blub\0, divVerent
//darkplaces implementation: Blub\0
//cvar definitions:
//   r_font_disable_freetype 0/1 : disable freetype fonts loading (uttetly disables freetype library initialization)
//   r_font_antialias 0/1 : antialiasing when loading font
//   r_font_hint 0/1/2/3  : hinting when loading font, 0 is no hinting, 1 light autohinting , 2 full autohinting, 3 full hinting
//   r_font_postprocess_blur X      : font outline blur amount
//   r_font_postprocess_outline X   : font outline width
//   r_font_postprocess_shadow_x X  : font outline shadow x shift amount, applied during outlining
//   r_font_postprocess_shadow_y X  : font outline shadow y shift amount, applied during outlining
//   r_font_postprocess_shadow_z X  : font outline shadow z shift amount, applied during blurring
//description: engine support for truetype/freetype fonts
//so .AFM+.PFB/.OTF/.TTF files could be stuffed as fontmaps in loadfont() 
//(console command version will support them as well)

//DP_CSQC_BINDMAPS
//idea: daemon, motorsep
//darkplaces implementation: divVerent
//builtin definitions:
string(float key, float bindmap) getkeybind_bindmap = #342;
float(float key, string bind, float bindmap) setkeybind_bindmap = #630;
vector(void) getbindmaps = #631;
float(vector bm) setbindmaps = #632;
string(string command, float bindmap) findkeysforcommand = #610;
//<already in EXT_CSQC> float(string key) stringtokeynum = #341;
//<already in EXT_CSQC> string(float keynum) keynumtostring = #340;
//description: key bind setting/getting including support for switchable
//bindmaps.

//DP_CRYPTO
//idea: divVerent
//darkplaces implementation: divVerent
//builtin definitions: (CSQC)
float(string url, float id, string content_type, string delim, float buf, float keyid) crypto_uri_postbuf = #513;
//description:
//use -1 as buffer handle to justs end delim as postdata
