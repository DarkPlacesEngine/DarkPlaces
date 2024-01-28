/*
 * Include this BEFORE darkplaces.h because it breaks wrapping
 * _Static_assert. Cloudwalk has no idea how or why so don't ask.
 */
#include <SDL.h>

#include "darkplaces.h"

#include <emscripten.h>
#include <emscripten/html5.h>
#include <string.h>



// =======================================================================
// General routines
// =======================================================================

EM_JS(char*,getclipboard,(void),{
	//Thank you again, stack overflow
	return stringToNewUTF8(navigator.clipboard.readText());
})

EM_JS(bool,syncFS,(bool x),{
	FS.syncfs(x,function(e){
		if(e){
			alert("FileSystem Save Error: "+e);
			return false;
		} else{
			console.log("Filesystem Saved!");
			return true;
		}
	})});

EM_JS(char*,rm,(char* x),{
	const mode = FS.lookupPath(UTF8ToString(x)).node.mode;
	if(FS.isFile(mode)){
		FS.unlink(UTF8ToString(x));
		return stringToNewUTF8("File removed"); 
	}
	else {
		return stringToNewUTF8(UTF8ToString(x)+" is not a File.");
	}
	});

EM_JS(char*,rmdir,(char* x),{
	const mode = FS.lookupPath(UTF8ToString(x)).node.mode;
	if(FS.isDir(mode)){
		try{FS.rmdir(UTF8ToString(x));} catch (error) {return stringToNewUTF8("Unable to remove directory. Is it not empty?");}

		return stringToNewUTF8("Directory removed"); 
	}
	else {
		return stringToNewUTF8(UTF8ToString(x)+" is not a directory.");
	}
	});

EM_JS(char*,mkd,(char* x),{

	try{FS.mkdir(UTF8ToString(x));} catch (error) {return stringToNewUTF8("Unable to create directory. Does it already exist?");}
	return stringToNewUTF8(UTF8ToString(x)+" directory was created.");
});

EM_JS(char*,move,(char* x,char* y),{
	try{FS.rename(UTF8ToString(x),UTF8ToString(y))}catch(error){return stringToNewUTF8("unable to move.")}
	return stringToNewUTF8("File Moved")
});

EM_JS(char*,upload,(char* todirectory),{
	if(UTF8ToString(todirectory).slice(-1) != "/"){
		currentname = UTF8ToString(todirectory) + "/";
	}
	else{
		currentname = UTF8ToString(todirectory);
	}

	file_selector.click();
	return stringToNewUTF8("Upload started");

});

EM_JS(char*, listfiles,(char* directory),{ if(UTF8ToString(directory) == ""){
	console.log("listing cwd"); 
	return stringToNewUTF8(FS.readdir(FS.cwd()).toString())
}  
try{
return  stringToNewUTF8(FS.readdir(UTF8ToString(directory)).toString()); 
} catch(error){
	return stringToNewUTF8("directory not found");
}
});

void listfiles_f(cmd_state_t *cmd){
	if(Cmd_Argc(cmd) != 2){

		Con_Printf(listfiles(""));
		Con_Printf("\n");
	}
	else{
		Con_Printf(listfiles(Cmd_Argv(cmd,1)) );
		Con_Printf("\n");
	}
}
void savefs_f(cmd_state_t *cmd){
	Con_Printf("Saving Files\n");
	syncFS(false);
}

void upload_f(cmd_state_t *cmd){
	if(Cmd_Argc(cmd) != 2){
		Con_Printf(upload("/save"));
		Con_Printf("\n");
	}
	else{
		Con_Printf(upload(Cmd_Argv(cmd,1)));
		Con_Printf("\n");
	}
}

void rm_f(cmd_state_t *cmd){
	if(Cmd_Argc(cmd) != 2){
		Con_Printf("No file to remove");
		Con_Printf("\n");
	}
	else{
		Con_Printf(rm(Cmd_Argv(cmd,1)));
		Con_Printf("\n");
	}
}

void rmdir_f(cmd_state_t *cmd){
	if(Cmd_Argc(cmd) != 2){
		Con_Printf("No directory to remove");
		Con_Printf("\n");
	}
	else{
		Con_Printf(rmdir(Cmd_Argv(cmd,1)));
		Con_Printf("\n");
	}
}

void mkdir_f(cmd_state_t *cmd){
	if(Cmd_Argc(cmd) != 2){
		Con_Printf("No directory to create");
		Con_Printf("\n");
	}
	else{
		Con_Printf(mkd(Cmd_Argv(cmd,1)));
		Con_Printf("\n");
	}
}

void mv_f(cmd_state_t *cmd){
	if(Cmd_Argc(cmd) != 3){
		Con_Printf("Nothing to move");
		Con_Printf("\n");
	}
	else{
		Con_Printf(move(Cmd_Argv(cmd,1),Cmd_Argv(cmd,2)));
		Con_Printf("\n");
	}
}

void wss_f(cmd_state_t *cmd){
	if(Cmd_Argc(cmd) != 3){
		Con_Printf("Not Enough Arguments (Expected URL and subprotocol)");
		Con_Printf("\n");
	}
	else{

		if(strcmp(Cmd_Argv(cmd,2),"binary") == 0 || strcmp(Cmd_Argv(cmd,2),"text") == 0){

			Con_Printf("Set Websocket URL to %s and subprotocol to %s.", Cmd_Argv(cmd,1), Cmd_Argv(cmd,2));
		} else{
			Con_Printf("subprotocol must be either binary or text");
		}
		Con_Printf("\n");
	}
}
void Sys_SDL_Shutdown(void)
{
	syncFS(false);
	SDL_Quit();
}

// Sys_Abort early in startup might screw with automated
// workflows or something if we show the dialog by default.
static qbool nocrashdialog = true;
void Sys_SDL_Dialog(const char *title, const char *string)
{
	if(!nocrashdialog)
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, title, string, NULL);
}

char *Sys_SDL_GetClipboardData (void)
{
	char *data = NULL;
	char *cliptext;

	cliptext = getclipboard();
	if (cliptext != NULL) {
		size_t allocsize;
		allocsize = min(MAX_INPUTLINE, strlen(cliptext) + 1);
		data = (char *)Z_Malloc (allocsize);
		dp_strlcpy (data, cliptext, allocsize);
		SDL_free(cliptext);
	}

	return data;
}

void Sys_SDL_Init(void)
{
	// we don't know which systems we'll want to init, yet...
	if (SDL_Init(0) < 0)
		Sys_Abort("SDL_Init failed: %s\n", SDL_GetError());

	// COMMANDLINEOPTION: sdl: -nocrashdialog disables "Engine Error" crash dialog boxes
	if(!Sys_CheckParm("-nocrashdialog"))
		nocrashdialog = false;
}

void Sys_Register_Commands(void){
	Cmd_AddCommand(CF_SHARED, "em_ls", listfiles_f, "Lists Files in specified directory defaulting to the current working directory (Emscripten Only)");
	Cmd_AddCommand(CF_SHARED, "em_upload", upload_f, "Upload file to specified directory defaulting to /save (Emscripten Only)");
	Cmd_AddCommand(CF_SHARED, "em_save", savefs_f, "Save file changes to browser (Emscripten Only)");
	Cmd_AddCommand(CF_SHARED, "em_rm", rm_f, "Remove a file from game Filesystem (Emscripten Only)");
	Cmd_AddCommand(CF_SHARED, "em_rmdir", rmdir_f, "Remove a directory from game Filesystem (Emscripten Only)");
	Cmd_AddCommand(CF_SHARED, "em_mkdir", mkdir_f, "Make a directory in game Filesystem (Emscripten Only)");
	Cmd_AddCommand(CF_SHARED, "em_mv", mv_f, "Rename or Move an item in game Filesystem (Emscripten only)");
	Cmd_AddCommand(CF_SHARED, "em_wss", wss_f, "Set Websocket URL and Protocol");
}

qbool sys_supportsdlgetticks = true;
unsigned int Sys_SDL_GetTicks (void)
{
	return SDL_GetTicks();
}
void Sys_SDL_Delay (unsigned int milliseconds)
{
	SDL_Delay(milliseconds);
}
