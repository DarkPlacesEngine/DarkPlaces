#ifndef NODEGRAPH_H
#define NODEGRAPH_H

// ============================================================================
#define NODEGRAPH_GRAPHSET_SIZE_LIMIT 8

// ============================================================================
#define NODEGRAPH_MOVEPROBE_TYPE_FLY_WHATEVER 0
#define NODEGRAPH_MOVEPROBE_TYPE_FLY_AIR 1
#define NODEGRAPH_MOVEPROBE_TYPE_FLY_WATER 2

// ============================================================================
qbool nodegraph_graphset_clear(void);

qbool nodegraph_graphset_load(void);
qbool nodegraph_graphset_save(void);

qbool nodegraph_graph_clear(short graphid);

short nodegraph_graph_nodes_count(short graphid);

qbool nodegraph_graph_add_node(short graphid, const vec3_t node);
qbool nodegraph_graph_remove_node(short graphid, short nodeid);
qbool nodegraph_graph_is_node_valid(short graphid, short nodeid);

qbool nodegraph_graph_get_node(short graphid, short nodeid, vec3_t outnode);

qbool nodegraph_graph_add_link(short graphid, short nodeidfrom, short nodeidto);
qbool nodegraph_graph_remove_link(short graphid, short nodeidfrom, short nodeidto);
qbool nodegraph_graph_does_link_exist(short graphid, short nodeidfrom, short nodeidto);

short nodegraph_graph_find_nearest_nodeid(short graphid, const vec3_t position);

short nodegraph_graph_query_path(short graphid, short nodeidfrom, short nodeidto);
short nodegraph_graph_query_nodes_linked(short graphid, short nodeid);
short nodegraph_graph_query_nodes_in_radius(short graphid, const vec3_t position, float radius);

qbool nodegraph_query_release(short queryid);
short nodegraph_query_entries_count(short queryid);
qbool nodegraph_query_is_valid(short queryid);
short nodegraph_query_get_graphid(short queryid);
short nodegraph_query_get_nodeid(short queryid, short entryid);

qbool nodegraph_moveprobe_fly(const vec3_t nodefrom, const vec3_t nodeto, const vec3_t mins, const vec3_t maxs, short type);
qbool nodegraph_moveprobe_walk(const vec3_t nodefrom, const vec3_t nodeto, const vec3_t mins, const vec3_t maxs, float stepheight, float dropheight);

short nodegraph_graph_query_nodes_in_radius_fly_reachable(short graphid, const vec3_t position, float radius, const vec3_t mins, const vec3_t maxs, short type);
short nodegraph_graph_query_nodes_in_radius_walk_reachable(short graphid, const vec3_t position, float radius, const vec3_t mins, const vec3_t maxs, float stepheight, float dropheight);

#endif // NODEGRAPH_H
