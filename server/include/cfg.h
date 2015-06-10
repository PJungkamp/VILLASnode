/** Configuration file parser.
 *
 * The server program is configured by a single file.
 * This config file is parsed with a third-party library:
 *  libconfig http://www.hyperrealm.com/libconfig/
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2015, Institute for Automation of Complex Power Systems, EONERC
 *   This file is part of S2SS. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited. 
 *********************************************************************************/

#ifndef _CFG_H_
#define _CFG_H_

#include <libconfig.h>

/* Forward declarations */
struct list;
struct node;
struct path;
struct interface;

struct socket;
struct opal;
struct gtfpga;
struct netem;
struct settings;

/** Parse config file and store settings in supplied struct settings.
 *
 * @param filename The path to the configration file (relative or absolute)
 * @param cfg A initialized libconfig object
 * @param set The global configuration structure
 * @param nodes A linked list of nodes which should be parsed
 * @param paths A linked list of paths which should be parsed
 * @retval 0 Success. Everything went well.
 * @retval <0 Error. Something went wrong.
 */
int config_parse(const char *filename, config_t *cfg, struct settings *set,
	struct list *nodes, struct list *paths);

/** Parse the global section of a configuration file.
 *
 * @param cfg A libconfig object pointing to the root of the file
 * @param set The global configuration file
 * @retval 0 Success. Everything went well.
 * @retval <0 Error. Something went wrong.
 */
int config_parse_global(config_setting_t *cfg, struct settings *set);

/** Parse a single path and add it to the global configuration.
 *
 * @param cfg A libconfig object pointing to the path
 * @param paths Add new paths to this linked list
 * @param nodes A linked list of all existing nodes
 * @retval 0 Success. Everything went well.
 * @retval <0 Error. Something went wrong.
 */
int config_parse_path(config_setting_t *cfg,
	struct list *paths, struct list *nodes);

/** Parse an array or single node and checks if they exist in the "nodes" section.
 *
 * Examples:
 *     out = [ "sintef", "scedu" ]
 *     out = "acs"
 *
 * @param cfg The libconfig object handle for "out".
 * @param nodes The nodes will be added to this list.
 * @param all This list contains all valid nodes.
 */
int config_parse_nodelist(config_setting_t *cfg, struct list *nodes, struct list *all);

/** Parse an array or single hook function.
 *
 * Examples:
 *     hooks = [ "print", "fir" ]
 *     hooks = "log"
 **/
int config_parse_hooklist(config_setting_t *cfg, struct list *hooks);

/** Parse a single node and add it to the global configuration.
 *
 * @param cfg A libconfig object pointing to the node.
 * @param nodes Add new nodes to this linked list.
 * @retval 0 Success. Everything went well.
 * @retval <0 Error. Something went wrong.
 */
int config_parse_node(config_setting_t *cfg, struct list *nodes);

#endif /* _CFG_H_ */
