#include "quakedef.h"
#include "nodegraph.h"

// ============================================================================
#define NODEGRAPH_NODES_COUNT_LIMIT 4096
#define NODEGRAPH_QUERY_ENTRIES_LIMIT NODEGRAPH_NODES_COUNT_LIMIT
#define NODEGRAPH_QUERIES_COUNT_LIMIT 128

// ============================================================================
#define NODEGRAPH_NODES_DATA_LENGTH (NODEGRAPH_NODES_COUNT_LIMIT * 3)
#define NODEGRAPH_LINKS_DATA_LENGTH (NODEGRAPH_NODES_COUNT_LIMIT * NODEGRAPH_NODES_COUNT_LIMIT / 8)

// ============================================================================
#define GRAPH_MATRIX_ELEMENT_INDEX(i, j) GRAPH_MATRIX_ELEMENT_INDEX_SIZED(i, j, NODEGRAPH_NODES_COUNT_LIMIT)
#define GRAPH_MATRIX_ELEMENT_INDEX_SIZED(i, j, size) (i * size + j)

// ============================================================================
typedef struct nodegraph_s
{
	vec_t nodes[NODEGRAPH_NODES_DATA_LENGTH];
	char links[NODEGRAPH_LINKS_DATA_LENGTH];
	int nodes_count;
}
nodegraph_t;

typedef struct nodegraph_query_s
{
	short entries[NODEGRAPH_QUERY_ENTRIES_LIMIT];
	short graphid;
	short entries_count;
}
nodegraph_query_t;

typedef struct nodegraph_floyd_warshall_matrix_s
{
	short indexes[NODEGRAPH_NODES_COUNT_LIMIT * NODEGRAPH_NODES_COUNT_LIMIT];
}
nodegraph_floyd_warshall_matrix_t;

// ============================================================================
typedef struct nodegraph_query_sort_data_s
{
	short queryid;
	vec3_t point;
}
nodegraph_query_sort_data_t;

// ============================================================================
static nodegraph_t g_nodegraph_set[NODEGRAPH_GRAPHSET_SIZE_LIMIT];
static nodegraph_query_t g_nodegraph_queries[NODEGRAPH_QUERIES_COUNT_LIMIT];
static nodegraph_floyd_warshall_matrix_t g_nodegraph_floyd_warshall_matrices[NODEGRAPH_GRAPHSET_SIZE_LIMIT];

// ============================================================================
static nodegraph_query_sort_data_t g_nodegraph_query_sort_data;

// ============================================================================
static int nodegraph_query_sort_function(const void *left, const void *right)
{
	const short queryid = g_nodegraph_query_sort_data.queryid;

	const short leftid = *(const short *)left;
	const short rightid = *(const short *)right;

	vec3_t pointleft;
	vec3_t pointright;

	float distanceleft;
	float distanceright;

	nodegraph_query_t *query;
	nodegraph_t *nodegraph;

	if (queryid < 0 || queryid >= NODEGRAPH_QUERIES_COUNT_LIMIT)
	{
		Con_DPrintf("%s, queryid is out of bounds: %d\n", __FUNCTION__, queryid);
		return 0;
	}

	query = &g_nodegraph_queries[queryid];
	nodegraph = &g_nodegraph_set[query->graphid];

	if (leftid < 0 || leftid >= nodegraph->nodes_count)
	{
		Con_DPrintf("%s, leftid is out of bounds: %d\n", __FUNCTION__, leftid);
		return 0;
	}

	if (rightid < 0 || rightid >= nodegraph->nodes_count)
	{
		Con_DPrintf("%s, rightid is out of bounds: %d\n", __FUNCTION__, rightid);
		return 0;
	}

	nodegraph_graph_get_node(query->graphid, leftid, pointleft);
	nodegraph_graph_get_node(query->graphid, rightid, pointright);

	distanceleft = VectorDistance(pointleft, g_nodegraph_query_sort_data.point);
	distanceright = VectorDistance(pointright, g_nodegraph_query_sort_data.point);

	return distanceleft - distanceright;
}

// ============================================================================
static qboolean nodegraph_graph_queries_clear(short graphid)
{
	short i;

	if (graphid < 0 || graphid >= NODEGRAPH_GRAPHSET_SIZE_LIMIT)
	{
		Con_DPrintf("%s, graphid is out of bounds: %d\n", __FUNCTION__, graphid);
		return false;
	}

	for (i = 0; i < NODEGRAPH_QUERIES_COUNT_LIMIT; i++)
	{
		nodegraph_query_t *query = &g_nodegraph_queries[i];

		if (query->graphid == graphid)
		{
			nodegraph_query_release(i);
		}
	}

	return true;
}

// ============================================================================
static qboolean nodegraph_graph_rebuild_floyd_warshall_matrices(void)
{
	short graphid, i, j, k;

	float *floyd_matrix_measures = (float *)Mem_Alloc(tempmempool, NODEGRAPH_NODES_COUNT_LIMIT * NODEGRAPH_NODES_COUNT_LIMIT * sizeof(float));

	if (!floyd_matrix_measures)
	{
		return false;
	}

	for (graphid = 0; graphid < NODEGRAPH_GRAPHSET_SIZE_LIMIT; graphid++)
	{
		nodegraph_t *nodegraph = &g_nodegraph_set[graphid];
		nodegraph_floyd_warshall_matrix_t *floyd_matrix = &g_nodegraph_floyd_warshall_matrices[graphid];

		for (i = 0; i < nodegraph->nodes_count; i++)
		{
			for (j = 0; j < nodegraph->nodes_count; j++)
			{
				floyd_matrix_measures[GRAPH_MATRIX_ELEMENT_INDEX(i, j)] = 16777216.0f;
				floyd_matrix->indexes[GRAPH_MATRIX_ELEMENT_INDEX(i, j)] = -1;

				if (nodegraph_graph_does_link_exist(graphid, i, j))
				{
					vec3_t nodefrom;
					vec3_t nodeto;

					float distance;

					nodegraph_graph_get_node(graphid, i, nodefrom);
					nodegraph_graph_get_node(graphid, j, nodeto);

					distance = VectorDistance(nodefrom, nodeto);

					floyd_matrix_measures[GRAPH_MATRIX_ELEMENT_INDEX(i, j)] = distance;
					floyd_matrix->indexes[GRAPH_MATRIX_ELEMENT_INDEX(i, j)] = j;
				}
			}
		}

		for (i = 0; i < nodegraph->nodes_count; i++)
		{
			floyd_matrix_measures[GRAPH_MATRIX_ELEMENT_INDEX(i, i)] = 0.0f;
			floyd_matrix->indexes[GRAPH_MATRIX_ELEMENT_INDEX(i, i)] = i;
		}

		for (k = 0; k < nodegraph->nodes_count; k++)
		{
			for (i = 0; i < nodegraph->nodes_count; i++)
			{
				for (j = 0; j < nodegraph->nodes_count; j++)
				{
					float distance = floyd_matrix_measures[GRAPH_MATRIX_ELEMENT_INDEX(i, k)] + floyd_matrix_measures[GRAPH_MATRIX_ELEMENT_INDEX(k, j)];

					if (floyd_matrix_measures[GRAPH_MATRIX_ELEMENT_INDEX(i, j)] > distance)
					{
						floyd_matrix_measures[GRAPH_MATRIX_ELEMENT_INDEX(i, j)] = distance;
						floyd_matrix->indexes[GRAPH_MATRIX_ELEMENT_INDEX(i, j)] = floyd_matrix->indexes[GRAPH_MATRIX_ELEMENT_INDEX(i, k)];
					}
				}
			}
		}
	}

	Mem_Free(floyd_matrix_measures);

	return true;
}

// ============================================================================
qboolean nodegraph_graphset_clear(void)
{
	short i;

	for (i = 0; i < NODEGRAPH_GRAPHSET_SIZE_LIMIT; i++)
	{
		nodegraph_graph_clear(i);
	}

	return true;
}

// ============================================================================
qboolean nodegraph_graphset_load(void)
{
	char vabuf[1024];
	char *graphset_data;

	qboolean nodegraph_graphset_has_been_loaded;

	nodegraph_graphset_has_been_loaded = (graphset_data = (char *)FS_LoadFile(va(vabuf, sizeof(vabuf), "%s.qng", sv.worldnamenoextension), tempmempool, true, NULL)) != NULL;

	if (nodegraph_graphset_has_been_loaded)
	{
		short graphid;
		short graphset_nodes_count[NODEGRAPH_GRAPHSET_SIZE_LIMIT];

		size_t offset, length;

		Con_Printf("Loaded %s.qng\n", sv.worldnamenoextension);

		nodegraph_graphset_clear();

		offset = 0;

		length = sizeof(short) * NODEGRAPH_GRAPHSET_SIZE_LIMIT;
		memcpy((void *)graphset_nodes_count, (const void *)(graphset_data + offset), length);

		offset += length;

		for (graphid = 0; graphid < NODEGRAPH_GRAPHSET_SIZE_LIMIT; graphid++)
		{
			nodegraph_t *nodegraph = &g_nodegraph_set[graphid];
			nodegraph->nodes_count = graphset_nodes_count[graphid];

			if (nodegraph->nodes_count > 0)
			{
				short i, j;
				char *nodegraph_links_sub_matrix;

				length = sizeof(float) * 3 * nodegraph->nodes_count;
				memcpy((void *)nodegraph->nodes, (const void *)(graphset_data + offset), length);

				offset += length;

				nodegraph_links_sub_matrix = graphset_data + offset;

				for (i = 0; i < nodegraph->nodes_count; i++)
				{
					for (j = 0; j < nodegraph->nodes_count; j++)
					{
						int entryindex = GRAPH_MATRIX_ELEMENT_INDEX_SIZED(i, j, nodegraph->nodes_count);
						qboolean does_link_exist = ((nodegraph_links_sub_matrix[entryindex / 8] & (1 << (entryindex % 8))) != 0);

						if (does_link_exist)
						{
							nodegraph_graph_add_link(graphid, i, j);
						}
					}
				}
				
				length = (nodegraph->nodes_count * nodegraph->nodes_count - 1) / 8 + 1;
				offset += length;
			}
		}

		for (graphid = 0; graphid < NODEGRAPH_GRAPHSET_SIZE_LIMIT; graphid++)
		{
			nodegraph_t *nodegraph = &g_nodegraph_set[graphid];
			nodegraph_floyd_warshall_matrix_t *floyd_matrix = &g_nodegraph_floyd_warshall_matrices[graphid];

			short i, j;
			short *floyd_sub_matrix_indexes = (short *)(graphset_data + offset);

			for (i = 0; i < nodegraph->nodes_count; i++)
			{
				for (j = 0; j < nodegraph->nodes_count; j++)
				{
					floyd_matrix->indexes[GRAPH_MATRIX_ELEMENT_INDEX(i, j)] = floyd_sub_matrix_indexes[GRAPH_MATRIX_ELEMENT_INDEX_SIZED(i, j, nodegraph->nodes_count)];
				}
			}

			offset += sizeof(short) * nodegraph->nodes_count * nodegraph->nodes_count;
		}

		Mem_Free(graphset_data);

		return true;
	}

	return false;
}

// ============================================================================
qboolean nodegraph_graphset_save(void)
{   
	char vabuf[1024];

	char *graphset_data;
	size_t graphset_data_size;

	qboolean nodegraph_graphset_has_been_saved;

	short graphid;
	short graphset_nodes_count[NODEGRAPH_GRAPHSET_SIZE_LIMIT];

	size_t offset, length;

	nodegraph_graph_rebuild_floyd_warshall_matrices();

	graphset_data_size = sizeof(short) * NODEGRAPH_GRAPHSET_SIZE_LIMIT + sizeof(g_nodegraph_set) + sizeof(g_nodegraph_floyd_warshall_matrices);
	graphset_data = (char *)Mem_Alloc(tempmempool, graphset_data_size);

	if (!graphset_data)
	{
		return false;
	}

	memset((void *)graphset_data, 0, graphset_data_size);

	for (graphid = 0; graphid < NODEGRAPH_GRAPHSET_SIZE_LIMIT; graphid++)
	{
		nodegraph_t *nodegraph = &g_nodegraph_set[graphid];
		graphset_nodes_count[graphid] = nodegraph->nodes_count;
	}

	offset = 0;

	length = sizeof(short) * NODEGRAPH_GRAPHSET_SIZE_LIMIT;
	memcpy((void *)(graphset_data + offset), (const void *)graphset_nodes_count, length);

	offset += length;

	for (graphid = 0; graphid < NODEGRAPH_GRAPHSET_SIZE_LIMIT; graphid++)
	{
		nodegraph_t *nodegraph = &g_nodegraph_set[graphid];

		if (nodegraph->nodes_count > 0)
		{
			short i, j;
			char *nodegraph_links_sub_matrix;

			length = sizeof(float) * 3 * nodegraph->nodes_count;
			memcpy((void *)(graphset_data + offset), (const void *)nodegraph->nodes, length);

			offset += length;
			
			nodegraph_links_sub_matrix = graphset_data + offset;

			for (i = 0; i < nodegraph->nodes_count; i++)
			{
				for (j = 0; j < nodegraph->nodes_count; j++)
				{				  
					if (nodegraph_graph_does_link_exist(graphid, i, j))
					{
						int entryindex = GRAPH_MATRIX_ELEMENT_INDEX_SIZED(i, j, nodegraph->nodes_count);
						nodegraph_links_sub_matrix[entryindex / 8] |= 1 << (entryindex % 8);
					}
				}
			}
			
			length = (nodegraph->nodes_count * nodegraph->nodes_count - 1) / 8 + 1;
			offset += length;
		}
	}

	for (graphid = 0; graphid < NODEGRAPH_GRAPHSET_SIZE_LIMIT; graphid++)
	{
		nodegraph_t *nodegraph = &g_nodegraph_set[graphid];
		nodegraph_floyd_warshall_matrix_t *floyd_matrix = &g_nodegraph_floyd_warshall_matrices[graphid];

		short i, j;
		short *floyd_sub_matrix_indexes = (short *)(graphset_data + offset);

		for (i = 0; i < nodegraph->nodes_count; i++)
		{
			for (j = 0; j < nodegraph->nodes_count; j++)
			{
				floyd_sub_matrix_indexes[GRAPH_MATRIX_ELEMENT_INDEX_SIZED(i, j, nodegraph->nodes_count)] = floyd_matrix->indexes[GRAPH_MATRIX_ELEMENT_INDEX(i, j)];
			}
		}

		offset += sizeof(short) * nodegraph->nodes_count * nodegraph->nodes_count;
	}

	graphset_data_size = offset;

	nodegraph_graphset_has_been_saved = FS_WriteFile(va(vabuf, sizeof(vabuf), "%s.qng", sv.worldnamenoextension), (const void *)graphset_data, (fs_offset_t)graphset_data_size);

	Mem_Free(graphset_data);

	if (nodegraph_graphset_has_been_saved)
	{
		Con_Printf("Saved %s.qng\n", sv.worldnamenoextension);
	}

	return nodegraph_graphset_has_been_saved;
}

// ============================================================================
qboolean nodegraph_graph_clear(short graphid)
{
	nodegraph_t *nodegraph;
	nodegraph_floyd_warshall_matrix_t *floyd_matrix;

	if (graphid < 0 || graphid >= NODEGRAPH_GRAPHSET_SIZE_LIMIT)
	{
		Con_DPrintf("%s, graphid is out of bounds: %d\n", __FUNCTION__, graphid);
		return false;
	}

	nodegraph = &g_nodegraph_set[graphid];
	memset((void *)nodegraph, 0, sizeof(nodegraph_t));

	nodegraph_graph_queries_clear(graphid);

	floyd_matrix = &g_nodegraph_floyd_warshall_matrices[graphid];
	memset((void *)floyd_matrix, 0, sizeof(nodegraph_floyd_warshall_matrix_t));

	return true;
}

// ============================================================================
short nodegraph_graph_nodes_count(short graphid)
{
	nodegraph_t *nodegraph;

	if (graphid < 0 || graphid >= NODEGRAPH_GRAPHSET_SIZE_LIMIT)
	{
		Con_DPrintf("%s, graphid is out of bounds: %d\n", __FUNCTION__, graphid);
		return -1;
	}

	nodegraph = &g_nodegraph_set[graphid];

	return nodegraph->nodes_count;
}

// ============================================================================
qboolean nodegraph_graph_add_node(short graphid, const vec3_t node)
{
	nodegraph_t *nodegraph;

	if (graphid < 0 || graphid >= NODEGRAPH_GRAPHSET_SIZE_LIMIT)
	{
		Con_DPrintf("%s, graphid is out of bounds: %d\n", __FUNCTION__, graphid);
		return false;
	}

	nodegraph = &g_nodegraph_set[graphid];

	if (nodegraph->nodes_count >= NODEGRAPH_NODES_COUNT_LIMIT)
	{
		Con_DPrintf("%s, the number of nodes exceeds the limit: %d\n", __FUNCTION__, NODEGRAPH_NODES_COUNT_LIMIT);
		return false;
	}

	VectorCopy(node, &nodegraph->nodes[nodegraph->nodes_count * 3]);
	nodegraph->nodes_count++;

	nodegraph_graph_queries_clear(graphid);

	return true;
}

// ============================================================================
qboolean nodegraph_graph_remove_node(short graphid, short nodeid)
{
	nodegraph_t *nodegraph;

	short i, j;

	if (graphid < 0 || graphid >= NODEGRAPH_GRAPHSET_SIZE_LIMIT)
	{
		Con_DPrintf("%s, graphid is out of bounds: %d\n", __FUNCTION__, graphid);
		return false;
	}

	nodegraph = &g_nodegraph_set[graphid];

	if (nodeid < 0 || nodeid >= nodegraph->nodes_count)
	{
		Con_DPrintf("%s, nodeid is out of bounds: %d\n", __FUNCTION__, nodeid);
		return false;
	}

	for (i = nodeid; i < nodegraph->nodes_count - 1; i++)
	{
		VectorCopy(&nodegraph->nodes[(i + 1) * 3], &nodegraph->nodes[i * 3]);

		for (j = 0; j < nodegraph->nodes_count; j++)
		{
			nodegraph_graph_does_link_exist(graphid, i + 1, j) ? nodegraph_graph_add_link(graphid, i, j) : nodegraph_graph_remove_link(graphid, i, j);
			nodegraph_graph_does_link_exist(graphid, j, i + 1) ? nodegraph_graph_add_link(graphid, j, i) : nodegraph_graph_remove_link(graphid, j, i);
		}
	}

	VectorSet(&nodegraph->nodes[(nodegraph->nodes_count - 1) * 3], 0.0f, 0.0f, 0.0f);
	nodegraph->nodes_count--;

	nodegraph_graph_queries_clear(graphid);

	return true;
}

// ============================================================================
qboolean nodegraph_graph_is_node_valid(short graphid, short nodeid)
{
	nodegraph_t *nodegraph;

	if (graphid < 0 || graphid >= NODEGRAPH_GRAPHSET_SIZE_LIMIT)
	{
		Con_DPrintf("%s, graphid is out of bounds: %d\n", __FUNCTION__, graphid);
		return false;
	}

	nodegraph = &g_nodegraph_set[graphid];

	if (nodeid < 0 || nodeid >= nodegraph->nodes_count)
	{
		return false;
	}

	return true;
}

// ============================================================================
qboolean nodegraph_graph_get_node(short graphid, short nodeid, vec3_t outnode)
{
	nodegraph_t *nodegraph;

	VectorSet(outnode, NAN, NAN, NAN);

	if (graphid < 0 || graphid >= NODEGRAPH_GRAPHSET_SIZE_LIMIT)
	{
		Con_DPrintf("%s, graphid is out of bounds: %d\n", __FUNCTION__, graphid);
		return false;
	}

	nodegraph = &g_nodegraph_set[graphid];

	if (nodeid < 0 || nodeid >= nodegraph->nodes_count)
	{
		Con_DPrintf("%s, nodeid is out of bounds: %d\n", __FUNCTION__, nodeid);
		return false;
	}

	VectorCopy(&nodegraph->nodes[nodeid * 3], outnode);

	return true;
}

// ============================================================================
qboolean nodegraph_graph_add_link(short graphid, short nodeidfrom, short nodeidto)
{
	nodegraph_t *nodegraph;

	int entryindex;

	if (graphid < 0 || graphid >= NODEGRAPH_GRAPHSET_SIZE_LIMIT)
	{
		Con_DPrintf("%s, graphid is out of bounds: %d\n", __FUNCTION__, graphid);
		return false;
	}

	nodegraph = &g_nodegraph_set[graphid];

	if (nodeidfrom < 0 || nodeidfrom >= nodegraph->nodes_count)
	{
		Con_DPrintf("%s, nodeidfrom is out of bounds: %d\n", __FUNCTION__, nodeidfrom);
		return false;
	}

	if (nodeidto < 0 || nodeidto >= nodegraph->nodes_count)
	{
		Con_DPrintf("%s, nodeidto is out of bounds: %d\n", __FUNCTION__, nodeidto);
		return false;
	}

	entryindex = GRAPH_MATRIX_ELEMENT_INDEX(nodeidfrom, nodeidto);
	nodegraph->links[entryindex / 8] |= 1 << (entryindex % 8);

	return true;
}

// ============================================================================
qboolean nodegraph_graph_remove_link(short graphid, short nodeidfrom, short nodeidto)
{
	nodegraph_t *nodegraph;

	int entryindex;

	if (graphid < 0 || graphid >= NODEGRAPH_GRAPHSET_SIZE_LIMIT)
	{
		Con_DPrintf("%s, graphid is out of bounds: %d\n", __FUNCTION__, graphid);
		return false;
	}

	nodegraph = &g_nodegraph_set[graphid];

	if (nodeidfrom < 0 || nodeidfrom >= nodegraph->nodes_count)
	{
		Con_DPrintf("%s, nodeidfrom is out of bounds: %d\n", __FUNCTION__, nodeidfrom);
		return false;
	}

	if (nodeidto < 0 || nodeidto >= nodegraph->nodes_count)
	{
		Con_DPrintf("%s, nodeidto is out of bounds: %d\n", __FUNCTION__, nodeidto);
		return false;
	}

	entryindex = GRAPH_MATRIX_ELEMENT_INDEX(nodeidfrom, nodeidto);
	nodegraph->links[entryindex / 8] &= ~(1 << (entryindex % 8));

	return true;
}

// ============================================================================
qboolean nodegraph_graph_does_link_exist(short graphid, short nodeidfrom, short nodeidto)
{
	nodegraph_t *nodegraph;

	int entryindex;

	if (graphid < 0 || graphid >= NODEGRAPH_GRAPHSET_SIZE_LIMIT)
	{
		Con_DPrintf("%s, graphid is out of bounds: %d\n", __FUNCTION__, graphid);
		return false;
	}

	nodegraph = &g_nodegraph_set[graphid];

	if (nodeidfrom < 0 || nodeidfrom >= nodegraph->nodes_count)
	{
		Con_DPrintf("%s, nodeidfrom is out of bounds: %d\n", __FUNCTION__, nodeidfrom);
		return false;
	}

	if (nodeidto < 0 || nodeidto >= nodegraph->nodes_count)
	{
		Con_DPrintf("%s, nodeidto is out of bounds: %d\n", __FUNCTION__, nodeidto);
		return false;
	}

	entryindex = GRAPH_MATRIX_ELEMENT_INDEX(nodeidfrom, nodeidto);

	return ((nodegraph->links[entryindex / 8] & (1 << (entryindex % 8))) != 0);
}

// ============================================================================
short nodegraph_graph_find_nearest_nodeid(short graphid, const vec3_t position)
{
	nodegraph_t *nodegraph;

	short i, nodeid;
	float distance, shortestdistance;

	if (graphid < 0 || graphid >= NODEGRAPH_GRAPHSET_SIZE_LIMIT)
	{
		Con_DPrintf("%s, graphid is out of bounds: %d\n", __FUNCTION__, graphid);
		return -1;
	}

	nodegraph = &g_nodegraph_set[graphid];

	nodeid = -1;
	shortestdistance = 16777216.0f;

	for (i = 0; i < nodegraph->nodes_count; i++)
	{
		distance = VectorDistance(&nodegraph->nodes[i * 3], position);
		
		if (shortestdistance > distance)
		{
			nodeid = i;
			shortestdistance = distance;
		}
	}

	return nodeid;
}

// ============================================================================
short nodegraph_graph_query_path(short graphid, short nodeidfrom, short nodeidto)
{
	nodegraph_t *nodegraph;
	nodegraph_floyd_warshall_matrix_t *floyd_matrix;

	short i, queryid;
	nodegraph_query_t *query;

	if (graphid < 0 || graphid >= NODEGRAPH_GRAPHSET_SIZE_LIMIT)
	{
		Con_DPrintf("%s, graphid is out of bounds: %d\n", __FUNCTION__, graphid);
		return -1;
	}

	nodegraph = &g_nodegraph_set[graphid];
	floyd_matrix = &g_nodegraph_floyd_warshall_matrices[graphid];

	if (nodeidfrom < 0 || nodeidfrom >= nodegraph->nodes_count)
	{
		Con_DPrintf("%s, nodeidfrom is out of bounds: %d\n", __FUNCTION__, nodeidfrom);
		return -1;
	}

	if (nodeidto < 0 || nodeidto >= nodegraph->nodes_count)
	{
		Con_DPrintf("%s, nodeidto is out of bounds: %d\n", __FUNCTION__, nodeidto);
		return -1;
	}

	queryid = -1;

	for (i = 0; i < NODEGRAPH_QUERIES_COUNT_LIMIT; i++)
	{
		if (!nodegraph_query_is_valid(i))
		{
			queryid = i;
			break;
		}
	}

	if (queryid != -1)
	{
		query = &g_nodegraph_queries[queryid];

		query->graphid = graphid;

		if (floyd_matrix->indexes[GRAPH_MATRIX_ELEMENT_INDEX(nodeidfrom, nodeidto)] != -1)
		{
			query->entries[query->entries_count] = nodeidfrom;
			query->entries_count++;

			while (nodeidfrom != nodeidto)
			{
				nodeidfrom = floyd_matrix->indexes[GRAPH_MATRIX_ELEMENT_INDEX(nodeidfrom, nodeidto)];

				query->entries[query->entries_count] = nodeidfrom;
				query->entries_count++;

				if (query->entries_count >= NODEGRAPH_QUERY_ENTRIES_LIMIT)
				{
					break;
				}
			}
		}

		if (query->entries_count == 0)
		{
			nodegraph_query_release(queryid);
			queryid = -1;
		}
	}

	return queryid;
}

// ============================================================================
short nodegraph_graph_query_nodes_linked(short graphid, short nodeid)
{
	nodegraph_t *nodegraph;

	short i, queryid;
	nodegraph_query_t *query;

	if (graphid < 0 || graphid >= NODEGRAPH_GRAPHSET_SIZE_LIMIT)
	{
		Con_DPrintf("%s, graphid is out of bounds: %d\n", __FUNCTION__, graphid);
		return -1;
	}

	nodegraph = &g_nodegraph_set[graphid];

	if (nodeid < 0 || nodeid >= nodegraph->nodes_count)
	{
		Con_DPrintf("%s, nodeid is out of bounds: %d\n", __FUNCTION__, nodeid);
		return -1;
	}

	queryid = -1;

	for (i = 0; i < NODEGRAPH_QUERIES_COUNT_LIMIT; i++)
	{
		if (!nodegraph_query_is_valid(i))
		{
			queryid = i;
			break;
		}
	}

	if (queryid != -1)
	{
		query = &g_nodegraph_queries[queryid];

		query->graphid = graphid;

		for (i = 0; i < nodegraph->nodes_count; i++)
		{
			if (nodegraph_graph_does_link_exist(graphid, nodeid, i))
			{
				query->entries[query->entries_count] = i;
				query->entries_count++;
			}

			if (query->entries_count >= NODEGRAPH_QUERY_ENTRIES_LIMIT)
			{
				break;
			}
		}

		if (query->entries_count == 0)
		{
			nodegraph_query_release(queryid);
			queryid = -1;
		}
		else
		{
			g_nodegraph_query_sort_data.queryid = queryid;
			nodegraph_graph_get_node(graphid, nodeid, g_nodegraph_query_sort_data.point);

			qsort(query->entries, query->entries_count, sizeof(short), nodegraph_query_sort_function);
		}
		
	}

	return queryid;
}

// ============================================================================
short nodegraph_graph_query_nodes_in_radius(short graphid, const vec3_t position, float radius)
{
	nodegraph_t *nodegraph;

	vec3_t node;
	short i, queryid;
	nodegraph_query_t *query;

	if (graphid < 0 || graphid >= NODEGRAPH_GRAPHSET_SIZE_LIMIT)
	{
		Con_DPrintf("%s, graphid is out of bounds: %d\n", __FUNCTION__, graphid);
		return -1;
	}

	nodegraph = &g_nodegraph_set[graphid];

	queryid = -1;

	for (i = 0; i < NODEGRAPH_QUERIES_COUNT_LIMIT; i++)
	{
		if (!nodegraph_query_is_valid(i))
		{
			queryid = i;
			break;
		}
	}

	if (queryid != -1)
	{
		query = &g_nodegraph_queries[queryid];

		query->graphid = graphid;

		for (i = 0; i < nodegraph->nodes_count; i++)
		{
			nodegraph_graph_get_node(graphid, i, node);
			
			if (VectorDistance(position, node) <= radius)
			{
				query->entries[query->entries_count] = i;
				query->entries_count++;
			}

			if (query->entries_count >= NODEGRAPH_QUERY_ENTRIES_LIMIT)
			{
				break;
			}
		}

		if (query->entries_count == 0)
		{
			nodegraph_query_release(queryid);
			queryid = -1;
		}
		else
		{
			g_nodegraph_query_sort_data.queryid = queryid;
			VectorCopy(position, g_nodegraph_query_sort_data.point);

			qsort(query->entries, query->entries_count, sizeof(short), nodegraph_query_sort_function);
		}
	}

	return queryid;
}

// ============================================================================
qboolean nodegraph_query_release(short queryid)
{
	nodegraph_query_t *query;

	if (queryid < 0 || queryid >= NODEGRAPH_QUERIES_COUNT_LIMIT)
	{
		Con_DPrintf("%s, queryid is out of bounds: %d\n", __FUNCTION__, queryid);
		return false;
	}

	query = &g_nodegraph_queries[queryid];
	memset((void *)query, 0, sizeof(nodegraph_query_t));
	
	return true;
}

// ============================================================================
short nodegraph_query_entries_count(short queryid)
{
	nodegraph_query_t *query;

	if (queryid < 0 || queryid >= NODEGRAPH_QUERIES_COUNT_LIMIT)
	{
		Con_DPrintf("%s, queryid is out of bounds: %d\n", __FUNCTION__, queryid);
		return -1;
	}

	query = &g_nodegraph_queries[queryid];

	return query->entries_count;
}

// ============================================================================
qboolean nodegraph_query_is_valid(short queryid)
{
	nodegraph_query_t *query;

	if (queryid < 0 || queryid >= NODEGRAPH_QUERIES_COUNT_LIMIT)
	{
		return false;
	}

	query = &g_nodegraph_queries[queryid];

	return query->entries_count > 0;
}

// ============================================================================
short nodegraph_query_get_graphid(short queryid)
{
	nodegraph_query_t *query;

	if (queryid < 0 || queryid >= NODEGRAPH_QUERIES_COUNT_LIMIT)
	{
		Con_DPrintf("%s, queryid is out of bounds: %d\n", __FUNCTION__, queryid);
		return -1;
	}

	query = &g_nodegraph_queries[queryid];

	return query->graphid;
}

// ============================================================================
short nodegraph_query_get_nodeid(short queryid, short entryid)
{
	nodegraph_query_t *query;

	if (queryid < 0 || queryid >= NODEGRAPH_QUERIES_COUNT_LIMIT)
	{
		Con_DPrintf("%s, queryid is out of bounds: %d\n", __FUNCTION__, queryid);
		return -1;
	}

	query = &g_nodegraph_queries[queryid];

	if (entryid < 0 || entryid >= query->entries_count)
	{
		Con_DPrintf("%s, entryid is out of bounds: %d\n", __FUNCTION__, entryid);
		return -1;
	}

	if (query->graphid < 0 || query->graphid >= NODEGRAPH_GRAPHSET_SIZE_LIMIT)
	{
		Con_DPrintf("%s, graphid is out of bounds: %d\n", __FUNCTION__, query->graphid);
		return -1;
	}

	return query->entries[entryid];
}

// ============================================================================
qboolean nodegraph_moveprobe_fly(const vec3_t nodefrom, const vec3_t nodeto, const vec3_t mins, const vec3_t maxs, short type)
{
	int contents = SUPERCONTENTS_SOLID | SUPERCONTENTS_MONSTERCLIP | SUPERCONTENTS_BOTCLIP;

	vec3_t from, to;
	trace_t trace;

	qboolean connected;

	if (type == NODEGRAPH_MOVEPROBE_TYPE_FLY_AIR || type == NODEGRAPH_MOVEPROBE_TYPE_FLY_WATER)
	{
		contents |= SUPERCONTENTS_LIQUIDSMASK;
	}

	VectorCopy(nodefrom, from);
	from[2] -= mins[2];

	VectorCopy(nodeto, to);
	to[2] -= mins[2];

	trace = SV_TraceBox(from, mins, maxs, to, MOVE_NOMONSTERS, NULL, contents, 0, 0, 0.0f);

	connected = trace.fraction == 1.0;

	if (type == NODEGRAPH_MOVEPROBE_TYPE_FLY_AIR)
	{
		connected = connected && (SV_PointSuperContents(from) & (SUPERCONTENTS_LIQUIDSMASK)) == 0;
		connected = connected && (SV_PointSuperContents(to) & (SUPERCONTENTS_LIQUIDSMASK)) == 0;
	}

	if (type == NODEGRAPH_MOVEPROBE_TYPE_FLY_WATER)
	{
		connected = connected && (SV_PointSuperContents(from) & (SUPERCONTENTS_LIQUIDSMASK)) != 0;
		connected = connected && (SV_PointSuperContents(to) & (SUPERCONTENTS_LIQUIDSMASK)) != 0;
	}

	return connected;
}

// ============================================================================
qboolean nodegraph_moveprobe_walk(const vec3_t nodefrom, const vec3_t nodeto, const vec3_t mins, const vec3_t maxs, float stepheight, float dropheight)
{
	int contents = SUPERCONTENTS_SOLID | SUPERCONTENTS_MONSTERCLIP | SUPERCONTENTS_BOTCLIP;

	float distance, walked;
	float tracestep = max(1.0f, min(maxs[0] - mins[0], maxs[1] - mins[1]) / 2.0f);

	vec3_t from, to, direction, destination;

	qboolean connected = false;

	VectorSubtract(nodeto, nodefrom, direction);
	distance = VectorLength(direction);

	if (distance <= 0.015625f)
	{
		return true;
	}

	direction[2] = 0.0f;
	VectorNormalize(direction);

	VectorCopy(nodefrom, from);
	from[2] -= mins[2];

	VectorCopy(nodeto, destination);
	destination[2] -= mins[2];

	walked = 0.0f;

	while (walked <= distance)
	{
		trace_t trace;

		VectorMA(from, tracestep, direction, from);
		from[2] += stepheight;

		VectorCopy(from, to);
		to[2] -= stepheight + dropheight + 0.5f;

		trace = SV_TraceBox(from, mins, maxs, to, MOVE_NOMONSTERS, NULL, contents, 0, 0, 0.0f);

		if (trace.startsolid || trace.fraction == 1.0)
		{
			break;
		}

		if (VectorDistance(trace.endpos, destination) <= tracestep)
		{
			connected = true;
			break;
		}

		VectorCopy(trace.endpos, from);

		walked += tracestep;
	}

	return connected;
}

// ============================================================================
short nodegraph_graph_query_nodes_in_radius_fly_reachable(short graphid, const vec3_t position, float radius, const vec3_t mins, const vec3_t maxs, short type)
{
	nodegraph_t *nodegraph;

	vec3_t node;
	short i, queryid;
	nodegraph_query_t *query;

	if (graphid < 0 || graphid >= NODEGRAPH_GRAPHSET_SIZE_LIMIT)
	{
		Con_DPrintf("%s, graphid is out of bounds: %d\n", __FUNCTION__, graphid);
		return -1;
	}

	nodegraph = &g_nodegraph_set[graphid];

	queryid = -1;

	for (i = 0; i < NODEGRAPH_QUERIES_COUNT_LIMIT; i++)
	{
		if (!nodegraph_query_is_valid(i))
		{
			queryid = i;
			break;
		}
	}

	if (queryid != -1)
	{
		query = &g_nodegraph_queries[queryid];

		query->graphid = graphid;

		for (i = 0; i < nodegraph->nodes_count; i++)
		{
			nodegraph_graph_get_node(graphid, i, node);
			
			if (VectorDistance(position, node) <= radius)
			{
				if (nodegraph_moveprobe_fly(position, node, mins, maxs, type))
				{
					query->entries[query->entries_count] = i;
					query->entries_count++;
				}
			}

			if (query->entries_count >= NODEGRAPH_QUERY_ENTRIES_LIMIT)
			{
				break;
			}
		}

		if (query->entries_count == 0)
		{
			nodegraph_query_release(queryid);
			queryid = -1;
		}
		else
		{
			g_nodegraph_query_sort_data.queryid = queryid;
			VectorCopy(position, g_nodegraph_query_sort_data.point);

			qsort(query->entries, query->entries_count, sizeof(short), nodegraph_query_sort_function);
		}
	}

	return queryid;
}

// ============================================================================
short nodegraph_graph_query_nodes_in_radius_walk_reachable(short graphid, const vec3_t position, float radius, const vec3_t mins, const vec3_t maxs, float stepheight, float dropheight)
{
	nodegraph_t *nodegraph;

	vec3_t node;
	short i, queryid;
	nodegraph_query_t *query;

	if (graphid < 0 || graphid >= NODEGRAPH_GRAPHSET_SIZE_LIMIT)
	{
		Con_DPrintf("%s, graphid is out of bounds: %d\n", __FUNCTION__, graphid);
		return -1;
	}

	nodegraph = &g_nodegraph_set[graphid];

	queryid = -1;

	for (i = 0; i < NODEGRAPH_QUERIES_COUNT_LIMIT; i++)
	{
		if (!nodegraph_query_is_valid(i))
		{
			queryid = i;
			break;
		}
	}

	if (queryid != -1)
	{
		query = &g_nodegraph_queries[queryid];

		query->graphid = graphid;

		for (i = 0; i < nodegraph->nodes_count; i++)
		{
			nodegraph_graph_get_node(graphid, i, node);
			
			if (VectorDistance(position, node) <= radius)
			{
				if (nodegraph_moveprobe_walk(position, node, mins, maxs, stepheight, dropheight))
				{
					query->entries[query->entries_count] = i;
					query->entries_count++;
				}
			}

			if (query->entries_count >= NODEGRAPH_QUERY_ENTRIES_LIMIT)
			{
				break;
			}
		}

		if (query->entries_count == 0)
		{
			nodegraph_query_release(queryid);
			queryid = -1;
		}
		else
		{
			g_nodegraph_query_sort_data.queryid = queryid;
			VectorCopy(position, g_nodegraph_query_sort_data.point);

			qsort(query->entries, query->entries_count, sizeof(short), nodegraph_query_sort_function);
		}
	}

	return queryid;
}
