/**
 * Configuration parser
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014, Institute for Automation of Complex Power Systems, EONERC
 */

#include <stdlib.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <grp.h>
#include <pwd.h>

#include "if.h"
#include "tc.h"
#include "cfg.h"
#include "node.h"
#include "path.h"
#include "utils.h"

int config_parse(const char *filename, config_t *cfg, struct settings *set,
	struct node **nodes, struct path **paths, struct interface **interfaces)
{
	if (!config_read_file(cfg, filename)) {
		error("Failed to parse configuration: %s in %s:%d",
			config_error_text(cfg),
			config_error_file(cfg),
			config_error_line(cfg)
		);
	}

	/* Get and check config sections */
	config_setting_t *cfg_root = config_root_setting(cfg);
	if (!cfg_root || !config_setting_is_group(cfg_root))
		error("Missing global section in config file: %s", filename);

	config_setting_t *cfg_nodes = config_setting_get_member(cfg_root, "nodes");
	if (!cfg_nodes || !config_setting_is_group(cfg_nodes))
		error("Missing node section in config file: %s", filename);

	config_setting_t *cfg_paths = config_setting_get_member(cfg_root, "paths");
	if (!cfg_paths || !config_setting_is_list(cfg_paths))
		error("Missing path section in config file: %s", filename);

	/* Parse global settings */
	config_parse_global(cfg_root, set);

	/* Parse nodes */
	for (int i = 0; i < config_setting_length(cfg_nodes); i++) {
		config_setting_t *cfg_node = config_setting_get_elem(cfg_nodes, i);
		config_parse_node(cfg_node, nodes, interfaces);
	}

	/* Parse paths */
	for (int i = 0; i < config_setting_length(cfg_paths); i++) {
		config_setting_t *cfg_path = config_setting_get_elem(cfg_paths, i);
		config_parse_path(cfg_path, paths, nodes);
	}

	return CONFIG_TRUE;
}

int config_parse_global(config_setting_t *cfg, struct settings *set)
{
	if (!config_setting_lookup_string(cfg, "name", &set->name))
		cerror(cfg, "Missing node name");

	config_setting_lookup_int(cfg, "affinity", &set->affinity);
	config_setting_lookup_int(cfg, "priority", &set->priority);
	config_setting_lookup_int(cfg, "protocol", &set->protocol);

	const char *user = NULL;
	const char *group = NULL;

	config_setting_lookup_string(cfg, "user", &user);
	config_setting_lookup_string(cfg, "group", &group);

	/* Lookup uid and gid */
	if (user) {
		struct passwd *pw = getpwnam(user);
		if (!pw)
			error("Unknown username: '%s'", user);

		set->uid = pw->pw_uid;
		set->gid = pw->pw_gid;
	}

	if (group) {
		struct group *gr = getgrnam(group);
		if (!gr)
			error("Unknown group: '%s'", group);

		set->gid = gr->gr_gid;
	}

	set->cfg = cfg;

	return CONFIG_TRUE;
}

int config_parse_path(config_setting_t *cfg,
	struct path **paths, struct node **nodes)
{
	const char *in_str = NULL;
	const char *out_str = NULL;
	int enabled = 1;
	int reverse = 0;

	struct path *path = (struct path *) malloc(sizeof(struct path));
	if (!path)
		error("Failed to allocate memory for path");
	else
		memset(path, 0, sizeof(struct path));

	/* Required settings */
	if (!config_setting_lookup_string(cfg, "in", &in_str))
		cerror(cfg, "Missing input node for path");

	if (!config_setting_lookup_string(cfg, "out", &out_str))
		cerror(cfg, "Missing output node for path");

	path->in = node_lookup_name(in_str, *nodes);
	if (!path->in)
		cerror(cfg, "Invalid input node '%s'");

	path->out = node_lookup_name(out_str, *nodes);
	if (!path->out)
		cerror(cfg, "Invalid output node '%s'", out_str);

	/* Optional settings */
	config_setting_lookup_bool(cfg, "enabled", &enabled);
	config_setting_lookup_bool(cfg, "reverse", &reverse);
	config_setting_lookup_float(cfg, "rate", &path->rate);

	path->cfg = cfg;

	debug(3, "Loaded path from '%s' to '%s'", path->in->name, path->out->name);

	if (enabled) {
		list_add(*paths, path);

		if (reverse) {
			struct path *path_rev = (struct path *) malloc(sizeof(struct path));
			if (!path_rev)
				error("Failed to allocate memory for path");
			else
				memcpy(path_rev, path, sizeof(struct path));

			path_rev->in = path->out; /* Swap in/out */
			path_rev->out = path->in;

			list_add(*paths, path_rev);
		}
	}
	else {
		free(path);
		warn("  Path is not enabled");
	}

	return 0;
}

int config_parse_node(config_setting_t *cfg,
	struct node **nodes, struct interface **interfaces)
{
	const char *type_str = NULL;
	const char *remote_str = NULL;
	const char *local_str = NULL;

	struct node *node;

	/* Allocate memory */
	node = (struct node *) malloc(sizeof(struct node));
	if (!node)
		error("Failed to allocate memory for node");
	else
		memset(node, 0, sizeof(struct node));

	/* Required settings */
	node->name = config_setting_name(cfg);
	if (!node->name)
		cerror(cfg, "Missing node name");

	if (!config_setting_lookup_int(cfg, "id", &node->id))
		cerror(cfg, "Missing id for node '%s'", node->name);

	if (!config_setting_lookup_string(cfg, "type", &type_str))
		cerror(cfg, "Missing type for node '%s'", node->name);

	if (!config_setting_lookup_string(cfg, "remote", &remote_str))
		cerror(cfg, "Missing remote address for node '%s'", node->name);

	if (!config_setting_lookup_string(cfg, "local", &local_str))
		cerror(cfg, "Missing local address for node '%s'", node->name);

	node->type = node_lookup_type(type_str);
	if (node->type == NODE_INVALID)
		cerror(cfg, "Invalid type '%s' for node '%s'", type_str, node->name);

	if (resolve_addr(local_str, &node->local, AI_PASSIVE))
		cerror(cfg, "Failed to resolve local address '%s' of node '%s'", local_str, node->name);

	if (resolve_addr(remote_str, &node->remote, 0))
		cerror(cfg, "Failed to resolve remote address '%s' of node '%s'", remote_str, node->name);

	/* Optional settings */
	config_setting_t *cfg_netem = config_setting_get_member(cfg, "netem");
	if (cfg_netem) {
		node->netem = (struct netem *) malloc(sizeof(struct netem));
		config_parse_netem(cfg_netem, node->netem);
	}

	/* Determine outgoing interface */
	int index = if_getegress(&node->remote);
	struct interface *i = if_lookup_index(index, *interfaces);
	if (!i) {
		i = malloc(sizeof(struct interface));
		memset(i, 0, sizeof(struct interface));

		i->index = index;

		list_add(*interfaces, i);
	}

	node->mark = 1 + i->refcnt++;
	node->interface = i;

	node->cfg = cfg;

	list_add(*nodes, node);

	debug(3, "Loaded %s node '%s'", type_str, node->name);

	return 0;
}

int config_parse_netem(config_setting_t *cfg, struct netem *em)
{
	em->valid = 0;

	if (config_setting_lookup_string(cfg, "distribution", &em->distribution))
		em->valid |= TC_NETEM_DISTR;
	if (config_setting_lookup_int(cfg, "limit", &em->limit))
		em->valid |= TC_NETEM_LIMIT;
	if (config_setting_lookup_int(cfg, "delay", &em->delay))
		em->valid |= TC_NETEM_DELAY;
	if (config_setting_lookup_int(cfg, "jitter", &em->jitter))
		em->valid |= TC_NETEM_JITTER;
	if (config_setting_lookup_int(cfg, "loss", &em->loss))
		em->valid |= TC_NETEM_LOSS;
	if (config_setting_lookup_int(cfg, "duplicate", &em->duplicate))
		em->valid |= TC_NETEM_DUPL;
	if (config_setting_lookup_int(cfg, "corrupt", &em->corrupt))
		em->valid |= TC_NETEM_CORRUPT;

	// TODO: check values

	return 0;
}
