
void R_Modules_Init();
void R_RegisterModule(char *name, void(*start)(), void(*shutdown)());
void R_StartModules ();
void R_ShutdownModules ();
void R_Restart ();
