
void R_Modules_Init();
void R_RegisterModule(char *name, void(*start)(), void(*shutdown)(), void(*newmap)());
void R_Modules_Start();
void R_Modules_Shutdown();
void R_Modules_NewMap();
void R_Modules_Restart();
