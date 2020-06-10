#ifndef NODEGRAPH_H
#define NODEGRAPH_H

// ============================================================================
#define NODEGRAPH_GRAPHSET_SIZE_LIMIT 8

// ============================================================================
#define NODEGRAPH_MOVEPROBE_TYPE_FLY_WHATEVER 0
#define NODEGRAPH_MOVEPROBE_TYPE_FLY_AIR 1
#define NODEGRAPH_MOVEPROBE_TYPE_FLY_WATER 2

// ============================================================================
qboolean nodegraph_graphset_clear(void);

qboolean nodegraph_graphset_load(void);
qboolean nodegraph_graphset_save(void);

qboolean nodegraph_graph_clear(short graphid);

short nodegraph_graph_nodes_count(short graphid);

qboolean nodegraph_graph_add_node(short graphid, const vec3_t node);
qboolean nodegraph_graph_remove_node(short graphid, short nodeid);
qboolean nodegraph_graph_is_node_valid(short graphid, short nodeid);

qboolean nodegraph_graph_get_node(short graphid, short nodeid, vec3_t outnode);

qboolean nodegraph_graph_add_link(short graphid, short nodeidfrom, short nodeidto);
qboolean nodegraph_graph_remove_link(short graphid, short nodeidfrom, short nodeidto);
qboolean nodegraph_graph_does_link_exist(short graphid, short nodeidfrom, short nodeidto);

short nodegraph_graph_find_nearest_nodeid(short graphid, const vec3_t position);

short nodegraph_graph_query_path(short graphid, short nodeidfrom, short nodeidto);
short nodegraph_graph_query_nodes_linked(short graphid, short nodeid);
short nodegraph_graph_query_nodes_in_radius(short graphid, const vec3_t position, float radius);

qboolean nodegraph_query_release(short queryid);
short nodegraph_query_entries_count(short queryid);
qboolean nodegraph_query_is_valid(short queryid);
short nodegraph_query_get_graphid(short queryid);
short nodegraph_query_get_nodeid(short queryid, short entryid);

qboolean nodegraph_moveprobe_fly(const vec3_t nodefrom, const vec3_t nodeto, const vec3_t mins, const vec3_t maxs, short type);
qboolean nodegraph_moveprobe_walk(const vec3_t nodefrom, const vec3_t nodeto, const vec3_t mins, const vec3_t maxs, float stepheight, float dropheight);

short nodegraph_graph_query_nodes_in_radius_fly_reachable(short graphid, const vec3_t position, float radius, const vec3_t mins, const vec3_t maxs, short type);
short nodegraph_graph_query_nodes_in_radius_walk_reachable(short graphid, const vec3_t position, float radius, const vec3_t mins, const vec3_t maxs, float stepheight, float dropheight);

#endif // NODEGRAPH_H
