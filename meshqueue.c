
#include "quakedef.h"
#include "meshqueue.h"

typedef struct meshqueue_s
{
	struct meshqueue_s *next;
	void (*callback)(const entity_render_t *ent, const rtlight_t *rtlight, int numsurfaces, int *surfaceindices);
	const entity_render_t *ent;
	int surfacenumber;
	const rtlight_t *rtlight;
	float dist;
}
meshqueue_t;

float mqt_viewplanedist;
float mqt_viewmaxdist;
meshqueue_t *mqt_array;
int mqt_count;
int mqt_total;

void R_MeshQueue_BeginScene(void)
{
	mqt_count = 0;
	mqt_viewplanedist = DotProduct(r_refdef.view.origin, r_refdef.view.forward);
	mqt_viewmaxdist = 0;
}

void R_MeshQueue_AddTransparent(const vec3_t center, void (*callback)(const entity_render_t *ent, const rtlight_t *rtlight, int numsurfaces, int *surfacelist), const entity_render_t *ent, int surfacenumber, const rtlight_t *rtlight)
{
	meshqueue_t *mq;
	if (mqt_count >= mqt_total || !mqt_array)
	{
		int newtotal = max(1024, mqt_total * 2);
		meshqueue_t *newarray = (meshqueue_t *)Mem_Alloc(cls.permanentmempool, newtotal * sizeof(meshqueue_t));
		if (mqt_array)
		{
			memcpy(newarray, mqt_array, mqt_total * sizeof(meshqueue_t));
			Mem_Free(mqt_array);
		}
		mqt_array = newarray;
		mqt_total = newtotal;
	}
	mq = &mqt_array[mqt_count++];
	mq->callback = callback;
	mq->ent = ent;
	mq->surfacenumber = surfacenumber;
	mq->rtlight = rtlight;
	mq->dist = DotProduct(center, r_refdef.view.forward) - mqt_viewplanedist;
	mq->next = NULL;
	mqt_viewmaxdist = max(mqt_viewmaxdist, mq->dist);
}

void R_MeshQueue_RenderTransparent(void)
{
	int i;
	int hashdist;
	int batchnumsurfaces;
	float distscale;
	const entity_render_t *ent;
	const rtlight_t *rtlight;
	void (*callback)(const entity_render_t *ent, const rtlight_t *rtlight, int numsurfaces, int *surfaceindices);
	meshqueue_t *mqt;
	static meshqueue_t *hash[4096], **hashpointer[4096];
	int batchsurfaceindex[256];
	if (!mqt_count)
		return;
	memset(hash, 0, sizeof(hash));
	for (i = 0;i < 4096;i++)
		hashpointer[i] = &hash[i];
	distscale = 4095.0f / max(mqt_viewmaxdist, 4095);
	for (i = 0, mqt = mqt_array;i < mqt_count;i++, mqt++)
	{
		// generate index
		hashdist = (int) (mqt->dist * distscale);
		hashdist = bound(0, hashdist, 4095);
		// link to tail of hash chain (to preserve render order)
		mqt->next = NULL;
		*hashpointer[hashdist] = mqt;
		hashpointer[hashdist] = &mqt->next;
	}
	callback = NULL;
	ent = NULL;
	rtlight = NULL;
	batchnumsurfaces = 0;
	for (i = 4095;i >= 0;i--)
	{
		if (hash[i])
		{
			for (mqt = hash[i];mqt;mqt = mqt->next)
			{
				if (ent != mqt->ent || rtlight != mqt->rtlight || callback != mqt->callback || batchnumsurfaces >= 256)
				{
					if (batchnumsurfaces)
						callback(ent, rtlight, batchnumsurfaces, batchsurfaceindex);
					batchnumsurfaces = 0;
					ent = mqt->ent;
					rtlight = mqt->rtlight;
					callback = mqt->callback;
				}
				batchsurfaceindex[batchnumsurfaces++] = mqt->surfacenumber;
			}
		}
	}
	if (batchnumsurfaces)
		callback(ent, rtlight, batchnumsurfaces, batchsurfaceindex);
	mqt_count = 0;
}
