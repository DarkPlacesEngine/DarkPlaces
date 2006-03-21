
#include "quakedef.h"
#include "meshqueue.h"

cvar_t r_meshqueue_entries = {CVAR_SAVE, "r_meshqueue_entries", "16", "maximum number of meshes to batch together and sort before issuing render calls (unused)"};
cvar_t r_meshqueue_immediaterender = {0, "r_meshqueue_immediaterender", "0", "immediately render non-transparent meshes rather than batching"};
cvar_t r_meshqueue_sort = {0, "r_meshqueue_sort", "0", "whether to sort meshes in a batch before issuing calls"};

typedef struct meshqueue_s
{
	struct meshqueue_s *next;
	void (*callback)(const entity_render_t *ent, int surfacenumber, const rtlight_t *rtlight);
	const entity_render_t *ent;
	int surfacenumber;
	const rtlight_t *rtlight;
	float dist;
}
meshqueue_t;

float mqt_viewplanedist;
float mqt_viewmaxdist;
meshqueue_t *mq_array, *mqt_array, *mq_listhead;
int mq_count, mqt_count;
int mq_total, mqt_total;

void R_MeshQueue_Init(void)
{
	Cvar_RegisterVariable(&r_meshqueue_entries);
	Cvar_RegisterVariable(&r_meshqueue_immediaterender);
	Cvar_RegisterVariable(&r_meshqueue_sort);

	mq_total = 0;
	mqt_total = 0;
	mq_array = NULL;
	mqt_array = NULL;
}

void R_MeshQueue_Render(void)
{
	meshqueue_t *mq;
	if (!mq_count)
		return;
	for (mq = mq_listhead;mq;mq = mq->next)
		mq->callback(mq->ent, mq->surfacenumber, mq->rtlight);
	mq_count = 0;
	mq_listhead = NULL;
}

static void R_MeshQueue_EnlargeTransparentArray(int newtotal)
{
	meshqueue_t *newarray;
	newarray = (meshqueue_t *)Mem_Alloc(cls.permanentmempool, newtotal * sizeof(meshqueue_t));
	if (mqt_array)
	{
		memcpy(newarray, mqt_array, mqt_total * sizeof(meshqueue_t));
		Mem_Free(mqt_array);
	}
	mqt_array = newarray;
	mqt_total = newtotal;
}

void R_MeshQueue_Add(void (*callback)(const entity_render_t *ent, int surfacenumber, const rtlight_t *rtlight), const entity_render_t *ent, int surfacenumber, const rtlight_t *rtlight)
{
	meshqueue_t *mq, **mqnext;
	if (r_meshqueue_immediaterender.integer)
	{
		callback(ent, surfacenumber, rtlight);
		return;
	}
	if (mq_count >= mq_total)
		R_MeshQueue_Render();
	mq = &mq_array[mq_count++];
	mq->callback = callback;
	mq->ent = ent;
	mq->surfacenumber = surfacenumber;
	mq->rtlight = rtlight;

	if (r_meshqueue_sort.integer)
	{
		// bubble-insert sort into meshqueue
		for(mqnext = &mq_listhead;*mqnext;mqnext = &(*mqnext)->next)
		{
			if (mq->callback == (*mqnext)->callback)
			{
				if (mq->ent == (*mqnext)->ent)
				{
					if (mq->surfacenumber == (*mqnext)->surfacenumber)
					{
						if (mq->rtlight <= (*mqnext)->rtlight)
							break;
					}
					else if (mq->surfacenumber < (*mqnext)->surfacenumber)
						break;
				}
				else if (mq->ent < (*mqnext)->ent)
					break;
			}
			else if (mq->callback < (*mqnext)->callback)
				break;
		}
	}
	else
	{
		// maintain the order
		for(mqnext = &mq_listhead;*mqnext;mqnext = &(*mqnext)->next);
	}
	mq->next = *mqnext;
	*mqnext = mq;
}

void R_MeshQueue_AddTransparent(const vec3_t center, void (*callback)(const entity_render_t *ent, int surfacenumber, const rtlight_t *rtlight), const entity_render_t *ent, int surfacenumber, const rtlight_t *rtlight)
{
	meshqueue_t *mq;
	if (mqt_count >= mqt_total)
		R_MeshQueue_EnlargeTransparentArray(mqt_total + 100);
	mq = &mqt_array[mqt_count++];
	mq->callback = callback;
	mq->ent = ent;
	mq->surfacenumber = surfacenumber;
	mq->rtlight = rtlight;
	mq->dist = DotProduct(center, r_viewforward) - mqt_viewplanedist;
	mq->next = NULL;
	mqt_viewmaxdist = max(mqt_viewmaxdist, mq->dist);
}

void R_MeshQueue_RenderTransparent(void)
{
	int i;
	int hashdist;
	float distscale;
	meshqueue_t *mqt;
	meshqueue_t *hash[4096], **hashpointer[4096];
	if (mq_count)
		R_MeshQueue_Render();
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
	for (i = 4095;i >= 0;i--)
		if (hash[i])
			for (mqt = hash[i];mqt;mqt = mqt->next)
				mqt->callback(mqt->ent, mqt->surfacenumber, mqt->rtlight);
	mqt_count = 0;
}

void R_MeshQueue_BeginScene(void)
{
	if (r_meshqueue_entries.integer < 1)
		Cvar_SetValueQuick(&r_meshqueue_entries, 1);
	if (r_meshqueue_entries.integer > 65536)
		Cvar_SetValueQuick(&r_meshqueue_entries, 65536);

	if (mq_total != r_meshqueue_entries.integer || mq_array == NULL)
	{
		mq_total = r_meshqueue_entries.integer;
		if (mq_array)
			Mem_Free(mq_array);
		mq_array = (meshqueue_t *)Mem_Alloc(cls.permanentmempool, mq_total * sizeof(meshqueue_t));
	}

	if (mqt_array == NULL)
		mqt_array = (meshqueue_t *)Mem_Alloc(cls.permanentmempool, mqt_total * sizeof(meshqueue_t));

	mq_count = 0;
	mqt_count = 0;
	mq_listhead = NULL;
	mqt_viewplanedist = DotProduct(r_vieworigin, r_viewforward);
	mqt_viewmaxdist = 0;
}

void R_MeshQueue_EndScene(void)
{
	if (mq_count)
	{
		Con_Printf("R_MeshQueue_EndScene: main mesh queue still has %i items left, flushing\n", mq_count);
		R_MeshQueue_Render();
	}
	if (mqt_count)
	{
		Con_Printf("R_MeshQueue_EndScene: transparent mesh queue still has %i items left, flushing\n", mqt_count);
		R_MeshQueue_RenderTransparent();
	}
}

