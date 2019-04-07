
/** Cast hook.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2019, Institute for Automation of Complex Power Systems, EONERC
 * @license GNU General Public License (version 3)
 *
 * VILLASnode
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *********************************************************************************/

/** @addtogroup hooks Hook functions
 * @{
 */

#include <string.h>

#include <villas/hook.hpp>
#include <villas/sample.h>

namespace villas {
namespace node {

class CastHook : public Hook {

protected:
	unsigned signal_index;
	char *signal_name;

	enum signal_type new_type;
	char *new_name;
	char *new_unit;

public:
	CastHook(struct path *p, struct node *n, int fl, int prio, bool en = true) :
		Hook(p, n, fl, prio, en),
		signal_index(-1),
		signal_name(nullptr),
		new_type(SIGNAL_TYPE_INVALID),
		new_name(nullptr),
		new_unit(nullptr)
	{ }

	~CastHook()
	{
		if (signal_name)
			free(signal_name);

		if (new_name)
			free(new_name);

		if (new_unit)
			free(new_unit);
	}

	virtual void prepare()
	{
		struct signal *orig_sig, *new_sig;

		assert(state == STATE_CHECKED);

		if (signal_name) {
			signal_index = vlist_lookup_index(&signals, signal_name);
			if (signal_index < 0)
				throw RuntimeError("Failed to find signal: {}", signal_name);
		}

		char *name, *unit;
		enum signal_type type;

		orig_sig = (struct signal *) vlist_at_safe(&signals, signal_index);

		type = new_type != SIGNAL_TYPE_INVALID ? new_type : orig_sig->type;
		name = new_name ? new_name : orig_sig->name;
		unit = new_unit ? new_unit : orig_sig->unit;

		new_sig = signal_create(name, unit, type);

		vlist_set(&signals, signal_index, new_sig);
		signal_decref(orig_sig);

		state = STATE_PREPARED;
	}

	virtual void parse(json_t *cfg)
	{
		int ret;

		json_error_t err;
		json_t *json_signal;

		assert(state != STATE_STARTED);

		const char *name = nullptr;
		const char *unit = nullptr;
		const char *type = nullptr;

		ret = json_unpack_ex(cfg, &err, 0, "{ s: o, s?: s, s?: s, s?: s }",
			"signal", &json_signal,
			"new_type", &type,
			"new_name", &name,
			"new_unit", &unit
		);
		if (ret)
			throw ConfigError(cfg, err, "node-config-hook-cast");

		switch (json_typeof(json_signal)) {
			case JSON_STRING:
				signal_name = strdup(json_string_value(json_signal));
				break;

			case JSON_INTEGER:
				signal_name = nullptr;
				signal_index = json_integer_value(json_signal);
				break;

			default:
				throw ConfigError(json_signal, "node-config-hook-cast-signals", "Invalid value for setting 'signal'");
		}

		if (type) {
			new_type = signal_type_from_str(type);
			if (new_type == SIGNAL_TYPE_INVALID)
				throw RuntimeError("Invalid signal type: {}", type);
		}
		else
			/* We use this constant to indicate that we dont want to change the type. */
			new_type = SIGNAL_TYPE_INVALID;

		if (name)
			new_name = strdup(name);

		if (unit)
			new_unit = strdup(unit);

		state = STATE_PARSED;
	}

	virtual int process(sample *smp)
	{
		assert(state == STATE_STARTED);

		struct signal *orig_sig = (struct signal *) vlist_at(smp->signals, signal_index);
		struct signal *new_sig  = (struct signal *) vlist_at(&signals,  signal_index);

		signal_data_cast(&smp->data[signal_index], orig_sig, new_sig);

		/* Replace signal descriptors of sample */
		smp->signals = &signals;

		return HOOK_OK;
	}
};

/* Register hook */
static HookPlugin<CastHook> p(
	"cast",
	"Cast signals types",
	HOOK_NODE_READ | HOOK_PATH,
	99
);

} /* namespace node */
} /* namespace villas */

/** @} */

