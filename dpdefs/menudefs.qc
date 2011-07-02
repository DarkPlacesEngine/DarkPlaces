//////////////////////////////////////////////////////////
// sys globals

entity self;

/////////////////////////////////////////////////////////
void		end_sys_globals;
/////////////////////////////////////////////////////////
// sys fields

/////////////////////////////////////////////////////////
void		end_sys_fields;
/////////////////////////////////////////////////////////
// sys functions

void() m_init;
void(float keynr, float ascii) m_keydown;
void() m_draw;
void() m_toggle;
void() m_shutdown;

/////////////////////////////////////////////////////////
// sys constants
///////////////////////////
// key constants

//
// these are the key numbers that should be passed to Key_Event
//
float K_TAB			=	9;
float K_ENTER		=	13;
float K_ESCAPE		=	27;
float K_SPACE		=	32;

// normal keys should be passed as lowercased ascii

float K_BACKSPACE	=	127;
float K_UPARROW		=	128;
float K_DOWNARROW	=	129;
float K_LEFTARROW	=	130;
float K_RIGHTARROW	=	131;

float K_ALT		=	132;
float K_CTRL	=	133;
float K_SHIFT	=	134;
float K_F1		=	135;
float K_F2		=	136;
float K_F3		=	137;
float K_F4		=	138;
float K_F5		=	139;
float K_F6		=	140;
float K_F7		=	141;
float K_F8		=	142;
float K_F9		=	143;
float K_F10		=	144;
float K_F11		=	145;
float K_F12		=	146;
float K_INS		=	147;
float K_DEL		=	148;
float K_PGDN	=	149;
float K_PGUP	=	150;
float K_HOME	=	151;
float K_END		=	152;

float K_PAUSE	=	153;

float K_NUMLOCK		= 154;
float K_CAPSLOCK	= 155;
float K_SCROLLLOCK	= 156;

float K_KP_0	=	157;
float K_KP_INS	=	K_KP_0;
float K_KP_1	=	158;
float K_KP_END	=	K_KP_1;
float K_KP_2	=	159;
float K_KP_DOWNARROW = K_KP_2;
float K_KP_3	=	160;
float K_KP_PGDN = K_KP_3;
float K_KP_4	=	161
float K_KP_LEFTARROW = K_KP_4;
float K_KP_5	=	162;
float K_KP_6	=	163;
float K_KP_RIGHTARROW = K_KP_6;
float K_KP_7	=	164;
float K_KP_HOME = K_KP_7;
float K_KP_8	=	165;
float K_KP_UPARROW = K_KP_8;
float K_KP_9	= 166;
float K_KP_PGUP = K_KP_9;
float K_KP_PERIOD = 167;
float K_KP_DEL = K_KP_PERIOD;
float K_KP_DIVIDE = 168;
float K_KP_SLASH = K_KP_DIVIDE;
float K_KP_MULTIPLY = 169;
float K_KP_MINUS	= 170;
float K_KP_PLUS		= 171;
float K_KP_ENTER	= 172;
float K_KP_EQUALS	= 173;

// mouse buttons generate virtual keys

float K_MOUSE1	=	512;
float K_MOUSE2	=	513;
float K_MOUSE3	=	514;
float K_MOUSE4	=	515;
float K_MWHEELUP = K_MOUSE4;
float K_MOUSE5	=	516;
float K_MWHEELDOWN = K_MOUSE5;
float K_MOUSE6	=	517;
float K_MOUSE7	=	518;
float K_MOUSE8	=	519;
float K_MOUSE9	=	520;
float K_MOUSE10	=	521;
float K_MOUSE11	=	522;
float K_MOUSE12	=	523;
float K_MOUSE13	=	524;
float K_MOUSE14	=	525;
float K_MOUSE15	=	526;
float K_MOUSE16	=	527;

//
// joystick buttons
//
float K_JOY1 = 768;
float K_JOY2 = 769;
float K_JOY3 = 770;
float K_JOY4 = 771;

//
//
// aux keys are for multi-buttoned joysticks to generate so they can use
// the normal binding process
//
float K_AUX1	=	772;
float K_AUX2	=	773;
float K_AUX3	=	774;
float K_AUX4	=	775;
float K_AUX5	=	776;
float K_AUX6	=	777;
float K_AUX7	=	778;
float K_AUX8	=	779;
float K_AUX9	=	780;
float K_AUX10	=	781;
float K_AUX11	=	782;
float K_AUX12	=	783;
float K_AUX13	=	784;
float K_AUX14	=	785;
float K_AUX15	=	786;
float K_AUX16	=	787;
float K_AUX17	=	788;
float K_AUX18	=	789;
float K_AUX19	=	790;
float K_AUX20	=	791;
float K_AUX21	=	792;
float K_AUX22	=	793;
float K_AUX23	=	794;
float K_AUX24	=	795;
float K_AUX25	=	796;
float K_AUX26	=	797;
float K_AUX27	=	798;
float K_AUX28	=	799;
float K_AUX29	=	800;
float K_AUX30	=	801;
float K_AUX31	=	802;
float K_AUX32	=	803;

///////////////////////////
// key dest constants

float KEY_GAME 		=	0;
float KEY_MENU		=	2;
float KEY_UNKNOWN	= 	3;

///////////////////////////
// file constants

float FILE_READ = 0;
float FILE_APPEND = 1;
float FILE_WRITE = 2;

///////////////////////////
// logical constants (just for completeness)

float TRUE 	= 1;
float FALSE = 0;

///////////////////////////
// boolean constants

float true = 1;
float false = 0;

///////////////////////////
// msg constants

float MSG_BROADCAST		= 0;		// unreliable to all
float MSG_ONE			= 1;		// reliable to one (msg_entity)
float MSG_ALL			= 2;		// reliable to all
float MSG_INIT			= 3;		// write to the init string

/////////////////////////////
// mouse target constants

float MT_MENU = 1;
float MT_CLIENT = 2;

/////////////////////////
// client state constants

float CS_DEDICATED 		= 0;
float CS_DISCONNECTED 	= 1;
float CS_CONNECTED		= 2;

///////////////////////////
// blend flags

float DRAWFLAG_NORMAL		= 0;
float DRAWFLAG_ADDITIVE 	= 1;
float DRAWFLAG_MODULATE 	= 2;
float DRAWFLAG_2XMODULATE   = 3;

///////////////////////////
// null entity (actually it is the same like the world entity)

entity null_entity;

///////////////////////////
// error constants

// file handling
float ERR_CANNOTOPEN			= -1; // fopen
float ERR_NOTENOUGHFILEHANDLES 	= -2; // fopen
float ERR_INVALIDMODE 			= -3; // fopen
float ERR_BADFILENAME 			= -4; // fopen

// drawing functions

float ERR_NULLSTRING			= -1;
float ERR_BADDRAWFLAG			= -2;
float ERR_BADSCALE				= -3;
float ERR_BADSIZE				= ERR_BADSCALE;
float ERR_NOTCACHED				= -4;

/* not supported at the moment
///////////////////////////
// os constants

float OS_WINDOWS	= 0;
float OS_LINUX		= 1;
float OS_MAC		= 2;
*/










//////////////////////////////////////////////////
// common cmd
//////////////////////////////////////////////////
// AK FIXME: Create perhaps a special builtin file for the common cmds

void 	checkextension(string ext) 	= #1;

// error cmds
void 	error(string err,...) 		= #2;
void 	objerror(string err,...) 	= #3;

// print

void 	print(string text,...) 		 	 = #4;
void 	bprint(string text,...)			 = #5;
void	sprint(float clientnum, string text,...) = #6;
void 	centerprint(string text,...)		 = #7;

// vector stuff

vector	normalize(vector v) 		= #8;
float 	vlen(vector v)			= #9;
float  	vectoyaw(vector v)		= #10;
vector 	vectoangles(vector v)		= #11;

float	random(void)  = #12;

void	cmd(string command) = #13;

// cvar cmds

float 	cvar(string name) 			= #14;
const string str_cvar(string name)		= #71;
void	cvar_set(string name, string value)	= #15;

void	dprint(string text,...) = #16;

// conversion functions

string	ftos(float f)  	= #17;
float	fabs(float f)	= #18;
string	vtos(vector v)  = #19;
string	etos(entity e)	= #20;

float	stof(string val,...)  = #21;

entity	spawn(void) 		= #22;
void	remove(entity e) 	= #23;

entity	findstring(entity start, .string field, string match)  	= #24;
entity	findfloat(entity start, .float field, float match) 	= #25;
entity	findentity(entity start, .entity field, entity match) 	= #25;

entity	findchainstring(.string field, string match) 	= #26;
entity	findchainfloat(.float field, float match) 	= #27;
entity	findchainentity(.entity field, entity match)	= #27;

string	precache_file(string file) 	= #28;
string	precache_sound(string sample) 	= #29;

void	crash(void)	= #72;
void	coredump(void) 	= #30;
void	stackdump(void) = #73;
void	traceon(void) 	= #31;
void	traceoff(void) 	= #32;

void	eprint(entity e)  	= #33;
float	rint(float f) 		= #34;
float	floor(float f)  	= #35;
float	ceil(float f)  		= #36;
entity	nextent(entity e)  	= #37;
float	sin(float f)  		= #38;
float	cos(float f)  		= #39;
float	sqrt(float f)  		= #40;
vector	randomvec(void)  	= #41;

float	registercvar(string name, string value, float flags)  = #42; // returns 1 if success

float	min(float f,...)  = #43;
float	max(float f,...)  = #44;

float	bound(float min,float value, float max)  = #45;
float	pow(float a, float b)  = #46;

void	copyentity(entity src, entity dst)  = #47;

float	fopen(string filename, float mode)  = #48;
void	fclose(float fhandle)  = #49;
string	fgets(float fhandle)  = #50;
void	fputs(float fhandle, string s)  = #51;

float	strlen(string s)  = #52;
string	strcat(string s1,string s2,...)  = #53;
string	substring(string s, float start, float length)  = #54;

vector	stov(string s)  = #55;

string	strzone(string s)  = #56;
void	strunzone(string s) = #57;

float	tokenize(string s)  = #58
string	argv(float n)  = #59;

float	isserver(void)  = #60;
float	clientcount(void)  = #61;
float	clientstate(void)  = #62;
void	clientcommand(float client, string s)  = #63;
void	changelevel(string map)  = #64;
void	localsound(string sample)  = #65;
vector	getmousepos(void)  	= #66;
float	gettime(void)		= #67;
void 	loadfromdata(string data) = #68;
void	loadfromfile(string file) = #69;

float	mod(float val, float m) = #70;

float	search_begin(string pattern, float caseinsensitive, float quiet) = #74;
void	search_end(float handle) = #75;
float	search_getsize(float handle) = #76;
string	search_getfilename(float handle, float num) = #77;

string	chr(float ascii) = #78;

/////////////////////////////////////////////////
// Write* Functions
/////////////////////////////////////////////////
void	WriteByte(float data, float dest, float desto)	= #401;
void	WriteChar(float data, float dest, float desto)	= #402;
void	WriteShort(float data, float dest, float desto)	= #403;
void	WriteLong(float data, float dest, float desto)	= #404;
void	WriteAngle(float data, float dest, float desto)	= #405;
void	WriteCoord(float data, float dest, float desto)	= #406;
void	WriteString(string data, float dest, float desto)= #407;
void	WriteEntity(entity data, float dest, float desto) = #408;

//////////////////////////////////////////////////
// Draw funtions
//////////////////////////////////////////////////

float	iscachedpic(string name)	= #451;
string	precache_pic(string name)	= #452;
void	freepic(string name)		= #453;

float	drawcharacter(vector position, float character, vector scale, vector rgb, float alpha, float flag) = #454;

float	drawstring(vector position, string text, vector scale, vector rgb, float alpha, float flag) = #455;

float	drawcolorcodedstring(vector position, string text, vector scale, float alpha, float flag) = #467;

vector	drawcolorcodedstring2(vector position, string text, vector scale, vector rgb, float alpha, float flag) = #467;
 
float	drawpic(vector position, string pic, vector size, vector rgb, float alpha, float flag) = #456;

float	drawfill(vector position, vector size, vector rgb, float alpha, float flag) = #457;

void	drawsetcliparea(float x, float y, float width, float height) = #458;

void	drawresetcliparea(void) = #459;

vector  drawgetimagesize(string pic) = #460;

////////////////////////////////////////////////
// Menu functions
////////////////////////////////////////////////

void	setkeydest(float dest) 	= #601;
float	getkeydest(void)	= #602;

void	setmousetarget(float trg) = #603;
float	getmousetarget(void)	  = #604;

float	isfunction(string function_name) = #607;
void	callfunction(...) = #605;
void	writetofile(float fhandle, entity ent) = #606;
vector	getresolution(float number) = #608;
string	keynumtostring(float keynum) = #609;

float	gethostcachevalue(float type) = #611;
string	gethostcachestring(float type, float hostnr) = #612;

//DP_CSQC_BINDMAPS
//idea: daemon, motorsep
//darkplaces implementation: divVerent
//builtin definitions:
string(float key, float bindmap) getkeybind_bindmap = #342;
float(float key, string bind, float bindmap) setkeybind_bindmap = #630;
vector(void) getbindmaps = #631;
float(vector bm) setbindmaps = #632;
string(string command, float bindmap) findkeysforcommand = #610;
float(string key) stringtokeynum = #341;
//<also allowed builtin number to match EXT_CSQC> string(float keynum) keynumtostring = #340;
//description: key bind setting/getting including support for switchable
//bindmaps.

//DP_CRYPTO
//idea: divVerent
//darkplaces implementation: divVerent
//field definitions: (MENUQC)
string(string serveraddress) crypto_getkeyfp = #633; // retrieves the cached host key's CA fingerprint of a server given by IP address
string(string serveraddress) crypto_getidfp = #634; // retrieves the cached host key fingerprint of a server given by IP address
string(string serveraddress) crypto_getencryptlevel = #635; // 0 if never encrypting, 1 supported, 2 requested, 3 required, appended by list of allowed methods in order of preference ("AES128"), preceded by a space each
string(float i) crypto_getmykeyfp = #636; // retrieves the CA key fingerprint of a given CA slot, or "" if slot is unused but more to come, or string_null if end of list
string(float i) crypto_getmyidfp = #637; // retrieves the ID fingerprint of a given CA slot, or "" if slot is unused but more to come, or string_null if end of list
float(string url, float id, string content_type, string delim, float buf, float keyid) crypto_uri_postbuf = #513;
//description:
//use -1 as buffer handle to justs end delim as postdata

//DP_GECKO_SUPPORT
//idea: Res2k, BlackHC
//darkplaces implementation: Res2k, BlackHC
//constant definitions:
float GECKO_BUTTON_DOWN         = 0;
float GECKO_BUTTON_UP           = 1;
// either use down and up or just press but not all of them!
float GECKO_BUTTON_PRESS        = 2;
// use this for mouse events if needed?
float GECKO_BUTTON_DOUBLECLICK  = 3;
//builtin definitions:
float gecko_create( string name ) = #487;
void gecko_destroy( string name ) = #488;
void gecko_navigate( string name, string URI ) = #489;
float gecko_keyevent( string name, float key, float eventtype ) = #490;
void gecko_mousemove( string name, float x, float y ) = #491;
void gecko_resize( string name, float w, float h ) = #492;
vector gecko_get_texture_extent( string name ) = #493;
//engine-called QC prototypes:
//string(string name, string query) Qecko_Query;
//description:
//provides an interface to the offscreengecko library and allows for internet browsing in games

