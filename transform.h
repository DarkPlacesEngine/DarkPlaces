// LordHavoc: software transform support, intended for transpoly and wallpoly systems

#define tft_translate 1
#define tft_rotate 2

extern vec3_t softwaretransform_offset;
extern vec3_t softwaretransform_x;
extern vec3_t softwaretransform_y;
extern vec3_t softwaretransform_z;
extern int softwaretransform_type;

extern void softwaretransformforentity (entity_t *e);
extern void softwaretransformidentity ();
extern void softwaretransformset (vec3_t origin, vec3_t angles, vec_t scale);
extern void (*softwaretransform) (vec3_t in, vec3_t out);
