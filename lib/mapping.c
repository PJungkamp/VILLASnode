/** Sample value remapping for mux.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
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

#include <string.h>

#include "mapping.h"
#include "stats.h"
#include "sample.h"
#include "list.h"
#include "utils.h"

int mapping_parse_str(struct mapping_entry *e, const char *str, struct list *nodes)
{
	int id;
	char *cpy, *node, *type, *field, *subfield, *end;

	cpy = strdup(str);
	if (!cpy)
		return -1;
	
	if (nodes) {
		node = strtok(cpy, ".");
		if (!node)
			goto invalid_format;
		
		e->node = list_lookup(nodes, node);
		if (!e->node)
			goto invalid_format;
		
		type = strtok(NULL, ".[");
		if (!type)
			type = "data";
	}
	else {
		e->node = NULL;
	
		type = strtok(cpy, ".[");
		if (!type)
			goto invalid_format;
	}

	if      (!strcmp(type, "stats")) {
		e->type = MAPPING_TYPE_STATS;
		e->length = 1;

		field = strtok(NULL, ".");
		if (!field)
			goto invalid_format;

		subfield = strtok(NULL, ".");
		if (!subfield)
			goto invalid_format;

		id = stats_lookup_id(field);
		if (id < 0)
			goto invalid_format;

		e->stats.id = id;

		if      (!strcmp(subfield, "total"))
			e->stats.type = MAPPING_STATS_TYPE_TOTAL;
		else if (!strcmp(subfield, "last"))
			e->stats.type = MAPPING_STATS_TYPE_LAST;
		else if (!strcmp(subfield, "lowest"))
			e->stats.type = MAPPING_STATS_TYPE_LOWEST;
		else if (!strcmp(subfield, "highest"))
			e->stats.type = MAPPING_STATS_TYPE_HIGHEST;
		else if (!strcmp(subfield, "mean"))
			e->stats.type = MAPPING_STATS_TYPE_MEAN;
		else if (!strcmp(subfield, "var"))
			e->stats.type = MAPPING_STATS_TYPE_VAR;
		else if (!strcmp(subfield, "stddev"))
			e->stats.type = MAPPING_STATS_TYPE_STDDEV;
		else
			goto invalid_format;
	}
	else if (!strcmp(type, "hdr")) {
		e->type = MAPPING_TYPE_HDR;
		e->length = 1;

		field = strtok(NULL, ".");
		if (!field)
			goto invalid_format;

		if      (!strcmp(field, "sequence"))
			e->hdr.id = MAPPING_HDR_SEQUENCE;
		else if (!strcmp(field, "length"))
			e->hdr.id = MAPPING_HDR_LENGTH;
		else if (!strcmp(field, "id"))
			e->hdr.id = MAPPING_HDR_ID;
		else if (!strcmp(field, "format"))
			e->hdr.id = MAPPING_HDR_FORMAT;
		else
			goto invalid_format;
	}
	else if (!strcmp(type, "ts")) {
		e->type = MAPPING_TYPE_TS;
		e->length = 2;

		field = strtok(NULL, ".");
		if (!field)
			goto invalid_format;

		if      (!strcmp(field, "origin"))
			e->ts.id = MAPPING_TS_ORIGIN;
		else if (!strcmp(field, "received"))
			e->ts.id = MAPPING_TS_RECEIVED;
		else if (!strcmp(field, "sent"))
			e->ts.id = MAPPING_TS_SEND;
		else
			goto invalid_format;
	}
	else if (!strcmp(type, "data")) {
		char *first_str, *last_str, *endptr;

		e->type = MAPPING_TYPE_DATA;

		first_str = strtok(NULL, "-]");
		if (first_str) {
			int first, last;

			last_str = strtok(NULL, "]");
			if (!last_str)
				last_str = first_str; /* single element: data[5] => data[5-5] */

			first = strtoul(first_str, &endptr, 10);
			if (endptr != first_str + strlen(first_str))
				goto invalid_format;

			last = strtoul(last_str, &endptr, 10);
			if (endptr != last_str + strlen(last_str))
				goto invalid_format;

			if (last < first)
				goto invalid_format;

			e->data.offset = first;
			e->length = last - first + 1;
		}
		else {
			e->data.offset = 0;
			e->length = 0; /* Length in unkown.. we will take all values */
		}
	}
	else
		goto invalid_format;

	/* Check that there is no garbage at the end */
	end = strtok(NULL, "");
	if (end)
		goto invalid_format;

	free(cpy);

	return 0;

invalid_format:

	free(cpy);

	return -1;
}

int mapping_parse(struct mapping_entry *e, json_t *j, struct list *nodes)
{
	const char *str;

	str = json_string_value(j);
	if (!str)
		return -1;

	return mapping_parse_str(e, str, nodes);
}

int mapping_update(struct mapping_entry *me, struct sample *remapped, struct sample *original, struct stats *s)
{
	int len = me->length;
	int off = me->offset;
	
	/* me->length == 0 means that we want to take all values */
	if (!len)
		len = original->length;
	
	if (len + off > remapped->capacity)
		return -1;
	
	if (len + off > remapped->length)
		remapped->length = len + off;

	switch (me->type) {
		case MAPPING_TYPE_STATS: {
			struct hist *h = &s->histograms[me->stats.id];

			switch (me->stats.type) {
				case MAPPING_STATS_TYPE_TOTAL:
					sample_set_data_format(remapped, off, SAMPLE_DATA_FORMAT_INT);
					remapped->data[off++].f = h->total;
					break;
				case MAPPING_STATS_TYPE_LAST:
					remapped->data[off++].f = h->last;
					break;
				case MAPPING_STATS_TYPE_HIGHEST:
					remapped->data[off++].f = h->highest;
					break;
				case MAPPING_STATS_TYPE_LOWEST:
					remapped->data[off++].f = h->lowest;
					break;
				case MAPPING_STATS_TYPE_MEAN:
					remapped->data[off++].f = hist_mean(h);
					break;
				case MAPPING_STATS_TYPE_STDDEV:
					remapped->data[off++].f = hist_stddev(h);
					break;
				case MAPPING_STATS_TYPE_VAR:
					remapped->data[off++].f = hist_var(h);
					break;
				default:
					return -1;
			}
		}

		case MAPPING_TYPE_TS: {
			struct timespec *ts;

			switch (me->ts.id) {
				case MAPPING_TS_RECEIVED:
					ts = &original->ts.received;
					break;
				case MAPPING_TS_ORIGIN:
					ts = &original->ts.origin;
					break;
				default:
					return -1;
			}

			sample_set_data_format(remapped, off,   SAMPLE_DATA_FORMAT_INT);
			sample_set_data_format(remapped, off+1, SAMPLE_DATA_FORMAT_INT);

			remapped->data[off++].i = ts->tv_sec;
			remapped->data[off++].i = ts->tv_nsec;

			break;
		}

		case MAPPING_TYPE_HDR:
			sample_set_data_format(remapped, off, SAMPLE_DATA_FORMAT_INT);

			switch (me->hdr.id) {
				case MAPPING_HDR_LENGTH:
					remapped->data[off++].i = original->length;
					break;
				case MAPPING_HDR_SEQUENCE:
					remapped->data[off++].i = original->sequence;
					break;
				case MAPPING_HDR_ID:
					remapped->data[off++].i = original->id;
					break;
				case MAPPING_HDR_FORMAT:
					remapped->data[off++].i = original->format;
					break;
				default:
					return -1;
			}

			break;

		case MAPPING_TYPE_DATA:
			for (int j = me->data.offset; j < len + me->data.offset; j++) {
				if (j >= original->length) {
					sample_set_data_format(remapped, off, SAMPLE_DATA_FORMAT_FLOAT);
					remapped->data[off++].f = 0;
				}
				else {
					sample_set_data_format(remapped, off, sample_get_data_format(original, j));
					remapped->data[off++] = original->data[j];
				}
			}

			break;
	}
	
	return 0;
}

int mapping_remap(struct list *m, struct sample *remapped, struct sample *original, struct stats *s)
{
	int ret;

	/* We copy all the header fields */
	remapped->sequence = original->sequence;
	remapped->pool_off = (char *) sample_pool(original) - (char *) remapped;
	remapped->source   = original->source;
	remapped->ts       = original->ts;

	for (size_t i = 0; i < list_length(m); i++) {
		struct mapping_entry *me = list_at(m, i);

		ret = mapping_update(me, remapped, original, s);
		if (ret)
			return ret;
	}

	return 0;
}
